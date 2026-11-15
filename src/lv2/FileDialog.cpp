// -----------------------------------------------------------------------------
// src/lv2/FileDialog.cpp
//
// Autor:      blazman
// Licencia:   MIT
// Versión:    2.0.0
// Fecha:      2026-11-15
//
// Implementación del diálogo modal de archivos X11.
// -----------------------------------------------------------------------------

#include "FileDialog.h"
#include <algorithm>
#include <cstdio>

FileDialog::FileDialog(Display* display, Window parent, Mode mode)
    : display(display), parent(parent), dialog(0), gc(nullptr), mode(mode) {
    
    Window root_return, child_return;
    int x_return, y_return;
    unsigned int width_return, height_return, border_return, depth_return;
    
    if (!XGetGeometry(display, parent, &root_return, &x_return, &y_return,
                 &width_return, &height_return, &border_return, &depth_return)) {
        return;
    }
    
    XTranslateCoordinates(display, parent, root_return, 0, 0, &x_return, &y_return, &child_return);
    
    int dialog_x = x_return + (width_return - DIALOG_W) / 2;
    int dialog_y = y_return + (height_return - DIALOG_H) / 2;
    
    dialog = XCreateSimpleWindow(display, root_return,
                                 dialog_x, dialog_y,
                                 DIALOG_W, DIALOG_H, 1,
                                 BlackPixel(display, DefaultScreen(display)),
                                 WhitePixel(display, DefaultScreen(display)));
    
    XSelectInput(display, dialog, ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask);
    XStoreName(display, dialog, mode == SAVE ? "Save Preset" : "Load Preset");
    
    gc = XCreateGC(display, dialog, 0, nullptr);
    
    const char* home = getenv("HOME");
    current_path = home ? home : "/";
    
    if (mode == SAVE) {
        filename = "preset.json";
        cursor_pos = filename.length() - 5;
    }
    
    refreshDirectory();
}

FileDialog::~FileDialog() {
    if (gc) XFreeGC(display, gc);
    if (dialog) XDestroyWindow(display, dialog);
}

std::string FileDialog::getParentPath(const std::string& path) {
    size_t pos = path.find_last_of('/');
    if (pos == 0 || pos == std::string::npos) {
        return "/";
    }
    return path.substr(0, pos);
}

void FileDialog::sortEntries() {
    std::sort(entries.begin(), entries.end(), [](const FileEntry& a, const FileEntry& b) {
        if (a.is_dir != b.is_dir) return a.is_dir > b.is_dir;
        return a.name < b.name;
    });
}

void FileDialog::refreshDirectory() {
    entries.clear();
    
    if (current_path != "/") {
        entries.push_back({"..", true});
    }
    
    DIR* dir = opendir(current_path.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_name[0] == '.') continue;
            
            std::string full_path = current_path + "/" + entry->d_name;
            struct stat st;
            if (stat(full_path.c_str(), &st) == 0) {
                FileEntry fe;
                fe.name = entry->d_name;
                fe.is_dir = S_ISDIR(st.st_mode);
                
                if (mode == LOAD && !fe.is_dir) {
                    if (fe.name.length() < 5 || 
                        fe.name.substr(fe.name.length() - 5) != ".json") {
                        continue;
                    }
                }
                
                entries.push_back(fe);
            }
        }
        closedir(dir);
    }
    
    sortEntries();
    selected_index = 0;
    scroll_offset = 0;
}

