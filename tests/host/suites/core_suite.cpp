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

#include "../framework.h"
#include "../harness.h"

#include <gui/button.h>
#include <gui/label.h>
#include <gui/progressbar.h>
#include <gui/render/raster.h>
#include <gui/widget.h>

namespace {

uint64_t g_rng = 0x2545F4914F6CDD1DULL;

uint32_t Rnd() {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 7;
    g_rng ^= g_rng << 17;
    return (uint32_t)(g_rng >> 32);
}

int32_t RndRange(int32_t lo, int32_t hi) {
    if (hi <= lo) return lo;
    return lo + (int32_t)(Rnd() % (uint32_t)(hi - lo));
}

}  // namespace

// ---------------------------------------------------------------------------
// Hit-testing: every grid point against every child must agree with the
// direct bounds test. The root sits at the origin, so root coordinates and
// child-local coordinates coincide.
// ---------------------------------------------------------------------------
TEST(hit_test_grid_sweep) {
    const int kTrees = 6;
    const int kGrid = 16;
    for (int t = 0; t < kTrees; t++) {
        Widget root(nullptr, 0, 0, 256, 256);
        Widget* kids[8];
        int n = 1 + (int)(Rnd() % 8);
        for (int i = 0; i < n; i++) {
            int32_t x = RndRange(0, 256);
            int32_t y = RndRange(0, 256);
            int32_t w = RndRange(1, 64);
            int32_t h = RndRange(1, 64);
            kids[i] = new Widget(&root, x, y, w, h);
            root.AddChild(kids[i]);
        }

        for (int gx = 0; gx < kGrid; gx++) {
            for (int gy = 0; gy < kGrid; gy++) {
                int32_t px = gx * (256 / kGrid) + (256 / kGrid) / 2;
                int32_t py = gy * (256 / kGrid) + (256 / kGrid) / 2;
                for (int i = 0; i < n; i++) {
                    bool inside = kids[i]->ContainsCoordinate(px, py);
                    bool expected = (px >= kids[i]->x && px < kids[i]->x + kids[i]->w &&
                                     py >= kids[i]->y && py < kids[i]->y + kids[i]->h);
                    CHECK_EQ(inside, expected);
                }
            }
        }

        for (int i = 0; i < n; i++) {
            root.RemoveChild(kids[i]);
            delete kids[i];
        }
    }
}

// ---------------------------------------------------------------------------
// Coordinate transform: ModelToScreen of a child origin must equal the parent
// origin plus the child offset, recursively.
// ---------------------------------------------------------------------------
TEST(model_to_screen_matches_offsets) {
    for (int t = 0; t < 200; t++) {
        Widget root(nullptr, RndRange(-20, 20), RndRange(-20, 20), 200, 200);
        Widget mid(&root, RndRange(0, 40), RndRange(0, 40), 100, 100);
        Widget leaf(&mid, RndRange(0, 30), RndRange(0, 30), 20, 20);
        root.AddChild(&mid);
        mid.AddChild(&leaf);

        int32_t x = 0, y = 0;
        leaf.ModelToScreen(x, y);
        CHECK_EQ(x, root.x + mid.x + leaf.x);
        CHECK_EQ(y, root.y + mid.y + leaf.y);
    }
}

// ---------------------------------------------------------------------------
// Focus uniqueness: at most one child of a composite is focused at any time,
// across a long random focus sequence.
// ---------------------------------------------------------------------------
TEST(focus_uniqueness_property) {
    for (int s = 0; s < 40; s++) {
        CompositeWidget comp(nullptr, 0, 0, 128, 128);
        Widget* kids[6];
        for (int i = 0; i < 6; i++) {
            kids[i] = new Widget(&comp, i * 20, 0, 18, 18);
            comp.AddChild(kids[i]);
        }

        for (int e = 0; e < 100; e++) {
            int i = (int)(Rnd() % 6);
            comp.GetFocus(kids[i]);
            int focused = 0;
            for (int j = 0; j < 6; j++) {
                if (kids[j]->IsFocused()) focused++;
            }
            CHECK(focused <= 1);
        }
        // comp destructor deletes the children.
    }
}

