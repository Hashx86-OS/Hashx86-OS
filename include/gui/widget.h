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
#include <types.h>
#include <utils/linkedList.h>
#include <string.h>

struct Padding {
    uint8_t t, r, b, l;
};

enum SizeMode {
    FIXED = 0,
    CONTENT = 1,
    FILL = 2,
};

/**
 * @class       Widget
 * @brief       Base class for all GUI elements.
 * Handles coordinate systems, focus, and the draw cache.
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
    // Hierarchy
    Widget* parent = nullptr;
    LinkedList<Widget*> childrenList;

    // Dimensions & State
    int32_t x, y, w, h;
    uint32_t* cache = nullptr;
    bool isDirty = true;
    bool isVisible = true;
    uint32_t minWidth = 0, minHeight = 0;
    Padding padding = {0, 0, 0, 0};
    SizeMode sizeMode = FIXED;
    int32_t fontSize = 14;

    // Process & Identification
    uint32_t PID = 0;
    uint32_t ID = 0;

    Widget(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h);
    virtual ~Widget();

    // -- Drawing & Updates --
    virtual void MarkDirty();
    virtual void RedrawToCache();
    virtual void Draw(GraphicsDriver* gc);
    virtual void Recalc();

    // -- Focus & Interaction --
    virtual void GetFocus(Widget* widget);
    virtual void SetFocus(bool result);
    virtual void SetFocussable(bool focussable);

    // -- Hierarchy Management --
    virtual bool AddChild(Widget* child);
    virtual bool RemoveChild(Widget* child);

    // -- Coordinate Helpers --
    virtual void ModelToScreen(int32_t& x, int32_t& y);
    virtual bool ContainsCoordinate(int32_t x, int32_t y);

    // -- Identification --
    virtual void SetPID(uint32_t PID);
    virtual void SetID(uint32_t ID);
    virtual Widget* FindWidgetByID(uint32_t searchID);
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

    uint32_t GetColor() const { return argbColor; }
    void SetColor(uint32_t c) { argbColor = c; MarkDirty(); }
    void SetAlpha(uint8_t alpha) { argbColor = (argbColor & 0x00FFFFFF) | ((uint32_t)alpha << 24); MarkDirty(); }
    uint8_t GetAlpha() const { return (uint8_t)(argbColor >> 24); }
};

/**
 * @class       CompositeWidget
 * @brief       A widget that can contain other widgets (e.g., Windows, Desktop).
 * Handles routing events to children.
 */
class CompositeWidget : public Widget {
private:
    Widget* focusedChild = nullptr;

public:
    CompositeWidget(CompositeWidget* parent, int32_t x, int32_t y, int32_t w, int32_t h);
    virtual ~CompositeWidget();

    bool IsComposite() const override;

    virtual void GetFocus(Widget* widget) override;
    virtual bool RemoveChild(Widget* child) override;
    virtual void Draw(GraphicsDriver* gc) override;

    // Event routing
    virtual void OnMouseDown(int32_t x, int32_t y, uint8_t button) override;
    virtual void OnMouseUp(int32_t x, int32_t y, uint8_t button) override;
    virtual void OnMouseMove(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy) override;

    virtual void OnKeyDown(const char* key) override;
    virtual void OnSpecialKeyDown(uint8_t key) override;
    virtual void OnKeyUp(const char* key) override;
    virtual void OnSpecialKeyUp(uint8_t key) override;
};

#endif  // WIDGET_H
