#pragma once

#include <cstdint>
#include <atomic>

#include <lv2/atom/atom.h>
#include <lv2/atom/forge.h>
#include <lv2/core/lv2.h>
#include <lv2/midi/midi.h>
#include <lv2/state/state.h>
#include <lv2/urid/urid.h>
#include <lv2/worker/worker.h>

#include "src/core/Curve.h"
#include "src/core/Interpolator.h"

struct Uris {
    LV2_URID atom_Sequence;
    LV2_URID atom_String;
    LV2_URID atom_eventTransfer;
    LV2_URID midi_MidiEvent;
    LV2_URID curve_state;
    LV2_URID midi_note_info;

    void map(LV2_URID_Map* m) {
        atom_Sequence      = m->map(m->handle, LV2_ATOM__Sequence);
        atom_String        = m->map(m->handle, LV2_ATOM__String);
        atom_eventTransfer = m->map(m->handle, LV2_ATOM__eventTransfer);
        midi_MidiEvent     = m->map(m->handle, LV2_MIDI__MidiEvent);
        curve_state        = m->map(m->handle, "https://brania.org/plugins/blazmidicurve#curve-state");
        midi_note_info     = m->map(m->handle, "https://brania.org/plugins/blazmidicurve#midi-note-info");
    }
};

enum PortIndex : uint32_t {
    PORT_MIDI_IN  = 0,
    PORT_MIDI_OUT = 1,
    PORT_UI_NOTIFY = 2,
    PORT_UI_REQUEST = 3
};

struct Plugin {
    LV2_URID_Map* map = nullptr;
    LV2_Worker_Schedule* worker = nullptr;
    Uris uris;
    LV2_Atom_Forge forge;
    LV2_Atom_Forge_Frame frame;
    
    const LV2_Atom_Sequence* midi_in  = nullptr;
    LV2_Atom_Sequence*       midi_out = nullptr;
    LV2_Atom_Sequence*       ui_notify = nullptr;
    const LV2_Atom_Sequence* ui_request = nullptr;
    
    Curve curve;
    uint8_t velocityMap[128];
    
    Node pending_nodes[128];
    int pending_count = 0;
    std::atomic<bool> pending_update{false};
    
    char state_json[2048];
    size_t state_json_len = 0;
    bool state_dirty = true;
    
    void applyNodes(const Node* nodes, int count);
    void sendState();
    void sendMidiNoteToUI(uint8_t note, uint8_t vel_in, uint8_t vel_out);
};
