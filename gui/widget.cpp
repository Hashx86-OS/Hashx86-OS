/**
 * @file        widget.cpp
 * @brief       Base Widget System (part of #x86 GUI Framework)
 *
 * @date        01/02/2026
 * @version     1.0.0
 */

#define KDBG_COMPONENT "GUI:WIDGET"
#include <gui/Hgui.h>
#include <gui/widget.h>
#include <core/Iguard.h>

// Widget Base Class

Widget::Widget(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h) {
    this->parent = parent;
    this->x = x;
    this->y = y;
    this->w = w;
    this->h = h;

    // Allocate and zero-init cache buffer
    if (w > 0 && h > 0) {
        size_t count = (size_t)w * (size_t)h;
        if (count / (size_t)w != (size_t)h || count > (0xFFFFFFFFu / sizeof(uint32_t))) {
            HALT("CRITICAL: Widget dimensions overflow!\n");
        }
        cache = new uint32_t[count]();
        if (!cache) {
            HALT("CRITICAL: Failed to allocate widget cache!\n");
        }
    }
}

Widget::~Widget() {
    // Defensive invariant: a widget being freed must never remain linked in a
    // living parent's childrenList, otherwise a later CompositeWidget redraw
    // dereferences freed memory.  Detach from the parent before freeing.
    if (parent) parent->RemoveChild(this);
    HguiHandler::RemoveWidget(this);
    if (cache) delete[] cache;
}

void Widget::MarkDirty() {
    this->isDirty = true;

    // Propagate dirty flag upward
    if (this->parent != nullptr) {
        this->parent->MarkDirty();
    }
}

void Widget::Recalc() {
    int32_t newW = w, newH = h;
    switch (sizeMode) {
        case CONTENT:
            // Subclasses with text (e.g. Label) override Recalc for text measurement
            break;
        case FILL:
            if (parent) {
                newW = parent->w;
                newH = parent->h;
            }
            break;
        case FIXED:
            return;
    }
    if (newW < (int32_t)minWidth) newW = (int32_t)minWidth;
    if (newH < (int32_t)minHeight) newH = (int32_t)minHeight;
    if (newW != w || newH != h) {
        w = newW; h = newH;
        if (cache) delete[] cache;
        cache = (w > 0 && h > 0) ? new uint32_t[w * h]() : nullptr;
        MarkDirty();
    }
}

void Widget::RedrawToCache() {
    // Clear cache (override in child classes)
    if (cache) memset(cache, 0, sizeof(uint32_t) * w * h);
}

void Widget::Draw(GraphicsDriver* gc) {
    // Update cache if dirty
    if (isDirty) {
        if (isVisible) {
            RedrawToCache();
        } else {
            // Clear cache if hidden
            if (cache) memset(cache, 0, sizeof(uint32_t) * w * h);
        }
        isDirty = false;
    }
}

// Coordinates

void Widget::ModelToScreen(int32_t& x_out, int32_t& y_out) {
    // Calculate absolute screen position
    if (parent) parent->ModelToScreen(x_out, y_out);
    x_out += this->x;
    y_out += this->y;
}

bool Widget::ContainsCoordinate(int32_t targetX, int32_t targetY) {
    // Check if local coordinate is inside widget
    return (targetX >= this->x) && (targetX < this->x + this->w) && (targetY >= this->y) &&
           (targetY < this->y + this->h);
}

// Focus

void Widget::GetFocus(Widget* widget) {
    if (parent) parent->GetFocus(widget);
}

void Widget::SetFocus(bool result) {
    if (result == this->isFocused) return;
    this->isFocused = result;
    if (result) {
        OnFocusGained();
    } else {
        OnFocusLost();
    }
    this->MarkDirty();
}

void Widget::SetFocussable(bool focussable) {
    this->isFocussable = focussable;

    if (!focussable && isFocused) {
        SetFocus(false);
        if (parent) {
            parent->GetFocus(nullptr);
        }
    }
}

// Child Management

bool Widget::AddChild(Widget* child) {
    if (!child) return false;
    if (child->parent != nullptr && child->parent != this)
        return false;  // different parent, detach first
    if (childrenList.Find([&](Widget* c) { return c == child; }) != nullptr)
        return false;  // already in list
    childrenList.PushBack(child);
    child->parent = this;
    this->MarkDirty();
    return true;
}

bool Widget::RemoveChild(Widget* child) {
    bool result = childrenList.Remove([&](Widget* c) { return c == child; });
    if (result) {
        if (child && child->parent == this) {
            child->parent = nullptr;
        }
        this->MarkDirty();
    }
    return result;
}

// Identification

void Widget::SetPID(uint32_t pid) {
    this->PID = pid;
    childrenList.ForEach([&](Widget* c) { c->SetPID(pid); });
}

void Widget::SetID(uint32_t id) {
    this->ID = id;
}

Widget* Widget::FindWidgetByID(uint32_t searchID) {
    if (this->ID == searchID) return this;

    Widget* result = nullptr;
    childrenList.ForEach([&](Widget* c) {
        if (!result) result = c->FindWidgetByID(searchID);
    });
    return result;
}

Widget* Widget::FindWidgetByPID(uint32_t pid) {
    if (this->PID == pid) return this;

    Widget* result = nullptr;
    childrenList.ForEach([&](Widget* c) {
        if (!result) result = c->FindWidgetByPID(pid);
    });
    return result;
}

// Default Input Handlers