void FileDialog::draw() {
    if (!dialog) return;
    
    XClearWindow(display, dialog);
    
    int y = PADDING;
    
    XSetForeground(display, gc, BlackPixel(display, DefaultScreen(display)));
    XDrawString(display, dialog, gc, PADDING, y + 15, "Path:", 5);
    
    XSetForeground(display, gc, WhitePixel(display, DefaultScreen(display)));
    XFillRectangle(display, dialog, gc, PADDING + 40, y, DIALOG_W - 2 * PADDING - 40, INPUT_H);
    
    XSetForeground(display, gc, BlackPixel(display, DefaultScreen(display)));
    XDrawRectangle(display, dialog, gc, PADDING + 40, y, DIALOG_W - 2 * PADDING - 40, INPUT_H);
    XDrawString(display, dialog, gc, PADDING + 45, y + 17, 
                current_path.c_str(), current_path.length());
    
    y += INPUT_H + PADDING;
    
    XSetForeground(display, gc, WhitePixel(display, DefaultScreen(display)));
    XFillRectangle(display, dialog, gc, PADDING, y, DIALOG_W - 2 * PADDING, LIST_H);
    
    XSetForeground(display, gc, BlackPixel(display, DefaultScreen(display)));
    XDrawRectangle(display, dialog, gc, PADDING, y, DIALOG_W - 2 * PADDING, LIST_H);
    
    int list_y = y + 5;
    for (int i = 0; i < VISIBLE_LINES && (i + scroll_offset) < (int)entries.size(); i++) {
        int idx = i + scroll_offset;
        int entry_y = list_y + i * LINE_HEIGHT;
        
        if (idx == selected_index) {
            XSetForeground(display, gc, BlackPixel(display, DefaultScreen(display)));
            XFillRectangle(display, dialog, gc, PADDING + 2, entry_y - 2, 
                          DIALOG_W - 2 * PADDING - 4, LINE_HEIGHT - 2);
            XSetForeground(display, gc, WhitePixel(display, DefaultScreen(display)));
        } else {
            XSetForeground(display, gc, BlackPixel(display, DefaultScreen(display)));
        }
        
        std::string display_name = entries[idx].name;
        if (entries[idx].is_dir) {
            display_name = "[" + display_name + "]";
        }
        
        XDrawString(display, dialog, gc, PADDING + 10, entry_y + 12,
                   display_name.c_str(), display_name.length());
    }
    
    y += LIST_H + PADDING;
    
    XSetForeground(display, gc, BlackPixel(display, DefaultScreen(display)));
    XDrawString(display, dialog, gc, PADDING, y + 17, "Name:", 5);
    
    XSetForeground(display, gc, WhitePixel(display, DefaultScreen(display)));
    XFillRectangle(display, dialog, gc, PADDING + 50, y, DIALOG_W - 2 * PADDING - 50, INPUT_H);
    
    XSetForeground(display, gc, BlackPixel(display, DefaultScreen(display)));
    XDrawRectangle(display, dialog, gc, PADDING + 50, y, DIALOG_W - 2 * PADDING - 50, INPUT_H);
    XDrawString(display, dialog, gc, PADDING + 55, y + 17, 
                filename.c_str(), filename.length());
    
    int cursor_x = PADDING + 55 + cursor_pos * 7;
    XDrawLine(display, dialog, gc, cursor_x, y + 3, cursor_x, y + INPUT_H - 3);
    
    y += INPUT_H + PADDING;
    
    int btn_x = DIALOG_W - 2 * PADDING - 2 * BTN_W - 10;
    
    XSetForeground(display, gc, BlackPixel(display, DefaultScreen(display)));
    XFillRectangle(display, dialog, gc, btn_x, y, BTN_W, BTN_H);
    XSetForeground(display, gc, WhitePixel(display, DefaultScreen(display)));
    const char* ok_text = mode == SAVE ? "Save" : "Load";
    int ok_len = strlen(ok_text);
    XDrawString(display, dialog, gc, btn_x + 20, y + BTN_H / 2 + 5, ok_text, ok_len);
    
    btn_x += BTN_W + 10;
    
    XSetForeground(display, gc, BlackPixel(display, DefaultScreen(display)));
    XFillRectangle(display, dialog, gc, btn_x, y, BTN_W, BTN_H);
    XSetForeground(display, gc, WhitePixel(display, DefaultScreen(display)));
    const char* cancel_text = "Cancel";
    int cancel_len = strlen(cancel_text);
    XDrawString(display, dialog, gc, btn_x + 15, y + BTN_H / 2 + 5, cancel_text, cancel_len);
    
    XFlush(display);
}

void FileDialog::selectEntry(int index) {
    if (index < 0 || index >= (int)entries.size()) return;
    
    selected_index = index;
    
    if (selected_index < scroll_offset) {
        scroll_offset = selected_index;
    } else if (selected_index >= scroll_offset + VISIBLE_LINES) {
        scroll_offset = selected_index - VISIBLE_LINES + 1;
    }
    
    if (!entries[selected_index].is_dir) {
        filename = entries[selected_index].name;
        cursor_pos = filename.length();
    }
    
    draw();
}

void FileDialog::navigateUp() {
    if (current_path != "/") {
        current_path = getParentPath(current_path);
        refreshDirectory();
        draw();
    }
}

void FileDialog::confirm() {
    if (selected_index < 0 || selected_index >= (int)entries.size()) return;
    
    if (entries[selected_index].is_dir) {
        if (entries[selected_index].name == "..") {
            navigateUp();
        } else {
            current_path += "/" + entries[selected_index].name;
            refreshDirectory();
            draw();
        }
    } else {
        done = true;
    }
}

