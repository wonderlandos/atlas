// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_TOOLBAR_TOOLBAR_IDS_H_
#define CHROME_BROWSER_UI_ATLAS_TOOLBAR_TOOLBAR_IDS_H_

#include "ui/base/interaction/element_identifier.h"

namespace atlas::toolbar {

// Element identifiers of the Atlas toolbar controls (tests and anchors find the views by them).
// The bookmark button carries the engine's kBookmarkStarViewElementId, so the bookmark bubble
// highlights it as it does the omnibox star.
DECLARE_ELEMENT_IDENTIFIER_VALUE(kPrivacyButtonElementId);

}  // namespace atlas::toolbar

#endif  // CHROME_BROWSER_UI_ATLAS_TOOLBAR_TOOLBAR_IDS_H_
