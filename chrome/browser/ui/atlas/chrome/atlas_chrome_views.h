// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_CHROME_ATLAS_CHROME_VIEWS_H_
#define CHROME_BROWSER_UI_ATLAS_CHROME_ATLAS_CHROME_VIEWS_H_

#include "chrome/browser/ui/atlas/chrome/atlas_tokens.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/size_f.h"

namespace gfx {
class Canvas;
}

namespace views {
class Label;
}

namespace atlas {

// Paints the load bar of the omnibox (tokens.md section 4): a solid
// `kProgressBarHeight` px bar in `color` along the bottom edge of a view of
// `size`, `progress_width` px long, at opacity `fade` (1 while loading, falling
// to 0 at the end). No glow, no pulse: the bar follows the load value. Returns
// false (and paints nothing) when the geometry is degenerate.
bool PaintProgressBar(gfx::Canvas* canvas,
                      const gfx::SizeF& size,
                      float progress_width,
                      float fade,
                      SkColor color);

// Styles `label` as a section header (tokens.md section 3): upper case, 11 px,
// semibold, `dim`. The downloads list header uses it.
void StyleSectionHeader(views::Label* label);

// `base` at the secondary text size (11 px, tokens.md section 1).
gfx::FontList SecondaryFontList(const gfx::FontList& base);

}  // namespace atlas

#endif  // CHROME_BROWSER_UI_ATLAS_CHROME_ATLAS_CHROME_VIEWS_H_
