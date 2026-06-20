/**
 * @file        listview.cpp
 * @brief       ListView Component (part of #x86 GUI Framework)
 *
 * @date        22/02/2026
 * @version     1.0.0
 */

#define KDBG_COMPONENT "GUI:LISTVIEW"
#include <core/scheduler.h>
#include <gui/desktop.h>
#include <gui/listview.h>

ListView::ListView(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h)
    : Widget(parent, x, y, w, h),
      itemCount(0),
      scrollOffset(0),
      selectedIndex(-1),
      hoveredIndex(-1) {
    this->font = FontManager::activeInstance->getNewFont();
    this->font->setSize(TINY);
    const char* defaultHeader = "Name";
    int i = 0;
    while (defaultHeader[i] && i < 31) {
        headerText[i] = defaultHeader[i];
        i++;
    }
    headerText[i] = 0;

    // cache is allocated by Widget constructor; do not reallocate here
}

ListView::~ListView() {
    if (font) delete font;
}

void ListView::Clear() {
    itemCount = 0;
    scrollOffset = 0;
    selectedIndex = -1;
    hoveredIndex = -1;
    for (int i = 0; i < LISTVIEW_MAX_ITEMS; i++) {
        items[i].valid = false;
    }
    MarkDirty();
}

void ListView::AddItem(const char* name, uint32_t size, uint8_t type) {
    if (itemCount >= LISTVIEW_MAX_ITEMS) return;
    ListViewItem& item = items[itemCount];
    const char* safeName = name ? name : "";
    int i = 0;
    while (safeName[i] && i < 63) {
        item.name[i] = safeName[i];
        i++;
    }
    item.name[i] = 0;
    item.size = size;
    item.type = type;
    item.valid = true;
    itemCount++;
    MarkDirty();
}

void ListView::SetHeader(const char* text) {
    const char* safeText = text ? text : "";
    int i = 0;
    while (safeText[i] && i < 31) {
        headerText[i] = safeText[i];
        i++;
    }
    headerText[i] = 0;
    MarkDirty();
}

const ListViewItem* ListView::GetItem(int index) const {
    if (index < 0 || index >= itemCount) return nullptr;
    return &items[index];
}

void ListView::SetItemHeight(int h) {
    if (h < 10) h = 10;
    itemHeight = h;
    MarkDirty();
}

void ListView::OnKeyDown(const char* key) {
    if (!enabled || !isVisible || !key) return;
    if (key[0] == 0) return;

    if (key[0] == 'H') {
        // Up arrow (scancode 0x48)
        if (selectedIndex > 0) {
            selectedIndex--;
            if (selectedIndex < scrollOffset) {
                scrollOffset = selectedIndex;
            }
            MarkDirty();
        }
    } else if (key[0] == 'P') {
        // Down arrow (scancode 0x50)
        if (selectedIndex < itemCount - 1) {
            selectedIndex++;
            int contentH = h - LISTVIEW_HEADER_HEIGHT - 2;
            int visibleItems = contentH / itemHeight;
            if (selectedIndex >= scrollOffset + visibleItems) {
                scrollOffset = selectedIndex - visibleItems + 1;
            }
            MarkDirty();
        }
    } else if (key[0] == '\r' || key[0] == '\n' || key[0] == ' ') {
        // Enter or Space → fire click event
        if (selectedIndex >= 0) {
            Event* new_event = new Event{this->ID, ON_CLICK};
            if (!new_event) return;
            if (!Desktop::activeInstance) { delete new_event; return; }
            EventHandler* handler = Desktop::activeInstance->getHandler(this->PID);
            if (!handler) { delete new_event; return; }
            handler->eventQueue.Add(new_event);
            if (g_scheduler && handler->thread) {
                g_scheduler->WakeThread(handler->thread);
            }
        }
    }
}

void ListView::update() {
    MarkDirty();
}

