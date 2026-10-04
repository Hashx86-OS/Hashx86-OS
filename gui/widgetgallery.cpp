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

#define KDBG_COMPONENT "GUI:GALLERY"
#include <gui/checkbox.h>
#include <gui/combobox.h>
#include <gui/desktop.h>
#include <gui/iconbutton.h>
#include <gui/imageview.h>
#include <gui/inputbox.h>
#include <gui/label.h>
#include <gui/listview.h>
#include <gui/progressbar.h>
#include <gui/radiobutton.h>
#include <gui/scrollbar.h>
#include <gui/slider.h>
#include <gui/spinbox.h>
#include <gui/table.h>
#include <gui/tabs.h>
#include <gui/textbox.h>
#include <gui/toggle.h>
#include <gui/tooltip.h>
#include <gui/widgetgallery.h>
#include <gui/window.h>

namespace {

/** AddCaption() - Add a small section label inside the gallery window. */
Label* AddCaption(Window* win, int32_t x, int32_t y, const char* text) {
    Label* caption = new Label(win, x, y, 280, 18, text);
    caption->setSize(SMALL);
    win->AddChild(caption);
    return caption;
}

}  // namespace

Window* OpenWidgetGallery(Desktop* desktop) {
    if (!desktop) return nullptr;

    Window* win = new Window(desktop, 120, 60, 960, 620);
    if (!win) return nullptr;
    win->setWindowTitle("Widget Gallery");

    const int32_t col1 = 24;
    const int32_t col2 = 344;
    const int32_t col3 = 648;

    // ---- Column 1: buttons and value widgets ------------------------------
    AddCaption(win, col1, 36, "Button / IconButton");
    Button* button = new Button(win, col1, 56, 140, 30, "Click me");
    win->AddChild(button);
    IconButton* iconButton = new IconButton(win, col1 + 152, 56, 120, 30, "fa-power-off", "Power");
    win->AddChild(iconButton);

    AddCaption(win, col1, 100, "CheckBox");
    CheckBox* check = new CheckBox(win, col1, 120, 200, 24, "Enable feature");
    win->AddChild(check);

    AddCaption(win, col1, 156, "RadioButton");
    RadioButton* radioA = new RadioButton(win, col1, 176, 200, 20, "Option A", 1);
    RadioButton* radioB = new RadioButton(win, col1, 198, 200, 20, "Option B", 1);
    win->AddChild(radioA);
    win->AddChild(radioB);
    radioA->SetChecked(true);

    AddCaption(win, col1, 232, "Toggle");
    Toggle* toggle = new Toggle(win, col1, 252, 44, 22);
    win->AddChild(toggle);
    toggle->SetOn(true);

    AddCaption(win, col1, 292, "Slider");
    Slider* slider = new Slider(win, col1, 312, 230, 20);
    win->AddChild(slider);
    slider->SetRange(0, 100);
    slider->SetValue(40);

    AddCaption(win, col1, 348, "ProgressBar");
    ProgressBar* progress = new ProgressBar(win, col1, 368, 230, 16, 0.65f);
    win->AddChild(progress);

    AddCaption(win, col1, 402, "SpinBox");
    SpinBox* spin = new SpinBox(win, col1, 422, 110, 26);
    win->AddChild(spin);
    spin->SetRange(0, 100, 5);
    spin->SetValue(25);

    // ---- Column 2: text and list widgets ----------------------------------
    AddCaption(win, col2, 36, "ComboBox");
    ComboBox* combo = new ComboBox(win, col2, 56, 200, 24);
    win->AddChild(combo);
    combo->AddItem("Alpha");
    combo->AddItem("Beta");
    combo->AddItem("Gamma");

    AddCaption(win, col2, 100, "InputBox");
    InputBox* input = new InputBox(win, col2, 120, 220, 26, 64);
    win->AddChild(input);

    AddCaption(win, col2, 156, "TextBox (multi-line)");
    TextBox* textbox = new TextBox(win, col2, 176, 260, 90, 256);
    win->AddChild(textbox);
    textbox->SetText("Multi-line\nText box:\nediting works");

    AddCaption(win, col2, 280, "ListView");
    ListView* list = new ListView(win, col2, 300, 260, 150);
    win->AddChild(list);
    list->SetHeader("Name");
    list->AddItem("Documents", 0, 1);
    list->AddItem("photo.png", 2048, 0);
    list->AddItem("app.exe", 65536, 2);
    list->AddItem("readme.txt", 512, 0);

    AddCaption(win, col2, 464, "ScrollBar");
    ScrollBar* scroll = new ScrollBar(win, col2, 484, 260, 12, false);
    win->AddChild(scroll);
    scroll->SetRange(0, 100, 25);
    scroll->SetValue(30);

    // ---- Column 3: container and display widgets --------------------------
    AddCaption(win, col3, 36, "Tabs");
    Tabs* tabs = new Tabs(win, col3, 56, 280, 170);
    win->AddChild(tabs);
    tabs->AddTab("Files");
    tabs->AddTab("Edit");
    tabs->AddTab("View");

    AddCaption(win, col3, 240, "ImageView");
    const int32_t imgW = 16;
    const int32_t imgH = 10;
    uint32_t* checker = new uint32_t[imgW * imgH];
    for (int32_t y = 0; y < imgH; y++) {
        for (int32_t x = 0; x < imgW; x++) {
            checker[y * imgW + x] = ((x / 2 + y / 2) % 2) ? 0xFF0078D4 : 0xFFEFEFEF;
        }
    }
    ImageView* image = new ImageView(win, col3, 260, 160, 100, checker, imgW, imgH);
    win->AddChild(image);
    delete[] checker;

    AddCaption(win, col3, 376, "Tooltip");
    Tooltip* tip = new Tooltip(win, col3, 396, 200, 26, "This is a tooltip");
    win->AddChild(tip);
    tip->Show();

    AddCaption(win, col3, 436, "Table");
    Table* table = new Table(win, col3, 456, 280, 130, 2);
    win->AddChild(table);
    table->SetHeader(0, "Key");
    table->SetHeader(1, "Value");
    const char* row0[] = {"width", "960"};
    const char* row1[] = {"height", "620"};
    const char* row2[] = {"mode", "ARGB"};
    table->AddRow(row0);
    table->AddRow(row1);
    table->AddRow(row2);

    desktop->AddChild(win);
    win->RequestTopmostFocus();

    if (desktop->GetTaskbar()) {
        desktop->GetTaskbar()->AddTab(0, "Widget Gallery", win);
    }

    return win;
}
