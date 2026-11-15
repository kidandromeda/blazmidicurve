// -----------------------------------------------------------------------------
// src/lv2/MidiCurveUI.cpp
//
// Autor:      blazman
// Licencia:   MIT
// Versión:    2.0.0
// Fecha:      2026-11-15
//
// Interfaz gráfica X11/Cairo y comunicación UI-plugin.
// -----------------------------------------------------------------------------

#include "MidiCurveUI.h"
#include "FileDialog.h"
#include "src/Version.h"
#include "src/core/Interpolator.h"

#include <lv2/atom/util.h>

#include <X11/Xatom.h>
#include <X11/keysym.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#define LOG_UI(fmt, ...) fprintf(stderr, "[MidiCurveUI] " fmt "\n", ##__VA_ARGS__)

// ===================================================================
// Constants
// ===================================================================
static constexpr const char* kUIURI = "https://brania.org/plugins/blazmidicurve#ui";
static constexpr int kNumButtons = 3;
static constexpr const char* kButtonLabels[kNumButtons] = {
    "Reset", "Save", "Load"
};
static constexpr int kBtnW = 62;
static constexpr int kBtnH = 24;
static constexpr int kBtnY = 10;
static constexpr int kBtnGap = 6;

// ===================================================================
// UI Methods
// ===================================================================

int UI::hitTestNode(int px, int py) const {
    const int r2 = NODE_HIT_RADIUS * NODE_HIT_RADIUS;
    for (int i = 0; i < curve.size(); ++i) {
        int dx = px - velToPixelX(curve.nodeAt(i).x);
        int dy = py - velToPixelY(curve.nodeAt(i).y);
        if (dx * dx + dy * dy <= r2)
            return i;
    }
    return -1;
}

// Envia un atom:String plano al plugin por el puerto ui_request (3).
// IMPORTANTE: NO envolver en Sequence. Con atom:eventTransfer el host
// ya mete el atom como un evento dentro del Sequence del puerto.
// Si nosotros enviamos un Sequence, el plugin recibe un Sequence anidado
// y el cuerpo no es texto legible (bug de v1.0.3/1.0.4).
static void writeStringToPlugin(UI* self, const char* msg) {
    if (!self || !self->map || !self->write_function || !msg) {
        LOG_UI("writeStringToPlugin ABORTADO (map=%p write=%p msg=%p)",
               (void*)(self ? self->map : nullptr),
               (void*)(self ? self->write_function : nullptr),
               (void*)msg);
        return;
    }

    const uint32_t msg_len = static_cast<uint32_t>(strlen(msg) + 1);
    uint8_t buf[4096];
    if (sizeof(LV2_Atom) + msg_len > sizeof(buf)) {
        LOG_UI("writeStringToPlugin ABORTADO: mensaje demasiado largo (%u)", msg_len);
        return;
    }

    // Atom String plano: [size|type] + "texto\0"
    auto* atom = reinterpret_cast<LV2_Atom*>(buf);
    atom->size = msg_len;
    atom->type = self->uris.atom_String;
    memcpy(buf + sizeof(LV2_Atom), msg, msg_len);

    const uint32_t total = sizeof(LV2_Atom) + msg_len;
    self->write_function(self->controller, 3, total,
                         self->uris.atom_eventTransfer, atom);
    LOG_UI("writeStringToPlugin OK -> puerto 3 (%u bytes, type=String): %.60s", total, msg);
}

void UI::sendCurveToPlugin() {
    if (curve.size() < 2) {
        LOG_UI("sendCurveToPlugin: curva invalida (%d nodos)", curve.size());
        return;
    }

    char json[2048];
    int pos = 0;
    pos += snprintf(json + pos, sizeof(json) - pos, "{\"nodes\":[");
    for (int i = 0; i < curve.size(); ++i) {
        if (i > 0) pos += snprintf(json + pos, sizeof(json) - pos, ",");
        pos += snprintf(json + pos, sizeof(json) - pos, "{\"x\":%d,\"y\":%d}",
                       curve.nodeAt(i).x, curve.nodeAt(i).y);
    }
    snprintf(json + pos, sizeof(json) - pos, "]}");

    LOG_UI("ENVIANDO CURVA AL PLUGIN (%d nodos): %s", curve.size(), json);
    writeStringToPlugin(this, json);
}

