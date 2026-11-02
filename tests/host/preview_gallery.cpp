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

// Host-side gallery preview: renders one example of every widget into a PPM so
// the widget set can be inspected without booting the OS. It uses the exact
// same widget sources as the kernel, so what it draws is what the kernel draws.

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <gui/button.h>
#include <gui/checkbox.h>
#include <gui/combobox.h>
#include <gui/fonts/font.h>
#include <gui/iconbutton.h>
#include <gui/imageview.h>
#include <gui/inputbox.h>
#include <gui/label.h>
#include <gui/listview.h>
#include <gui/progressbar.h>
#include <gui/radiobutton.h>
#include <gui/render/raster.h>
#include <gui/scrollbar.h>
#include <gui/slider.h>
#include <gui/spinbox.h>
#include <gui/table.h>
#include <gui/tabs.h>
#include <gui/textbox.h>
#include <gui/toggle.h>
#include <gui/tooltip.h>
#include <gui/widget.h>
#include <gui/window.h>

namespace {

const int32_t W = 980;
const int32_t H = 660;

Label* Caption(Window* win, int32_t x, int32_t y, const char* text) {
    Label* caption = new Label(win, x, y, 280, 18, text);
    caption->setSize(SMALL);
    win->AddChild(caption);
    return caption;
}

bool LoadFont() {
    FILE* f = std::fopen("bin/fonts/CascadiaMono-Regular.ttf", "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long len = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (len <= 0) {
        std::fclose(f);
        return false;
    }
    uint8_t* bytes = new uint8_t[len];
    size_t got = std::fread(bytes, 1, (size_t)len, f);
    std::fclose(f);
    if (got != (size_t)len) {
        delete[] bytes;
        return false;
    }
    FontManager* fm = new FontManager();
    FontManager::activeInstance = fm;
    fm->LoadMemory(bytes, (size_t)len, REGULAR, nullptr, 32, 95);
    delete[] bytes;
    return true;
}

bool WritePPM(const char* path, RasterSurface& surface) {
    FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    std::fprintf(f, "P6\n%d %d\n255\n", (int)surface.GetWidth(), (int)surface.GetHeight());
    const uint32_t* px = surface.GetBackBuffer();
    for (int32_t i = 0; i < W * H; i++) {
        uint8_t rgb[3] = {(uint8_t)((px[i] >> 16) & 0xFF), (uint8_t)((px[i] >> 8) & 0xFF),
                          (uint8_t)(px[i] & 0xFF)};
        std::fwrite(rgb, 1, 3, f);
    }
    std::fclose(f);
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    const char* out = (argc > 1) ? argv[1] : "gallery.ppm";

    if (!LoadFont()) {
        std::fprintf(stderr, "preview_gallery: failed to load the UI font\n");
        return 1;
    }

    RasterSurface surface(W, H);
    CompositeWidget root(nullptr, 0, 0, W, H);
    Window* win = new Window(&root, 0, 0, W, H);
    win->setWindowTitle("Widget Gallery");
    root.AddChild(win);

    const int32_t col1 = 24;
    const int32_t col2 = 344;
    const int32_t col3 = 648;

    Caption(win, col1, 36, "Button / IconButton");
    win->AddChild(new Button(win, col1, 56, 140, 30, "Click me"));
    win->AddChild(new IconButton(win, col1 + 152, 56, 120, 30, "fa-power-off", "Power"));

    Caption(win, col1, 100, "CheckBox");
    CheckBox* check = new CheckBox(win, col1, 120, 200, 24, "Enable feature");
    win->AddChild(check);
    check->SetChecked(true);

    Caption(win, col1, 156, "RadioButton");
    RadioButton* radioA = new RadioButton(win, col1, 176, 200, 20, "Option A", 1);
    RadioButton* radioB = new RadioButton(win, col1, 198, 200, 20, "Option B", 1);
    win->AddChild(radioA);
    win->AddChild(radioB);
    radioA->SetChecked(true);

    Caption(win, col1, 232, "Toggle");
    Toggle* toggle = new Toggle(win, col1, 252, 44, 22);
    win->AddChild(toggle);
    toggle->SetOn(true);

    Caption(win, col1, 292, "Slider");
    Slider* slider = new Slider(win, col1, 312, 230, 20);
    win->AddChild(slider);
    slider->SetRange(0, 100);
    slider->SetValue(40);

    Caption(win, col1, 348, "ProgressBar");
    win->AddChild(new ProgressBar(win, col1, 368, 230, 16, 0.65f));

    Caption(win, col1, 402, "SpinBox");
    SpinBox* spin = new SpinBox(win, col1, 422, 110, 26);
    win->AddChild(spin);
    spin->SetRange(0, 100, 5);
    spin->SetValue(25);

    Caption(win, col2, 36, "ComboBox");
    ComboBox* combo = new ComboBox(win, col2, 56, 200, 24);
    win->AddChild(combo);
    combo->AddItem("Alpha");
    combo->AddItem("Beta");
    combo->AddItem("Gamma");

    Caption(win, col2, 100, "InputBox");
    win->AddChild(new InputBox(win, col2, 120, 220, 26, 64));

    Caption(win, col2, 156, "TextBox (multi-line)");
    TextBox* textbox = new TextBox(win, col2, 176, 260, 90, 256);
    win->AddChild(textbox);
    textbox->SetText("Multi-line\nText box:\nediting works");

    Caption(win, col2, 280, "ListView");
    ListView* list = new ListView(win, col2, 300, 260, 150);
    win->AddChild(list);
    list->AddColumn("Name", 0);
    const char* rowDocs[] = {"Documents"};
    const char* rowPhoto[] = {"photo.png"};
    const char* rowApp[] = {"app.exe"};
    const char* rowReadme[] = {"readme.txt"};
    list->AddItem(rowDocs, 1, 1);
    list->AddItem(rowPhoto, 1, 0);
    list->AddItem(rowApp, 1, 2);
    list->AddItem(rowReadme, 1, 0);

    Caption(win, col2, 464, "ScrollBar");
    ScrollBar* scroll = new ScrollBar(win, col2, 484, 260, 12, false);
    win->AddChild(scroll);
    scroll->SetRange(0, 100, 25);
    scroll->SetValue(30);

    Caption(win, col3, 36, "Tabs");
    Tabs* tabs = new Tabs(win, col3, 56, 280, 170);
    win->AddChild(tabs);
    tabs->AddTab("Files");
    tabs->AddTab("Edit");
    tabs->AddTab("View");
    Label* tabBody = new Label(tabs->GetContentArea(), 8, 8, 250, 18, "Content for the active tab");
    tabBody->setSize(SMALL);
    tabs->GetContentArea()->AddChild(tabBody);

    Caption(win, col3, 240, "ImageView");
    const int32_t imgW = 16;
    const int32_t imgH = 10;
    uint32_t* checker = new uint32_t[imgW * imgH];
    for (int32_t y = 0; y < imgH; y++) {
        for (int32_t x = 0; x < imgW; x++) {
            checker[y * imgW + x] = ((x / 2 + y / 2) % 2) ? 0xFF0078D4 : 0xFFEFEFEF;
        }
    }
    win->AddChild(new ImageView(win, col3, 260, 160, 100, checker, imgW, imgH));
    delete[] checker;

    Caption(win, col3, 376, "Tooltip");
    Tooltip* tip = new Tooltip(win, col3, 396, 200, 26, "This is a tooltip");
    win->AddChild(tip);
    tip->Show();

    Caption(win, col3, 436, "Table");
    Table* table = new Table(win, col3, 456, 280, 130, 2);
    win->AddChild(table);
    table->SetHeader(0, "Key");
    table->SetHeader(1, "Value");
    const char* row0[] = {"width", "980"};
    const char* row1[] = {"height", "660"};
    const char* row2[] = {"mode", "ARGB"};
    table->AddRow(row0);
    table->AddRow(row1);
    table->AddRow(row2);

    surface.FillRectangle(0, 0, W, H, 0xFF151515);
    win->MarkDirty();
    win->Draw(&surface);

    if (!WritePPM(out, surface)) {
        std::fprintf(stderr, "failed to write %s\n", out);
        return 1;
    }
    std::printf("wrote %s (%dx%d)\n", out, (int)W, (int)H);
    return 0;
}
