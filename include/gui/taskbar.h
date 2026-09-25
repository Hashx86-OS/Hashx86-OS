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

#ifndef TASKBAR_H
#define TASKBAR_H

#include <core/elf.h>
#include <core/filesystem/FileSystem.h>
#include <core/globals.h>
#include <gui/label.h>
#include <gui/widget.h>

// Layout constants.
#define TASKBAR_HEIGHT 44
#define TASKBAR_PADDING 6

// Start button.
#define START_BUTTON_WIDTH 42
#define START_BUTTON_HEIGHT 34

// Start menu.
#define START_MENU_WIDTH 240
#define START_MENU_ITEM_HEIGHT 36
#define START_MENU_HEADER_HEIGHT 40
#define START_MENU_PADDING 6
#define START_MENU_MAX_ITEMS 8

// Taskbar tabs.
#define TASKBAR_TAB_HEIGHT 28
#define TASKBAR_TAB_MAX_WIDTH 160
#define TASKBAR_TAB_MIN_WIDTH 60
#define TASKBAR_TAB_PADDING 4
#define TASKBAR_TAB_MAX_TABS 10

// Clock.
#define TASKBAR_CLOCK_WIDTH 70

// Color palette (0xAARRGGBB).

// Taskbar.
#define TASKBAR_BG_COLOR 0xFF1E1E1E
#define TASKBAR_BG_COLOR_TOP 0xFF2A2A2A
#define TASKBAR_BORDER_COLOR 0xFF3A3A3A
#define TASKBAR_SEPARATOR_COLOR 0xFF3A3A3A

// Start button.
#define START_BTN_BG_NORMAL 0xFF2D2D2D
#define START_BTN_BG_HOVER 0xFF383838
#define START_BTN_BG_PRESSED 0xFF252525
#define START_BTN_BG_ACTIVE 0xFF0078D4
#define START_BTN_BORDER 0xFF404040

// Start menu.
#define START_MENU_BG 0xFF252525
#define START_MENU_BORDER 0xFF404040
#define START_MENU_HEADER_BG 0xFF1E1E1E
#define START_MENU_HEADER_TEXT 0xFF8A8A8A
#define START_MENU_ITEM_BG_NORMAL 0x00000000
#define START_MENU_ITEM_BG_HOVER 0xFF353535
#define START_MENU_ITEM_BG_PRESSED 0xFF2A2A2A
#define START_MENU_ITEM_TEXT 0xFFE0E0E0
#define START_MENU_ITEM_DESC_TEXT 0xFF888888
#define START_MENU_SEPARATOR 0xFF3A3A3A

// Taskbar tab colors.
#define TASKBAR_TAB_BG_NORMAL 0xFF2D2D2D
#define TASKBAR_TAB_BG_HOVER 0xFF383838
#define TASKBAR_TAB_BG_ACTIVE 0xFF404040
#define TASKBAR_TAB_BORDER 0xFF4A4A4A
#define TASKBAR_TAB_TEXT_NORMAL 0xFFB0B0B0
#define TASKBAR_TAB_TEXT_ACTIVE 0xFFFFFFFF
#define TASKBAR_TAB_INDICATOR_ACTIVE 0xFF0078D4

// Clock.
#define TASKBAR_CLOCK_TEXT 0xFFB0B0B0
#define TASKBAR_CLOCK_BG_HOVER 0xFF353535

/**
 * struct TaskbarAppEntry - A description of an application in the start menu.
 * @name: Short display name.
 * @binPath: Program path relative to the root filesystem.
 * @description: One-line description shown under the name.
 */
struct TaskbarAppEntry {
    char name[32];
    char binPath[64];
    char description[64];
};

// Widget classes.

/**
 * class StartMenuButton - A start menu entry that launches an application.
 *
 * Renders a label with an optional description and runs the referenced binary
 * through the process loader when clicked.
 */
class StartMenuButton : public Widget {
private:
    char* label;
    char* description;
    char* binPath;
    bool isPressed;
    bool isHovered;

public:
    /**
     * StartMenuButton() - Construct a start menu entry.
     * @parent: Parent widget, or NULL for a root widget.
     * @x: X position relative to @parent.
     * @y: Y position relative to @parent.
     * @w: Entry width in pixels.
     * @h: Entry height in pixels.
     * @label: Entry label text.
     * @description: One-line description text.
     * @binPath: Program path to launch on click.
     */
    StartMenuButton(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* label,
                    const char* description, const char* binPath);

