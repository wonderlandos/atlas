// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/kit/kit_privacy_popover.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/ui/atlas/kit/kit_popover.h"
#include "chrome/browser/ui/atlas/toolbar/privacy_bubble.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace atlas::kit {

namespace {

using atlas::toolbar::PrivacyRow;

constexpr int kWidth = 300;
constexpr int kInset = 8;

struct RowText {
  int name_id;
  int info_id;
};

RowText TextFor(PrivacyRow row) {
  switch (row) {
    case PrivacyRow::kDoNotTrack:
      return {IDS_ATLAS_TOOLBAR_ROW_DNT, IDS_ATLAS_TOOLBAR_ROW_DNT_INFO};
    case PrivacyRow::kGlobalPrivacyControl:
      return {IDS_ATLAS_TOOLBAR_ROW_GPC, IDS_ATLAS_TOOLBAR_ROW_GPC_INFO};
    case PrivacyRow::kTrackerBlocking:
      return {IDS_ATLAS_TOOLBAR_ROW_TRACKERS,
              IDS_ATLAS_TOOLBAR_ROW_TRACKERS_INFO};
    case PrivacyRow::kFingerprintNoise:
      return {IDS_ATLAS_TOOLBAR_ROW_NOISE, IDS_ATLAS_TOOLBAR_ROW_NOISE_INFO};
  }
}

// A setting: its name and On or Off on one line, under it what it does in the
// dim 11 px style. Not a button: the popover only reports (the person changes
// a setting in Settings).
class SettingRow : public views::BoxLayoutView {
  METADATA_HEADER(SettingRow, views::BoxLayoutView)

 public:
  SettingRow(PrivacyRow row, bool on) {
    const RowText text = TextFor(row);
    SetOrientation(views::BoxLayout::Orientation::kVertical);
    SetBetweenChildSpacing(2);
    SetInsideBorderInsets(gfx::Insets::VH(4, kInset));

    auto* head = AddChildView(std::make_unique<views::BoxLayoutView>());
    head->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
    auto* name = head->AddChildView(std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(text.name_id), views::style::CONTEXT_LABEL,
        views::style::STYLE_PRIMARY));
    name->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    head->SetFlexForView(name, 1);
    head->AddChildView(std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(on ? IDS_ATLAS_TOOLBAR_VALUE_ON
                                     : IDS_ATLAS_TOOLBAR_VALUE_OFF),
        views::style::CONTEXT_LABEL,
        on ? views::style::STYLE_PRIMARY : views::style::STYLE_SECONDARY));

    auto* info = AddChildView(std::make_unique<views::Label>(
        l10n_util::GetStringUTF16(text.info_id), views::style::CONTEXT_LABEL,
        views::style::STYLE_SECONDARY));
    info->SetFontList(info->font_list().DeriveWithSizeDelta(-2));
    info->SetMultiLine(true);
    info->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
    GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(text.name_id) + u", " +
        l10n_util::GetStringUTF16(on ? IDS_ATLAS_TOOLBAR_VALUE_ON
                                     : IDS_ATLAS_TOOLBAR_VALUE_OFF));
    // The width of the popover; the height follows the text. (Not an override
    // of CalculatePreferredSize: the box layout asks the host's height for
    // width, which would ask this again.)
    SetPreferredSize(gfx::Size(kWidth, GetHeightForWidth(kWidth)));
  }
};

BEGIN_METADATA(SettingRow)
END_METADATA

}  // namespace

views::Widget* ShowKitPrivacyPopover(views::View* anchor,
                                     const atlas::toolbar::PrivacyState& state,
                                     base::RepeatingClosure open_settings) {
  auto popover =
      std::make_unique<KitPopover>(anchor, views::BubbleBorder::TOP_RIGHT);
  popover->AddHeader(l10n_util::GetStringUTF16(IDS_ATLAS_TOOLBAR_PRIVACY_TITLE));
  for (PrivacyRow row : atlas::toolbar::PrivacyRows()) {
    popover->AddCustom(
        std::make_unique<SettingRow>(row, atlas::toolbar::IsRowOn(state, row)));
  }
  popover->AddSeparator();
  KitPopover::ItemSpec link;
  link.label = l10n_util::GetStringUTF16(IDS_ATLAS_TOOLBAR_PRIVACY_SETTINGS_LINK);
  popover->AddItem(std::move(link),
                   base::BindOnce([](base::RepeatingClosure open) { open.Run(); },
                                  std::move(open_settings)));
  return KitPopover::Show(std::move(popover));
}

void InstallKitPrivacyPresenter() {
  atlas::toolbar::PrivacyBubbleController::SetPresenter(
      base::BindRepeating(&ShowKitPrivacyPopover));
}

}  // namespace atlas::kit
