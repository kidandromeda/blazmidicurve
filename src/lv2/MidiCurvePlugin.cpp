// -----------------------------------------------------------------------------
// src/lv2/MidiCurvePlugin.cpp
//
// Autor:      blazman
// Licencia:   MIT
// Versión:    2.0.0
// Fecha:      2026-11-15
//
// Implementación del plugin LV2: DSP, estado y persistencia de la curva.
// -----------------------------------------------------------------------------

#include "MidiCurvePlugin.h"
#include "src/Version.h"

#include <cstdlib>
#include <cstring>
#include <cstdio>

static constexpr const char* kURI = "https://brania.org/plugins/blazmidicurve";

// Logs SOLO en eventos raros (save/restore/aplicar curva/request).
// Nunca en el camino caliente de cada sample MIDI.
#define LOG(fmt, ...) fprintf(stderr, "[MidiCurve] " fmt "\n", ##__VA_ARGS__)

static void serializeCurveJSON(const Curve& curve, char* buf, size_t bufsize) {
    int pos = 0;
    pos += snprintf(buf + pos, bufsize - pos, "{\"nodes\":[");
    for (int i = 0; i < curve.size(); ++i) {
        if (i > 0) pos += snprintf(buf + pos, bufsize - pos, ",");
        pos += snprintf(buf + pos, bufsize - pos, "{\"x\":%d,\"y\":%d}",
                       curve.nodeAt(i).x, curve.nodeAt(i).y);
    }
    snprintf(buf + pos, bufsize - pos, "]}");
}

static bool parseCurveJSON(const char* json, Node* nodes, int* out_count) {
    if (!json || !nodes || !out_count) return false;

    int count = 0;
    const char* p = json;
    while (*p && count < 128) {
        p = strstr(p, "\"x\":");
        if (!p) break;
        p += 4;
        int x = atoi(p);

        p = strstr(p, "\"y\":");
        if (!p) break;
        p += 4;
        int y = atoi(p);

        nodes[count++] = {x, y};
        p++;
    }

    if (count < 2) return false;
    *out_count = count;
    return true;
}

void Plugin::applyNodes(const Node* nodes, int count) {
    if (!nodes || count < 2) return;
    curve.setNodes(nodes, count);
    Interpolator::generate(curve, velocityMap);
    serializeCurveJSON(curve, state_json, sizeof(state_json));
    state_json_len = strlen(state_json);
    state_dirty = true;
    LOG("CURVA GUARDADA en plugin (%d nodos, json_len=%zu): %s",
        count, state_json_len, state_json);
}

void Plugin::sendState() {
    if (!ui_notify || state_json_len == 0 || !map) {
        LOG("sendState ABORTADO (ui_notify=%p json_len=%zu map=%p)",
            (void*)ui_notify, state_json_len, (void*)map);
        return;
    }

    const uint32_t cap = sizeof(LV2_Atom) + ui_notify->atom.size;
    const uint32_t needed = sizeof(LV2_Atom_Sequence_Body) + sizeof(LV2_Atom_Event) + state_json_len + 8;

    if (cap < needed) {
        LOG("sendState ABORTADO: buffer pequeno (cap=%u needed=%u)", cap, needed);
        return;
    }

    LV2_Atom_Forge forge;
    lv2_atom_forge_init(&forge, map);
    lv2_atom_forge_set_buffer(&forge, reinterpret_cast<uint8_t*>(ui_notify), cap);

    LV2_Atom_Forge_Frame frame;
    if (!lv2_atom_forge_sequence_head(&forge, &frame, 0)) {
        LOG("sendState ABORTADO: forge_sequence_head fallo");
        return;
    }
    lv2_atom_forge_frame_time(&forge, 0);
    lv2_atom_forge_atom(&forge, state_json_len, uris.atom_String);
    lv2_atom_forge_write(&forge, state_json, state_json_len);
    lv2_atom_forge_pop(&forge, &frame);
    LOG("CURVA ENVIADA a la UI (%zu bytes)", state_json_len);
}