    /**
     * ~StartMenuButton() - Release the text buffers.
     */
    ~StartMenuButton();

    /** RedrawToCache() - Paint the label and description with hover shading. */
    void RedrawToCache() override;

    void OnMouseDown(int32_t x, int32_t y, uint8_t button) override;
    void OnMouseUp(int32_t x, int32_t y, uint8_t button) override;
    void OnMouseMove(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy) override;

    /** LaunchProgram() - Load and run the configured program. */
    void LaunchProgram();
};

/**
 * class TaskbarTab - A taskbar button representing a running window.
 *
 * Tracks the process ID and window a tab refers to and toggles that window's
 * visibility and focus when clicked.
 */
class TaskbarTab : public Widget {
private:
    char* label;
    uint32_t pid;
    Widget* windowWidget;  // The window this tab represents.
    bool isHovered;
    bool isActive;

public:
    /**
     * TaskbarTab() - Construct a taskbar tab bound to a window.
     * @parent: Parent widget, or NULL for a root widget.
     * @x: X position relative to @parent.
     * @y: Y position relative to @parent.
     * @w: Tab width in pixels.
     * @h: Tab height in pixels.
     * @label: Tab label text.
     * @pid: Process ID owning @window.
     * @window: Window the tab represents.
     */
    TaskbarTab(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* label,
               uint32_t pid, Widget* window);

    /**
     * ~TaskbarTab() - Release the label buffer.
     */
    ~TaskbarTab();

    /** GetPID() - Return the process ID this tab represents. */
    uint32_t GetPID() const {
        return pid;
    }

    /** GetWindow() - Return the window this tab represents. */
    Widget* GetWindow() const {
        return windowWidget;
    }

    /**
     * SetActive() - Highlight or unhighlight the tab.
     * @active: True for the active window's tab.
     */
    void SetActive(bool active);

    /** IsActive() - Report whether this tab is the active one. */
    bool IsActive() const {
        return isActive;
    }

    /** GetLabel() - Return the tab label text. */
    const char* GetLabel() const {
        return label;
    }

    /** RedrawToCache() - Paint the tab face and active-state indicator. */
    void RedrawToCache() override;

    void OnMouseDown(int32_t x, int32_t y, uint8_t button) override;
    void OnMouseUp(int32_t x, int32_t y, uint8_t button) override;
    void OnMouseMove(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy) override;
};

/**
 * class StartMenu - The popup menu listing launchable applications.
 *
 * A composite widget displayed above the taskbar when the Start button is
 * active; each child is a StartMenuButton entry.
 */
class StartMenu : public CompositeWidget {
private:
    int32_t itemCount;

public:
    /**
     * StartMenu() - Construct the popup menu.
     * @parent: Parent widget, or NULL for a root widget.
     * @x: X position relative to @parent.
     * @y: Y position relative to @parent.
     * @w: Menu width in pixels.
     * @h: Menu height in pixels.
     */
    StartMenu(CompositeWidget* parent, int32_t x, int32_t y, int32_t w, int32_t h);

    /**
     * ~StartMenu() - Destroy the menu and its entries.
     */
    ~StartMenu();

    /**
     * AddApp() - Append a launchable application entry.
     * @name: Short display name.
     * @description: One-line description.
     * @binPath: Program path to launch.
     */
    void AddApp(const char* name, const char* description, const char* binPath);

    /** Draw() - Paint the menu with its entries. */
    void Draw(GraphicsDriver* gc) override;

    /** RedrawToCache() - Paint the menu background and border. */
    void RedrawToCache() override;
};

/**
 * class StartButton - Taskbar button that toggles the start menu.
 */
class StartButton : public Widget {
private:
    bool isPressed;
    bool isActive;  // Menu is open.

public:
    /**
     * StartButton() - Construct the taskbar start button.
     * @parent: Parent widget, or NULL for a root widget.
     * @x: X position relative to @parent.
     * @y: Y position relative to @parent.
     * @w: Button width in pixels.
     * @h: Button height in pixels.
     */
    StartButton(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h);

