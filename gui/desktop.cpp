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
#include <core/globals.h>
#include <gui/contextmenu.h>
#include <gui/desktop.h>
#include <gui/widgetHost.h>
#include <gui/widgetgallery.h>

Desktop* Desktop::activeInstance = nullptr;

// Defined below, next to the input routing that uses them.
static bool TreeContainsWidget(Widget* root, Widget* target);
static void ScreenToParentFrame(Widget* widget, int32_t& x, int32_t& y);

/**
 * LaunchProgram() - Load and run a userspace ELF from the boot partition.
 * @binPath: Path relative to the filesystem root.
 */
static void LaunchProgram(const char* binPath) {
    if (!g_bootPartition || !g_elfLoader || !g_scheduler || !binPath || binPath[0] == '\0') return;

    File* file = g_bootPartition->Open(binPath);
    if (!file || file->size == 0) {
        KDBG1("Desktop: program not found: %s", binPath);
        if (file) {
            file->Close();
            delete file;
        }
        return;
    }

    ProcessControlBlock* proc = g_elfLoader->loadELF(file, nullptr);
    file->Close();
    delete file;

    if (!proc) {
        KDBG1("Desktop: failed to load ELF: %s", binPath);
    } else if (!g_scheduler->StartProcess(proc, nullptr)) {
        KDBG1("Desktop: failed to start ELF: %s", binPath);
    }
}

/**
 * HandleDesktopMenu() - Run the action for a desktop context-menu item.
 * @menu: The menu that fired (unused).
 * @index: Selected item index.
 * @user: The Desktop that owns the menu.
 */
static void HandleDesktopMenu(ContextMenu* menu, int32_t index, void* user) {
    (void)menu;
    Desktop* desktop = static_cast<Desktop*>(user);
    switch (index) {
        case 0:  // Widget Gallery.
            if (desktop) OpenWidgetGallery(desktop);
            break;
        case 1:  // Display Settings.
            LaunchProgram(PATH_SETTINGS);
            break;
        case 2:  // Terminal.
            LaunchProgram(PATH_TERMINAL);
            break;
        default:
            break;
    }
}

Desktop::Desktop(int32_t w, int32_t h)
    : CompositeWidget(0, 0, 0, w, h), MouseEventHandler(), KeyboardEventHandler() {
    MouseX = w / 2;
    MouseY = h / 2;
    activeInstance = this;

    KDBG1("DESKTOP Initialized with ID 0x%x", this->ID);

    // Initialize the wallpaper. The source is decoded once and kept; the
    // screen-sized copy is rescaled from it for the current geometry.
    this->WallpaperSource = new Bitmap(PATH_DESKTOP_BMP);
    if (this->WallpaperSource && !this->WallpaperSource->IsValid()) {
        delete this->WallpaperSource;
        this->WallpaperSource = nullptr;
    }

    this->Wallpaper = BuildWallpaper(w, h);

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
    taskbar->AddApp("Settings", "Display & resolution", PATH_SETTINGS);
    taskbar->AddApp("Terminal", "CLI preview", PATH_TERMINAL);
    taskbar->AddApp("Game3D", "3D Game Engine", PATH_GAME3D);

    // Power controls, as small round buttons on the right of the start menu
    // header so they stay clear of the application list. These act
    // immediately on click with no confirmation.
    taskbar->AddPowerButton(START_MENU_ACTION_RESTART, "fa-restart");
    taskbar->AddPowerButton(START_MENU_ACTION_SHUTDOWN, "fa-power-off");

    // Right-click menu over the wallpaper. It is a normal child (so it draws
    // and receives events through the composite tree) but starts hidden.
    contextMenu = new ContextMenu(this);
    if (contextMenu) {
        contextMenu->AddItem("Widget Gallery");
        contextMenu->AddItem("Display Settings");
        contextMenu->AddItem("Terminal");
        contextMenu->SetClickHandler(&HandleDesktopMenu, this);
        this->AddChild(contextMenu);
    }

    // NOTE: Taskbar is NOT added to childrenList.
}

