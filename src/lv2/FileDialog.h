// -----------------------------------------------------------------------------
// src/lv2/FileDialog.h
//
// Autor:      blazman
// Licencia:   MIT
// Versión:    2.0.0
// Fecha:      2026-11-15
//
// Diálogo modal X11 para guardar y cargar presets.
// -----------------------------------------------------------------------------

#pragma once

#include <string>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

struct FileEntry {
    std::string name;
    bool is_dir;
};

class FileDialog {
public:
    enum Mode { SAVE, LOAD };
    
    FileDialog(Display* display, Window parent, Mode mode);
    ~FileDialog();
    
    // Returns selected file path, or empty string if cancelled
    std::string show();
    
private:
    Display* display;
    Window parent;
    Window dialog;
    Window file_list;
    Window path_display;
    Window name_input;
    Window btn_ok;
    Window btn_cancel;
    
    GC gc;
    Mode mode;
    
    std::string current_path;
    std::string filename;
    std::vector<FileEntry> entries;
    int selected_index = 0;
    int scroll_offset = 0;
    int cursor_pos = 0;
    
    bool done = false;
    bool cancelled = false;
    
    // Layout constants
    static constexpr int DIALOG_W = 500;
    static constexpr int DIALOG_H = 400;
    static constexpr int LIST_H = 280;
    static constexpr int INPUT_H = 25;
    static constexpr int BTN_W = 80;
    static constexpr int BTN_H = 30;
    static constexpr int PADDING = 10;
    static constexpr int LINE_HEIGHT = 20;
    static constexpr int VISIBLE_LINES = LIST_H / LINE_HEIGHT;
    
    void refreshDirectory();
    void draw();
    void handleEvent(XEvent& ev);
    void selectEntry(int index);
    void navigateUp();
    void confirm();
    void cancel();
    
    std::string getParentPath(const std::string& path);
    void sortEntries();
};
