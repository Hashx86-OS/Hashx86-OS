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

#ifndef PROGRESSBAR_H
#define PROGRESSBAR_H

#include <gui/config/config.h>
#include <gui/widget.h>
#include <types.h>

/**
 * class ProgressBar - A horizontal bar showing fractional completion.
 *
 * Renders a filled portion proportional to the current progress value on a
 * background track with a configurable fill color.
 */
class ProgressBar : public Widget {
private:
    float progress;  // 0.0 to 1.0.
    uint32_t barColor;
    uint32_t backgroundColor;

public:
    /**
     * ProgressBar() - Construct a progress bar.
     * @parent: Parent widget, or NULL for a root widget.
     * @x: X position relative to @parent.
     * @y: Y position relative to @parent.
     * @w: Bar width in pixels.
     * @h: Bar height in pixels.
     * @initialProgress: Starting fill fraction, 0.0 to 1.0.
     */
    ProgressBar(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h,
                float initialProgress = 0.0f);

    /**
     * ~ProgressBar() - Destroy the progress bar.
     */
    ~ProgressBar();

    // Progress management.

    /**
     * SetProgress() - Set the fill fraction and repaint.
     * @progress: Fraction filled, clamped to 0.0-1.0.
     */
    void SetProgress(float progress);

    /**
     * GetProgress() - Return the current fill fraction, 0.0 to 1.0.
     */
    float GetProgress() const;

    /**
     * SetPercentage() - Set the fill as a percentage and repaint.
     * @percentage: Percent filled, clamped to 0-100.
     */
    void SetPercentage(int32_t percentage);

    // Appearance (optional override).

    /**
     * SetBarColor() - Override the filled-portion color.
     * @color: Fill color as 0xAARRGGBB.
     */
    void SetBarColor(uint32_t color);

    /**
     * SetBackgroundColor() - Override the track color.
     * @color: Track color as 0xAARRGGBB.
     */
    void SetBackgroundColor(uint32_t color);

    // Redraw.

    /** RedrawToCache() - Paint the track and the filled portion. */
    void RedrawToCache() override;

    /** update() - Repaint the cache and redraw the bar. */
    void update();
};

#endif  // PROGRESSBAR_H