void UI::requestState() {
    LOG_UI("PIDIENDO ESTADO al plugin (request_state)");
    writeStringToPlugin(this, "request_state");
}

void UI::setCurveFromJSON(const char* json) {
    if (!json || strlen(json) == 0) {
        LOG_UI("setCurveFromJSON: JSON vacio");
        return;
    }
    
    Node nodes[128];
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
    
    if (count >= 2) {
        curve.setNodes(nodes, count);
        selected_node = -1;
        needs_redraw = true;
        LOG_UI("CURVA RECIBIDA DEL PLUGIN (%d nodos): %s", count, json);
    } else {
        LOG_UI("setCurveFromJSON: nodos insuficientes (%d)", count);
    }
}

void UI::savePreset() {
    if (curve.size() < 2) {
        LOG_UI("savePreset: curve has less than 2 nodes");
        return;
    }
    
    FileDialog dialog(display, window, FileDialog::SAVE);
    std::string path = dialog.show();
    
    if (!path.empty()) {
        FILE* f = fopen(path.c_str(), "w");
        if (f) {
            fprintf(f, "{\"nodes\":[");
            for (int i = 0; i < curve.size(); ++i) {
                if (i > 0) fprintf(f, ",");
                fprintf(f, "{\"x\":%d,\"y\":%d}", curve.nodeAt(i).x, curve.nodeAt(i).y);
            }
            fprintf(f, "]}");
            fclose(f);
            LOG_UI("savePreset: saved to %s", path.c_str());
        } else {
            LOG_UI("savePreset: failed to open file %s", path.c_str());
        }
    }
}

void UI::loadPreset() {
    FileDialog dialog(display, window, FileDialog::LOAD);
    std::string path = dialog.show();
    
    if (!path.empty()) {
        FILE* f = fopen(path.c_str(), "r");
        if (f) {
            char buf[4096];
            size_t len = fread(buf, 1, sizeof(buf) - 1, f);
            buf[len] = '\0';
            fclose(f);
            
            setCurveFromJSON(buf);
            sendCurveToPlugin();
            needs_redraw = true;
            LOG_UI("loadPreset: loaded from %s", path.c_str());
        } else {
            LOG_UI("loadPreset: failed to open file %s", path.c_str());
        }
    }
}

// ===================================================================
// LV2 UI Callbacks
// ===================================================================

