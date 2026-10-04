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

#ifndef DESKTOP_H
#define DESKTOP_H

#include <core/drivers/GraphicsDriver.h>
#include <core/drivers/keyboard.h>
#include <core/drivers/mouse.h>
#include <gui/bmp.h>
#include <gui/taskbar.h>
#include <gui/widget.h>

/**
 * ApplyDisplayMode() - Switch the display to a new resolution and re-lay out.
 * @width: Requested width in pixels.
 * @height: Requested height in pixels.
 *
 * Every resolution change funnels through here: at boot from the persisted
 * settings, and at runtime from the Settings app. Lives in the GUI layer
 * because re-laying out the desktop is a GUI concern, while the actual hardware
 * switch is delegated to the active GraphicsDriver.
 *
 * Return: True when the new mode is active and the desktop has been re-laid out.
 */
bool ApplyDisplayMode(uint16_t width, uint16_t height);

/**
 * class Desktop - The root widget, managing wallpaper, cursor, and windows.
 *
 * Acts as the top-level composite: it owns every window, tracks the mouse,
 * and keeps the per-process GUI event handlers. A single active instance
 * (activeInstance) is set at boot for global access.
 */
class FileSystem;
class Desktop : public CompositeWidget, public MouseEventHandler, public KeyboardEventHandler {
protected:
    uint32_t MouseX;
    uint32_t MouseY;
    uint32_t current_id = 1000;
    Bitmap* Wallpaper;
    // Native-resolution wallpaper, decoded once and kept so resolution changes
    // rescale from memory instead of re-reading the BMP off the disk.
    Bitmap* WallpaperSource;

    // BuildWallpaper() - Produce a screen-sized wallpaper for w x h.
    Bitmap* BuildWallpaper(int32_t w, int32_t h);

    // Cursor optimization buffers.
    int32_t oldMouseX = 0;
    int32_t oldMouseY = 0;

    // Buffer for the pixels behind the cursor (max 32x32 support).
    static const int CURSOR_SIZE = 20;
    uint32_t cursorBackBuffer[CURSOR_SIZE * CURSOR_SIZE];
    bool hasBackBuffer = false;

    LinkedList<EventHandler*> HguiEventHandlers;
    Taskbar* taskbar;

    // While set, every mouse event is routed to this widget alone so a modal
    // dialog cannot be clicked through. NULL when no dialog is up.
    Widget* modalWidget = nullptr;

    // Action to run once the next frame is on screen, plus its argument. Used to
    // sequence an irreversible action behind the repaint that removes whatever
    // asked for it, so the machine never halts still showing that UI.
    void (*postPresentAction)(void*) = nullptr;
    void* postPresentInstance = nullptr;

public:
    static Desktop* activeInstance;

    /**
     * Desktop() - Construct the root widget covering the whole screen.
     * @w: Screen width in pixels.
     * @h: Screen height in pixels.
     */
    Desktop(int32_t w, int32_t h);

    /**
     * ~Desktop() - Destroy every window and release event handlers.
     */
    ~Desktop();

    /**
     * createNewHandler() - Register a GUI event handler for a process.
     * @pid: Owning process ID.
     * @thread: Owner thread.
     */
    void createNewHandler(uint32_t pid, ThreadControlBlock* thread);

    /**
     * deleteEventHandler() - Remove and destroy a process event handler.
     * @pid: Owning process ID.
     */
    void deleteEventHandler(uint32_t pid);

    /**
     * getHandler() - Look up the event handler for a process.
     * @pid: Owning process ID.
     *
     * Return: The matching EventHandler, or NULL if not found.
     */
    EventHandler* getHandler(uint32_t pid);

    /**
     * Draw() - Master draw: restore the cursor, paint widgets and wallpaper.
     * @gc: Target graphics driver.
     */
    void Draw(GraphicsDriver* gc) override;

    /**
     * getNewID() - Allocate a unique widget instance ID.
     *
     * Return: The next monotonically increasing ID.
     */
    uint32_t getNewID();

