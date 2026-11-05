// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATLAS_PAGES_NEWTAB_NEWTAB_CATEGORIES_H_
#define COMPONENTS_ATLAS_PAGES_NEWTAB_NEWTAB_CATEGORIES_H_

#include <string_view>

class GURL;

namespace atlas::newtab {

// The category of a pinned page, one of the design palette's icon categories
// ("icon.categories"): "web", "talk", "files", "media", "store", "system". It
// picks the colour of the tile behind the card (categories.css). The category
// comes from the words in the host name, so a page of a kind not listed is
// "web". Never returns an empty string.
std::string_view CategoryForUrl(const GURL& url);

// The same for a bare host name (lower case, no port).
std::string_view CategoryForHost(std::string_view host);

}  // namespace atlas::newtab

#endif  // COMPONENTS_ATLAS_PAGES_NEWTAB_NEWTAB_CATEGORIES_H_
