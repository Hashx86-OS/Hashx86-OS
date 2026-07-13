#ifndef TERMINALVIEW_H
#define TERMINALVIEW_H

#include <gui/widget.h>
#include <gui/fonts/font.h>
#include <utils/linkedList.h>

class TerminalView : public Widget {
public:
    enum KeyEventType {
        KEY_EVENT_NONE,
        KEY_EVENT_NORMAL,
        KEY_EVENT_SPECIAL,
    };

    struct KeyEvent {
        KeyEventType type;
        char key;
        uint8_t specialKey;
    };

private:
    char* text;
    FontSize fontSize;
    int scrollTotal;
    int scrollVisible;
    int scrollOffset;
    int pendingScrollAction;

    bool isDraggingThumb;
    int dragStartY;
    int dragStartOffset;
    LinkedList<KeyEvent> keyEventQueue;

    void PutPixel(int32_t px, int32_t py, uint32_t color);
    void DrawScrollBar();
    void DrawCharacter(char c, int32_t x, int32_t y, uint32_t color);

    // Scrollbar geometry helpers
    int ScrollBarX() const;
    int ScrollBarY() const;
    int ScrollBarW() const;
    int ScrollBarH() const;
    int ScrollBtnH() const;
    int TrackY() const;
    int TrackH() const;
    int ThumbH() const;
    int ThumbY() const;
    int MaxOffset() const;

public:
    TerminalView(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* text);
    ~TerminalView();

    void setText(const char* newText);
    void setSize(FontSize size);
    void SetFontSize(int32_t px);
    void setScrollMeta(int totalLines, int visibleLines, int offset);
    int consumeScrollAction();
    bool IsTerminalView() const override {
        return true;
    }

    void RedrawToCache() override;
    void OnMouseDown(int32_t x, int32_t y, uint8_t button) override;
    void OnMouseUp(int32_t x, int32_t y, uint8_t button) override;
    void OnMouseMove(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy) override;
    void OnKeyDown(const char* key) override;
    void OnSpecialKeyDown(uint8_t key) override;

    KeyEvent consumeKeyEvent();
};

#endif  // TERMINALVIEW_H
