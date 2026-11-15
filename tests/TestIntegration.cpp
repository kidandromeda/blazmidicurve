// -----------------------------------------------------------------------------
// tests/TestIntegration.cpp
//
// Autor:      blazman
// Licencia:   MIT
// Versión:    2.0.0
// Fecha:      2026-11-15
//
// Pruebas de integración del plugin LV2 mediante lilv.
// -----------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cassert>

#include <lilv/lilv.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/forge.h>
#include <lv2/atom/util.h>
#include <lv2/midi/midi.h>
#include <lv2/urid/urid.h>
#include <lv2/worker/worker.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name, expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "  FAIL: %s\n  (%s)\n", name, #expr); \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while(0)

// Simple URID map implementation for testing
typedef struct {
    LV2_URID_Map map;
    LV2_URID_Unmap unmap;
    LV2_URID urids[256];
    const char* strings[256];
    int count;
} TestURIDMap;

static LV2_URID test_map(LV2_URID_Map_Handle handle, const char* uri) {
    TestURIDMap* m = (TestURIDMap*)handle;
    for (int i = 0; i < m->count; i++)
        if (!strcmp(m->strings[i], uri)) return m->urids[i];
    m->urids[m->count] = m->count + 1;
    m->strings[m->count] = uri;
    return m->urids[m->count++];
}

static const char* test_unmap(LV2_URID_Unmap_Handle handle, LV2_URID urid) {
    TestURIDMap* m = (TestURIDMap*)handle;
    for (int i = 0; i < m->count; i++)
        if (m->urids[i] == urid) return m->strings[i];
    return nullptr;
}

// Worker schedule (no-op for test)
static LV2_Worker_Status test_worker_schedule(LV2_Worker_Schedule_Handle handle,
                                               uint32_t size, const void* data) {
    (void)handle; (void)size; (void)data;
    return LV2_WORKER_SUCCESS;
}

