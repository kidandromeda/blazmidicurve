// -----------------------------------------------------------------------------
// src/lv2/MidiCurveUI.h
//
// Autor:      blazman
// Licencia:   MIT
// Versión:    2.0.0
// Fecha:      2026-11-15
//
// Definiciones de la interfaz gráfica X11/Cairo del plugin.
// -----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <string>

#include <lv2/atom/atom.h>
#include <lv2/atom/forge.h>
#include <lv2/core/lv2.h>
#include <lv2/ui/ui.h>
#include <lv2/urid/urid.h>
#include <lv2/worker/worker.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <cairo.h>
#include <cairo-xlib.h>

#include "src/core/Curve.h"

struct Uris {
    LV2_URID atom_Sequence;
    LV2_URID atom_String;
    LV2_URID atom_eventTransfer;
    LV2_URID curve_state;
    LV2_URID midi_note_info;

    void map(LV2_URID_Map* m) {
        atom_Sequence      = m->map(m->handle, LV2_ATOM__Sequence);
        atom_String        = m->map(m->handle, LV2_ATOM__String);
        atom_eventTransfer = m->map(m->handle, LV2_ATOM__eventTransfer);
        curve_state        = m->map(m->handle, "https://brania.org/plugins/blazmidicurve#curve-state");
        midi_note_info     = m->map(m->handle, "https://brania.org/plugins/blazmidicurve#midi-note-info");
    }
};

struct MidiNoteInfo {
    uint8_t note;
    uint8_t velocity_in;
    uint8_t velocity_out;
};

struct UI {
    Display* display = nullptr;
    Window   parent  = 0;
    Window   window  = 0;
    GC       gc      = nullptr;
    cairo_surface_t* cairo_surface = nullptr;
    cairo_t*         cairo_cr      = nullptr;
    
    LV2UI_Write_Function write_function = nullptr;
    LV2UI_Controller controller = nullptr;
    LV2_Worker_Schedule* worker = nullptr;
    LV2_URID_Map* map = nullptr;
    Uris uris;
    
    Curve curve;
    int selected_node = -1;
    bool dragging = false;
    bool needs_redraw = true;
    
    Time last_click_time = 0;
    int  last_click_x = 0;
    int  last_click_y = 0;
    
    MidiNoteInfo last_note = {0, 0, 0};
    bool has_note = false;
    int note_fade = 0;
    
    bool state_received = false;
    int state_request_counter = 0;
    
    static constexpr int W = 700;
    static constexpr int H = 520;
    static constexpr int MARGIN_L = 58;
    static constexpr int MARGIN_R = 24;
    static constexpr int MARGIN_T = 48;
    static constexpr int MARGIN_B = 48;
    static constexpr int GRID_W = W - MARGIN_L - MARGIN_R;
    static constexpr int GRID_H = H - MARGIN_T - MARGIN_B;
    static constexpr int NODE_RADIUS = 4;
    static constexpr int NODE_HIT_RADIUS = 10;
    
    static constexpr int NUM_BUTTONS = 3;
    static constexpr int BTN_W = 62;
    static constexpr int BTN_H = 24;
    static constexpr int BTN_Y = 10;
    static constexpr int BTN_GAP = 6;
    
    // Compat: MARGIN usado en codigo antiguo -> esquina superior izquierda del grid
    static constexpr int MARGIN = MARGIN_L;
    
    float velToPixelXf(float vel) const {
        return MARGIN_L + (vel * GRID_W) / 127.f;
    }
    float velToPixelYf(float vel) const {
        return MARGIN_T + GRID_H - (vel * GRID_H) / 127.f;
    }
    int velToPixelX(int vel) const { return static_cast<int>(velToPixelXf(static_cast<float>(vel)) + 0.5f); }
    int velToPixelY(int vel) const { return static_cast<int>(velToPixelYf(static_cast<float>(vel)) + 0.5f); }
    int pixelToVelX(int px) const {
        float v = ((px - MARGIN_L) * 127.f) / GRID_W;
        if (v < 0.f) v = 0.f;
        if (v > 127.f) v = 127.f;
        return static_cast<int>(v + 0.5f);
    }
    int pixelToVelY(int py) const {
        float v = ((MARGIN_T + GRID_H - py) * 127.f) / GRID_H;
        if (v < 0.f) v = 0.f;
        if (v > 127.f) v = 127.f;
        return static_cast<int>(v + 0.5f);
    }
    
    int hitTestNode(int px, int py) const;
    void sendCurveToPlugin();
    void requestState();
    void setCurveFromJSON(const char* json);
    void savePreset();
    void loadPreset();
};