LV2UI_Handle ui_instantiate(const LV2UI_Descriptor*,
                            const char*,
                            const char*,
                            LV2UI_Write_Function write_function,
                            LV2UI_Controller controller,
                            LV2UI_Widget* widget,
                            const LV2_Feature* const* features) {
    LOG_UI("ui_instantiate v%s (autor=%s)", BLAZMIDICURVE_VERSION_STRING, BLAZMIDICURVE_AUTHOR);
    
    if (!features) {
        LOG_UI("ui_instantiate: no features provided");
        return nullptr;
    }
    
    Window parent = 0;
    UI* ui = new UI();
    ui->write_function = write_function;
    ui->controller = controller;
    
    for (int i = 0; features[i] && features[i]->URI; ++i) {
        if (!std::strcmp(features[i]->URI, LV2_UI__parent)) {
            parent = reinterpret_cast<Window>(features[i]->data);
        }
        if (!std::strcmp(features[i]->URI, LV2_URID__map)) {
            ui->map = static_cast<LV2_URID_Map*>(features[i]->data);
        }
        if (!std::strcmp(features[i]->URI, LV2_WORKER__schedule)) {
            ui->worker = static_cast<LV2_Worker_Schedule*>(features[i]->data);
        }
    }
    
    if (!parent) {
        LOG_UI("ui_instantiate: no parent window");
        delete ui;
        return nullptr;
    }
    
    ui->parent = parent;
    if (ui->map) ui->uris.map(ui->map);
    ui->curve.reset();
    
    ui->display = XOpenDisplay(nullptr);
    if (!ui->display) {
        LOG_UI("ui_instantiate: failed to open X display");
        delete ui;
        return nullptr;
    }
    
    XSetWindowAttributes attrs;
    attrs.background_pixel = 0x1E1E1E;
    attrs.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | Button1MotionMask;
    
    ui->window = XCreateWindow(ui->display, parent,
                                0, 0, UI::W, UI::H, 0,
                                CopyFromParent, InputOutput,
                                CopyFromParent,
                                CWBackPixel | CWEventMask, &attrs);
    
    // Titulo de la ventana: nombre, version, autor y categoria
    XStoreName(ui->display, ui->window, BLAZMIDICURVE_WINDOW_TITLE);
    
    Atom xembed = XInternAtom(ui->display, "_XEMBED_INFO", False);
    if (xembed != None) {
        unsigned long data[] = {1, 0};
        XChangeProperty(ui->display, ui->window, xembed, xembed,
                        32, PropModeReplace, (unsigned char*)data, 2);
    }
    
    XMapWindow(ui->display, ui->window);
    ui->gc = XCreateGC(ui->display, ui->window, 0, nullptr);
    
    XWindowAttributes wattr;
    XGetWindowAttributes(ui->display, ui->window, &wattr);
    ui->cairo_surface = cairo_xlib_surface_create(ui->display, ui->window, wattr.visual, UI::W, UI::H);
    ui->cairo_cr = cairo_create(ui->cairo_surface);
    
    uintptr_t win = static_cast<uintptr_t>(ui->window);
    *widget = reinterpret_cast<LV2UI_Widget>(win);
    
    // Pedir el estado actual al plugin. Si este write se pierde,
    // ui_idle lo reintentara hasta que llegue la respuesta.
    ui->state_received = false;
    ui->requestState();
    
    return ui;
}

void ui_cleanup(LV2UI_Handle ui) {
    LOG_UI("ui_cleanup: destroying UI");
    auto* self = static_cast<UI*>(ui);
    if (!self) return;
    
    if (self->cairo_cr)       { cairo_destroy(self->cairo_cr); self->cairo_cr = nullptr; }
    if (self->cairo_surface)  { cairo_surface_destroy(self->cairo_surface); self->cairo_surface = nullptr; }
    if (self->gc && self->display)             XFreeGC(self->display, self->gc);
    if (self->window && self->display)         XDestroyWindow(self->display, self->window);
    if (self->display)        XCloseDisplay(self->display);
    delete self;
}

// Procesa un mensaje de texto recibido del plugin (JSON de curva o nota).
static void handlePluginMessage(UI* self, const char* json) {
    if (!self || !json || !json[0]) return;

    if (strstr(json, "\"note\":")) {
        int note = 0, vel_in = 0, vel_out = 0;
        sscanf(json, "{\"note\":%d,\"vel_in\":%d,\"vel_out\":%d}", &note, &vel_in, &vel_out);
        self->last_note = {static_cast<uint8_t>(note), static_cast<uint8_t>(vel_in), static_cast<uint8_t>(vel_out)};
        self->has_note = true;
        self->note_fade = 100;
        self->needs_redraw = true;
    } else if (strstr(json, "\"nodes\":")) {
        LOG_UI("ui_port_event: recibida curva del plugin: %.80s", json);
        self->setCurveFromJSON(json);
        self->state_received = true;
    } else {
        LOG_UI("ui_port_event: mensaje desconocido: %.60s", json);
    }
}

