// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/toolbar/privacy_bubble.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"

namespace atlas::toolbar {

namespace {

PrivacyBubbleController::Presenter& PresenterSlot() {
  static base::NoDestructor<PrivacyBubbleController::Presenter> slot;
  return *slot;
}

// Spacing steps of the design (space-2 = 8, space-3 = 12); not new values.
constexpr int kSpace2 = 8;
constexpr int kSpace3 = 12;

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

std::unique_ptr<views::View> BuildRow(PrivacyRow row, bool on) {
  const RowText text = TextFor(row);

  auto name = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(text.name_id), views::style::CONTEXT_LABEL,
      views::style::STYLE_PRIMARY);
  name->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  auto value = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(on ? IDS_ATLAS_TOOLBAR_VALUE_ON
                                   : IDS_ATLAS_TOOLBAR_VALUE_OFF),
      views::style::CONTEXT_LABEL,
      on ? views::style::STYLE_PRIMARY : views::style::STYLE_SECONDARY);

  auto head = std::make_unique<views::BoxLayoutView>();
  head->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  views::View* name_view = head->AddChildView(std::move(name));
  head->AddChildView(std::move(value));
  head->SetFlexForView(name_view, 1);

  auto info = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(text.info_id), views::style::CONTEXT_LABEL,
      views::style::STYLE_SECONDARY);
  info->SetMultiLine(true);
  info->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  auto box = std::make_unique<views::BoxLayoutView>();
  box->SetOrientation(views::BoxLayout::Orientation::kVertical);
  box->SetBetweenChildSpacing(kSpace2 / 2);
  box->AddChildView(std::move(head));
  box->AddChildView(std::move(info));
  return box;
}

}  // namespace

std::unique_ptr<views::View> BuildPrivacyContents(
    const PrivacyState& state,
    base::RepeatingClosure open_settings) {
  auto contents = std::make_unique<views::BoxLayoutView>();
  contents->SetOrientation(views::BoxLayout::Orientation::kVertical);
  contents->SetBetweenChildSpacing(kSpace3);
  for (PrivacyRow row : PrivacyRows()) {
    contents->AddChildView(BuildRow(row, IsRowOn(state, row)));
  }

  auto link = std::make_unique<views::Link>(
      l10n_util::GetStringUTF16(IDS_ATLAS_TOOLBAR_PRIVACY_SETTINGS_LINK));
  link->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  link->SetCallback(std::move(open_settings));
  contents->AddChildView(std::move(link));

  // The width of a bubble; the height follows the text.
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
                        views::DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                    ChromeLayoutProvider::Get()
                            ->GetInsetsMetric(views::INSETS_DIALOG)
                            .width();
  contents->SetPreferredSize(
      gfx::Size(width, contents->GetHeightForWidth(width)));
  return contents;
}

PrivacyBubbleController::PrivacyBubbleController() = default;
PrivacyBubbleController::~PrivacyBubbleController() {
  Close();
}

// static
void PrivacyBubbleController::SetPresenter(Presenter presenter) {
  PresenterSlot() = std::move(presenter);
}

bool PrivacyBubbleController::IsShowing() const {
  return widget_ && !widget_->IsClosed();
}

void PrivacyBubbleController::Close() {
  if (IsShowing()) {
    widget_->Close();
  }
  widget_.reset();
}

void PrivacyBubbleController::Toggle(views::View* anchor,
                                     const PrivacyState& state,
                                     base::RepeatingClosure open_settings) {
  if (IsShowing()) {
    Close();
    return;
  }

  // Following the link closes the popover first, then opens Settings.
  auto follow = base::BindRepeating(
      [](base::WeakPtr<PrivacyBubbleController> controller,
         base::RepeatingClosure open) {
        if (controller) {
          controller->Close();
        }
        open.Run();
      },
      weak_factory_.GetWeakPtr(), std::move(open_settings));

  if (const Presenter& present = PresenterSlot()) {
    views::Widget* shown = present.Run(anchor, state, follow);
    widget_ = shown ? shown->GetWeakPtr() : nullptr;
    return;
  }

  auto model =
      ui::DialogModel::Builder()
          .SetInternalName("AtlasPrivacyBubble")
          .SetTitle(l10n_util::GetStringUTF16(IDS_ATLAS_TOOLBAR_PRIVACY_TITLE))
          .AddCustomField(
              std::make_unique<views::BubbleDialogModelHost::CustomView>(
                  BuildPrivacyContents(state, std::move(follow)),
                  views::BubbleDialogModelHost::FieldType::kText))
          .Build();

  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      std::move(model), anchor, views::BubbleBorder::TOP_RIGHT);
  views::Widget* widget = views::BubbleDialogDelegate::CreateBubbleDeprecated(
      std::move(bubble), views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
  widget_ = widget->GetWeakPtr();
  widget->Show();
}

}  // namespace atlas::toolbar
