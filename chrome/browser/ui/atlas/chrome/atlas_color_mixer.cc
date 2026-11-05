// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/chrome/atlas_color_mixer.h"

#include "chrome/browser/ui/atlas/chrome/atlas_tokens.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"

namespace atlas {

void AddAtlasColorMixer(ui::ColorProvider* provider,
                        const ui::ColorProviderKey& key) {
  // In GTK mode the key carries the GTK theme supplier as its custom theme, so
  // only the system theme decides; a theme from the web store has none.
  if (key.system_theme != ui::SystemTheme::kGtk) {
    return;
  }

  // The tokens, as ids the system theme fills (see the header).
  const ui::ColorId text = ui::kColorNativeLabelForeground;
  const ui::ColorId accent = ui::kColorAccent;
  const ui::ColorId on_accent = ui::kColorButtonForegroundProminent;
  const ui::ColorId glass = kColorToolbar;
  const ui::ColorId card = ui::kColorMenuBackground;
  const ui::ColorId control = ui::kColorTextfieldBackground;
  const ui::ColorId line = ui::kColorMidground;

  const bool dark = key.color_mode == ui::ColorProviderKey::ColorMode::kDark;
  const SkAlpha selection_alpha =
      dark ? tokens::kSelectionAlphaDark : tokens::kSelectionAlphaLight;

  ui::ColorMixer& mixer = provider->AddMixer();

  // `dim`: secondary text, the lock and the path of an address.
  mixer[kColorToolbarText] = {text};
  mixer[kColorToolbarTextDefault] = {text};
  mixer[kColorOmniboxText] = {text};
  mixer[kColorOmniboxTextDimmed] =
      ui::AlphaBlend(text, glass, tokens::kDimAlpha);

  // Accent, and the selection pill (accent at 22% / 30%).
  mixer[ui::kColorSysPrimary] = {accent};
  mixer[ui::kColorSysOnPrimary] = {on_accent};
  // The focus ring is `accent` itself: tokens.md section 4 says 90%, but over
  // the toolbar that is 7-10 dE away from `accent` and the reel check wants the
  // ring within dE 4 of it.
  mixer[ui::kColorFocusableBorderFocused] = {accent};
  mixer[ui::kColorSysStateFocusRing] = {accent};

  // Toolbar (section 2): `glass`, a 1 px `line` below; buttons hover `control`,
  // pressed accent tint.
  mixer[kColorToolbarContentAreaSeparator] = {line};
  mixer[kColorToolbarContentAreaSeparatorSolid] =
      ui::GetResultingPaintColor(line, glass);
  mixer[kColorToolbarInkDropHover] = {control};
  mixer[kColorToolbarInkDropRipple] = ui::SetAlpha(accent, selection_alpha);
  mixer[kColorToolbarButtonIconHovered] = {text};
  mixer[kColorToolbarButtonIconPressed] = {text};

  // Omnibox (section 4): `control` fill, 1 px `line`, hover one step toward
  // `card`.
  mixer[kColorLocationBarBackground] = {control};
  mixer[kColorLocationBarBackgroundHovered] =
      ui::AlphaBlend(card, control, tokens::kHoverTowardCardAlpha);
  mixer[kColorLocationBarBorder] = {line};
  mixer[kColorLocationBarBorderOpaque] =
      ui::GetResultingPaintColor(line, glass);
  mixer[kColorOmniboxResultsBackground] = {card};
  mixer[kColorOmniboxResultsBackgroundHovered] =
      ui::AlphaBlend(text, card, 0x10);
  mixer[kColorOmniboxResultsBackgroundSelected] =
      ui::GetResultingPaintColor(ui::SetAlpha(accent, selection_alpha), card);
  mixer[kColorOmniboxResultsTextSelected] = {text};
  mixer[kColorOmniboxResultsTextDimmed] =
      ui::AlphaBlend(text, card, tokens::kDimAlpha);
  mixer[kColorOmniboxResultsTextDimmedSelected] =
      ui::AlphaBlend(text, card, tokens::kDimAlpha);

  // Menus and popovers (section 8): `card`; the highlighted row is solid accent
  // with the on-accent label (the reel's context menu), the separator `line`.
  mixer[ui::kColorMenuBorder] = {line};
  mixer[ui::kColorMenuSeparator] = {line};
  mixer[ui::kColorMenuItemBackgroundSelected] = {accent};
  mixer[ui::kColorMenuItemForegroundSelected] = {on_accent};

  // Find bar (section 8): `glass` bar, `control` field, the count 11 px `dim`.
  mixer[kColorFindBarBackground] = {glass};
  mixer[kColorFindBarForeground] = {text};
  mixer[kColorFindBarMatchCount] =
      ui::AlphaBlend(text, glass, tokens::kDimAlpha);
  mixer[kColorFindBarButtonIcon] = {text};
  mixer[kColorFindBarButtonIconHovered] = {text};

  // Downloads (section 8): the ring is `accent` while a download runs; the list
  // is a card whose hovered row is the selection pill.
  mixer[kColorDownloadToolbarButtonActive] = {accent};
  mixer[kColorDownloadToolbarButtonRingBackground] = {line};
  mixer[kColorDownloadBubbleRowHover] = ui::SetAlpha(accent, selection_alpha);
  mixer[kColorDownloadBubblePrimaryIcon] = {accent};
}

}  // namespace atlas
