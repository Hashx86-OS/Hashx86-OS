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

#define KDBG_COMPONENT "GUI:WIDGET"
#include <gui/guiplat.h>
#include <gui/widget.h>

namespace {

/**
 * HasFocusableDescendant() - Test whether a subtree contains a focus stop.
 * @widget: Root of the subtree to inspect.
 *
 * Only visible widgets count; hidden children are skipped along with their
 * subtrees.
 */
bool HasFocusableDescendant(Widget* widget) {
    bool found = false;
    widget->childrenList.ForEach([&](Widget* child) {
        if (found || !child->isVisible) return;
        if (child->IsFocussable() || HasFocusableDescendant(child)) found = true;
    });
    return found;
}

/**
 * CollectTabStops() - Gather focus stops below @container in tab order.
 * @container: Composite whose descendants should be visited.
 * @stops: Destination array, or NULL to only count the stops.
 * @count: In/out number of stops collected so far.
 *
 * A composite that owns focusable descendants is a container, not a stop, so
 * traversal descends into it. Composites without focusable descendants (and
 * ordinary focusable widgets) are stops themselves.
 */
void CollectTabStops(Widget* container, Widget** stops, int& count) {
    container->childrenList.ForEach([&](Widget* child) {
        if (!child->isVisible) return;

        if (!child->IsFocussable()) {
            if (child->IsComposite()) CollectTabStops(child, stops, count);
            return;
        }

        if (child->IsComposite() && HasFocusableDescendant(child)) {
            CollectTabStops(child, stops, count);
            return;
        }

        if (stops) stops[count] = child;
        count++;
    });
}

/**
 * FindDeepestFocused() - Follow the focused chain to its deepest widget.
 * @container: Composite whose focused path should be followed.
 *
 * Return: The deepest focused widget, or NULL when no descendant is focused.
 */
Widget* FindDeepestFocused(Widget* container) {
    Widget* focused = nullptr;
    container->childrenList.ForEach([&](Widget* child) {
        if (!focused && child->IsFocused()) focused = child;
    });
    if (!focused) return nullptr;

    Widget* deeper = FindDeepestFocused(focused);
    return deeper ? deeper : focused;
}

}  // namespace

// Widget base class.

void (*Widget::destructorNotify)(Widget*) = nullptr;

Widget::Widget(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h) {
    this->parent = parent;
    this->x = x;
    this->y = y;
    this->w = w;
    this->h = h;

    // Allocate and zero-init the cache buffer.
    if (w > 0 && h > 0) {
        size_t count = (size_t)w * (size_t)h;
        if (count / (size_t)w != (size_t)h || count > (0xFFFFFFFFu / sizeof(uint32_t))) {
            guiplat::Abort("CRITICAL: Widget dimensions overflow!\n");
        }
        cache = new uint32_t[count]();
        if (!cache) {
            guiplat::Abort("CRITICAL: Failed to allocate widget cache!\n");
        }
    }
}

Widget::~Widget() {
    // Defensive invariant: a widget being freed must never remain linked in a
    // living parent's childrenList, otherwise a later CompositeWidget redraw
    // dereferences freed memory.  Detach from the parent before freeing.
    if (parent) parent->RemoveChild(this);
    if (destructorNotify) destructorNotify(this);
    if (cache) delete[] cache;
}

bool Widget::Resize(int32_t newW, int32_t newH) {
    if (newW < 0 || newH < 0) return false;
    if (newW == this->w && newH == this->h) return true;

    uint32_t* newCache = nullptr;
    if (newW > 0 && newH > 0) {
        size_t count = (size_t)newW * (size_t)newH;
        if (count / (size_t)newW != (size_t)newH || count > (0xFFFFFFFFu / sizeof(uint32_t))) {
            return false;
        }
        newCache = new uint32_t[count]();
        if (!newCache) return false;
    }

    if (cache) delete[] cache;
    cache = newCache;
    this->w = newW;
    this->h = newH;
    this->MarkDirty();
    return true;
}

