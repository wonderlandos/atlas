// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_BRAND_ATLAS_ABOUT_H_
#define CHROME_BROWSER_UI_ATLAS_BRAND_ATLAS_ABOUT_H_

#include <string>
#include <string_view>

namespace content {
class WebUIDataSource;
}

namespace atlas::brand {

// The line of Settings > About that names the version: Atlas, the WolOS release
// when there is one, and the engine. `channel` and `bits` are the already
// localized "Official Build" and "(64-bit)".
std::u16string AnnotatedVersion(std::string_view atlas_version,
                                std::string_view engine_version,
                                const std::u16string& channel,
                                const std::u16string& bits);

// The sentence of Settings > About that links to the credits page.
std::u16string LicenseSentence(const std::u16string& credits_url);

// The values the version page (atlas://version) shows for the WolOS row and the
// labels of its rows.
void AddVersionPageStrings(content::WebUIDataSource* html_source);

// Opens the WolOS Software Update pane: runs `wol-settings "Software Update"`,
// which is what the shell does for its own update badge. Does nothing when the
// program is not installed.
void OpenSoftwareUpdate();

}  // namespace atlas::brand

#endif  // CHROME_BROWSER_UI_ATLAS_BRAND_ATLAS_ABOUT_H_