// Extrae strings de un atom (String directo o Sequence con eventos String).
static void handlePluginAtom(UI* self, const LV2_Atom* atom) {
    if (!self || !atom) return;

    if (atom->type == self->uris.atom_String && atom->size > 0) {
        handlePluginMessage(self, reinterpret_cast<const char*>(atom + 1));
        return;
    }

    if (atom->type == self->uris.atom_Sequence) {
        const auto* seq = reinterpret_cast<const LV2_Atom_Sequence*>(atom);
        LV2_ATOM_SEQUENCE_FOREACH(seq, ev) {
            if (!ev) continue;
            if (ev->body.type == self->uris.atom_String && ev->body.size > 0) {
                const char* msg = reinterpret_cast<const char*>(LV2_ATOM_BODY_CONST(&ev->body));
                handlePluginMessage(self, msg);
            }
        }
        return;
    }

    LOG_UI("ui_port_event: atom type=%u size=%u (no String ni Sequence)", atom->type, atom->size);
}

void ui_port_event(LV2UI_Handle ui, uint32_t port_index,
                   uint32_t, uint32_t format,
                   const void* buffer) {
    auto* self = static_cast<UI*>(ui);
    if (!self || !self->map || !buffer) return;
    if (port_index != 2) return;

    if (format == self->uris.atom_eventTransfer) {
        handlePluginAtom(self, static_cast<const LV2_Atom*>(buffer));
    }
}

// Interpolacion continua lineal entre nodos (igual que el audio).
// El suavizado visual viene de muestrear 1 punto por pixel, no de curvas Bezier.
static float sampleCurveY(const Curve& curve, float x) {
    if (curve.size() < 2) return x;
    if (x <= curve.nodeAt(0).x) return static_cast<float>(curve.nodeAt(0).y);
    if (x >= curve.nodeAt(curve.size() - 1).x)
        return static_cast<float>(curve.nodeAt(curve.size() - 1).y);

    for (int i = 0; i < curve.size() - 1; ++i) {
        const Node& n0 = curve.nodeAt(i);
        const Node& n1 = curve.nodeAt(i + 1);
        if (x >= n0.x && x <= n1.x) {
            if (n1.x == n0.x) return static_cast<float>(n1.y);
            float t = (x - n0.x) / static_cast<float>(n1.x - n0.x);
            return n0.y + t * (n1.y - n0.y);
        }
    }
    return x;
}

