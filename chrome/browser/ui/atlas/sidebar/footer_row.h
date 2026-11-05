// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_SIDEBAR_FOOTER_ROW_H_
#define CHROME_BROWSER_UI_ATLAS_SIDEBAR_FOOTER_ROW_H_

namespace gfx {
class Canvas;
class Rect;
}  // namespace gfx

namespace ui {
class ColorProvider;
}

namespace atlas::sidebar {

// The footer of the sidebar is the "New tab" row (tokens.md section 3): the
// engine's new-tab button draws the glyph and the label; this adds what is
// specific to the footer.

// The rule above the row, as wide as the rows (inset on both sides).
inline constexpr int kFooterRuleThickness = 1;
// The shortcut sits this far from the right edge, like the rows' content inset.
inline constexpr int kFooterShortcutInset = 8;
// Below this width the sidebar is collapsed to a column of glyphs and the row
// shows its glyph alone.
inline constexpr int kFooterMinWidthForText = 120;

// True when a footer of this width shows the shortcut and the rule.
bool FooterShowsDecorations(int width);

// Paints the rule at the top of `bounds` and the shortcut at its right (11 px,
// dim). `row_inset` is the sidebar's horizontal row padding. `rtl` mirrors the
// side of the shortcut.
void PaintFooterDecorations(gfx::Canvas* canvas,
                            const gfx::Rect& bounds,
                            int row_inset,
                            bool rtl,
                            const ui::ColorProvider& provider);

}  // namespace atlas::sidebar

#endif  // CHROME_BROWSER_UI_ATLAS_SIDEBAR_FOOTER_ROW_H_