void ListView::RedrawToCache() {
    if (!cache) {
        isDirty = false;
        return;
    }
    memset(cache, 0, (size_t)w * (size_t)h * sizeof(uint32_t));

    // Background
    NINA::activeInstance->FillRoundedRectangle(cache, w, h, 0, 0, w, h, 4, LISTVIEW_BG);

    // Border
    NINA::activeInstance->DrawRoundedRectangle(cache, w, h, 0, 0, w, h, 4, LISTVIEW_BORDER);

    // Header bar
    NINA::activeInstance->FillRectangle(cache, w, h, 1, 1, w - 2, LISTVIEW_HEADER_HEIGHT,
                                        LISTVIEW_HEADER_BG);
    NINA::activeInstance->DrawString(cache, w, h, 28, 4, headerText, font, LISTVIEW_HEADER_TEXT);

    // Size column header
    NINA::activeInstance->DrawString(cache, w, h, w - 80, 4, "Size", font, LISTVIEW_HEADER_TEXT);

    // Separator under header
    NINA::activeInstance->DrawHorizontalLine(cache, w, h, 1, LISTVIEW_HEADER_HEIGHT, w - 2,
                                             LISTVIEW_BORDER);

    // Calculate visible range
    int contentH = h - LISTVIEW_HEADER_HEIGHT - 2;
    int visibleItems = contentH / itemHeight;
    int startItem = scrollOffset;
    int endItem = startItem + visibleItems;
    if (endItem > itemCount) endItem = itemCount;

    // Draw items
    for (int i = startItem; i < endItem; i++) {
        if (!items[i].valid) continue;

        int itemY = LISTVIEW_HEADER_HEIGHT + 1 + (i - startItem) * itemHeight;

        // Background
        uint32_t bgColor;
        if (i == selectedIndex) {
            bgColor = LISTVIEW_ITEM_BG_SELECTED;
        } else if (i == hoveredIndex) {
            bgColor = LISTVIEW_ITEM_BG_HOVER;
        } else {
            bgColor = (i % 2 == 0) ? LISTVIEW_ITEM_BG_EVEN : LISTVIEW_ITEM_BG_ODD;
        }
        NINA::activeInstance->FillRectangle(cache, w, h, 1, itemY, w - 2, itemHeight,
                                            bgColor);

        // Icon indicator (small colored circle)
        uint32_t iconColor;
        switch (items[i].type) {
            case 1:
                iconColor = LISTVIEW_ICON_DIR;
                break;
            case 2:
                iconColor = LISTVIEW_ICON_EXE;
                break;
            default:
                iconColor = LISTVIEW_ICON_FILE;
                break;
        }
        NINA::activeInstance->FillCircle(cache, w, h, 12, itemY + itemHeight / 2, 4,
                                         iconColor);

        // Name text
        NINA::activeInstance->DrawString(cache, w, h, 22, itemY + 2, items[i].name, font,
                                         LISTVIEW_ITEM_TEXT);

        // Size text (if not directory)
        if (items[i].type != 1) {
            char sizeStr[16];
            uint32_t sz = items[i].size;
            if (sz >= 1024) {
                int kb = sz / 1024;
                int pos = 0;
                char tmp[16];
                if (kb == 0) {
                    tmp[pos++] = '0';
                } else {
                    while (kb > 0) {
                        tmp[pos++] = '0' + (kb % 10);
                        kb /= 10;
                    }
                }
                for (int j = 0; j < pos; j++) {
                    sizeStr[j] = tmp[pos - 1 - j];
                }
                sizeStr[pos] = ' ';
                sizeStr[pos + 1] = 'K';
                sizeStr[pos + 2] = 'B';
                sizeStr[pos + 3] = 0;
            } else {
                int pos = 0;
                char tmp[16];
                uint32_t v = sz;
                if (v == 0) {
                    tmp[pos++] = '0';
                } else {
                    while (v > 0) {
                        tmp[pos++] = '0' + (v % 10);
                        v /= 10;
                    }
                }
                for (int j = 0; j < pos; j++) {
                    sizeStr[j] = tmp[pos - 1 - j];
                }
                sizeStr[pos] = ' ';
                sizeStr[pos + 1] = 'B';
                sizeStr[pos + 2] = 0;
            }
            NINA::activeInstance->DrawString(cache, w, h, w - 80, itemY + 2, sizeStr, font,
                                             0xFF6C7086);
        } else {
            NINA::activeInstance->DrawString(cache, w, h, w - 80, itemY + 2, "<DIR>", font,
                                             0xFF89B4FA);
        }
    }

    // Scrollbar (if needed)
    if (itemCount > visibleItems && visibleItems > 0) {
        int sbX = w - LISTVIEW_SCROLLBAR_WIDTH - 1;
        int sbY = LISTVIEW_HEADER_HEIGHT + 1;
        int sbH = contentH;
        NINA::activeInstance->FillRectangle(cache, w, h, sbX, sbY, LISTVIEW_SCROLLBAR_WIDTH, sbH,
                                            LISTVIEW_SCROLLBAR_BG);

        // Thumb
        int thumbH = (visibleItems * sbH) / itemCount;
        if (thumbH < 10) thumbH = 10;
        int thumbY = sbY + (scrollOffset * (sbH - thumbH)) / (itemCount - visibleItems);
        NINA::activeInstance->FillRoundedRectangle(cache, w, h, sbX, thumbY,
                                                   LISTVIEW_SCROLLBAR_WIDTH, thumbH, 3,
                                                   LISTVIEW_SCROLLBAR_THUMB);
    }

    // Empty state
    if (itemCount == 0) {
        Font* msgFont = FontManager::activeInstance->getNewFont();
        if (msgFont) {
            msgFont->setSize(SMALL);
            NINA::activeInstance->DrawString(cache, w, h, w / 2 - 40, h / 2 - 8, "No items",
                                             msgFont, 0xFF6C7086);
            delete msgFont;
        }
    }

    isDirty = false;
}

