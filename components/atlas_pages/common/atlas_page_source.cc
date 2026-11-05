// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/atlas_pages/common/atlas_page_source.h"

#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "components/atlas_pages/common/atlas_theme_css.h"
#include "components/atlas_pages/grit/atlas_pages_resources.h"
#include "components/atlas_pages/grit/atlas_pages_resources_map.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/webui/resource_path.h"

namespace atlas_pages {

namespace {

constexpr char kThemeCssPath[] = "theme.css";
constexpr char kCommonDir[] = "common/";

struct ThemeState {
  std::unique_ptr<ThemeCssProvider> provider;
  AtlasPageSource::ColorProviderGetter color_provider;
};

bool IsThemeRequest(const std::string& path) {
  return path == kThemeCssPath;
}

void ServeTheme(const ThemeState* state,
                const std::string& path,
                content::WebUIDataSource::GotDataCallback callback) {
  const ui::ColorProvider* colors =
      state->color_provider ? state->color_provider.Run() : nullptr;
  std::move(callback).Run(base::MakeRefCounted<base::RefCountedString>(
      state->provider->GetThemeCss(colors)));
}

}  // namespace

// static
content::WebUIDataSource* AtlasPageSource::CreateAndAdd(
    content::BrowserContext* browser_context,
    std::string_view host,
    std::string_view page_dir,
    std::unique_ptr<ThemeCssProvider> theme,
    ColorProviderGetter color_provider) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      browser_context, std::string(host));

  const std::string prefix = base::StrCat({page_dir, "/"});
  std::vector<webui::ResourcePath> paths;
  int default_resource = -1;
  for (const webui::ResourcePath& entry : base::span(kAtlasPagesResources)) {
    std::string_view path(entry.path);
    if (base::StartsWith(path, prefix)) {
      path.remove_prefix(prefix.size());
      if (path == "index.html") {
        default_resource = entry.id;
      }
    } else if (base::StartsWith(path, kCommonDir)) {
      path.remove_prefix(sizeof(kCommonDir) - 1);
    } else {
      continue;
    }
    // `path` points into the static map, so it outlives the source.
    paths.push_back({path.data(), entry.id});
  }
  source->AddResourcePaths(paths);
  if (default_resource != -1) {
    source->SetDefaultResource(default_resource);
  }

  auto state = std::make_unique<ThemeState>();
  state->provider = theme ? std::move(theme) : CreateBuiltinThemeCssProvider();
  state->color_provider = std::move(color_provider);
  source->SetRequestFilter(
      base::BindRepeating(&IsThemeRequest),
      base::BindRepeating(&ServeTheme, base::Owned(state.release())));

  ApplyContentSecurityPolicy(source);
  return source;
}

// static
void AtlasPageSource::ApplyContentSecurityPolicy(
    content::WebUIDataSource* source) {
  using network::mojom::CSPDirectiveName;
  static constexpr std::pair<CSPDirectiveName, const char*> kPolicy[] = {
      {CSPDirectiveName::DefaultSrc, "default-src 'none';"},
      {CSPDirectiveName::ScriptSrc, "script-src 'self';"},
      {CSPDirectiveName::StyleSrc, "style-src 'self';"},
      {CSPDirectiveName::ImgSrc, "img-src 'self' data:;"},
      {CSPDirectiveName::FontSrc, "font-src 'none';"},
      {CSPDirectiveName::MediaSrc, "media-src 'none';"},
      {CSPDirectiveName::ConnectSrc, "connect-src 'none';"},
      {CSPDirectiveName::ChildSrc, "child-src 'none';"},
      {CSPDirectiveName::FrameSrc, "frame-src 'none';"},
      {CSPDirectiveName::WorkerSrc, "worker-src 'none';"},
      {CSPDirectiveName::ObjectSrc, "object-src 'none';"},
      {CSPDirectiveName::BaseURI, "base-uri 'none';"},
      {CSPDirectiveName::FormAction, "form-action 'none';"},
      {CSPDirectiveName::FrameAncestors, "frame-ancestors 'none';"},
      {CSPDirectiveName::RequireTrustedTypesFor,
       "require-trusted-types-for 'script';"},
      {CSPDirectiveName::TrustedTypes, "trusted-types 'none';"},
  };
  for (const auto& [directive, value] : kPolicy) {
    source->OverrideContentSecurityPolicy(directive, value);
  }
}

}  // namespace atlas_pages