void Widget::MarkDirty() {
    this->isDirty = true;

    // Propagate the dirty flag upward.
    if (this->parent != nullptr) {
        this->parent->MarkDirty();
    }
}

void Widget::Recalc() {
    int32_t newW = w, newH = h;
    switch (sizeMode) {
        case CONTENT:
            // Subclasses with text (e.g. Label) override Recalc for text measurement.
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
        // Resize() performs the overflow check and only swaps the buffer once
        // the replacement exists, so a failed recalc leaves the widget intact.
        Resize(newW, newH);
    }
}

void Widget::RedrawToCache() {
    // Clear the cache (overridden in child classes).
    if (cache) memset(cache, 0, sizeof(uint32_t) * w * h);
}

void Widget::Draw(RasterSurface* gc) {
    // Refresh the cache if dirty.
    if (isDirty) {
        if (isVisible) {
            RedrawToCache();
        } else if (cache && w > 0 && h > 0) {
            // Clear the cache if hidden. Guard the dimensions: a widget that
            // was resized while hidden must not memset a huge unsigned size.
            memset(cache, 0, sizeof(uint32_t) * (size_t)w * (size_t)h);
        }
        isDirty = false;
    }
}

// Coordinates.

void Widget::ModelToScreen(int32_t& x_out, int32_t& y_out) {
    // Calculate the absolute screen position.
    if (parent) parent->ModelToScreen(x_out, y_out);
    x_out += this->x;
    y_out += this->y;
}

bool Widget::ContainsCoordinate(int32_t targetX, int32_t targetY) {
    // Check whether the local coordinate lies inside the widget.
    return (targetX >= this->x) && (targetX < this->x + this->w) && (targetY >= this->y) &&
           (targetY < this->y + this->h);
}

Widget* Widget::HitTest(int32_t parentX, int32_t parentY) {
    return (isVisible && ContainsCoordinate(parentX, parentY)) ? this : nullptr;
}

Widget* CompositeWidget::HitTest(int32_t parentX, int32_t parentY) {
    if (!isVisible || !ContainsCoordinate(parentX, parentY)) return nullptr;

    int32_t localX = parentX - this->x;
    int32_t localY = parentY - this->y;

    Widget* found = nullptr;
    childrenList.ReverseForEach([&](Widget* child) {
        if (!found) found = child->HitTest(localX, localY);
    });
    return found ? found : this;
}

// Focus.

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

void Widget::SetEnabled(bool value) {
    if (enabled == value) return;
    enabled = value;

    if (!enabled) {
        // A drag or key focus must not survive the widget becoming disabled.
        mouseCaptured = false;
        if (isFocused) {
            SetFocus(false);
            if (parent) parent->GetFocus(nullptr);
        }
    }
    MarkDirty();
}

void Widget::RequestTopmostFocus() {
    // Raise this widget to the top of its parent's Z-order. Mirrors what
    // Desktop::Focus() does for top-level windows, but works for any container.
    if (!parent) return;
    parent->childrenList.Remove([&](Widget* c) { return c == this; });
    parent->childrenList.PushBack(this);
    parent->MarkDirty();
}

// Child management.