    /**
     * RemoveAppByPID() - Remove and destroy every widget owned by a process.
     * @PID: Owner process ID.
     */
    void RemoveAppByPID(uint32_t PID);

    /** GetFocus() - Forward focus to the widget root and focus @widget. */
    void GetFocus(Widget* widget) override;

    /**
     * Focus() - Set the focused widget and fire focus events.
     * @widget: Widget to focus.
     */
    void Focus(Widget* widget);

    /**
     * FocusTopMostWindow() - Focus the topmost visible child window.
     *
     * Hands keyboard focus to the window behind a dialog that is closing, or
     * clears it when none is visible. Used instead of dropping focus on the
     * floor, which would strand input until the user clicked a window.
     */
    void FocusTopMostWindow();

    /**
     * SetModalWidget() - Give one widget exclusive mouse input, or release it.
     * @widget: Widget to receive all mouse events, or NULL to release.
     */
    void SetModalWidget(Widget* widget);

    /** GetModalWidget() - Return the widget holding exclusive input, or NULL. */
    Widget* GetModalWidget() const {
        return modalWidget;
    }

    /**
     * PostAfterPresent() - Queue an action to run after the next presented frame.
     * @action: Callback taking @instance. May not return.
     * @instance: Opaque argument passed to @action.
     *
     * Mouse input arrives on the IRQ 12 path, so a callback queued from a click
     * handler would otherwise run in interrupt context. Posting it here moves it
     * onto the desktop task, after a frame that no longer shows the widget the
     * user clicked. Only one action is queued; a second post replaces the first,
     * since anything still pending is a stale response to UI that is gone.
     */
    void PostAfterPresent(void (*action)(void*), void* instance);

    /** HasPostPresentAction() - Report whether an action is waiting for a frame. */
    bool HasPostPresentAction() const {
        return postPresentAction != nullptr;
    }

    /**
     * RunPostPresentAction() - Run the queued action, if any.
     *
     * Called by the desktop task once the frame is on the hardware. Clears the
     * slot before invoking so an action that does not return cannot be run twice.
     */
    void RunPostPresentAction();

    /** GetTaskbar() - Return the taskbar attached to the desktop. */
    Taskbar* GetTaskbar() {
        return taskbar;
    }

    /**
     * OnResolutionChanged() - Re-lay out the whole desktop for a new screen size.
     * @w: New screen width in pixels.
     * @h: New screen height in pixels.
     *
     * Called after the graphics driver accepted a mode switch. Resizes the
     * root widget, rebuilds the wallpaper for the new geometry, re-docks the
     * taskbar, pulls windows that no longer fit back on screen, and clamps the
     * cursor. A full repaint is scheduled so the first frame after the switch
     * is already correct.
     */
    void OnResolutionChanged(uint32_t w, uint32_t h);

    // Driver inputs.
    void OnMouseDown(uint8_t button) override;
    void OnMouseUp(uint8_t button) override;
    void OnMouseMove(int32_t dx, int32_t dy) override;
    void OnKeyDown(const char* key) override;
    void OnSpecialKeyDown(uint8_t key) override;
    void OnKeyUp(const char* key) override;
    void OnSpecialKeyUp(uint8_t key) override;

    /** MouseMoved() - Report whether the cursor has moved since the last draw. */
    bool MouseMoved() {
        return (MouseX != oldMouseX || MouseY != oldMouseY);
    }
};

/**
 * struct DesktopArgs - Bootstrap parameters handed to the desktop task.
 * @screen: Active graphics driver.
 * @desktop: The desktop root widget.
 * @boot_partition: Boot partition holding loaded programs.
 */
struct DesktopArgs {
    GraphicsDriver* screen;
    Desktop* desktop;
    FileSystem* boot_partition;
};

/**
 * struct LoadProgArgs - Parameters for the program loader task.
 * @elfLoader: ELF loader used to map programs.
 * @boot_partition: Boot partition holding the program images.
 */
struct LoadProgArgs {
    ELFLoader* elfLoader;
    FileSystem* boot_partition;
};

#endif  // DESKTOP_H