Desktop::~Desktop() {
    if (Wallpaper) delete Wallpaper;
    if (WallpaperSource) delete WallpaperSource;
    if (taskbar) delete taskbar;
}

/**
 * Desktop::BuildWallpaper() - Produce a screen-sized wallpaper for w x h.
 * @w: Screen width in pixels.
 * @h: Screen height in pixels.
 *
 * Rescales the resident source to cover the screen with its aspect ratio
 * preserved, centered so the overflow crops evenly on both axes. An image
 * larger than the screen is center-cropped at native resolution rather than
 * downscaled, so it stays sharp instead of turning to mush.
 *
 * Falls back to a solid bluish fill if the source is missing or the rescale
 * cannot be allocated, so the desktop always has a valid background.
 */
Bitmap* Desktop::BuildWallpaper(int32_t w, int32_t h) {
    if (WallpaperSource) {
        Bitmap* scaled = Bitmap::ScaledCover(WallpaperSource, w, h);
        if (scaled && scaled->IsValid()) return scaled;
        if (scaled) delete scaled;
    }

    Bitmap* solid = new Bitmap(w, h, 0xFF0000FF);
    if (!solid || !solid->IsValid()) {
        if (solid) delete solid;
        HALT("CRITICAL: Failed to allocate fallback wallpaper bitmap!\n");
        return nullptr;  // HALT does not return; keeps static analysis happy.
    }
    return solid;
}

/**
 * Desktop::OnResolutionChanged() - Re-lay out the whole desktop for a new screen size.
 * @w: New screen width in pixels.
 * @h: New screen height in pixels.
 *
 * The wallpaper is rescaled to cover the new screen with its aspect ratio kept,
 * so a screen larger than the image upscales it and a screen smaller than the
 * image centers and crops it. Windows are clamped instead of scaled, so their
 * contents keep their size.
 */