bool Widget::AddChild(Widget* child) {
    if (!child) return false;
    if (child->parent != nullptr && child->parent != this)
        return false;  // Different parent; detach first.
    if (childrenList.Find([&](Widget* c) { return c == child; }) != nullptr)
        return false;  // Already in the list.
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

// Identification.

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

// Default input handlers.

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

void Widget::OnMouseWheel(int32_t) {}
void Widget::OnDoubleClick(int32_t, int32_t, uint8_t) {}
void Widget::OnDragStart(int32_t, int32_t, uint8_t) {}
void Widget::OnDragMove(int32_t, int32_t) {}
void Widget::OnDragEnd(int32_t, int32_t, uint8_t) {}
void Widget::OnKeyRepeat(const char*, uint32_t) {}
void Widget::OnKeyDownMod(const char* key, uint8_t) {
    OnKeyDown(key);
}
void Widget::OnTabFocus(bool) {}

bool Widget::IsComposite() const {
    return false;
}

bool Widget::IsMouseCaptured() const {
    return mouseCaptured;
}

bool Widget::IsPressed() const {
    return false;
}

// Composite widget.

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

    // Deselect the previously focused child.
    if (focusedChild) {
        focusedChild->SetFocus(false);
    }

    // Select the new focused child.
    focusedChild = widget;
    if (widget) {
        widget->SetFocus(true);
        // Move to the front for Z-order. Only reorder an actual child: pushing
        // an arbitrary widget would link it into this list without adopting it
        // (leaving its real parent dangling), which corrupts both trees.
        if (childrenList.Remove([&](Widget* c) { return c == widget; })) {
            childrenList.PushBack(widget);
        }
    }

    if (parent) parent->GetFocus(this);
}

void CompositeWidget::SetFocus(bool result) {
    if (result == IsFocused()) return;
    Widget::SetFocus(result);
    // The focused child follows the container so a nested chain never keeps a
    // stale focused widget after its ancestor loses focus.
    if (focusedChild) focusedChild->SetFocus(result);
}

bool CompositeWidget::RemoveChild(Widget* child) {
    if (!child) return false;

    if (focusedChild && focusedChild == child) {
        focusedChild->SetFocus(false);
        focusedChild = nullptr;
    }

    return Widget::RemoveChild(child);
}

void CompositeWidget::Draw(RasterSurface* gc) {
    // Serialize against concurrent tree mutation (process-exit teardown and
    // widget syscalls run on other threads and can free children mid-iteration).
    GuiCriticalGuard guard;

    // Draw the widget itself.
    Widget::Draw(gc);

    if (!isVisible) return;

    // Draw children back-to-front.
    childrenList.ForEach([&](Widget* child) { child->Draw(gc); });
}

void CompositeWidget::RedrawToCache() {
    if (!cache) {
        isDirty = false;
        return;
    }

    // Start from a transparent surface, then composite the subtree into it.
    // Each child's cache already contains its own subtree because composites
    // recurse here; a leaf cache is drawn by the leaf's RedrawToCache().
    Widget::RedrawToCache();

    if (!NINA::activeInstance) {
        isDirty = false;
        return;
    }

    childrenList.ForEach([&](Widget* child) {
        if (!child || !child->isVisible) return;
        if (child->w <= 0 || child->h <= 0 || !child->cache) return;
        if (child->isDirty) child->RedrawToCache();
        NINA::activeInstance->DrawBitmapToBuffer(cache, w, h, child->x, child->y, child->cache,
                                                 child->w, child->h);
    });
    isDirty = false;
}

bool CompositeWidget::IsMouseCaptured() const {
    if (mouseCaptured) return true;
    bool captured = false;
    childrenList.ForEach([&](Widget* child) {
        if (!captured && child->isVisible && (child->IsMouseCaptured() || child->IsPressed())) {
            captured = true;
        }
    });
    return captured;
}

bool CompositeWidget::IsPressed() const {
    bool pressed = false;
    childrenList.ForEach([&](Widget* child) {
        if (!pressed && child->isVisible && child->IsPressed()) pressed = true;
    });
    return pressed;
}