static void render(UI* self) {
    if (!self || !self->cairo_cr) return;
    
    cairo_t* cr = self->cairo_cr;
    
    // Fondo
    cairo_set_source_rgb(cr, 0.10, 0.10, 0.11);
    cairo_paint(cr);
    
    // Area del grid
    cairo_set_source_rgb(cr, 0.13, 0.13, 0.14);
    cairo_rectangle(cr, UI::MARGIN_L, UI::MARGIN_T, UI::GRID_W, UI::GRID_H);
    cairo_fill(cr);
    
    // Rejilla secundaria (cada 16)
    cairo_set_line_width(cr, 0.6);
    for (int t = 0; t <= 127; t += 16) {
        float px = self->velToPixelXf(static_cast<float>(t));
        float py = self->velToPixelYf(static_cast<float>(t));
        
        bool major = (t == 0 || t == 64 || t == 127);
        if (major)
            cairo_set_source_rgb(cr, 0.28, 0.28, 0.30);
        else
            cairo_set_source_rgb(cr, 0.18, 0.18, 0.20);
        
        cairo_move_to(cr, px, UI::MARGIN_T);
        cairo_line_to(cr, px, UI::MARGIN_T + UI::GRID_H);
        cairo_stroke(cr);
        
        cairo_move_to(cr, UI::MARGIN_L, py);
        cairo_line_to(cr, UI::MARGIN_L + UI::GRID_W, py);
        cairo_stroke(cr);
    }
    
    // Diagonal de referencia y=x (lineal)
    cairo_set_source_rgba(cr, 0.45, 0.45, 0.48, 0.35);
    cairo_set_line_width(cr, 1.0);
    cairo_set_dash(cr, (double[]){4.0, 4.0}, 2, 0);
    cairo_move_to(cr, self->velToPixelXf(0), self->velToPixelYf(0));
    cairo_line_to(cr, self->velToPixelXf(127), self->velToPixelYf(127));
    cairo_stroke(cr);
    cairo_set_dash(cr, nullptr, 0, 0);
    
    // Marco del grid
    cairo_set_source_rgb(cr, 0.38, 0.38, 0.42);
    cairo_set_line_width(cr, 1.2);
    cairo_rectangle(cr, UI::MARGIN_L, UI::MARGIN_T, UI::GRID_W, UI::GRID_H);
    cairo_stroke(cr);
    
    // ---- Curva: un punto por pixel horizontal (suave, sin escalones) ----
    // Halo suave debajo
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_source_rgba(cr, 0.15, 0.75, 0.35, 0.20);
    cairo_set_line_width(cr, 4.0);
    cairo_move_to(cr, self->velToPixelXf(0.f), self->velToPixelYf(sampleCurveY(self->curve, 0.f)));
    for (int px = 1; px <= UI::GRID_W; ++px) {
        float xin = px * 127.f / UI::GRID_W;
        float yout = sampleCurveY(self->curve, xin);
        cairo_line_to(cr, UI::MARGIN_L + px, self->velToPixelYf(yout));
    }
    cairo_stroke(cr);
    
    // Linea principal fina
    cairo_set_source_rgb(cr, 0.25, 0.92, 0.45);
    cairo_set_line_width(cr, 1.25);
    cairo_move_to(cr, self->velToPixelXf(0.f), self->velToPixelYf(sampleCurveY(self->curve, 0.f)));
    for (int px = 1; px <= UI::GRID_W; ++px) {
        float xin = px * 127.f / UI::GRID_W;
        float yout = sampleCurveY(self->curve, xin);
        cairo_line_to(cr, UI::MARGIN_L + px, self->velToPixelYf(yout));
    }
    cairo_stroke(cr);
    
    // Feedback de nota MIDI
    if (self->has_note && self->note_fade > 0) {
        float x = self->velToPixelXf(self->last_note.velocity_in);
        float y_orig = self->velToPixelYf(self->last_note.velocity_in);
        float y_mapped = self->velToPixelYf(self->last_note.velocity_out);
        float alpha = self->note_fade / 100.0f;
        
        cairo_set_source_rgba(cr, 1.0, 0.55, 0.1, alpha * 0.9);
        cairo_arc(cr, x, y_orig, 5.5, 0, 2 * M_PI);
        cairo_fill(cr);
        
        cairo_set_source_rgba(cr, 0.2, 0.85, 1.0, alpha * 0.9);
        cairo_arc(cr, x, y_mapped, 5.5, 0, 2 * M_PI);
        cairo_fill(cr);
        
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, alpha * 0.35);
        cairo_set_line_width(cr, 0.8);
        cairo_move_to(cr, x, y_orig);
        cairo_line_to(cr, x, y_mapped);
        cairo_stroke(cr);
        
        char text[64];
        snprintf(text, sizeof(text), "Note %d  %d→%d",
                 self->last_note.note, self->last_note.velocity_in, self->last_note.velocity_out);
        cairo_set_source_rgba(cr, 0.9, 0.9, 0.9, alpha);
        cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 11.0);
        cairo_move_to(cr, UI::MARGIN_L + 8, UI::MARGIN_T + UI::GRID_H - 8);
        cairo_show_text(cr, text);
        
        self->note_fade -= 2;
    }
    
    // Nodos
    for (int i = 0; i < self->curve.size(); ++i) {
        float px = self->velToPixelXf(static_cast<float>(self->curve.nodeAt(i).x));
        float py = self->velToPixelYf(static_cast<float>(self->curve.nodeAt(i).y));
        bool sel = (i == self->selected_node);
        
        // Anillo exterior
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.45);
        cairo_arc(cr, px, py, UI::NODE_RADIUS + 1.5, 0, 2 * M_PI);
        cairo_fill(cr);
        
        if (sel)
            cairo_set_source_rgb(cr, 1.0, 0.65, 0.15);
        else
            cairo_set_source_rgb(cr, 0.95, 0.95, 0.97);
        cairo_arc(cr, px, py, UI::NODE_RADIUS, 0, 2 * M_PI);
        cairo_fill(cr);
        
        // Punto central
        cairo_set_source_rgb(cr, sel ? 0.35f : 0.15f, sel ? 0.2f : 0.15f, sel ? 0.05f : 0.18f);
        cairo_arc(cr, px, py, 1.4, 0, 2 * M_PI);
        cairo_fill(cr);
    }
    
    // ---- Leyendas de ejes ----
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 10.0);
    cairo_set_source_rgb(cr, 0.65, 0.65, 0.68);
    
    // Numeros en eje X (Vel In)
    for (int t : {0, 32, 64, 96, 127}) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", t);
        cairo_text_extents_t te;
        cairo_text_extents(cr, buf, &te);
        float px = self->velToPixelXf(static_cast<float>(t));
        cairo_move_to(cr, px - te.width / 2.0, UI::MARGIN_T + UI::GRID_H + 16);
        cairo_show_text(cr, buf);
    }
    
    // Numeros en eje Y (Vel Out)
    for (int t : {0, 32, 64, 96, 127}) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", t);
        cairo_text_extents_t te;
        cairo_text_extents(cr, buf, &te);
        float py = self->velToPixelYf(static_cast<float>(t));
        cairo_move_to(cr, UI::MARGIN_L - te.width - 8, py + te.height / 2.0);
        cairo_show_text(cr, buf);
    }
    
    // Titulos de ejes
    cairo_set_font_size(cr, 11.0);
    cairo_set_source_rgb(cr, 0.78, 0.78, 0.82);
    
    // Eje X: "Velocidad entrada"
    {
        const char* label = "Velocidad entrada →";
        cairo_text_extents_t te;
        cairo_text_extents(cr, label, &te);
        cairo_move_to(cr, UI::MARGIN_L + (UI::GRID_W - te.width) / 2.0,
                      UI::MARGIN_T + UI::GRID_H + 34);
        cairo_show_text(cr, label);
    }
    
    // Eje Y: "Velocidad salida" rotado
    {
        const char* label = "Velocidad salida →";
        cairo_text_extents_t te;
        cairo_text_extents(cr, label, &te);
        cairo_save(cr);
        float cx = 14.0f;
        float cy = UI::MARGIN_T + UI::GRID_H / 2.0f;
        cairo_translate(cr, cx, cy);
        cairo_rotate(cr, -M_PI / 2.0);
        cairo_move_to(cr, -te.width / 2.0, te.height / 2.0);
        cairo_show_text(cr, label);
        cairo_restore(cr);
    }
    
    // Botones
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 11.0);
    
    for (int i = 0; i < UI::NUM_BUTTONS; ++i) {
        int bx = 10 + i * (UI::BTN_W + UI::BTN_GAP);
        
        // Fondo redondeado simple
        cairo_set_source_rgb(cr, 0.22, 0.22, 0.26);
        cairo_rectangle(cr, bx, UI::BTN_Y, UI::BTN_W, UI::BTN_H);
        cairo_fill(cr);
        
        cairo_set_source_rgb(cr, 0.40, 0.40, 0.46);
        cairo_set_line_width(cr, 0.8);
        cairo_rectangle(cr, bx + 0.5, UI::BTN_Y + 0.5, UI::BTN_W - 1, UI::BTN_H - 1);
        cairo_stroke(cr);
        
        cairo_set_source_rgb(cr, 0.92, 0.92, 0.94);
        cairo_text_extents_t te;
        cairo_text_extents(cr, kButtonLabels[i], &te);
        cairo_move_to(cr, bx + (UI::BTN_W - te.width) / 2.0,
                      UI::BTN_Y + (UI::BTN_H + te.height) / 2.0);
        cairo_show_text(cr, kButtonLabels[i]);
    }
    
    cairo_surface_flush(self->cairo_surface);
}

