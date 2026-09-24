/*
 * MIT License
 *
 * Copyright (c) 2025 Malaka Gunawardana
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define KDBG_COMPONENT "GUI:DESKTOP"
#include <core/Iguard.h>
#include <core/filesystem/Paths.h>
#include <gui/desktop.h>

Desktop* Desktop::activeInstance = nullptr;

Desktop::Desktop(int32_t w, int32_t h)
    : CompositeWidget(0, 0, 0, w, h), MouseEventHandler(), KeyboardEventHandler() {
    MouseX = w / 2;
    MouseY = h / 2;
    activeInstance = this;

    KDBG1("DESKTOP Initialized with ID 0x%x", this->ID);

    // Initialize the wallpaper.
    Bitmap* wallpaperImg = new Bitmap(PATH_DESKTOP_BMP);
    if (!wallpaperImg) {
        HALT("CRITICAL: Failed to allocate desktop wallpaper bitmap!\n");
    }

    if (wallpaperImg->IsValid()) {
        this->Wallpaper = wallpaperImg;
    } else {
        // Fallback: a solid bluish color.
        this->Wallpaper = new Bitmap(w, h, 0xFF0000FF);
        if (!this->Wallpaper) {
            HALT("CRITICAL: Failed to allocate fallback wallpaper bitmap!\n");
        }
        delete wallpaperImg;
    }

    memset(cursorBackBuffer, 0, sizeof(cursorBackBuffer));

    // Initialize the taskbar.
    taskbar = new Taskbar(this, w, h);
    if (!taskbar) {
        HALT("CRITICAL: Failed to allocate Taskbar!\n");
    }
    taskbar->SetPID(0);  // Kernel-owned.
    taskbar->SetID(0);   // System widget.

    // Add application launchers.
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
    // Serialize against concurrent tree mutation.
    InterruptGuard guard;

    uint32_t screenW = gc->GetWidth();
    uint32_t screenH = gc->GetHeight();
    uint32_t* vesaBuffer = gc->GetBackBuffer();

    // Check whether the taskbar needs a redraw.
    if (taskbar && taskbar->isDirty) {
        this->isDirty = true;
    }
    // Also check the start menu dirty state.
    if (taskbar && taskbar->IsStartMenuOpen()) {
        StartMenu* menu = taskbar->GetStartMenu();
        if (menu && menu->isDirty) {
            this->isDirty = true;
        }
    }

    //    -----------------------------------------------------------------
    // Case 1: Full system redraw.
    // Triggered when a window moves, opens, closes, or invalidates the desktop.
    //    -----------------------------------------------------------------
    if (this->isDirty) {
        // Draw the wallpaper.
        gc->DrawBitmap(0, 0, (const uint32_t*)this->Wallpaper->GetBuffer(),
                       this->Wallpaper->GetWidth(), this->Wallpaper->GetHeight());

        // Composite all children (windows) onto the screen; this calls
        // CompositeWidget::Draw, which calls Window::Draw.
        CompositeWidget::Draw(gc);

        // Draw the taskbar on top of everything.
        taskbar->Draw(gc);

        // Save the background under the mouse (fresh capture).
        {
            InterruptGuard guard;
            for (int y = 0; y < CURSOR_SIZE; y++) {
                for (int x = 0; x < CURSOR_SIZE; x++) {
                    // Bounds check.
                    int destX = MouseX + x;
                    int destY = MouseY + y;

                    if (destX >= (int)screenW || destY >= (int)screenH) continue;

                    cursorBackBuffer[y * CURSOR_SIZE + x] = vesaBuffer[destY * screenW + destX];
                }
            }
            hasBackBuffer = true;

            // Draw the cursor.
            gc->DrawBitmap(MouseX, MouseY, (const uint32_t*)icon_cursor_20x20, CURSOR_SIZE,
                           CURSOR_SIZE);

            // Reset state.
            this->isDirty = false;
            oldMouseX = MouseX;
            oldMouseY = MouseY;
        }
        return;
    }

    //    -----------------------------------------------------------------
    // Case 2: Mouse movement only.
    // Optimization: if nothing else changed, just undraw/redraw the cursor.
    //    -----------------------------------------------------------------
    if (MouseX != oldMouseX || MouseY != oldMouseY) {
        // Erase the old cursor by restoring the saved pixels.
        {
            InterruptGuard guard;
            if (hasBackBuffer) {
                for (int y = 0; y < CURSOR_SIZE; y++) {
                    for (int x = 0; x < CURSOR_SIZE; x++) {
                        int destX = oldMouseX + x;
                        int destY = oldMouseY + y;

                        if (destX >= (int)screenW || destY >= (int)screenH) continue;

                        // Direct buffer write for speed.
                        vesaBuffer[destY * screenW + destX] = cursorBackBuffer[y * CURSOR_SIZE + x];
                    }
                }
            }
        }

        // Capture the background at the new position.
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

            // Draw the new cursor.
            gc->DrawBitmap(MouseX, MouseY, (const uint32_t*)icon_cursor_20x20, CURSOR_SIZE,
                           CURSOR_SIZE);

            // Update history.
            oldMouseX = MouseX;
            oldMouseY = MouseY;
        }
    }
}

uint32_t Desktop::getNewID() {
    return current_id++;
}

void Desktop::RemoveAppByPID(uint32_t pid) {
    // Serialize against a concurrent Desktop::Draw/CompositeWidget::Draw.
    InterruptGuard guard;

    Widget* result = nullptr;
    childrenList.ForEach([&](Widget* c) {
        if (!result && c->PID == pid) result = c;
    });

    if (result) {
        this->RemoveChild(result);
        this->MarkDirty();  // Ensure the screen clears the removed window.

        // Restore focus to the topmost remaining window so keyboard
        // input continues to flow (e.g., Terminal that launched the app).
        Widget* topmost = nullptr;
        childrenList.ForEach([&](Widget* c) {
            topmost = c;  // The last visited is the topmost in Z-order.
        });
        if (topmost) {
            this->GetFocus(topmost);
        }
    }

    // Remove the corresponding taskbar tab.
    if (taskbar) {
        taskbar->RemoveTabByPID(pid);
    }
}

void Desktop::Focus(Widget* widget) {
    if (!widget) return;

    // Find and remove the widget from the list.
    bool found = childrenList.Remove([&](Widget* w) { return w == widget; });

    // If found, add it to the back (top of Z-order).
    if (found) {
        childrenList.PushBack(widget);
        this->isDirty = true;  // Force a full redraw.
    }
}

// Inputs.

void Desktop::GetFocus(Widget* widget) {
    // Call the parent to handle normal focus/Z-ordering.
    CompositeWidget::GetFocus(widget);

    // Update the taskbar tab to reflect the newly focused window.
    if (taskbar) {
        taskbar->SetActiveTab(widget);
    }
}

void Desktop::OnMouseDown(uint8_t button) {
    // Check whether the click is in the start menu area (above the taskbar).
    if (taskbar && taskbar->IsStartMenuOpen() && taskbar->StartMenuContains(MouseX, MouseY)) {
        taskbar->OnMouseDown(MouseX, MouseY, button);
        return;
    }

    // Check whether the click is in the taskbar area.
    if (taskbar && taskbar->ContainsCoordinate(MouseX, MouseY)) {
        taskbar->OnMouseDown(MouseX, MouseY, button);
        return;
    }

    // A click on the desktop/window area dismisses the start menu if open.
    if (taskbar && taskbar->IsStartMenuOpen()) {
        taskbar->CloseStartMenu();
    }
    CompositeWidget::OnMouseDown(MouseX, MouseY, button);
}

void Desktop::OnMouseUp(uint8_t button) {
    // Route to the start menu.
    if (taskbar && taskbar->IsStartMenuOpen() && taskbar->StartMenuContains(MouseX, MouseY)) {
        taskbar->OnMouseUp(MouseX, MouseY, button);
        return;
    }

    // Route to the taskbar.
    if (taskbar && taskbar->ContainsCoordinate(MouseX, MouseY)) {
        taskbar->OnMouseUp(MouseX, MouseY, button);
        return;
    }
    CompositeWidget::OnMouseUp(MouseX, MouseY, button);
}

void Desktop::OnMouseMove(int32_t dx, int32_t dy) {
    // Use signed arithmetic to prevent unsigned wrap when dx/dy are negative.
    int32_t newX = (int32_t)MouseX + dx;
    int32_t newY = (int32_t)MouseY + dy;

    // Clamp to the screen bounds.
    if (newX < 0) newX = 0;
    if (newY < 0) newY = 0;
    if (newX >= w) newX = w - 1;
    if (newY >= h) newY = h - 1;

    // Capture the pre-update cursor position before clamping.
    int32_t oldX = (int32_t)MouseX;
    int32_t oldY = (int32_t)MouseY;

    MouseX = (uint32_t)newX;
    MouseY = (uint32_t)newY;

    // Pass the delta to the UI.
    CompositeWidget::OnMouseMove(oldX, oldY, MouseX, MouseY);

    // Also route mouse moves to the taskbar/start menu for hover effects.
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
