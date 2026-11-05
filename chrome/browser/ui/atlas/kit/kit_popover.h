// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_KIT_KIT_POPOVER_H_
#define CHROME_BROWSER_UI_ATLAS_KIT_KIT_POPOVER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"

#include "base/functional/callback.h"
#include "ui/base/models/image_model.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace gfx {
struct VectorIcon;
}

namespace atlas::kit {

// The Kit's popover and menu (tokens.md section 8; reel scene R07): a `card`
// with radius 12 and a 1 px `line` edge under the toolbar control it opens
// from. It holds section headers (11 px, 600, small caps, `dim`), rows of 28 px
// (label, an optional trailing hint in `dim`, an optional 16 px glyph) and
// separators. The row under the pointer or the keyboard is the solid accent
// with the on-accent label. Up and Down move between rows, Enter and Space run
// one, Esc closes. Running a row closes the popover first.
class KitPopover : public views::BubbleDialogDelegate {
 public:
  // `anchor` may be null when the caller sets an anchor rect (SetAnchorRect).
  KitPopover(views::View* anchor, views::BubbleBorder::Arrow arrow);
  KitPopover(const KitPopover&) = delete;
  KitPopover& operator=(const KitPopover&) = delete;
  ~KitPopover() override;

  // One row. `label` may carry the engine's mnemonic ampersand; it is not
  // drawn. `icon` is drawn at the start in the label colour (a vector icon is
  // recoloured, an image is drawn as it is); `end_glyph` at the end (a check
  // mark or the arrow of a submenu); `trailing` is a hint such as a shortcut.
  struct ItemSpec {
    std::u16string label;
    std::u16string trailing;
    ui::ImageModel icon;
    raw_ptr<const gfx::VectorIcon> end_glyph = nullptr;
    bool enabled = true;
  };

  void AddHeader(const std::u16string& text);
  views::View* AddItem(ItemSpec spec, base::OnceClosure action);
  void AddSeparator();
  views::View* AddCustom(std::unique_ptr<views::View> view);

  // Shows `popover` (owned by its widget) and focuses the first enabled row.
  static views::Widget* Show(std::unique_ptr<KitPopover> popover);

  // Runs `action` after the popover's widget is closed.
  void CloseAndRun(base::OnceClosure action);

 private:
  raw_ptr<views::View> contents_ = nullptr;
};

}  // namespace atlas::kit

#endif  // CHROME_BROWSER_UI_ATLAS_KIT_KIT_POPOVER_H_
