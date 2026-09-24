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

#ifndef WIDGET_H
#define WIDGET_H

#include <core/drivers/keyboard.h>
#include <core/memory.h>
#include <debug.h>
#include <gui/config/config.h>
#include <gui/eventHandler.h>
#include <gui/fonts/font.h>
#include <gui/icons.h>
#include <gui/renderer/nina.h>
#include <string.h>
#include <types.h>
#include <utils/linkedList.h>

struct Padding {
    uint8_t t, r, b, l;
};

enum SizeMode {
    FIXED = 0,
    CONTENT = 1,
    FILL = 2,
};

/**
 * class Widget - Base class for all GUI elements.
 *
 * Implements the shared coordinate system, focus handling, and per-widget
 * draw cache used by every concrete widget. Widgets form a tree; a
 * CompositeWidget owns its children and redraws them back-to-front.
 */
class Widget {
    friend class CompositeWidget;

protected:
    Font* font = nullptr;
    uint32_t argbColor = 0;
    bool isFocussable = true;
    bool isFocused = false;
    bool enabled = true;

public:
    // Hierarchy.
    Widget* parent = nullptr;
    LinkedList<Widget*> childrenList;

    // Dimensions & state.
    int32_t x, y, w, h;
    uint32_t* cache = nullptr;
    bool isDirty = true;
    bool isVisible = true;
    uint32_t minWidth = 0, minHeight = 0;
    Padding padding = {0, 0, 0, 0};
    SizeMode sizeMode = FIXED;
    int32_t fontSize = 14;

    // Process & identification.
    uint32_t PID = 0;
    uint32_t ID = 0;

    /**
     * Widget() - Construct a widget with a parent, position, and size.
     * @parent: Parent widget, or NULL for a root widget.
     * @x: X position relative to @parent.
     * @y: Y position relative to @parent.
     * @w: Widget width in pixels.
     * @h: Widget height in pixels.
     *
     * Allocates and zero-initializes the backing cache buffer for visible
     * dimensions.
     */
    Widget(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h);

    /**
     * ~Widget() - Free the cache buffer and detach this widget from its parent.
     */
    virtual ~Widget();

    // -- Drawing & Updates --

    /**
     * MarkDirty() - Mark the widget as needing a redraw.
     *
     * Propagates the dirty flag upward so that every ancestor composite is
     * also redrawn.
     */
    virtual void MarkDirty();

    /**
     * RedrawToCache() - Repaint the widget's cache buffer.
     *
     * Default implementation clears the cache; subclasses override with their
     * own drawing.
     */
    virtual void RedrawToCache();

    /**
     * Draw() - Blit the cache to the screen if the widget is dirty.
     * @gc: Target graphics driver.
     */
    virtual void Draw(GraphicsDriver* gc);

    /**
     * Recalc() - Recompute dimensions from the size mode.
     *
     * CONTENT sizes to the intrinsic content; FILL stretches to the parent;
     * FIXED leaves the dimensions untouched.
     */
    virtual void Recalc();

    // -- Focus & Interaction --

    /**
     * GetFocus() - Request focus, forwarding up the tree to the widget root.
     * @widget: Widget to focus.
     */
    virtual void GetFocus(Widget* widget);

    /**
     * SetFocus() - Set or clear the focused state.
     * @result: True to gain focus, false to lose it.
     *
     * Fires OnFocusGained()/OnFocusLost() on state change and marks the widget
     * dirty.
     */
    virtual void SetFocus(bool result);

    /**
     * SetFocussable() - Toggle whether the widget can take focus.
     * @focussable: True to allow focus.
     */
    virtual void SetFocussable(bool focussable);

    // -- Hierarchy Management --

    /**
     * AddChild() - Append a child and reparent it to this widget.
     * @child: Child widget to add.
     *
     * Return: True on success; false if the child is already owned elsewhere
     *         or already present in the list.
     */
    virtual bool AddChild(Widget* child);

    /**
     * RemoveChild() - Detach a child without destroying it.
     * @child: Child widget to remove.
     *
     * Return: True if the child was found and removed.
     */
    virtual bool RemoveChild(Widget* child);

    // -- Coordinate Helpers --

    /**
     * ModelToScreen() - Convert local coordinates to absolute screen position.
     * @x: In/out X coordinate.
     * @y: In/out Y coordinate.
     */
    virtual void ModelToScreen(int32_t& x, int32_t& y);

    /**
     * ContainsCoordinate() - Test whether a local point falls inside the widget.
     * @x: Local X coordinate.
     * @y: Local Y coordinate.
     *
     * Return: True if the point lies within the widget's bounds.
     */
    virtual bool ContainsCoordinate(int32_t x, int32_t y);

