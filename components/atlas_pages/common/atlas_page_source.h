// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATLAS_PAGES_COMMON_ATLAS_PAGE_SOURCE_H_
#define COMPONENTS_ATLAS_PAGES_COMMON_ATLAS_PAGE_SOURCE_H_

#include <memory>
#include <string_view>

#include "base/functional/callback.h"
#include "components/atlas_pages/common/atlas_theme_css.h"

namespace content {
class BrowserContext;
class WebUIDataSource;
}  // namespace content

namespace ui {
class ColorProvider;
}

namespace atlas_pages {

// The data source of an Atlas page: chrome://<host>/ serves the files of
// components/atlas_pages/<page_dir>/ (listed in <page_dir>_resources.grdp with
// resource_path "<page_dir>/<file>"), with <page_dir>/index.html as the
// default; the shared files of common/ under their own name
// (atlas_host_bridge.js); and theme.css, written by the theme CSS provider at
// each request so a theme change reaches the page on its next load.
class AtlasPageSource {
 public:
  // The policy of every Atlas page: the one of the first-run page's meta tag.
  // Nothing leaves the page: no connections, frames, workers or objects.
  static constexpr char kContentSecurityPolicy[] =
      "default-src 'none'; script-src 'self'; style-src 'self'; "
      "img-src 'self' data:; base-uri 'none'; form-action 'none'; "
      "require-trusted-types-for 'script'; trusted-types 'none'";

  // Returns the live colour provider for theme.css, or null.
  using ColorProviderGetter =
      base::RepeatingCallback<const ui::ColorProvider*()>;

  // Creates and adds the source. `theme` null means the built-in provider;
  // `color_provider` may be null. Returns the source so the page's controller
  // can add its strings (AddLocalizedString) before the first request.
  static content::WebUIDataSource* CreateAndAdd(
      content::BrowserContext* browser_context,
      std::string_view host,
      std::string_view page_dir,
      std::unique_ptr<ThemeCssProvider> theme = nullptr,
      ColorProviderGetter color_provider = ColorProviderGetter());

  // Applies kContentSecurityPolicy to `source`, one directive at a time.
  static void ApplyContentSecurityPolicy(content::WebUIDataSource* source);
};

}  // namespace atlas_pages

#endif  // COMPONENTS_ATLAS_PAGES_COMMON_ATLAS_PAGE_SOURCE_H_
