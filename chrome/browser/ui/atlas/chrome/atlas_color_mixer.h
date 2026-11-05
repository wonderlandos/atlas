// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_CHROME_ATLAS_COLOR_MIXER_H_
#define CHROME_BROWSER_UI_ATLAS_CHROME_ATLAS_COLOR_MIXER_H_

#include "ui/color/color_provider_key.h"

namespace ui {
class ColorProvider;
}

namespace atlas {

// Puts the WolOS tokens (brand/atlas/chrome/tokens.md section 1) on the
// engine's colours. The colours come from the system theme (GTK mode, the theme
// `WolOS-<id>` that `wol theme` writes), so they follow the theme, its accent
// and the light/dark switch live; this mixer only says which token paints which
// part of the window (tokens.md sections 2, 4 and 8) and derives the few tokens
// the theme does not carry. It adds no colour literal.
//
// What the system theme has to provide (the contract with the theme writer,
// crates/core/src/theme/gtk_atlas.rs):
//   text: the `label` colour (kColorNativeLabelForeground)
//   accent: the selected cell of a tree view (kColorAccent)
//   on-accent: the label on that cell (kColorButtonForegroundProminent)
//   surface: the window colour (kColorPrimaryBackground)
//   glass: the `toolbar` node (kColorToolbar)
//   card: the menu colour (kColorMenuBackground)
//   control: the text view colour (kColorTextfieldBackground)
//   line: the horizontal separator (kColorMidground)
// `dim` is derived: `text` over the toolbar at kDimAlpha. Does nothing for a
// key that is not the GTK system theme (a theme from the web store keeps its
// own colours).
//
// Called from AddChromeColorMixers() after the native mixer (patch 0070).
void AddAtlasColorMixer(ui::ColorProvider* provider,
                        const ui::ColorProviderKey& key);

}  // namespace atlas

#endif  // CHROME_BROWSER_UI_ATLAS_CHROME_ATLAS_COLOR_MIXER_H_
