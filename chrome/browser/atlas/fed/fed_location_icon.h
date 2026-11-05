// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ATLAS_FED_FED_LOCATION_ICON_H_
#define CHROME_BROWSER_ATLAS_FED_FED_LOCATION_ICON_H_

class GURL;

namespace content {
class WebContents;
}

namespace gfx {
struct VectorIcon;
}

// The location bar's icon on a fed:// page, in place of the padlock: the
// Federate mark when the page arrived verified (the helper answered 200), the
// dangerous icon when a check failed (502: nothing of the page was shown), and
// the engine's own icon otherwise (not found, unreachable, still loading).
namespace atlas_fed {

// The icon for a page whose committed load answered `http_status` (0: none
// yet), or null for the engine's own choice. `url` is the visible URL.
const gfx::VectorIcon* LocationIconFor(const GURL& url, int http_status);

// The same for the visible entry of `web_contents` (null: the engine's icon).
const gfx::VectorIcon* LocationIconFor(const GURL& url,
                                       content::WebContents* web_contents);

}  // namespace atlas_fed

#endif  // CHROME_BROWSER_ATLAS_FED_FED_LOCATION_ICON_H_