    // -- Identification --

    /**
     * SetPID() - Set the owning process ID, recursively for all children.
     * @PID: Process ID to assign.
     */
    virtual void SetPID(uint32_t PID);

    /**
     * SetID() - Set the widget's instance ID.
     * @ID: Instance ID to assign.
     */
    virtual void SetID(uint32_t ID);

    /**
     * FindWidgetByID() - Find a widget by its instance ID in this subtree.
     * @searchID: Instance ID to match.
     *
     * Return: The matching widget, or NULL if not found.
     */
    virtual Widget* FindWidgetByID(uint32_t searchID);

    /**
     * FindWidgetByPID() - Find a widget by its owning process ID in this subtree.
     * @pid: Process ID to match.
     *
     * Return: The matching widget, or NULL if not found.
     */
    virtual Widget* FindWidgetByPID(uint32_t PID);

    // -- Event Handlers --
    virtual void OnMouseDown(int32_t x, int32_t y, uint8_t button);
    virtual void OnMouseUp(int32_t x, int32_t y, uint8_t button);
    virtual void OnMouseMove(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy);
    virtual void OnMouseEnter();
    virtual void OnMouseLeave();
    virtual void OnFocusGained();
    virtual void OnFocusLost();

    virtual void OnKeyDown(const char* key);
    virtual void OnSpecialKeyDown(uint8_t key);
    virtual void OnKeyUp(const char* key);
    virtual void OnSpecialKeyUp(uint8_t key);

    virtual bool IsComposite() const;
    virtual bool IsMouseCaptured() const;
    virtual bool IsPressed() const;
    virtual bool IsWindow() const {
        return false;
    }
    virtual bool IsButton() const {
        return false;
    }
    virtual bool IsLabel() const {
        return false;
    }
    virtual bool IsListView() const {
        return false;
    }
    virtual bool IsTerminalView() const {
        return false;
    }
    virtual bool IsTaskbar() const {
        return false;
    }
    virtual bool IsIconButton() const {
        return false;
    }

    uint32_t GetColor() const {
        return argbColor;
    }
    void SetColor(uint32_t c) {
        argbColor = c;
        MarkDirty();
    }
    void SetAlpha(uint8_t alpha) {
        argbColor = (argbColor & 0x00FFFFFF) | ((uint32_t)alpha << 24);
        MarkDirty();
    }
    uint8_t GetAlpha() const {
        return (uint8_t)(argbColor >> 24);
    }
};

/**
 * class CompositeWidget - A widget that can contain and route events to children.
 *
 * Owns its child widgets, forwards input events to the child under the cursor,
 * and moves the focused child to the top of the Z-order before drawing. The
 * root windowing elements (Window, Desktop, Taskbar) derive from this class.
 */
class CompositeWidget : public Widget {
private:
    Widget* focusedChild = nullptr;

public:
    /**
     * CompositeWidget() - Construct a composite widget.
     * @parent: Parent widget, or NULL for a root widget.
     * @x: X position relative to @parent.
     * @y: Y position relative to @parent.
     * @w: Widget width in pixels.
     * @h: Widget height in pixels.
     */
    CompositeWidget(CompositeWidget* parent, int32_t x, int32_t y, int32_t w, int32_t h);

    /**
     * ~CompositeWidget() - Destroy all owned child widgets.
     */
    virtual ~CompositeWidget();

    /** IsComposite() - Return true for composite widget types. */
    bool IsComposite() const override;

    /**
     * GetFocus() - Track @widget as the focused child.
     * @widget: Child widget receiving focus.
     */
    virtual void GetFocus(Widget* widget) override;

    /**
     * RemoveChild() - Detach and destroy @child, clearing focus if needed.
     * @child: Child widget to remove.
     *
     * Return: True if the child was found and removed.
     */
    virtual bool RemoveChild(Widget* child) override;

    /**
     * Draw() - Raise the focused child and redraw all children back-to-front.
     * @gc: Target graphics driver.
     */
    virtual void Draw(GraphicsDriver* gc) override;

    // Event routing.
    virtual void OnMouseDown(int32_t x, int32_t y, uint8_t button) override;
    virtual void OnMouseUp(int32_t x, int32_t y, uint8_t button) override;
    virtual void OnMouseMove(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy) override;

    virtual void OnKeyDown(const char* key) override;
    virtual void OnSpecialKeyDown(uint8_t key) override;
    virtual void OnKeyUp(const char* key) override;
    virtual void OnSpecialKeyUp(uint8_t key) override;
};

#endif  // WIDGET_H
