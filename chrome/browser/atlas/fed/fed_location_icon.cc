// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/atlas/fed/fed_location_icon.h"

#include "chrome/browser/atlas/fed/fed_url.h"
#include "chrome/browser/atlas/fed/vector_icons/vector_icons.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/vector_icon_types.h"
#include "url/gurl.h"

namespace atlas_fed {

const gfx::VectorIcon* LocationIconFor(const GURL& url, int http_status) {
  if (!url.SchemeIs(kFedScheme)) {
    return nullptr;
  }
  if (http_status == 200) {
    return &kFedMarkIcon;
  }
  if (http_status == 502) {
    return &vector_icons::kDangerousFilledIcon;
  }
  return nullptr;
}

const gfx::VectorIcon* LocationIconFor(const GURL& url,
                                       content::WebContents* web_contents) {
  if (!url.SchemeIs(kFedScheme) || !web_contents) {
    return nullptr;
  }
  content::NavigationEntry* entry =
      web_contents->GetController().GetVisibleEntry();
  if (!entry || entry->GetURL() != url) {
    return nullptr;
  }
  return LocationIconFor(url, entry->GetHttpStatusCode());
}

}  // namespace atlas_fed