    /**
     * ~StartButton() - Destroy the start button.
     */
    ~StartButton();

    /**
     * SetActive() - Toggle the menu-open highlight state.
     * @active: True while the start menu is open.
     */
    void SetActive(bool active);

    /** IsActive() - Report whether the start menu is open. */
    bool IsActive() const {
        return isActive;
    }

    /** RedrawToCache() - Paint the button with state shading. */
    void RedrawToCache() override;

    void OnMouseDown(int32_t x, int32_t y, uint8_t button) override;
    void OnMouseUp(int32_t x, int32_t y, uint8_t button) override;
    void OnMouseMove(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy) override;
};

/**
 * class Taskbar - Bottom-docked bar with Start button, clock, and tabs.
 *
 * Hosts the start button and menu, a live clock, and one tab per running GUI
 * window. Tabs are repositioned when windows open or close.
 */
class Taskbar : public CompositeWidget {
private:
    StartButton* startButton;
    StartMenu* startMenu;
    Label* clockLabel;
    uint32_t lastUpdateTick;
    LinkedList<TaskbarTab*> tabs;
    int32_t tabCount = 0;

    /** UpdateClock() - Refresh the clock label from the RTC. */
    void UpdateClock();

public:
    /**
     * Taskbar() - Construct the taskbar docked at the bottom of the screen.
     * @parent: Parent widget, or NULL for a root widget.
     * @screenW: Screen width in pixels.
     * @screenH: Screen height in pixels.
     */
    Taskbar(CompositeWidget* parent, int32_t screenW, int32_t screenH);

    /**
     * ~Taskbar() - Destroy the taskbar and its components.
     */
    ~Taskbar();

    /**
     * AddApp() - Register an application in the start menu.
     * @name: Short display name.
     * @description: One-line description.
     * @binPath: Program path to launch.
     */
    void AddApp(const char* name, const char* description, const char* binPath);

    /**
     * ToggleStartMenu() - Open the menu if closed, close it if open.
     */
    void ToggleStartMenu();

    /** CloseStartMenu() - Close the start menu if it is open. */
    void CloseStartMenu();

    /** IsStartMenuOpen() - Report whether the start menu is currently open. */
    bool IsStartMenuOpen() const;

    /**
     * StartMenuContains() - Test whether a screen point lies over the menu.
     * @screenX: Screen X coordinate.
     * @screenY: Screen Y coordinate.
     *
     * Return: True if the point is inside the open start menu.
     */
    bool StartMenuContains(int32_t screenX, int32_t screenY) const;

    // Tab management.

    /**
     * AddTab() - Register a taskbar tab for a running window.
     * @pid: Process ID owning the window.
     * @title: Tab label text.
     * @window: Window the tab represents.
     */
    void AddTab(uint32_t pid, const char* title, Widget* window);

    /**
     * RemoveTabByPID() - Drop the tab for a process.
     * @pid: Process ID to match.
     */
    void RemoveTabByPID(uint32_t pid);

    /**
     * RemoveTabByWindow() - Drop the tab for a specific window.
     * @window: Window to match.
     */
    void RemoveTabByWindow(Widget* window);

    /**
     * SetActiveTab() - Mark the tab of @window as the active one.
     * @window: Window to activate.
     */
    void SetActiveTab(Widget* window);

    /**
     * RepositionTabs() - Lay out all tabs side by side in order.
     */
    void RepositionTabs();

    /** IsTaskbar() - Return true for taskbar widget types. */
    bool IsTaskbar() const override {
        return true;
    }

    /** Draw() - Paint the taskbar and raise the open start menu. */
    void Draw(GraphicsDriver* gc) override;

    /** RedrawToCache() - Paint the background, segments, and separators. */
    void RedrawToCache() override;

    void OnMouseDown(int32_t x, int32_t y, uint8_t button) override;
    void OnMouseUp(int32_t x, int32_t y, uint8_t button) override;
    void OnMouseMove(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy) override;

    /** GetStartMenu() - Expose the menu for the desktop to draw. */
    StartMenu* GetStartMenu() {
        return startMenu;
    }
};

#endif  // TASKBAR_H
