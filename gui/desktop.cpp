/**
 * @file        desktop.cpp
 * @brief       Desktop (part of #x86 GUI Framework)
 *
 * @date        11/02/2026
 * @version     1.0.0-beta
 */

#define KDBG_COMPONENT "GUI:DESKTOP"
#include <gui/desktop.h>
#include <core/filesystem/Paths.h>

Desktop* Desktop::activeInstance = nullptr;

Desktop::Desktop(int32_t w, int32_t h)
    : CompositeWidget(0, 0, 0, w, h), MouseEventHandler(), KeyboardEventHandler() {
    MouseX = w / 2;
    MouseY = h / 2;
    activeInstance = this;

    KDBG1("DESKTOP Initialized with ID 0x%x", this->ID);

    // Initialize Wallpaper
    char* wallpaperName = (char*)PATH_DESKTOP_BMP;
    Bitmap* wallpaperImg = new Bitmap(wallpaperName);
    if (!wallpaperImg) {
        HALT("CRITICAL: Failed to allocate desktop wallpaper bitmap!\n");
    }

    if (wallpaperImg->IsValid()) {
        this->Wallpaper = wallpaperImg;
    } else {
        // Fallback: Solid Color (Blue-ish)
        this->Wallpaper = new Bitmap(w, h, 0xFF0000FF);
        if (!this->Wallpaper) {
            HALT("CRITICAL: Failed to allocate fallback wallpaper bitmap!\n");
        }
        delete wallpaperImg;
    }

    memset(cursorBackBuffer, 0, sizeof(cursorBackBuffer));

    // Initialize Taskbar
    taskbar = new Taskbar(this, w, h);
    if (!taskbar) {
        HALT("CRITICAL: Failed to allocate Taskbar!\n");
    }
    taskbar->SetPID(0);  // Kernel-owned
    taskbar->SetID(0);   // System widget

    // Add application launchers
    taskbar->AddApp("MemViewer", "Memory inspector", PATH_MEMVIEW);
    taskbar->AddApp("Explorer", "File Manager", PATH_EXPLORER);
    taskbar->AddApp("Calculator", "Calculator GUI", PATH_CALCULATOR);
    taskbar->AddApp("Terminal", "CLI preview", PATH_TERMINAL);
    taskbar->AddApp("Game3D", "3D Game Engine", PATH_GAME3D);

    // NOTE: Taskbar is NOT added to childrenList.
}

Desktop::~Desktop() {
    if (Wallpaper) delete Wallpaper;
    if (taskbar) delete taskbar;
}

void Desktop::createNewHandler(uint32_t pid, ThreadControlBlock* thread) {
    EventHandler* handler = new EventHandler{};
    if (!handler) {
        HALT("CRITICAL: Failed to allocate event handler!\n");
    }
    handler->pid = pid;
    handler->thread = thread;
    HguiEventHandlers.Add(handler);
}

void Desktop::deleteEventHandler(uint32_t pid) {
    EventHandler* found = nullptr;
    HguiEventHandlers.ForEach([&](EventHandler* e) {
        if (e->pid == pid) found = e;
    });
    if (found) {
        HguiEventHandlers.Remove([&](EventHandler* e) { return e == found; });
        delete found;
    }
}

EventHandler* Desktop::getHandler(uint32_t pid) {
    EventHandler* _eventHandler = nullptr;
    HguiEventHandlers.ForEach([&](EventHandler* e) {
        if (e->pid == pid) _eventHandler = e;
    });
    return _eventHandler;
}