void ListView::OnMouseDown(int32_t mx, int32_t my, uint8_t button) {
    if (!isVisible) return;

    Widget::OnMouseDown(mx, my, button);

    int localY = my - this->y;
    int localX = mx - this->x;

    if (localY > LISTVIEW_HEADER_HEIGHT && localY < h && localX >= 0 &&
        localX < w - LISTVIEW_SCROLLBAR_WIDTH) {
        int clickedItem =
            scrollOffset + (localY - LISTVIEW_HEADER_HEIGHT - 1) / itemHeight;
        if (clickedItem >= 0 && clickedItem < itemCount) {
            selectedIndex = clickedItem;
            MarkDirty();

            Event* new_event = new Event{this->ID, ON_CLICK};
            if (!new_event) {
                HALT("CRITICAL: Failed to allocate ListView click event!\n");
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
    }

    // Scrollbar thumb drag
    if (localX >= w - LISTVIEW_SCROLLBAR_WIDTH - 1 && localX < w &&
        localY > LISTVIEW_HEADER_HEIGHT && localY < h) {
        isDraggingThumb = true;
        dragStartY = my;
        dragStartOffset = scrollOffset;
    }
}

void ListView::OnMouseUp(int32_t, int32_t, uint8_t) {
    isDraggingThumb = false;
}

void ListView::OnMouseMove(int32_t, int32_t oldy, int32_t mx, int32_t my) {
    if (!isFocused) return;

    if (isDraggingThumb) {
        int contentH = h - LISTVIEW_HEADER_HEIGHT - 2;
        int visibleItems = contentH / itemHeight;
        if (visibleItems <= 0 || itemCount <= visibleItems) return;
        int sbH = contentH;
        int thumbH = (visibleItems * sbH) / itemCount;
        if (thumbH < 10) thumbH = 10;
        int travel = sbH - thumbH;
        if (travel <= 0) return;
        int dy = my - dragStartY;
        int deltaOff = (dy * (itemCount - visibleItems)) / travel;
        int newOff = dragStartOffset + deltaOff;
        if (newOff < 0) newOff = 0;
        int maxOff = itemCount - visibleItems;
        if (newOff > maxOff) newOff = maxOff;
        if (newOff != scrollOffset) {
            scrollOffset = newOff;
            MarkDirty();
        }
        return;
    }

    int localY = my - this->y;
    int localX = mx - this->x;
    if (localY > LISTVIEW_HEADER_HEIGHT && localY < h) {
        if (localX >= 0 && localX < w - LISTVIEW_SCROLLBAR_WIDTH) {
            int hovered =
                scrollOffset + (localY - LISTVIEW_HEADER_HEIGHT - 1) / itemHeight;
            if (hovered >= 0 && hovered < itemCount && hovered != hoveredIndex) {
                hoveredIndex = hovered;
                MarkDirty();
            }
        } else if (hoveredIndex >= 0) {
            hoveredIndex = -1;
            MarkDirty();
        }
    } else if (hoveredIndex >= 0) {
        hoveredIndex = -1;
        MarkDirty();
    }
}
