/**
 * @file        terminalview.cpp
 * @brief       TerminalView Component (part of #x86 GUI Framework)
 *
 * @date        06/03/2026
 * @version     1.0.0
 */

#define KDBG_COMPONENT "GUI:TERMINALVIEW"
#include <gui/desktop.h>
#include <gui/terminalview.h>
#include <gui/fonts/font.h>
#include <gui/renderer/nina.h>
#include <utils/linkedList.h>

TerminalView::TerminalView(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h,
                           const char* text)
    : Widget(parent, x, y, w, h),
      text(nullptr),
      fontSize(SMALL),
      scrollTotal(0),
      scrollVisible(0),
      scrollOffset(0),
      pendingScrollAction(0),
      isDraggingThumb(false),
      dragStartY(0),
      dragStartOffset(0) {
    if (FontManager::activeInstance) {
        font = FontManager::activeInstance->getFontByIndex(1, SMALL, REGULAR);
        if (!font) font = FontManager::activeInstance->getNewFont(SMALL, REGULAR);
    }
    setText(text ? text : "");
}

TerminalView::~TerminalView() {
    if (text) delete[] text;
    if (font) delete font;
}

void TerminalView::PutPixel(int32_t px, int32_t py, uint32_t color) {
    if (px < 0 || py < 0 || px >= w || py >= h) return;
    cache[py * w + px] = color;
}

void TerminalView::DrawCharacter(char c, int32_t x, int32_t y, uint32_t color) {
    if (!font || !NINA::activeInstance) return;
    NINA::activeInstance->DrawCharacter(cache, w, h, x, y, c, font, color);
}

void TerminalView::setText(const char* newText) {
    if (!newText) newText = "";

    if (text && strcmp(text, newText) == 0) return;

    if (text) {
        delete[] text;
        text = nullptr;
    }

    int len = strlen(newText);
    text = new char[len + 1];
    if (!text) {
        HALT("CRITICAL: Failed to allocate TerminalView text!\n");
    }

    strcpy(text, newText);
    MarkDirty();
}

void TerminalView::SetFontSize(int32_t px) {
    if (!FontManager::activeInstance) return;
    FontSize slot = Font::PixelToFontSlot(px);
    FontType type = this->font ? this->font->fontType : REGULAR;
    Font* newFont = FontManager::activeInstance->getNewFont(slot, type);
    if (!newFont) return;
    delete this->font;
    this->font = newFont;
    MarkDirty();
}

void TerminalView::setSize(FontSize size) {
    if (fontSize == size || !font || !FontManager::activeInstance) return;
    FontType type = (FontType)font->fontType;
    Font* newFont = FontManager::activeInstance->getFontByIndex(1, size, type);
    if (!newFont) newFont = FontManager::activeInstance->getNewFont(size, type);
    if (!newFont) return;
    delete font;
    font = newFont;
    fontSize = size;
    MarkDirty();
}

void TerminalView::setScrollMeta(int totalLines, int visibleLines, int offset) {
    scrollTotal = (totalLines < 0) ? 0 : totalLines;
    scrollVisible = (visibleLines < 0) ? 0 : visibleLines;

    int maxOffset = 0;
    if (scrollTotal > scrollVisible) {
        maxOffset = scrollTotal - scrollVisible;
    }

    if (offset < 0) {
        scrollOffset = 0;
    } else if (offset > maxOffset) {
        scrollOffset = maxOffset;
    } else {
        scrollOffset = offset;
    }

    MarkDirty();
}

int TerminalView::consumeScrollAction() {
    int action = pendingScrollAction;
    pendingScrollAction = 0;
    return action;
}

// ─── Scrollbar geometry helpers ───

int TerminalView::ScrollBarW() const {
    return 12;
}
int TerminalView::ScrollBtnH() const {
    return 12;
}

int TerminalView::ScrollBarX() const {
    const int margin = 2;
    return w - ScrollBarW() - margin;
}

int TerminalView::ScrollBarY() const {
    return 2;
}

int TerminalView::ScrollBarH() const {
    const int margin = 2;
    return h - (margin * 2);
}

int TerminalView::MaxOffset() const {
    if (scrollTotal <= scrollVisible) return 0;
    return scrollTotal - scrollVisible;
}

int TerminalView::TrackY() const {
    return ScrollBarY() + ScrollBtnH();
}

int TerminalView::TrackH() const {
    return ScrollBarH() - (ScrollBtnH() * 2);
}

int TerminalView::ThumbH() const {
    int trackH = TrackH();
    if (scrollTotal <= 0 || scrollVisible <= 0 || scrollTotal <= scrollVisible) return trackH;

    int th = (trackH * scrollVisible) / scrollTotal;
    if (th < 10) th = 10;
    if (th > trackH) th = trackH;
    return th;
}