void FileDialog::cancel() {
    cancelled = true;
    done = true;
}

void FileDialog::handleEvent(XEvent& ev) {
    switch (ev.type) {
    case Expose:
        if (ev.xexpose.count == 0) {
            draw();
        }
        break;
        
    case ButtonPress: {
        int x = ev.xbutton.x;
        int y = ev.xbutton.y;
        
        int list_y = PADDING + INPUT_H + PADDING;
        if (x >= PADDING && x < DIALOG_W - PADDING &&
            y >= list_y && y < list_y + LIST_H) {
            int clicked_index = scroll_offset + (y - list_y - 5) / LINE_HEIGHT;
            if (clicked_index >= 0 && clicked_index < (int)entries.size()) {
                selectEntry(clicked_index);
                
                if (entries[clicked_index].is_dir) {
                    confirm();
                }
            }
        }
        
        int btn_y = DIALOG_H - PADDING - BTN_H;
        if (y >= btn_y && y < btn_y + BTN_H) {
            int ok_btn_x = DIALOG_W - 2 * PADDING - 2 * BTN_W - 10;
            int cancel_btn_x = ok_btn_x + BTN_W + 10;
            
            if (x >= ok_btn_x && x < ok_btn_x + BTN_W) {
                if (mode == SAVE || (selected_index >= 0 && selected_index < (int)entries.size() && !entries[selected_index].is_dir)) {
                    done = true;
                } else {
                    confirm();
                }
            } else if (x >= cancel_btn_x && x < cancel_btn_x + BTN_W) {
                cancel();
            }
        }
        break;
    }
        
    case KeyPress: {
        KeySym keysym;
        char text[2] = {0, 0};
        int n = XLookupString(&ev.xkey, text, 2, &keysym, nullptr);
        
        if (keysym == XK_Return) {
            if (mode == SAVE || (selected_index >= 0 && selected_index < (int)entries.size() && !entries[selected_index].is_dir)) {
                done = true;
            } else {
                confirm();
            }
        } else if (keysym == XK_Escape) {
            cancel();
        } else if (keysym == XK_Up) {
            selectEntry(selected_index - 1);
        } else if (keysym == XK_Down) {
            selectEntry(selected_index + 1);
        } else if (keysym == XK_BackSpace) {
            if (cursor_pos > 0 && cursor_pos <= (int)filename.length()) {
                filename.erase(cursor_pos - 1, 1);
                cursor_pos--;
                draw();
            }
        } else if (keysym == XK_Left) {
            if (cursor_pos > 0) cursor_pos--;
            draw();
        } else if (keysym == XK_Right) {
            if (cursor_pos < (int)filename.length()) cursor_pos++;
            draw();
        } else if (keysym == XK_Delete) {
            if (cursor_pos < (int)filename.length()) {
                filename.erase(cursor_pos, 1);
                draw();
            }
        } else if (n > 0 && text[0] >= 32 && text[0] < 127) {
            if (cursor_pos >= 0 && cursor_pos <= (int)filename.length()) {
                filename.insert(cursor_pos, 1, text[0]);
                cursor_pos++;
                draw();
            }
        }
        break;
    }
    }
}

// Predicado para XIfEvent: solo aceptar eventos destinados a la ventana
// del dialogo. Los eventos de la ventana principal de la UI y del resto
// de Ardour permanecen en la cola, intactos.
static Bool isDialogEvent(Display*, XEvent* ev, XPointer arg) {
    Window w = (Window)arg;
    return ev->xany.window == w ? True : False;
}

std::string FileDialog::show() {
    if (!dialog || !gc) return "";
    
    XMapWindow(display, dialog);
    XFlush(display);
    
    draw();
    
    while (!done) {
        XEvent ev;
        // CLAVE: solo sacar de la cola eventos de ESTA ventana.
        // Antes se usaba XNextEvent, que robaba eventos de la ventana
        // principal de la UI y los interpretaba mal -> crashes.
        XIfEvent(display, &ev, isDialogEvent, (XPointer)dialog);
        
        if (ev.type == DestroyNotify) {
            cancelled = true;
            break;
        }
        
        handleEvent(ev);
    }
    
    if (cancelled || filename.empty()) {
        return "";
    }
    
    if (mode == SAVE && filename.length() < 5) {
        filename += ".json";
    } else if (mode == SAVE && filename.substr(filename.length() - 5) != ".json") {
        filename += ".json";
    }
    
    return current_path + "/" + filename;
}