void Plugin::sendMidiNoteToUI(uint8_t note, uint8_t vel_in, uint8_t vel_out) {
    if (!ui_notify || !map || state_dirty) return; // no pisar un envio de estado pendiente

    char buf[64];
    int len = snprintf(buf, sizeof(buf), "{\"note\":%d,\"vel_in\":%d,\"vel_out\":%d}", note, vel_in, vel_out);

    const uint32_t cap = sizeof(LV2_Atom) + ui_notify->atom.size;
    const uint32_t needed = sizeof(LV2_Atom_Sequence_Body) + sizeof(LV2_Atom_Event) + len + 8;
    if (cap < needed) return;

    LV2_Atom_Forge forge;
    lv2_atom_forge_init(&forge, map);
    lv2_atom_forge_set_buffer(&forge, reinterpret_cast<uint8_t*>(ui_notify), cap);

    LV2_Atom_Forge_Frame frame;
    if (!lv2_atom_forge_sequence_head(&forge, &frame, 0)) return;
    lv2_atom_forge_frame_time(&forge, 0);
    lv2_atom_forge_atom(&forge, len, uris.atom_String);
    lv2_atom_forge_write(&forge, buf, len);
    lv2_atom_forge_pop(&forge, &frame);
}

static LV2_Handle instantiate(const LV2_Descriptor*, double, const char*,
                               const LV2_Feature* const* features) {
    LOG("instantiate v%s", BLAZMIDICURVE_VERSION_STRING);
    auto* p = new Plugin();
    for (int i = 0; features[i] && features[i]->URI; ++i) {
        if (!std::strcmp(features[i]->URI, LV2_URID__map))
            p->map = static_cast<LV2_URID_Map*>(features[i]->data);
        if (!std::strcmp(features[i]->URI, LV2_WORKER__schedule))
            p->worker = static_cast<LV2_Worker_Schedule*>(features[i]->data);
    }
    if (!p->map) {
        LOG("instantiate FALLO: sin URID map");
        delete p;
        return nullptr;
    }

    p->uris.map(p->map);
    lv2_atom_forge_init(&p->forge, p->map);

    p->curve.reset();
    Interpolator::generate(p->curve, p->velocityMap);
    serializeCurveJSON(p->curve, p->state_json, sizeof(p->state_json));
    p->state_json_len = strlen(p->state_json);
    p->state_dirty = true;
    LOG("instantiate OK, curva inicial: %s", p->state_json);
    return p;
}

static void connect_port(LV2_Handle instance, uint32_t port, void* data) {
    auto* p = static_cast<Plugin*>(instance);
    switch (port) {
    case PORT_MIDI_IN:     p->midi_in     = static_cast<const LV2_Atom_Sequence*>(data); break;
    case PORT_MIDI_OUT:    p->midi_out    = static_cast<LV2_Atom_Sequence*>(data); break;
    case PORT_UI_NOTIFY: {
        auto* buf = static_cast<LV2_Atom_Sequence*>(data);
        if (buf && !p->ui_notify) {
            p->state_dirty = true;
        }
        p->ui_notify = buf;
        break;
    }
    case PORT_UI_REQUEST:  p->ui_request  = static_cast<const LV2_Atom_Sequence*>(data); break;
    }
}

static void activate(LV2_Handle) {}
static void deactivate(LV2_Handle) {}