int main() {
    // Discover the plugin via lilv
    LilvWorld* world = lilv_world_new();
    lilv_world_load_all(world);

    LilvNode* uri = lilv_new_uri(world, "https://brania.org/plugins/blazmidicurve");
    const LilvPlugin* plugin = lilv_plugins_get_by_uri(
        lilv_world_get_all_plugins(world), uri);
    
    TEST("Plugin found", plugin != nullptr);
    if (!plugin) {
        lilv_node_free(uri);
        lilv_world_free(world);
        return 1;
    }

    // Instantiate the plugin
    // Set up features
    TestURIDMap urid_map;
    urid_map.count = 0;
    urid_map.map.handle = &urid_map;
    urid_map.map.map = test_map;
    urid_map.unmap.handle = &urid_map;
    urid_map.unmap.unmap = test_unmap;

    LV2_Worker_Schedule worker_sched;
    worker_sched.handle = nullptr;
    worker_sched.schedule_work = test_worker_schedule;

    LV2_Feature feat_map = { LV2_URID__map, &urid_map.map };
    LV2_Feature feat_worker = { LV2_WORKER__schedule, &worker_sched };
    const LV2_Feature* features[] = { &feat_map, &feat_worker, nullptr };

    // Get sample rate from the plugin (or default to 48000)
    double sample_rate = 48000.0;

    LilvInstance* instance = lilv_plugin_instantiate(
        plugin, sample_rate, (const LV2_Feature* const*)features);
    TEST("Plugin instantiated", instance != nullptr);

    if (!instance) {
        lilv_node_free(uri);
        lilv_world_free(world);
        return 1;
    }

    // Map URIs we need
    LV2_URID midi_MidiEvent   = test_map(&urid_map, LV2_MIDI__MidiEvent);
    LV2_URID atom_Sequence    = test_map(&urid_map, LV2_ATOM__Sequence);
    LV2_URID atom_Chunk       = test_map(&urid_map, LV2_ATOM__Chunk);
    LV2_URID atom_eventTransfer = test_map(&urid_map, LV2_ATOM__eventTransfer);
    LV2_URID atom_String      = test_map(&urid_map, LV2_ATOM__String);
    (void)atom_eventTransfer;
    (void)atom_String;
    (void)atom_Chunk;

    // Set up port buffers (256 bytes each)
    uint8_t midi_in_buf[256];
    uint8_t midi_out_buf[256];
    uint8_t state_buf[256];

    memset(midi_in_buf, 0, sizeof(midi_in_buf));
    memset(midi_out_buf, 0, sizeof(midi_out_buf));
    memset(state_buf, 0, sizeof(state_buf));

    // Initialize atom sequences (output needs proper header for forge capacity)
    LV2_Atom_Forge forge;
    lv2_atom_forge_init(&forge, &urid_map.map);

    // Initialize output buffer with an empty sequence (capacity = all usable space)
    {
        LV2_Atom_Sequence* out_seq = (LV2_Atom_Sequence*)midi_out_buf;
        out_seq->atom.type = atom_Sequence;
        out_seq->atom.size = sizeof(midi_out_buf) - sizeof(LV2_Atom);
    }
    {
        LV2_Atom_Sequence* st_seq = (LV2_Atom_Sequence*)state_buf;
        st_seq->atom.type = atom_Sequence;
        st_seq->atom.size = sizeof(state_buf) - sizeof(LV2_Atom);
    }

    // Connect ports
    lilv_instance_connect_port(instance, 0, midi_in_buf);
    lilv_instance_connect_port(instance, 1, midi_out_buf);
    lilv_instance_connect_port(instance, 2, state_buf);

    lilv_instance_activate(instance);

    // =================================================================
    // Test 1: Identity mapping — NOTE ON vel=100 should output vel=100
    // =================================================================
    lv2_atom_forge_set_buffer(&forge, midi_in_buf, sizeof(midi_in_buf));
    LV2_Atom_Forge_Frame frame;
    lv2_atom_forge_sequence_head(&forge, &frame, 0);
    lv2_atom_forge_frame_time(&forge, 0);
    uint8_t midi_data[] = {0x90, 60, 100}; // NOTE ON C4 vel=100
    lv2_atom_forge_atom(&forge, 3, midi_MidiEvent);
    lv2_atom_forge_write(&forge, midi_data, 3);
    lv2_atom_forge_pop(&forge, &frame);

    // Clear output for fresh read
    memset(midi_out_buf, 0, sizeof(midi_out_buf));
    LV2_Atom_Sequence* out_seq = (LV2_Atom_Sequence*)midi_out_buf;
    out_seq->atom.type = atom_Sequence;
    out_seq->atom.size = sizeof(midi_out_buf) - sizeof(LV2_Atom);

    // Run
    lilv_instance_run(instance, 1);

    // Check output
    const LV2_Atom_Sequence* out = (const LV2_Atom_Sequence*)midi_out_buf;
    int note_on_count = 0;
    bool velocity_ok = false;

    LV2_ATOM_SEQUENCE_FOREACH(out, ev) {
        if (ev->body.type == midi_MidiEvent) {
            const uint8_t* data = (const uint8_t*)(ev + 1);
            if ((data[0] & 0xF0) == 0x90) {
                note_on_count++;
                if (data[2] == 100) velocity_ok = true;
            }
        }
    }

    TEST("Test 1: NOTE ON in output", note_on_count == 1);
    TEST("Test 1: velocity=100 unchanged", velocity_ok);

    // =================================================================
    // Test 2: CC events pass through unchanged
    // =================================================================
    lv2_atom_forge_set_buffer(&forge, midi_in_buf, sizeof(midi_in_buf));
    lv2_atom_forge_sequence_head(&forge, &frame, 0);
    lv2_atom_forge_frame_time(&forge, 0);
    uint8_t cc_data[] = {0xB0, 7, 64}; // CC 7 vol=64
    lv2_atom_forge_atom(&forge, 3, midi_MidiEvent);
    lv2_atom_forge_write(&forge, cc_data, 3);
    lv2_atom_forge_pop(&forge, &frame);

    memset(midi_out_buf, 0, sizeof(midi_out_buf));
    out_seq->atom.type = atom_Sequence;
    out_seq->atom.size = sizeof(midi_out_buf) - sizeof(LV2_Atom);

    lilv_instance_run(instance, 1);

    out = (const LV2_Atom_Sequence*)midi_out_buf;
    int cc_count = 0;
    bool cc_unchanged = false;
    LV2_ATOM_SEQUENCE_FOREACH(out, ev) {
        if (ev->body.type == midi_MidiEvent) {
            const uint8_t* data = (const uint8_t*)(ev + 1);
            if ((data[0] & 0xF0) == 0xB0) {
                cc_count++;
                if (data[1] == 7 && data[2] == 64)
                    cc_unchanged = true;
            }
        }
    }

    TEST("Test 2: CC in output", cc_count == 1);
    TEST("Test 2: CC unchanged", cc_unchanged);

    // =================================================================
    // Test 3: NOTE OFF passes through unchanged
    // =================================================================
    lv2_atom_forge_set_buffer(&forge, midi_in_buf, sizeof(midi_in_buf));
    lv2_atom_forge_sequence_head(&forge, &frame, 0);
    lv2_atom_forge_frame_time(&forge, 0);
    uint8_t noteoff_data[] = {0x80, 60, 64}; // NOTE OFF
    lv2_atom_forge_atom(&forge, 3, midi_MidiEvent);
    lv2_atom_forge_write(&forge, noteoff_data, 3);
    lv2_atom_forge_pop(&forge, &frame);

    memset(midi_out_buf, 0, sizeof(midi_out_buf));
    out_seq->atom.type = atom_Sequence;
    out_seq->atom.size = sizeof(midi_out_buf) - sizeof(LV2_Atom);

    lilv_instance_run(instance, 1);

    out = (const LV2_Atom_Sequence*)midi_out_buf;
    int noteoff_count = 0;
    bool noteoff_unchanged = false;
    LV2_ATOM_SEQUENCE_FOREACH(out, ev) {
        if (ev->body.type == midi_MidiEvent) {
            const uint8_t* data = (const uint8_t*)(ev + 1);
            if ((data[0] & 0xF0) == 0x80) {
                noteoff_count++;
                if (data[2] == 64) noteoff_unchanged = true;
            }
        }
    }

    TEST("Test 3: NOTE OFF in output", noteoff_count == 1);
    TEST("Test 3: NOTE OFF unchanged", noteoff_unchanged);

    // =================================================================
    // Test 4: Pitch Bend passes through unchanged
    // =================================================================
    lv2_atom_forge_set_buffer(&forge, midi_in_buf, sizeof(midi_in_buf));
    lv2_atom_forge_sequence_head(&forge, &frame, 0);
    lv2_atom_forge_frame_time(&forge, 0);
    uint8_t pb_data[] = {0xE0, 0, 64};
    lv2_atom_forge_atom(&forge, 3, midi_MidiEvent);
    lv2_atom_forge_write(&forge, pb_data, 3);
    lv2_atom_forge_pop(&forge, &frame);

    memset(midi_out_buf, 0, sizeof(midi_out_buf));
    out_seq->atom.type = atom_Sequence;
    out_seq->atom.size = sizeof(midi_out_buf) - sizeof(LV2_Atom);

    lilv_instance_run(instance, 1);

    out = (const LV2_Atom_Sequence*)midi_out_buf;
    int pb_count = 0;
    LV2_ATOM_SEQUENCE_FOREACH(out, ev) {
        if (ev->body.type == midi_MidiEvent) {
            const uint8_t* data = (const uint8_t*)(ev + 1);
            if ((data[0] & 0xF0) == 0xE0) {
                pb_count++;
            }
        }
    }

    TEST("Test 4: Pitch Bend passed through", pb_count == 1);

    // =================================================================
    // Report
    // =================================================================
    lilv_instance_deactivate(instance);
    lilv_instance_free(instance);
    lilv_node_free(uri);
    lilv_world_free(world);

    fprintf(stderr, "\n=== Integration Tests ===\n");
    fprintf(stderr, "Results: %d passed, %d failed\n",
            tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
