// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_REST_REST_MARK_H_
#define CHROME_BROWSER_UI_ATLAS_REST_REST_MARK_H_

#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class Canvas;
class Rect;
}  // namespace gfx

namespace atlas::rest {

// The frost mark: the WolOS glyph weather-snow, 16 px, at the trailing end of the row where the
// close button sits on the tab in front (reel scene 9, tokens.md section 3).
inline constexpr int kRestMarkSize = 16;
inline constexpr int kRestMarkTrailingInset = 8;

// Whether a sidebar row shows the resting state: its tab gave its memory back (the engine
// discarded it) and it is not the tab in front. The tab in front never rests.
bool IsResting(bool discarded, bool active);

// The title colour of a resting row: the row's text colour toward its fill, as `dim`.
SkColor RestingTitleColor(SkColor text, SkColor fill);

// Where the mark goes in a row of `row_width` x `row_height` (left-to-right; mirrored by the view).
gfx::Rect RestMarkBounds(int row_width, int row_height);

// Paints the mark in `color` (the accent) into `bounds`.
void PaintRestMark(gfx::Canvas* canvas, const gfx::Rect& bounds, SkColor color);

}  // namespace atlas::rest

#endif  // CHROME_BROWSER_UI_ATLAS_REST_REST_MARK_H_