void Widget::OnMouseDown(int32_t, int32_t, uint8_t) {
    if (isFocussable) GetFocus(this);
}
void Widget::OnMouseUp(int32_t, int32_t, uint8_t) {}
void Widget::OnMouseMove(int32_t, int32_t, int32_t, int32_t) {}
void Widget::OnMouseEnter() {}
void Widget::OnMouseLeave() {}
void Widget::OnFocusGained() {}
void Widget::OnFocusLost() {}
void Widget::OnKeyDown(const char*) {}
void Widget::OnSpecialKeyDown(uint8_t) {}
void Widget::OnKeyUp(const char*) {}
void Widget::OnSpecialKeyUp(uint8_t) {}

bool Widget::IsComposite() const {
    return false;
}

bool Widget::IsMouseCaptured() const {
    return false;
}

bool Widget::IsPressed() const {
    return false;
}

// Composite Widget

CompositeWidget::CompositeWidget(CompositeWidget* parent, int32_t x, int32_t y, int32_t w,
                                 int32_t h)
    : Widget(parent, x, y, w, h), focusedChild(nullptr) {}

CompositeWidget::~CompositeWidget() {
    // Delete all children — they are owned by this CompositeWidget via
    // AddChild() which pushes them into childrenList.  Pop each child off the
    // list first and null its parent link so no child is ever freed while it
    // is still linked in a parent's childrenList, and so ~Widget's defensive
    // detach is a no-op instead of touching this (already torn down) list.
    while (childrenList.GetSize() > 0) {
        Widget* child = childrenList.PopFront();
        child->parent = nullptr;
        delete child;
    }
}

bool CompositeWidget::IsComposite() const {
    return true;
}

void CompositeWidget::GetFocus(Widget* widget) {
    if (widget && !widget->isFocussable) {
        widget = nullptr;
    }

    if (focusedChild == widget) {
        if (parent) parent->GetFocus(this);
        return;
    }

    // Deselect previous child
    if (focusedChild) {
        focusedChild->SetFocus(false);
    }

    // Select new child
    focusedChild = widget;
    if (widget) {
        widget->SetFocus(true);
        // Move to front for Z-order
        childrenList.Remove([&](Widget* c) { return c == widget; });
        childrenList.PushBack(widget);
    }

    if (parent) parent->GetFocus(this);
}

bool CompositeWidget::RemoveChild(Widget* child) {
    if (!child) return false;

    if (focusedChild && focusedChild == child) {
        focusedChild->SetFocus(false);
        focusedChild = nullptr;
    }

    return Widget::RemoveChild(child);
}

void CompositeWidget::Draw(GraphicsDriver* gc) {
    // Serialize against concurrent tree mutation (process-exit teardown and
    // widget syscalls run on other threads and can free children mid-iteration).
    InterruptGuard guard;

    // Draw self
    Widget::Draw(gc);

    // Draw children back-to-front
    childrenList.ForEach([&](Widget* child) { child->Draw(gc); });
}

void CompositeWidget::OnMouseDown(int32_t x, int32_t y, uint8_t button) {
    // Transform to local coordinates
    int32_t localX = x - this->x;
    int32_t localY = y - this->y;

    Widget* clicked = nullptr;

    // Hit test front-to-back
    childrenList.ReverseForEach([&](Widget* child) {
        if (!clicked && child->ContainsCoordinate(localX, localY)) {
            child->OnMouseDown(localX, localY, button);
            clicked = child;
        }
    });

    if (clicked) {
        GetFocus(clicked);
    } else {
        // Background click clears current focus in this container.
        GetFocus(nullptr);
    }
}

void CompositeWidget::OnMouseUp(int32_t x, int32_t y, uint8_t button) {
    // Transform to local
    int32_t localX = x - this->x;
    int32_t localY = y - this->y;

    Widget* hit = nullptr;
    childrenList.ReverseForEach([&](Widget* child) {
        if (!hit && child->ContainsCoordinate(localX, localY)) {
            child->OnMouseUp(localX, localY, button);
            hit = child;
        }
    });

    childrenList.ForEach([&](Widget* child) {
        if (child == hit) return;
        if (child->IsMouseCaptured() || child->IsPressed()) {
            child->OnMouseUp(localX, localY, button);
        }
    });
}

void CompositeWidget::OnMouseMove(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy) {
    int32_t localOldX = oldx - this->x;
    int32_t localOldY = oldy - this->y;
    int32_t localNewX = newx - this->x;
    int32_t localNewY = newy - this->y;

    childrenList.ForEach([&](Widget* child) {
        // Notify if mouse inside, entering/exiting, or child has captured input
        bool inOld = child->ContainsCoordinate(localOldX, localOldY);
        bool inNew = child->ContainsCoordinate(localNewX, localNewY);

        if (inOld || inNew || child->IsMouseCaptured() || child->IsPressed()) {
            child->OnMouseMove(localOldX, localOldY, localNewX, localNewY);
        }

        if (!inOld && inNew) {
            child->OnMouseEnter();
        } else if (inOld && !inNew) {
            child->OnMouseLeave();
        }
    });
}

void CompositeWidget::OnKeyDown(const char* key) {
    if (focusedChild) focusedChild->OnKeyDown(key);
}

void CompositeWidget::OnKeyUp(const char* key) {
    if (focusedChild) focusedChild->OnKeyUp(key);
}

void CompositeWidget::OnSpecialKeyDown(uint8_t key) {
    if (focusedChild) focusedChild->OnSpecialKeyDown(key);
}

void CompositeWidget::OnSpecialKeyUp(uint8_t key) {
    if (focusedChild) focusedChild->OnSpecialKeyUp(key);
}
