// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_TOOLBAR_TOOLBAR_BUTTONS_H_
#define CHROME_BROWSER_UI_ATLAS_TOOLBAR_TOOLBAR_BUTTONS_H_

class BrowserWindowInterface;
class PrefService;

namespace views {
class View;
}  // namespace views

namespace atlas::toolbar {

// Adds the privacy button and the bookmark button to `toolbar`, in that order, as its next two
// children (so a caller adds them right after the omnibox and before the Downloads button). Each
// button shows or hides itself as its pref says (toolbar_prefs.h). The hook in ToolbarView::Init()
// calls this once.
void AddToolbarButtons(views::View* toolbar, BrowserWindowInterface* browser);

// The bookmark button among the children of `toolbar` while it is visible, or null. The bookmark
// bubble anchors to it.
views::View* FindVisibleBookmarkButton(views::View* toolbar);

// Whether the bookmark control is the toolbar button, so the omnibox does not draw its own star.
// False when `prefs` does not know the pref.
bool BookmarkButtonReplacesStar(const PrefService* prefs);

}  // namespace atlas::toolbar

#endif  // CHROME_BROWSER_UI_ATLAS_TOOLBAR_TOOLBAR_BUTTONS_H_