// Procesa mensajes de la UI en el puerto ui_request.
// La UI envia atom:String plano via eventTransfer; el host lo mete
// como un evento en el Sequence del puerto.
static void processUiRequest(Plugin* p) {
    if (!p->ui_request) return;

    LV2_ATOM_SEQUENCE_FOREACH(p->ui_request, ev) {
        if (!ev || ev->body.size == 0) continue;

        const char* msg = nullptr;

        if (ev->body.type == p->uris.atom_String) {
            msg = static_cast<const char*>(LV2_ATOM_BODY_CONST(&ev->body));
        } else if (ev->body.type == p->uris.atom_Sequence) {
            // Sequence anidado (hosts antiguos / forge incorrecto)
            const auto* nested = reinterpret_cast<const LV2_Atom_Sequence*>(&ev->body);
            LV2_ATOM_SEQUENCE_FOREACH(nested, nev) {
                if (nev && nev->body.type == p->uris.atom_String && nev->body.size > 0) {
                    msg = static_cast<const char*>(LV2_ATOM_BODY_CONST(&nev->body));
                    break;
                }
            }
        }

        if (!msg || !msg[0]) {
            LOG("ui_request: evento type=%u size=%u SIN texto legible",
                ev->body.type, ev->body.size);
            continue;
        }

        LOG("ui_request recibido (type=%u size=%u): %.80s",
            ev->body.type, ev->body.size, msg);

        if (strstr(msg, "request_state")) {
            LOG("REQUEST_STATE: UI pide la curva. Respondiendo...");
            p->state_dirty = true;
        } else if (strstr(msg, "\"nodes\"")) {
            Node nodes[128];
            int count = 0;
            if (parseCurveJSON(msg, nodes, &count)) {
                LOG("UI envio nueva curva (%d nodos). Aplicando...", count);
                p->applyNodes(nodes, count);
            } else {
                LOG("UI envio JSON de nodos INVALIDO");
            }
        }
    }
}

static void run(LV2_Handle instance, uint32_t) {
    auto* p = static_cast<Plugin*>(instance);
    if (!p || !p->midi_out || !p->map) return;

    // Worker legacy (por si algun host aun lo usa)
    if (p->pending_update.load(std::memory_order_acquire)) {
        p->pending_update.store(false, std::memory_order_release);
        if (p->pending_count >= 2 && p->pending_count <= 128) {
            LOG("worker pending_update: aplicando %d nodos", p->pending_count);
            p->applyNodes(p->pending_nodes, p->pending_count);
        }
    }

    // PRIMERO: leer mensajes de la UI (guardar curva / pedir estado)
    processUiRequest(p);

    const uint32_t cap = sizeof(LV2_Atom) + p->midi_out->atom.size;
    if (cap < sizeof(LV2_Atom_Sequence_Body) + 64) return;

    lv2_atom_forge_set_buffer(&p->forge, reinterpret_cast<uint8_t*>(p->midi_out), cap);
    lv2_atom_forge_sequence_head(&p->forge, &p->frame, 0);

    if (p->midi_in) {
        LV2_ATOM_SEQUENCE_FOREACH(p->midi_in, ev) {
            if (!ev) continue;

            if (ev->body.type == p->uris.midi_MidiEvent && ev->body.size >= 3) {
                const uint8_t* data = reinterpret_cast<const uint8_t*>(ev + 1);
                uint8_t status = data[0] & 0xF0;

                if (status == 0x90 && data[2] > 0) {
                    uint8_t note = data[1];
                    uint8_t vel = data[2];
                    if (vel > 127) vel = 127;
                    uint8_t mapped_vel = p->velocityMap[vel];

                    p->sendMidiNoteToUI(note, vel, mapped_vel);

                    uint8_t modified[3] = {data[0], note, mapped_vel};
                    lv2_atom_forge_frame_time(&p->forge, ev->time.frames);
                    lv2_atom_forge_atom(&p->forge, 3, p->uris.midi_MidiEvent);
                    lv2_atom_forge_write(&p->forge, modified, 3);
                } else {
                    lv2_atom_forge_frame_time(&p->forge, ev->time.frames);
                    lv2_atom_forge_atom(&p->forge, ev->body.size, ev->body.type);
                    lv2_atom_forge_write(&p->forge, data, ev->body.size);
                }
            }
        }
    }

    lv2_atom_forge_pop(&p->forge, &p->frame);

    if (p->state_dirty && p->ui_notify) {
        p->sendState();
        p->state_dirty = false;
    }
}

static void cleanup(LV2_Handle instance) {
    LOG("cleanup");
    delete static_cast<Plugin*>(instance);
}

