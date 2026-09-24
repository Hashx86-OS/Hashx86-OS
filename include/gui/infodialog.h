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

#ifndef INFO_DIALOG_H
#define INFO_DIALOG_H

#include <gui/bmp.h>
#include <gui/elements/window_action_button.h>
#include <gui/label.h>
#include <gui/window.h>

/**
 * class InfoDialog - A modal-ish info window with title, content, and OK button.
 *
 * Lays out a summary label, an info label, an optional details label, an icon,
 * and a dismiss button inside a small window.
 */
class InfoDialog : public Window {
private:
    Label* summaryLabel;
    Label* infoLabel;
    Label* detailsLabel;
    ACButton* okButton;
    Bitmap* iconBitmap;

public:
    /**
     * InfoDialog() - Construct an empty info dialog.
     * @parent: Parent composite widget, or NULL for the root.
     * @width: Dialog width in pixels.
     * @height: Dialog height in pixels.
     */
    InfoDialog(CompositeWidget* parent, int32_t width = 540, int32_t height = 220);

    /**
     * ~InfoDialog() - Destroy the dialog and its child widgets.
     */
    ~InfoDialog();

    /**
     * SetTitleText() - Set the dialog title.
     * @title: New title text.
     */
    void SetTitleText(const char* title);

    /**
     * SetIconBitmap() - Load and show a leading icon image.
     * @bitmapPath: Path of the BMP file relative to the boot partition.
     */
    void SetIconBitmap(const char* bitmapPath);

    /**
     * SetContent() - Set the summary, info, and details text lines.
     * @summary: Summary line.
     * @info: Secondary info line.
     * @details: Optional details line.
     */
    void SetContent(const char* summary, const char* info, const char* details);

    /** ShowDialog() - Show the dialog window. */
    void ShowDialog();

    /** HideDialog() - Hide the dialog window. */
    void HideDialog();

    /** Draw() - Paint the dialog over its parent composite. */
    void Draw(GraphicsDriver* gc) override;
};

#endif  // INFO_DIALOG_H
