// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_KIT_KIT_DOWNLOADS_H_
#define CHROME_BROWSER_UI_ATLAS_KIT_KIT_DOWNLOADS_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/download/download_ui_model.h"
#include "ui/gfx/geometry/rect.h"

namespace views {
class View;
class Widget;
}  // namespace views

namespace atlas::kit {

// Shows the row menu of a download at `screen_rect` of `widget` and returns
// whatever must live until the menu is done (the engine's context menu object,
// which is the delegate of the menu model).
using DownloadRowMenu = base::RepeatingCallback<std::shared_ptr<void>(
    base::WeakPtr<DownloadUIModel>,
    views::Widget* widget,
    const gfx::Rect& screen_rect)>;

// The downloads list of the reel (scene 7, tokens.md section 8): a card headed
// DOWNLOADS in small capitals, rows of 52 px with the document glyph in the
// accent colour, the file name in bold, one dim line (size and state, or the
// progress), the hovered row as the accent pill, no close button and no footer.
// Click on a finished download opens it, right click shows the row menu
// (`menu`), `close` closes the bubble after a download is opened.
//
// Returns null, and leaves `models` alone, when the list needs the engine's
// views: a download with a warning (dangerous, insecure, being scanned) has a
// security page that only the engine's bubble has. Otherwise it takes the
// models.
std::unique_ptr<views::View> MakeKitDownloadsView(
    std::vector<DownloadUIModel::DownloadUIModelPtr>& models,
    DownloadRowMenu menu,
    base::RepeatingClosure close);

// Whether one download needs the engine's security page (see above).
bool NeedsSecurityPage(const DownloadUIModel& model);

}  // namespace atlas::kit

#endif  // CHROME_BROWSER_UI_ATLAS_KIT_KIT_DOWNLOADS_H_