void Desktop::Draw(GraphicsDriver* gc) {
    uint32_t screenW = gc->GetWidth();
    uint32_t screenH = gc->GetHeight();
    uint32_t* vesaBuffer = gc->GetBackBuffer();

    // Check if taskbar needs redraw
    if (taskbar && taskbar->isDirty) {
        this->isDirty = true;
    }
    // Also check start menu dirty state
    if (taskbar && taskbar->IsStartMenuOpen()) {
        StartMenu* menu = taskbar->GetStartMenu();
        if (menu && menu->isDirty) {
            this->isDirty = true;
        }
    }

    // -----------------------------------------------------------------
    // CASE 1: FULL SYSTEM REDRAW
    // Triggered when a window moves, opens, closes, or invalidates the desktop.
    // -----------------------------------------------------------------
    if (this->isDirty) {
        // Draw Wallpaper
        gc->DrawBitmap(0, 0, (const uint32_t*)this->Wallpaper->GetBuffer(),
                       this->Wallpaper->GetWidth(), this->Wallpaper->GetHeight());

        // Composite all Children (Windows) onto the screen
        // Calls CompositeWidget::Draw -> calls Window::Draw
        CompositeWidget::Draw(gc);

        // Draw Taskbar on top of everything
        taskbar->Draw(gc);

        // Save Background under Mouse (Fresh capture)
        {
            InterruptGuard guard;
            for (int y = 0; y < CURSOR_SIZE; y++) {
                for (int x = 0; x < CURSOR_SIZE; x++) {
                    // Bounds Check!
                    int destX = MouseX + x;
                    int destY = MouseY + y;

                    if (destX >= (int)screenW || destY >= (int)screenH) continue;

                    cursorBackBuffer[y * CURSOR_SIZE + x] = vesaBuffer[destY * screenW + destX];
                }
            }
            hasBackBuffer = true;

            // Draw Cursor
            gc->DrawBitmap(MouseX, MouseY, (const uint32_t*)icon_cursor_20x20, CURSOR_SIZE,
                           CURSOR_SIZE);

            // Reset State
            this->isDirty = false;
            oldMouseX = MouseX;
            oldMouseY = MouseY;
        }
        return;
    }

    // -----------------------------------------------------------------
    // CASE 2: MOUSE MOVEMENT ONLY
    // Optimization: If nothing else changed, just undraw/redraw cursor.
    // -----------------------------------------------------------------
    if (MouseX != oldMouseX || MouseY != oldMouseY) {
        // ERASE OLD CURSOR (Restore saved pixels)
        {
            InterruptGuard guard;
            if (hasBackBuffer) {
                for (int y = 0; y < CURSOR_SIZE; y++) {
                    for (int x = 0; x < CURSOR_SIZE; x++) {
                        int destX = oldMouseX + x;
                        int destY = oldMouseY + y;

                        if (destX >= (int)screenW || destY >= (int)screenH) continue;

                        // Direct buffer write for speed
                        vesaBuffer[destY * screenW + destX] = cursorBackBuffer[y * CURSOR_SIZE + x];
                    }
                }
            }
        }

        // CAPTURE BACKGROUND AT NEW POSITION
        {
            InterruptGuard guard;
            for (int y = 0; y < CURSOR_SIZE; y++) {
                for (int x = 0; x < CURSOR_SIZE; x++) {
                    int destX = MouseX + x;
                    int destY = MouseY + y;

                    if (destX >= (int)screenW || destY >= (int)screenH) continue;

                    cursorBackBuffer[y * CURSOR_SIZE + x] = vesaBuffer[destY * screenW + destX];
                }
            }
            hasBackBuffer = true;

            // DRAW NEW CURSOR
            gc->DrawBitmap(MouseX, MouseY, (const uint32_t*)icon_cursor_20x20, CURSOR_SIZE,
                           CURSOR_SIZE);

            // Update History
            oldMouseX = MouseX;
            oldMouseY = MouseY;
        }
    }
}

uint32_t Desktop::getNewID() {
    return current_id++;
}

