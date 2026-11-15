// -----------------------------------------------------------------------------
// src/core/MidiProcessor.h
//
// Autor:      blazman
// Licencia:   MIT
// Versión:    2.0.0
// Fecha:      2026-11-15
//
// Procesador MIDI en tiempo real: modifica solo la velocidad de NOTE ON.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>

/**
 * @brief Real-time-safe MIDI processor.
 *
 * Only modifies NOTE ON velocity.  Everything else passes through unchanged.
 * No heap allocation, no locks, no syscalls.
 */
class MidiProcessor {
public:
    MidiProcessor() = default;

    /**
     * @brief Process a single MIDI event.
     *
     * @param velocityMap  The 128-entry lookup table (read-only).
     * @param eventData    Raw MIDI event bytes (at least 3 bytes for NOTE ON).
     * @param[out] outVel  If the event is NOTE ON, receives the new velocity.
     *
     * @return true if the velocity was modified (NOTE ON), false otherwise.
     *
     * @note  For NOTE ON with velocity == 0, the velocity is still mapped
     *        (which preserves the zero / "note off via velocity 0" behaviour).
     */
    [[nodiscard]] static bool processNoteOn(
        const uint8_t velocityMap[128],
        const uint8_t eventData[3],
        uint8_t& outVel) noexcept
    {
        // MIDI status byte: 0x90 = NOTE ON (channel masked)
        if ((eventData[0] & 0xF0) != 0x90)
            return false; // Not a NOTE ON

        const uint8_t vel = eventData[2];
        outVel = velocityMap[vel];
        return true;
    }

    /** @return true if the status byte is a NOTE ON (channel agnostic). */
    [[nodiscard]] static constexpr bool isNoteOn(uint8_t status) noexcept {
        return (status & 0xF0) == 0x90;
    }
};
