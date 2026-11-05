// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_KIT_KIT_TEXT_H_
#define CHROME_BROWSER_UI_ATLAS_KIT_KIT_TEXT_H_

#include <string>

namespace atlas::kit {

// The label as the Kit shows it: the inherited project's name never reaches
// the screen (Wonderland Doctrine), whatever string the engine passes. "Helium",
// "Chromium" and "Chrome" (whole words) become "Atlas".
std::u16string DoctrineClean(const std::u16string& label);

// `text` broken at spaces into lines of at most `max_chars` characters (a word
// longer than that stays whole on a line of its own), joined with newlines.
// The rich tooltip of the Design System keeps to 40 characters a line
// (rich-tooltip.toml), so long tab titles and file names do not stretch a
// tooltip across the screen.
std::u16string WrapToLineChars(const std::u16string& text, size_t max_chars);

}  // namespace atlas::kit

#endif  // CHROME_BROWSER_UI_ATLAS_KIT_KIT_TEXT_H_
