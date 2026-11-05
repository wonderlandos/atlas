// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATLAS_PAGES_COMMON_ATLAS_THEME_CSS_H_
#define COMPONENTS_ATLAS_PAGES_COMMON_ATLAS_THEME_CSS_H_

#include <array>
#include <memory>
#include <string>
#include <string_view>

namespace ui {
class ColorProvider;
}

namespace atlas_pages {

// The 13 theme tokens of WolOS, in the order of the theme files. A page uses
// them as CSS variables in kebab case: "accentText" is --accent-text.
inline constexpr std::array<std::string_view, 13> kThemeTokens = {
    "surface", "sidebar", "card",    "line",    "text",
    "dim",     "accent",  "accentText", "glass", "control",
    "success", "warning", "danger"};

// "accentText" -> "--accent-text".
std::string ThemeTokenVariable(std::string_view token);

// Writes the theme.css every Atlas page links: a :root block that sets the
// 13 token variables (and the fixed scale: radius, spacing, type, motion).
class ThemeCssProvider {
 public:
  virtual ~ThemeCssProvider() = default;

  // The CSS for the colours of `color_provider`, the live provider of the
  // window that shows the page. Null when the page has no window yet; then the
  // built-in light and dark blocks are returned.
  virtual std::string GetThemeCss(
      const ui::ColorProvider* color_provider) const = 0;
};

// Round 1: returns the built-in light and dark blocks (the bundled theme.css,
// generated from the two built-in WolOS themes, prefers-color-scheme picks
// one) whatever the provider. The live mapping from ui::ColorProvider
// replaces it without a change to the pages.
std::unique_ptr<ThemeCssProvider> CreateBuiltinThemeCssProvider();

}  // namespace atlas_pages

#endif  // COMPONENTS_ATLAS_PAGES_COMMON_ATLAS_THEME_CSS_H_
