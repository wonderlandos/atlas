// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/atlas_pages/common/atlas_theme_css.h"

#include "base/strings/string_util.h"
#include "components/atlas_pages/grit/atlas_pages_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace atlas_pages {

namespace {

class BuiltinThemeCssProvider : public ThemeCssProvider {
 public:
  std::string GetThemeCss(
      const ui::ColorProvider* color_provider) const override {
    return ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
        IDR_ATLAS_PAGES_COMMON_THEME_CSS);
  }
};

}  // namespace

std::string ThemeTokenVariable(std::string_view token) {
  std::string out = "--";
  for (char c : token) {
    if (base::IsAsciiUpper(c)) {
      out.push_back('-');
      out.push_back(base::ToLowerASCII(c));
    } else {
      out.push_back(c);
    }
  }
  return out;
}

std::unique_ptr<ThemeCssProvider> CreateBuiltinThemeCssProvider() {
  return std::make_unique<BuiltinThemeCssProvider>();
}

}  // namespace atlas_pages