static LV2_State_Status state_save(LV2_Handle instance,
                                    LV2_State_Store_Function store,
                                    LV2_State_Handle handle,
                                    uint32_t, const LV2_Feature* const*) {
    auto* p = static_cast<Plugin*>(instance);
    if (!p || !store || !handle) return LV2_STATE_ERR_UNKNOWN;

    LOG("STATE_SAVE (sesion Ardour): guardando %zu bytes: %s",
        p->state_json_len, p->state_json);
    store(handle, p->uris.curve_state, p->state_json, p->state_json_len + 1,
          p->uris.atom_String, LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE);
    return LV2_STATE_SUCCESS;
}

static LV2_State_Status state_restore(LV2_Handle instance,
                                       LV2_State_Retrieve_Function retrieve,
                                       LV2_State_Handle handle,
                                       uint32_t, const LV2_Feature* const*) {
    auto* p = static_cast<Plugin*>(instance);
    if (!p || !retrieve || !handle) return LV2_STATE_ERR_UNKNOWN;

    size_t size = 0;
    uint32_t type = 0;
    const void* data = retrieve(handle, p->uris.curve_state, &size, &type, nullptr);

    if (data && size > 0 && type == p->uris.atom_String) {
        const char* json = static_cast<const char*>(data);
        LOG("STATE_RESTORE (sesion Ardour): recibidos %zu bytes: %s", size, json);
        Node nodes[128];
        int count = 0;
        if (parseCurveJSON(json, nodes, &count)) {
            p->curve.setNodes(nodes, count);
            LOG("STATE_RESTORE OK: %d nodos aplicados", count);
        } else {
            LOG("STATE_RESTORE FALLO parseo, usando lineal");
            p->curve.reset();
        }
    } else {
        LOG("STATE_RESTORE: sin estado previo (type=%u size=%zu)", type, size);
        p->curve.reset();
    }

    Interpolator::generate(p->curve, p->velocityMap);
    serializeCurveJSON(p->curve, p->state_json, sizeof(p->state_json));
    p->state_json_len = strlen(p->state_json);
    p->state_dirty = true;
    return LV2_STATE_SUCCESS;
}

// Worker se mantiene por compatibilidad, pero la UI ya no lo usa.
static LV2_Worker_Status work(LV2_Handle, LV2_Worker_Respond_Function respond,
                               LV2_Worker_Respond_Handle handle,
                               uint32_t size, const void* data) {
    if (!respond || !data || size == 0) return LV2_WORKER_ERR_UNKNOWN;
    Node nodes[128];
    int count = 0;
    if (parseCurveJSON(static_cast<const char*>(data), nodes, &count)) {
        respond(handle, sizeof(Node) * count, nodes);
    }
    return LV2_WORKER_SUCCESS;
}

static LV2_Worker_Status work_response(LV2_Handle instance, uint32_t size, const void* data) {
    auto* p = static_cast<Plugin*>(instance);
    if (!p || !data) return LV2_WORKER_ERR_UNKNOWN;
    int count = size / sizeof(Node);
    if (count >= 2 && count <= 128) {
        std::memcpy(p->pending_nodes, data, size);
        p->pending_count = count;
        p->pending_update.store(true, std::memory_order_release);
    }
    return LV2_WORKER_SUCCESS;
}

static const void* extension_data(const char* uri) {
    static const LV2_State_Interface state_iface = { state_save, state_restore };
    static const LV2_Worker_Interface worker_iface = { work, work_response, nullptr };
    if (!std::strcmp(uri, LV2_STATE__interface)) return &state_iface;
    if (!std::strcmp(uri, LV2_WORKER__interface)) return &worker_iface;
    return nullptr;
}

static LV2_Descriptor descriptor = {
    kURI, instantiate, connect_port, activate, run, deactivate, cleanup, extension_data
};

extern "C" {
const LV2_Descriptor* lv2_descriptor(uint32_t index) {
    return (index == 0) ? &descriptor : nullptr;
}
}