int TerminalView::ThumbY() const {
    int trackY = TrackY();
    int trackH = TrackH();
    int thumbH = ThumbH();
    int maxOff = MaxOffset();

    if (maxOff <= 0) return trackY;

    int travel = trackH - thumbH;
    // Invert: scrollOffset=max → thumb at top, scrollOffset=0 → thumb at bottom
    return trackY + travel - (travel * scrollOffset) / maxOff;
}

void TerminalView::DrawScrollBar() {
    int barX = ScrollBarX();
    int barY = ScrollBarY();
    int barW = ScrollBarW();
    int barH = ScrollBarH();
    int btnH = ScrollBtnH();

    if (barH <= (btnH * 2 + 2)) return;

    uint32_t border = LISTVIEW_BORDER;
    uint32_t bg = LISTVIEW_SCROLLBAR_BG;
    uint32_t thumb = LISTVIEW_SCROLLBAR_THUMB;
    uint32_t arrow = LABEL_TEXT_NORMAL;

    NINA::activeInstance->FillRectangle(cache, w, h, barX, barY, barW, barH, bg);
    NINA::activeInstance->DrawRectangle(cache, w, h, barX, barY, barW, barH, border);

    int upY = barY;
    int downY = barY + barH - btnH;
    NINA::activeInstance->FillRectangle(cache, w, h, barX + 1, upY + 1, barW - 2, btnH - 1, bg);
    NINA::activeInstance->FillRectangle(cache, w, h, barX + 1, downY + 1, barW - 2, btnH - 1, bg);

    // Up arrow (▲)
    int cx = barX + barW / 2;
    int upMid = upY + btnH / 2;
    int dnMid = downY + btnH / 2;
    PutPixel(cx, upMid - 3, arrow);
    PutPixel(cx - 1, upMid - 2, arrow);
    PutPixel(cx + 1, upMid - 2, arrow);
    PutPixel(cx - 2, upMid - 1, arrow);
    PutPixel(cx + 2, upMid - 1, arrow);

    // Down arrow (▼)
    PutPixel(cx, dnMid + 3, arrow);
    PutPixel(cx - 1, dnMid + 2, arrow);
    PutPixel(cx + 1, dnMid + 2, arrow);
    PutPixel(cx - 2, dnMid + 1, arrow);
    PutPixel(cx + 2, dnMid + 1, arrow);

    // Thumb
    int thumbY = ThumbY();
    int thumbH = ThumbH();
    NINA::activeInstance->FillRectangle(cache, w, h, barX + 2, thumbY, barW - 4, thumbH, thumb);
}

void TerminalView::RedrawToCache() {
    if (!cache || w <= 0 || h <= 0) return;
    memset(cache, 0, sizeof(uint32_t) * w * h);

    NINA::activeInstance->FillRectangle(cache, w, h, 0, 0, w, h, LISTVIEW_BG);
    NINA::activeInstance->DrawRectangle(cache, w, h, 0, 0, w, h, LISTVIEW_BORDER);

    DrawScrollBar();

    if (!text || !font || !NINA::activeInstance) {
        isDirty = false;
        return;
    }

    const int padX = 4;
    const int padY = 2;
    const int barReserve = 16;
    int lineHeight = font->getLineHeight();
    int penX = padX;
    int penY = padY;
    int maxX = w - barReserve - padX;
    int maxY = h - padY;

    // Skip lines according to scrollOffset (count newlines)
    int skippedLines = 0;
    int textIdx = 0;
    while (text[textIdx] != '\0' && skippedLines < scrollOffset) {
        if (text[textIdx] == '\n') skippedLines++;
        textIdx++;
    }

    for (int i = textIdx; text[i] != '\0'; i++) {
        char c = text[i];

        if (c == '\n') {
            penX = padX;
            penY += lineHeight;
            if (penY > maxY) break;
            continue;
        }

        // Clamp to supported range
        int idx = (uint8_t)c - 32;
        if (idx < 0 || idx >= font->glyph_count) {
            c = '?';
            idx = (uint8_t)c - 32;
        }

        int xadvance = font->font_glyphs[idx * 8 + 7];

        if (penX > maxX) {
            penX = padX;
            penY += lineHeight;
            if (penY > maxY) break;
        }

        DrawCharacter(c, penX, penY, LABEL_TEXT_NORMAL);
        penX += xadvance;
    }

    isDirty = false;
}