// ---------------------------------------------------------------------------
// Dirty propagation: MarkDirty on a descendant must mark every ancestor dirty,
// and a Draw on the root must clear the whole visible tree.
// ---------------------------------------------------------------------------
TEST(dirty_propagation) {
    RasterSurface surface(256, 256);
    for (int t = 0; t < 100; t++) {
        CompositeWidget root(nullptr, 0, 0, 256, 256);
        CompositeWidget child(&root, 10, 10, 100, 100);
        Widget leaf(&child, 5, 5, 20, 20);
        root.AddChild(&child);
        child.AddChild(&leaf);

        leaf.MarkDirty();
        CHECK(root.isDirty);
        CHECK(child.isDirty);
        CHECK(leaf.isDirty);

        root.Draw(&surface);
        CHECK(!root.isDirty);
        CHECK(!child.isDirty);
        CHECK(!leaf.isDirty);
        CHECK(!leaf.isDirty);
    }
}

// ---------------------------------------------------------------------------
// Resize invariants: the cache is non-null exactly when both dimensions are
// positive, and repeated resizes keep the reported geometry consistent.
// ---------------------------------------------------------------------------
TEST(resize_cache_invariants) {
    for (int t = 0; t < 150; t++) {
        int32_t w = RndRange(0, 40);
        int32_t h = RndRange(0, 40);
        Widget widget(nullptr, 0, 0, w, h);
        CHECK_EQ(widget.w, w);
        CHECK_EQ(widget.h, h);
        CHECK_EQ(widget.cache != nullptr, (w > 0 && h > 0));

        int32_t nw = RndRange(0, 40);
        int32_t nh = RndRange(0, 40);
        bool ok = widget.Resize(nw, nh);
        CHECK(ok);
        CHECK_EQ(widget.w, nw);
        CHECK_EQ(widget.h, nh);
        CHECK_EQ(widget.cache != nullptr, (nw > 0 && nh > 0));
    }
}

// ---------------------------------------------------------------------------
// Button click semantics: a press+release inside emits exactly one ON_CLICK
// for the owning process; a release outside emits none.
// ---------------------------------------------------------------------------
TEST(button_click_inside_emits_one) {
    CompositeWidget root(nullptr, 0, 0, 200, 200);
    Button* button = new Button(&root, 10, 10, 60, 30, "OK");
    root.AddChild(button);
    button->SetID(4242);
    button->SetPID(9);

    HostEventReset();
    button->OnMouseDown(20, 20, 1);
    button->OnMouseUp(20, 20, 1);
    CHECK_EQ(HostEventCount(), 1);
    if (HostEventCount() == 1) {
        CHECK_EQ((uint32_t)HostEventAt(0).widgetID, 4242u);
        CHECK_EQ((uint32_t)HostEventAt(0).pid, 9u);
    }
    CHECK(!button->IsPressed());
    // root destructor deletes button.
}

TEST(button_release_outside_emits_none) {
    CompositeWidget root(nullptr, 0, 0, 200, 200);
    Button* button = new Button(&root, 10, 10, 60, 30, "OK");
    root.AddChild(button);
    button->SetID(4243);
    button->SetPID(9);

    HostEventReset();
    button->OnMouseDown(20, 20, 1);
    button->OnMouseUp(150, 150, 1);
    CHECK_EQ(HostEventCount(), 0);
    CHECK(!button->IsPressed());
}

// ---------------------------------------------------------------------------
// Raster primitives: a filled rectangle touches every expected pixel and
// nothing outside it.
// ---------------------------------------------------------------------------
TEST(raster_fill_rectangle_exact) {
    const uint32_t bg = 0xFF000000;
    const uint32_t fg = 0xFF123456;
    RasterSurface surface(40, 40);
    surface.FillRectangle(0, 0, 40, 40, bg);
    surface.FillRectangle(10, 10, 20, 5, fg);

    int insideHits = 0;
    int outsideHits = 0;
    for (int y = 0; y < 40; y++) {
        for (int x = 0; x < 40; x++) {
            uint32_t px = surface.GetBackBuffer()[y * 40 + x];
            bool inside = (x >= 10 && x < 30 && y >= 10 && y < 15);
            if (px == fg) {
                insideHits++;
                CHECK(inside);
            } else {
                outsideHits++;
                CHECK(!inside);
                CHECK_EQ(px, bg);
            }
        }
    }
    CHECK_EQ(insideHits, 20 * 5);
    CHECK_EQ(outsideHits, 40 * 40 - 20 * 5);
}

// ---------------------------------------------------------------------------
// ProgressBar clamping: the stored progress always stays within [0, 1].
// ---------------------------------------------------------------------------
TEST(progressbar_clamps) {
    ProgressBar bar(nullptr, 0, 0, 100, 10);
    for (int t = 0; t < 300; t++) {
        float v = ((float)(int)RndRange(-200, 200)) / 100.0f;
        bar.SetProgress(v);
        float got = bar.GetProgress();
        CHECK(got >= 0.0f);
        CHECK(got <= 1.0f);
    }
}