void CompositeWidget::OnMouseDown(int32_t x, int32_t y, uint8_t button) {
    if (!enabled || !isVisible) return;

    // Transform to local coordinates.
    int32_t localX = x - this->x;
    int32_t localY = y - this->y;

    // A press also establishes the pointer position. Wheel events carry no
    // position of their own, so without this a click followed by a scroll
    // (with no intervening move) would use a stale target.
    this->lastMouseX = localX;
    this->lastMouseY = localY;

    Widget* clicked = nullptr;

    // Hit-test front to back. Hidden children must not intercept clicks.
    childrenList.ReverseForEach([&](Widget* child) {
        if (!clicked && child->isVisible && child->ContainsCoordinate(localX, localY)) {
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
    if (!isVisible) return;

    // Transform to local coordinates.
    int32_t localX = x - this->x;
    int32_t localY = y - this->y;

    Widget* hit = nullptr;
    childrenList.ReverseForEach([&](Widget* child) {
        if (!hit && enabled && child->isVisible && child->ContainsCoordinate(localX, localY)) {
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
    if (!enabled || !isVisible) return;

    int32_t localOldX = oldx - this->x;
    int32_t localOldY = oldy - this->y;
    int32_t localNewX = newx - this->x;
    int32_t localNewY = newy - this->y;

    this->lastMouseX = localNewX;
    this->lastMouseY = localNewY;

    childrenList.ForEach([&](Widget* child) {
        if (!child->isVisible) return;
        // Notify if the mouse is inside, entering/exiting, or the child has capture.
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
    if (!enabled || !isVisible) return;
    if (focusedChild) focusedChild->OnKeyDown(key);
}

void CompositeWidget::OnKeyUp(const char* key) {
    if (!enabled || !isVisible) return;
    if (focusedChild) focusedChild->OnKeyUp(key);
}

void CompositeWidget::OnSpecialKeyDown(uint8_t key) {
    if (!enabled || !isVisible) return;
    if (focusedChild) focusedChild->OnSpecialKeyDown(key);
}

void CompositeWidget::OnSpecialKeyUp(uint8_t key) {
    if (!enabled || !isVisible) return;
    if (focusedChild) focusedChild->OnSpecialKeyUp(key);
}

void CompositeWidget::OnMouseWheel(int32_t delta) {
    if (!enabled || !isVisible) return;
    // Wheel events carry no position; route to the child under the last
    // pointer position, front to back.
    Widget* target = nullptr;
    childrenList.ReverseForEach([&](Widget* child) {
        if (!target && child->isVisible && child->ContainsCoordinate(lastMouseX, lastMouseY)) {
            target = child;
        }
    });
    if (target) target->OnMouseWheel(delta);
}

void CompositeWidget::OnDoubleClick(int32_t x, int32_t y, uint8_t button) {
    if (!enabled || !isVisible) return;

    int32_t localX = x - this->x;
    int32_t localY = y - this->y;

    Widget* target = nullptr;
    childrenList.ReverseForEach([&](Widget* child) {
        if (!target && child->isVisible && child->ContainsCoordinate(localX, localY)) {
            child->OnDoubleClick(localX, localY, button);
            target = child;
        }
    });
}

void CompositeWidget::OnKeyRepeat(const char* key, uint32_t count) {
    if (!enabled || !isVisible) return;
    if (focusedChild) focusedChild->OnKeyRepeat(key, count);
}

void CompositeWidget::OnKeyDownMod(const char* key, uint8_t modifiers) {
    if (!enabled || !isVisible) return;
    if (focusedChild) focusedChild->OnKeyDownMod(key, modifiers);
}

void CompositeWidget::OnTabFocus(bool backwards) {
    if (!enabled || !isVisible) return;

    // Count the focus stops first so the traversal has no fixed-size limit.
    int count = 0;
    CollectTabStops(this, nullptr, count);
    if (count == 0) return;

    Widget** stops = new Widget*[count];
    if (!stops) return;
    int collected = 0;
    CollectTabStops(this, stops, collected);

    Widget* currentStop = FindDeepestFocused(this);
    int current = -1;
    for (int i = 0; i < collected; i++) {
        if (stops[i] == currentStop) current = i;
    }

    int next;
    if (backwards) {
        next = (current <= 0) ? collected - 1 : current - 1;
    } else {
        next = (current + 1) % collected;
    }

    // A composite stop is not its own child, so enter through the base
    // implementation, which forwards the request to its parent.
    Widget* target = stops[next];
    target->Widget::GetFocus(target);

    delete[] stops;
}