void TerminalView::OnMouseDown(int32_t x, int32_t y, uint8_t button) {
    if (!isVisible) return;

    Widget::OnMouseDown(x, y, button);

    if (button == 1) {
        Event* new_event = new Event{this->ID, ON_CLICK};
        if (!new_event) {
            HALT("CRITICAL: Failed to allocate terminal view click event!\n");
        }

        if (!Desktop::activeInstance) {
            delete new_event;
            return;
        }

        EventHandler* handler = Desktop::activeInstance->getHandler(this->PID);
        if (!handler) {
            delete new_event;
            return;
        }

        handler->eventQueue.Add(new_event);
        if (g_scheduler && handler->thread) {
            g_scheduler->WakeThread(handler->thread);
        }
    }

    if (button != 1) return;

    int localX = x - this->x;
    int localY = y - this->y;

    int barX = ScrollBarX();
    int barY = ScrollBarY();
    int barW = ScrollBarW();
    int barH = ScrollBarH();
    int btnH = ScrollBtnH();

    // Must be inside scrollbar column
    if (localX < barX || localX >= barX + barW || localY < barY || localY >= barY + barH) {
        return;
    }

    int upY = barY;
    int downY = barY + barH - btnH;

    // Up button → scroll up (show older content)
    if (localY >= upY && localY < upY + btnH) {
        pendingScrollAction = 1;
        return;
    }

    // Down button → scroll down (show newer content)
    if (localY >= downY && localY < downY + btnH) {
        pendingScrollAction = -1;
        return;
    }

    // Track area — check if click is on the thumb
    int thumbY = ThumbY();
    int thumbH = ThumbH();

    if (localY >= thumbY && localY < thumbY + thumbH) {
        // Start dragging the thumb
        isDraggingThumb = true;
        dragStartY = localY;
        dragStartOffset = scrollOffset;
        return;
    }

    // Click on track above/below thumb → page scroll
    if (localY < thumbY) {
        // Clicked above thumb → scroll up (increase offset)
        pendingScrollAction = 5;
    } else {
        // Clicked below thumb → scroll down (decrease offset)
        pendingScrollAction = -5;
    }
}

void TerminalView::OnMouseUp(int32_t x, int32_t y, uint8_t button) {
    (void)x;
    (void)y;
    if (button == 1) {
        isDraggingThumb = false;
    }
}

void TerminalView::OnMouseMove(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy) {
    if (!isDraggingThumb) return;

    int localY = newy - this->y;
    int dy = localY - dragStartY;

    int trackH = TrackH();
    int thumbH = ThumbH();
    int travel = trackH - thumbH;
    int maxOff = MaxOffset();

    if (travel <= 0 || maxOff <= 0) return;

    // Thumb is inverted: moving mouse DOWN should DECREASE scrollOffset
    // (thumb at top = max offset, thumb at bottom = 0 offset)
    int deltaOffset = -(dy * maxOff) / travel;
    int newOffset = dragStartOffset + deltaOffset;

    if (newOffset < 0) newOffset = 0;
    if (newOffset > maxOff) newOffset = maxOff;

    if (newOffset != scrollOffset) {
        scrollOffset = newOffset;
        pendingScrollAction = 0;  // clear any pending button action
        // Encode the absolute offset as a special action:
        // We use a large negative sentinel to signal "set absolute offset"
        // pendingScrollAction = -(1000000 + newOffset)
        pendingScrollAction = -(1000000 + newOffset);

        Event* new_event = new Event{this->ID, ON_CLICK};
        if (!new_event) {
            HALT("CRITICAL: Failed to allocate terminal view drag event!\n");
        }

        if (!Desktop::activeInstance) {
            delete new_event;
            return;
        }

        EventHandler* handler = Desktop::activeInstance->getHandler(this->PID);
        if (!handler) {
            delete new_event;
            return;
        }

        handler->eventQueue.Add(new_event);
        if (g_scheduler && handler->thread) {
            g_scheduler->WakeThread(handler->thread);
        }

        MarkDirty();
    }
}

TerminalView::KeyEvent TerminalView::consumeKeyEvent() {
    if (!keyEventQueue.IsEmpty()) {
        return keyEventQueue.PopFront();
    }
    KeyEvent none;
    none.type = KEY_EVENT_NONE;
    none.key = 0;
    none.specialKey = 0;
    return none;
}

static const size_t MAX_KEYEVENT_QUEUE = 256;

void TerminalView::OnKeyDown(const char* key) {
    if (key && key[0] != '\0') {
        if (keyEventQueue.GetSize() >= MAX_KEYEVENT_QUEUE) return;
        KeyEvent ev;
        ev.type = KEY_EVENT_NORMAL;
        ev.key = key[0];
        ev.specialKey = 0;
        keyEventQueue.Add(ev);
    }
}

void TerminalView::OnSpecialKeyDown(uint8_t key) {
    if (keyEventQueue.GetSize() >= MAX_KEYEVENT_QUEUE) return;
    KeyEvent ev;
    ev.type = KEY_EVENT_SPECIAL;
    ev.key = 0;
    ev.specialKey = key;
    keyEventQueue.Add(ev);
}