int ui_idle(LV2UI_Handle ui) {
    auto* self = static_cast<UI*>(ui);
    if (!self || !self->display) return 0;

    // Si aun no hemos recibido el estado del plugin, reintentar la
    // peticion periodicamente (por si el primer write se perdio).
    if (!self->state_received) {
        if (++self->state_request_counter >= 30) {
            self->state_request_counter = 0;
            self->requestState();
        }
    }

    while (XPending(self->display)) {
        XEvent ev;
        XNextEvent(self->display, &ev);
        
        switch (ev.type) {
        case Expose:
            if (ev.xexpose.count == 0)
                self->needs_redraw = true;
            break;
            
        case ButtonPress:
            if (ev.xbutton.button == Button1) {
                bool is_double = (ev.xbutton.time - self->last_click_time < 400 &&
                                  std::abs(ev.xbutton.x - self->last_click_x) < 10 &&
                                  std::abs(ev.xbutton.y - self->last_click_y) < 10);
                self->last_click_time = ev.xbutton.time;
                self->last_click_x = ev.xbutton.x;
                self->last_click_y = ev.xbutton.y;
                
                if (is_double) {
                    int hit = self->hitTestNode(ev.xbutton.x, ev.xbutton.y);
                    if (hit >= 0) {
                        self->curve.removeNode(hit);
                        self->selected_node = -1;
                    } else {
                        int vx = self->pixelToVelX(ev.xbutton.x);
                        int vy = self->pixelToVelY(ev.xbutton.y);
                        self->curve.addNode(vx, vy);
                    }
                    self->sendCurveToPlugin();
                } else {
                    for (int i = 0; i < UI::NUM_BUTTONS; ++i) {
                        int bx = 10 + i * (UI::BTN_W + UI::BTN_GAP);
                        if (ev.xbutton.x >= bx && ev.xbutton.x < bx + UI::BTN_W &&
                            ev.xbutton.y >= UI::BTN_Y && ev.xbutton.y < UI::BTN_Y + UI::BTN_H) {
                            switch (i) {
                            case 0: self->curve.reset(); break;
                            case 1: self->savePreset(); break;
                            case 2: self->loadPreset(); break;
                            }
                            self->sendCurveToPlugin();
                            self->needs_redraw = true;
                            return 0;
                        }
                    }
                    
                    int hit = self->hitTestNode(ev.xbutton.x, ev.xbutton.y);
                    if (hit >= 0) {
                        self->selected_node = hit;
                        self->dragging = true;
                    } else {
                        self->selected_node = -1;
                    }
                }
                self->needs_redraw = true;
            }
            break;
            
        case ButtonRelease:
            if (self->dragging) {
                self->dragging = false;
                self->sendCurveToPlugin();
            }
            break;
            
        case MotionNotify:
            if (self->dragging && self->selected_node >= 0 && 
                self->selected_node < self->curve.size()) {
                int newX = self->pixelToVelX(ev.xmotion.x);
                int newY = self->pixelToVelY(ev.xmotion.y);
                self->curve.moveNode(self->selected_node, newX, newY);
                self->needs_redraw = true;
            }
            break;
        }
    }

    if (self->needs_redraw) {
        render(self);
        self->needs_redraw = false;
    }
    return 0;
}

const void* ui_extension_data(const char* uri) {
    static const LV2UI_Idle_Interface idle_iface = { ui_idle };
    if (!std::strcmp(uri, LV2_UI__idleInterface))
        return &idle_iface;
    return nullptr;
}

static LV2UI_Descriptor ui_descriptor = {
    kUIURI, ui_instantiate, ui_cleanup, ui_port_event, ui_extension_data
};

extern "C" {
const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index) {
    return (index == 0) ? &ui_descriptor : nullptr;
}
}
