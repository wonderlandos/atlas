// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_KIT_KIT_TOOLBAR_VIEW_H_
#define CHROME_BROWSER_UI_ATLAS_KIT_KIT_TOOLBAR_VIEW_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/command_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class BrowserWindowInterface;

namespace atlas::kit {

class KitIconButton;

// The Kit toolbar (tokens.md section 2; reel scene R02): 36 px of `glass` with
// a 1 px `line` below; left to right the sidebar toggle, back, forward and
// reload (Kit buttons of this directory), the omnibox centred in the window
// (the engine's, re-hosted: 26 px high, at most 560 px wide), then the actions
// (privacy, bookmark, downloads, extensions, menu: the engine's or the Atlas
// toolbar area's buttons, re-hosted). It owns no browser state: the buttons
// run the browser's commands and follow their enabled state.
class KitToolbarView : public views::View, public CommandObserver {
  METADATA_HEADER(KitToolbarView, views::View)

 public:
  explicit KitToolbarView(BrowserWindowInterface* browser);
  KitToolbarView(const KitToolbarView&) = delete;
  KitToolbarView& operator=(const KitToolbarView&) = delete;
  ~KitToolbarView() override;

  // Takes `omnibox` and `actions` from wherever they are (their parent is the
  // engine's toolbar) and lays them out from now on.
  void Adopt(views::View* omnibox, const std::vector<views::View*>& actions);

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  gfx::Size GetMinimumSize() const override;
  void Layout(PassKey) override;
  void OnPaintBackground(gfx::Canvas* canvas) override;

  // CommandObserver:
  void EnabledStateChangedForCommand(int id, bool enabled) override;

 private:
  void Run(int command_id);
  void ToggleSidebar();

  const raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<KitIconButton> sidebar_ = nullptr;
  raw_ptr<KitIconButton> back_ = nullptr;
  raw_ptr<KitIconButton> forward_ = nullptr;
  raw_ptr<KitIconButton> reload_ = nullptr;
  raw_ptr<views::View> omnibox_ = nullptr;
  std::vector<raw_ptr<views::View>> actions_;
};

}  // namespace atlas::kit

#endif  // CHROME_BROWSER_UI_ATLAS_KIT_KIT_TOOLBAR_VIEW_H_
