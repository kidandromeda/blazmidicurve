// -----------------------------------------------------------------------------
// src/lv2/uris.h
//
// Autor:      blazman
// Licencia:   MIT
// Versión:    2.0.0
// Fecha:      2026-11-15
//
// URIs y utilidades de mapeo LV2 compartidas.
// -----------------------------------------------------------------------------

#pragma once

#include <lv2/atom/atom.h>
#include <lv2/core/lv2.h>
#include <lv2/midi/midi.h>
#include <lv2/urid/urid.h>

/**
 * @brief Mapped URIs used by the plugin.
 *
 * Mapped once during instantiation so the audio thread never calls
 * LV2_URID_Map.
 */
struct Uris {
    // Atom
    LV2_URID atom_Sequence;
    LV2_URID atom_String;
    LV2_URID atom_Chunk;
    LV2_URID atom_eventTransfer;

    // MIDI
    LV2_URID midi_MidiEvent;

    // Custom: curve state message
    LV2_URID curve_state; ///< URI: https://brania.org/plugins/blazmidicurve#curve-state

    /** Map all URIs through `map`. */
    void map(LV2_URID_Map* map) noexcept {
        atom_Sequence     = map->map(map->handle, LV2_ATOM__Sequence);
        atom_String       = map->map(map->handle, LV2_ATOM__String);
        atom_Chunk        = map->map(map->handle, LV2_ATOM__Chunk);
        atom_eventTransfer = map->map(map->handle, LV2_ATOM__eventTransfer);
        midi_MidiEvent    = map->map(map->handle, LV2_MIDI__MidiEvent);
        curve_state       = map->map(map->handle, "https://brania.org/plugins/blazmidicurve#curve-state");
    }
};

// -----------------------------------------------------------------
// Port indices
// -----------------------------------------------------------------
enum PortIndex : uint32_t {
    PORT_MIDI_IN  = 0, ///< MIDI input  (atom:Sequence)
    PORT_MIDI_OUT = 1, ///< MIDI output (atom:Sequence)
    PORT_STATE    = 2  ///< State notification output (atom:Sequence)
};
