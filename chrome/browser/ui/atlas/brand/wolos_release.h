// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_BRAND_WOLOS_RELEASE_H_
#define CHROME_BROWSER_UI_ATLAS_BRAND_WOLOS_RELEASE_H_

#include <string>
#include <string_view>

namespace atlas::brand {

// The WolOS release (VERSION_ID) when `os_release`, the text of an os-release
// file, says that this is WolOS (ID=wolos); an empty string for any other
// system or an unreadable file.
std::string ParseWolosRelease(std::string_view os_release);

// ParseWolosRelease() of /etc/os-release (or /usr/lib/os-release), read once.
const std::string& GetWolosRelease();

}  // namespace atlas::brand

#endif  // CHROME_BROWSER_UI_ATLAS_BRAND_WOLOS_RELEASE_H_
