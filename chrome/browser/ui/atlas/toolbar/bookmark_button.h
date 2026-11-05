// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_TOOLBAR_BOOKMARK_BUTTON_H_
#define CHROME_BROWSER_UI_ATLAS_TOOLBAR_BOOKMARK_BUTTON_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper_observer.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "components/prefs/pref_change_registrar.h"

class BrowserWindowInterface;

namespace atlas::toolbar {

// The bookmark button of the toolbar: an outline star that fills (in the accent colour) when the
// page in the current tab is bookmarked. Pressing it runs the same command as the omnibox star
// (Ctrl D); the tooltip and accessible name say "Bookmark this tab" or "Bookmarked".
class BookmarkButton : public ToolbarButton, public BookmarkTabHelperObserver {
  METADATA_HEADER(BookmarkButton, ToolbarButton)

 public:
  // `browser` must outlive the button.
  explicit BookmarkButton(BrowserWindowInterface* browser);
  BookmarkButton(const BookmarkButton&) = delete;
  BookmarkButton& operator=(const BookmarkButton&) = delete;
  ~BookmarkButton() override;

  bool is_starred() const { return starred_; }

  // ToolbarButton:
  SkColor GetForegroundColor(ButtonState state) const override;

  // BookmarkTabHelperObserver:
  void URLStarredChanged(content::WebContents* web_contents,
                         bool starred) override;

 private:
  void ActiveTabChanged();
  void SetStarred(bool starred);
  void UpdateVisibility();

  const raw_ptr<BrowserWindowInterface> browser_;
  bool starred_ = false;
  PrefChangeRegistrar pref_registrar_;
  base::CallbackListSubscription active_tab_subscription_;
  base::ScopedObservation<BookmarkTabHelper, BookmarkTabHelperObserver>
      tab_helper_observation_{this};
};

}  // namespace atlas::toolbar

#endif  // CHROME_BROWSER_UI_ATLAS_TOOLBAR_BOOKMARK_BUTTON_H_