void Desktop::OnResolutionChanged(uint32_t w, uint32_t h) {
    if (w == 0 || h == 0) return;
    if ((int32_t)w == this->w && (int32_t)h == this->h) return;

    // Serialize against the GUI task, which draws from this same tree.
    InterruptGuard guard;

    // A popup positioned for the old geometry is no longer meaningful.
    if (contextMenu) contextMenu->Hide();

    // Release the cache Widget sized for the old geometry. CompositeWidget::Draw()
    // still runs Widget::Draw() here, so a stale cache would be memset to the new
    // w*h and overrun into allocator metadata; null makes RedrawToCache() a no-op.
    if (cache) {
        delete[] cache;
        cache = nullptr;
    }
    this->w = (int32_t)w;
    this->h = (int32_t)h;

    // ---- Wallpaper --------------------------------------------------------
    Bitmap* rebuilt = BuildWallpaper((int32_t)w, (int32_t)h);
    if (rebuilt) {
        delete this->Wallpaper;
        this->Wallpaper = rebuilt;
    }
    // ---- Taskbar ----------------------------------------------------------
    if (taskbar) {
        taskbar->SetScreenDimensions((int32_t)w, (int32_t)h);
    }

    // ---- Windows ----------------------------------------------------------
    // Keep the usable area above the taskbar.
    int32_t usableH = (int32_t)h - TASKBAR_HEIGHT;
    childrenList.ForEach([&](Widget* child) {
        if (child->w > (int32_t)w) child->Resize((int32_t)w, child->h);
        if (child->h > usableH) child->Resize(child->w, usableH);
        if (child->x < 0) child->x = 0;
        if (child->y < 0) child->y = 0;
        if (child->x + child->w > (int32_t)w) child->x = (int32_t)w - child->w;
        if (child->y + child->h > usableH) child->y = usableH - child->h;
        child->MarkDirty();
    });

    // ---- Cursor -----------------------------------------------------------
    if (MouseX + CURSOR_SIZE > w) MouseX = (w > CURSOR_SIZE) ? w - CURSOR_SIZE : 0;
    if (MouseY + CURSOR_SIZE > h) MouseY = (h > CURSOR_SIZE) ? h - CURSOR_SIZE : 0;
    hasBackBuffer = false;

    // Force a full repaint on the next frame.
    this->isDirty = true;

    KDBG1("Desktop re-laid out for %ux%u", w, h);
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

void Desktop::Draw(RasterSurface* gc) {
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

        // Composite all children (windows) onto the screen. Draw them
        // directly: the desktop is itself a CompositeWidget, and letting the
        // base class run would first composite every window into the desktop's
        // own cache (which is never presented) before blitting them again.
        childrenList.ForEach([&](Widget* child) { child->Draw(gc); });

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
        // A drag that started inside the departing app must not outlive it.
        if (dragTarget && TreeContainsWidget(result, dragTarget)) {
            dragTarget = nullptr;
            pointerDragging = false;
        }

        this->RemoveChild(result);
        this->MarkDirty();  // Ensure the screen clears the removed window.

        // Restore focus to the topmost remaining window so keyboard
        // input continues to flow (e.g., Terminal that launched the app).
        Widget* topmost = nullptr;
        childrenList.ForEach([&](Widget* c) {
            if (c->isVisible && c->IsWindow()) topmost = c;
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

void Desktop::FocusTopMostWindow() {
    // The last visible child is the topmost in Z-order. Focusing a child raises
    // it, so this restores keyboard input to the window behind a closing dialog.
    Widget* topmost = nullptr;
    childrenList.ForEach([&](Widget* c) {
        // Only real windows can take focus; the hidden context menu must not
        // become the fallback owner of keyboard input.
        if (c->isVisible && c->IsWindow()) topmost = c;
    });
    GetFocus(topmost);
}

/**
 * Desktop::SetModalWidget() - Give one widget exclusive mouse input.
 * @widget: Widget to receive all mouse events, or NULL to release.
 *
 * Set by a modal dialog so clicks land on the prompt instead of the windows
 * behind it. A NULL widget clears the lock.
 */
void Desktop::SetModalWidget(Widget* widget) {
    modalWidget = widget;
    this->isDirty = true;
}

void Desktop::PostAfterPresent(void (*action)(void*), void* instance) {
    // Posted from the IRQ 12 path, so this races the desktop task draining the
    // slot. InterruptGuard nests safely, so take it here and keep the two-field
    // store indivisible.
    InterruptGuard guard;
    postPresentAction = action;
    postPresentInstance = instance;
}

void Desktop::RunPostPresentAction() {
    void (*action)(void*);
    void* instance;

    {
        // Copy out and clear atomically, so a post arriving from the IRQ path
        // can neither be dropped nor have its instance torn from under it.
        InterruptGuard guard;

        // A dirty desktop means the frame just flushed is already stale: the
        // post landed after Draw() captured the tree (Draw masks IRQs, so the
        // click that hid the dialog is serviced once Draw returns, before
        // Flush()). Keep the action queued - the next pass draws and presents
        // the frame without the dialog before draining it.
        if (this->isDirty) return;

        action = postPresentAction;
        instance = postPresentInstance;
        postPresentAction = nullptr;
        postPresentInstance = nullptr;
    }

    // Clear first: the action may never return, and a later frame must not be
    // able to run it a second time. The guard is released before the call
    // because the action drives power and may halt or reset.
    if (action) action(instance);
}

bool Desktop::HitChildAt(int32_t x, int32_t y, Widget** out) {
    bool found = false;
    childrenList.ReverseForEach([&](Widget* child) {
        if (!found && child->isVisible && child->ContainsCoordinate(x, y)) {
            // Return the deepest visible widget, not just the top-level window:
            // drag events have to reach sliders/lists nested inside a window.
            Widget* deepest = child->HitTest(x, y);
            *out = deepest ? deepest : child;
            found = true;
        }
    });
    return found;
}

/**
 * TreeContainsWidget() - Test whether @target is still in @root's subtree.
 * @root: Subtree root.
 * @target: Widget to find by address.
 *
 * Only addresses are compared, so this is safe to call with a pointer that may
 * point at freed memory: the target is never dereferenced unless it is found.
 */
static bool TreeContainsWidget(Widget* root, Widget* target) {
    if (!root || !target) return false;
    if (root == target) return true;

    bool found = false;
    root->childrenList.ForEach([&](Widget* child) {
        if (!found) found = TreeContainsWidget(child, target);
    });
    return found;
}

/**
 * ScreenToParentFrame() - Convert a screen point to @widget's parent frame.
 * @widget: Target widget.
 * @x: In/out screen X.
 * @y: In/out screen Y.
 *
 * Widget mouse handlers receive coordinates in their parent's frame; drag
 * callbacks follow the same contract.
 */
static void ScreenToParentFrame(Widget* widget, int32_t& x, int32_t& y) {
    Widget* parent = widget ? widget->parent : nullptr;
    if (!parent) return;
    int32_t originX = 0;
    int32_t originY = 0;
    parent->ModelToScreen(originX, originY);
    x -= originX;
    y -= originY;
}

/**
 * CopyKey() - Copy a driver key string into a fixed event field.
 * @dst: Four-byte destination (three characters plus NUL).
 * @src: Source string, possibly NULL.
 */
static void CopyKey(char (&dst)[4], const char* src) {
    int i = 0;
    if (src) {
        while (src[i] != '\0' && i < 3) {
            dst[i] = src[i];
            i++;
        }
    }
    dst[i] = '\0';
}

void Desktop::ProcessInput() {
    InputEvent event;

    while (true) {
        {
            // Drivers push from interrupt context. Only the ring indices are
            // shared, so protect the pop and dispatch outside the guard.
            InterruptGuard guard;
            if (!inputQueue.Pop(event)) break;
        }
        DispatchInputEvent(event);
    }

    // Deferred widget destruction is safe here: every handler has returned and
    // no widget code is on the stack, so a closed window can finally be freed.
    DrainDeferredWidgetDeletes();
}

void Desktop::DispatchInputEvent(const InputEvent& event) {
    switch (event.type) {
        case IE_MOUSE_DOWN:
            HandleMouseDown(event.button);
            break;
        case IE_MOUSE_UP:
            HandleMouseUp(event.button);
            break;
        case IE_MOUSE_MOVE:
            HandleMouseMove(event.x, event.y);
            break;
        case IE_MOUSE_WHEEL:
            HandleMouseWheel(event.y);
            break;
        case IE_KEY_DOWN:
            HandleKeyDown(event.key);
            break;
        case IE_KEY_UP:
            HandleKeyUp(event.key);
            break;
        case IE_KEY_REPEAT:
            CompositeWidget::OnKeyRepeat(event.key, event.repeatCount);
            break;
        case IE_SPECIAL_DOWN:
            HandleSpecialKeyDown(event.special);
            break;
        case IE_SPECIAL_UP:
            HandleSpecialKeyUp(event.special);
            break;
        case IE_FOCUS_TAB:
            CompositeWidget::OnTabFocus((event.modifiers & MOD_SHIFT) != 0);
            break;
        default:
            break;
    }
}

// ---- Driver entry points: interrupt context, enqueue only. ----

void Desktop::OnMouseDown(uint8_t button) {
    InputEvent event{};
    event.type = IE_MOUSE_DOWN;
    event.button = button;
    inputQueue.Push(event);
}

void Desktop::OnMouseUp(uint8_t button) {
    InputEvent event{};
    event.type = IE_MOUSE_UP;
    event.button = button;
    inputQueue.Push(event);
}

void Desktop::OnMouseMove(int32_t dx, int32_t dy) {
    InputEvent event{};
    event.type = IE_MOUSE_MOVE;
    event.x = dx;
    event.y = dy;
    inputQueue.Push(event);
}

void Desktop::OnMouseWheel(int32_t delta) {
    InputEvent event{};
    event.type = IE_MOUSE_WHEEL;
    event.y = delta;  // Wheel deltas ride in @y.
    inputQueue.Push(event);
}

void Desktop::OnKeyDown(const char* key) {
    InputEvent event{};
    event.type = IE_KEY_DOWN;
    CopyKey(event.key, key);
    inputQueue.Push(event);
}

void Desktop::OnKeyUp(const char* key) {
    InputEvent event{};
    event.type = IE_KEY_UP;
    CopyKey(event.key, key);
    inputQueue.Push(event);
}

void Desktop::OnSpecialKeyDown(uint8_t key) {
    InputEvent event{};
    event.type = IE_SPECIAL_DOWN;
    event.special = key;
    inputQueue.Push(event);
}

void Desktop::OnSpecialKeyUp(uint8_t key) {
    InputEvent event{};
    event.type = IE_SPECIAL_UP;
    event.special = key;
    inputQueue.Push(event);
}

// ---- Desktop-task handlers. ----

void Desktop::HandleMouseDown(uint8_t button) {
    // Any press outside an open context menu dismisses it, wherever the menu
    // lives in the tree.
    ContextMenu::DismissActiveAt((int32_t)MouseX, (int32_t)MouseY);

    // Track the press and detect a double-click (same button, within 400 ms and
    // 4 px of the previous click).
    uint64_t now = guiplat::MonotonicMillis();
    int32_t dx = (int32_t)MouseX - lastClickX;
    int32_t dy = (int32_t)MouseY - lastClickY;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    bool doubleClick =
        (button == lastClickButton) && (now - lastClickTime <= 400) && dx <= 4 && dy <= 4;

    lastClickTime = now;
    lastClickX = (int32_t)MouseX;
    lastClickY = (int32_t)MouseY;
    lastClickButton = button;
    pointerDown = true;
    pressX = (int32_t)MouseX;
    pressY = (int32_t)MouseY;
    pressButton = button;

    if (doubleClick && !(modalWidget && modalWidget->isVisible) &&
        !(taskbar && taskbar->ContainsCoordinate(MouseX, MouseY))) {
        CompositeWidget::OnDoubleClick((int32_t)MouseX, (int32_t)MouseY, button);
    }

    // A modal dialog owns the pointer until it is dismissed.
    if (modalWidget && modalWidget->isVisible) {
        modalWidget->OnMouseDown((int32_t)MouseX, (int32_t)MouseY, button);
        return;
    }

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

    // Right-clicking the wallpaper opens the desktop menu. A right-click over
    // a window is left to the window.
    if (button == 3 && contextMenu) {
        Widget* hit = nullptr;
        if (!HitChildAt((int32_t)MouseX, (int32_t)MouseY, &hit)) {
            contextMenu->ShowAt((int32_t)MouseX, (int32_t)MouseY);
            return;
        }
    }

    CompositeWidget::OnMouseDown(MouseX, MouseY, button);
}

void Desktop::HandleMouseUp(uint8_t button) {
    if (pointerDragging && dragTarget) {
        // A widget can be destroyed mid-drag (its owner exits); only forward
        // the end event if the target is still in the live tree.
        if (TreeContainsWidget(this, dragTarget)) {
            int32_t x = (int32_t)MouseX;
            int32_t y = (int32_t)MouseY;
            ScreenToParentFrame(dragTarget, x, y);
            dragTarget->OnDragEnd(x, y, button);
        }
    }
    pointerDown = false;
    pointerDragging = false;
    dragTarget = nullptr;

    // Route the release to the modal dialog that took the press.
    if (modalWidget && modalWidget->isVisible) {
        modalWidget->OnMouseUp((int32_t)MouseX, (int32_t)MouseY, button);
        return;
    }

    // A window owning a drag (slider thumb, scrollbar, ...) must receive the
    // release even if the cursor finishes over the taskbar or start menu;
    // otherwise the widget stays captured and keeps tracking the pointer.
    bool capturedWindow = false;
    childrenList.ForEach([&](Widget* child) {
        if (!capturedWindow && child->isVisible &&
            (child->IsMouseCaptured() || child->IsPressed())) {
            capturedWindow = true;
        }
    });
    if (capturedWindow) {
        CompositeWidget::OnMouseUp((int32_t)MouseX, (int32_t)MouseY, button);
        return;
    }

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

void Desktop::HandleMouseMove(int32_t dx, int32_t dy) {
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

    // Dispatch drag events once the pointer has moved past the threshold. The
    // target is the deepest widget under the press point, so nested sliders and
    // lists receive their own drag stream.
    if (pointerDown) {
        int32_t adx = (int32_t)MouseX - pressX;
        int32_t ady = (int32_t)MouseY - pressY;
        if (adx < 0) adx = -adx;
        if (ady < 0) ady = -ady;
        if (!pointerDragging && (adx + ady) > 4) {
            Widget* target = nullptr;
            if (HitChildAt(pressX, pressY, &target)) {
                pointerDragging = true;
                dragTarget = target;

                int32_t sx = pressX;
                int32_t sy = pressY;
                ScreenToParentFrame(target, sx, sy);
                target->OnDragStart(sx, sy, pressButton);
            }
        } else if (pointerDragging && dragTarget) {
            if (!TreeContainsWidget(this, dragTarget)) {
                dragTarget = nullptr;
                pointerDragging = false;
            } else {
                dragTarget->OnDragMove(dx, dy);
            }
        }
    }

    // A modal dialog consumes motion too, so hover states elsewhere do not
    // update behind it.
    if (modalWidget && modalWidget->isVisible) {
        modalWidget->OnMouseMove(oldX, oldY, (int32_t)MouseX, (int32_t)MouseY);
        return;
    }

    // Pass the delta to the UI.
    CompositeWidget::OnMouseMove(oldX, oldY, MouseX, MouseY);

    // Also route mouse moves to the taskbar/start menu for hover effects.
    if (taskbar) {
        taskbar->OnMouseMove(oldX, oldY, MouseX, MouseY);
    }
}

void Desktop::HandleMouseWheel(int32_t delta) {
    if (modalWidget && modalWidget->isVisible) {
        modalWidget->OnMouseWheel(delta);
        return;
    }
    if (taskbar && taskbar->ContainsCoordinate((int32_t)MouseX, (int32_t)MouseY)) {
        taskbar->OnMouseWheel(delta);
        return;
    }
    CompositeWidget::OnMouseWheel(delta);
}

void Desktop::HandleKeyDown(const char* key) {
    CompositeWidget::OnKeyDown(key);
}

void Desktop::HandleKeyUp(const char* key) {
    CompositeWidget::OnKeyUp(key);
}

void Desktop::HandleSpecialKeyDown(uint8_t key) {
    // Escape dismisses an open context menu before any widget sees it.
    if (key == 0x01) {
        ContextMenu::HideActive();
    }
    CompositeWidget::OnSpecialKeyDown(key);
}

void Desktop::HandleSpecialKeyUp(uint8_t key) {
    CompositeWidget::OnSpecialKeyUp(key);
}

/**
 * ApplyDisplayMode() - Switch the display to a new resolution and re-lay out.
 * @width: Requested width in pixels.
 * @height: Requested height in pixels.
 *
 * The active GraphicsDriver owns the hardware switch and reports the geometry
 * it actually adopted (an adapter may clamp what it accepted), so the relayout
 * is driven by the driver rather than by the request.
 *
 * Return: True when the new mode is active and the desktop has been re-laid out.
 */
bool ApplyDisplayMode(uint16_t width, uint16_t height) {
    if (!g_GraphicsDriver) return false;
    if (width == 0 || height == 0) return false;

    uint32_t currentW = g_GraphicsDriver->GetWidth();
    uint32_t currentH = g_GraphicsDriver->GetHeight();
    if (width == currentW && height == currentH) {
        return true;
    }

    // A fullscreen process mapped the back buffer into its own address space
    // (Hsys_getFramebuffer). Reallocating it would leave that mapping pointing at
    // freed memory, so the switch is refused until the app gives the screen up.
    if (g_stop_gui_rendering) {
        KDBG1("ApplyDisplayMode: refused, PID %d owns the framebuffer", g_gui_owner_pid);
        return false;
    }

    if (!g_GraphicsDriver->SetVideoMode(width, height)) {
        KDBG1("ApplyDisplayMode: driver refused %ux%u", width, height);
        return false;
    }

    // Re-lay out the desktop against the geometry the driver actually adopted.
    if (Desktop::activeInstance) {
        Desktop::activeInstance->OnResolutionChanged(g_GraphicsDriver->GetWidth(),
                                                     g_GraphicsDriver->GetHeight());
    }

    return true;
}
