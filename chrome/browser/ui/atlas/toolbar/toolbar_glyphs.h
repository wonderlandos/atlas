// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_TOOLBAR_TOOLBAR_GLYPHS_H_
#define CHROME_BROWSER_UI_ATLAS_TOOLBAR_TOOLBAR_GLYPHS_H_

namespace gfx {
struct VectorIcon;
}

namespace atlas::toolbar {

// The WolOS glyphs of the toolbar buttons the engine draws itself (reel scene 3, overlay patch 0096):
// open-menu for the app menu button, folder-download for the downloads button (the progress ring
// is drawn around it as before). A function, so engine files need not include a generated header.
const gfx::VectorIcon& AppMenuIcon();
const gfx::VectorIcon& DownloadsIcon();

}  // namespace atlas::toolbar

#endif  // CHROME_BROWSER_UI_ATLAS_TOOLBAR_TOOLBAR_GLYPHS_H_
