// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_KIT_KIT_ICON_BUTTON_H_
#define CHROME_BROWSER_UI_ATLAS_KIT_KIT_ICON_BUTTON_H_

#include <string>

#include "base/memory/raw_ref.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

namespace gfx {
struct VectorIcon;
}

namespace atlas::kit {

// The Kit's icon button (tokens.md section 2): a 16 px symbolic glyph in a
// 28 x 28 hit area, radius 8; hover paints `control`, pressed the accent tint;
// the glyph is `text`, and `dim` while the button is disabled. The tooltip is
// also the accessible name.
class KitIconButton : public views::Button {
  METADATA_HEADER(KitIconButton, views::Button)

 public:
  KitIconButton(PressedCallback callback,
                const gfx::VectorIcon& icon,
                const std::u16string& tooltip);
  KitIconButton(const KitIconButton&) = delete;
  KitIconButton& operator=(const KitIconButton&) = delete;
  ~KitIconButton() override;

  void SetIcon(const gfx::VectorIcon& icon);

  // views::Button:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void PaintButtonContents(gfx::Canvas* canvas) override;
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void StateChanged(ButtonState old_state) override;

 private:
  raw_ref<const gfx::VectorIcon> icon_;
};

}  // namespace atlas::kit

#endif  // CHROME_BROWSER_UI_ATLAS_KIT_KIT_ICON_BUTTON_H_