void Desktop::RemoveAppByPID(uint32_t pid) {
    Widget* result = nullptr;
    childrenList.ForEach([&](Widget* c) {
        if (!result && c->PID == pid) result = c;
    });

    if (result) {
        this->RemoveChild(result);
        this->MarkDirty();  // Ensure screen clears the removed window

        // Restore focus to the topmost remaining window so keyboard
        // input continues to flow (e.g., Terminal that launched the app).
        Widget* topmost = nullptr;
        childrenList.ForEach([&](Widget* c) {
            topmost = c;  // last one visited = topmost in Z-order
        });
        if (topmost) {
            this->GetFocus(topmost);
        }
    }

    // Remove corresponding taskbar tab
    if (taskbar) {
        taskbar->RemoveTabByPID(pid);
    }
}

void Desktop::Focus(Widget* widget) {
    if (!widget) return;

    // Find and remove the widget from the list
    bool found = childrenList.Remove([&](Widget* w) { return w == widget; });

    // If found, add it to the back (top of Z-order)
    if (found) {
        childrenList.PushBack(widget);
        this->isDirty = true;  // Force a full redraw
    }
}

// -- Inputs --

void Desktop::GetFocus(Widget* widget) {
    // Call parent to handle normal focus/z-ordering
    CompositeWidget::GetFocus(widget);

    // Update taskbar tab to reflect the newly focused window
    if (taskbar) {
        taskbar->SetActiveTab(widget);
    }
}

void Desktop::OnMouseDown(uint8_t button) {
    // Check if click is in start menu area (above taskbar)
    if (taskbar && taskbar->IsStartMenuOpen() && taskbar->StartMenuContains(MouseX, MouseY)) {
        taskbar->OnMouseDown(MouseX, MouseY, button);
        return;
    }

    // Check if click is in taskbar area
    if (taskbar && taskbar->ContainsCoordinate(MouseX, MouseY)) {
        taskbar->OnMouseDown(MouseX, MouseY, button);
        return;
    }

    // Click on desktop/window area — dismiss start menu if open
    if (taskbar && taskbar->IsStartMenuOpen()) {
        taskbar->CloseStartMenu();
    }
    CompositeWidget::OnMouseDown(MouseX, MouseY, button);
}

void Desktop::OnMouseUp(uint8_t button) {
    // Route to start menu
    if (taskbar && taskbar->IsStartMenuOpen() && taskbar->StartMenuContains(MouseX, MouseY)) {
        taskbar->OnMouseUp(MouseX, MouseY, button);
        return;
    }

    // Route to taskbar
    if (taskbar && taskbar->ContainsCoordinate(MouseX, MouseY)) {
        taskbar->OnMouseUp(MouseX, MouseY, button);
        return;
    }
    CompositeWidget::OnMouseUp(MouseX, MouseY, button);
}

void Desktop::OnMouseMove(int32_t dx, int32_t dy) {
    // Use signed arithmetic to prevent unsigned wrap when dx/dy are negative
    int32_t newX = (int32_t)MouseX + dx;
    int32_t newY = (int32_t)MouseY + dy;

    // Clamp to screen bounds
    if (newX < 0) newX = 0;
    if (newY < 0) newY = 0;
    if (newX >= w) newX = w - 1;
    if (newY >= h) newY = h - 1;

    // Capture pre-update cursor position before clamping
    int32_t oldX = (int32_t)MouseX;
    int32_t oldY = (int32_t)MouseY;

    MouseX = (uint32_t)newX;
    MouseY = (uint32_t)newY;

    // Pass delta to UI
    CompositeWidget::OnMouseMove(oldX, oldY, MouseX, MouseY);

    // Also route mouse moves to taskbar/start menu for hover effects
    if (taskbar) {
        taskbar->OnMouseMove(oldX, oldY, MouseX, MouseY);
    }
}

void Desktop::OnKeyDown(const char* key) {
    CompositeWidget::OnKeyDown(key);
}
void Desktop::OnKeyUp(const char* key) {
    CompositeWidget::OnKeyUp(key);
}
void Desktop::OnSpecialKeyDown(uint8_t key) {
    CompositeWidget::OnSpecialKeyDown(key);
}
void Desktop::OnSpecialKeyUp(uint8_t key) {
    CompositeWidget::OnSpecialKeyUp(key);
}
