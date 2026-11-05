// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/kit/kit_downloads.h"

#include <algorithm>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/timer/timer.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/download/download_commands.h"
#include "chrome/browser/ui/atlas/chrome/atlas_tokens.h"
#include "chrome/browser/ui/atlas/kit/kit_layout_provider.h"
#include "chrome/browser/ui/atlas/kit/vector_icons/vector_icons.h"
#include "chrome/browser/ui/atlas/sidebar/row_style.h"
#include "chrome/grit/generated_resources.h"
#include "components/download/public/common/download_danger_type.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace atlas::kit {

namespace {

// The card is the side sheet's width (side-sheet.toml): 336.
const int kWidth = DownloadsCardWidth();
constexpr int kPad = 8;
constexpr int kRowHeight = 52;
constexpr int kHeaderHeight = 32;
constexpr int kGlyph = 16;
constexpr int kMaxRows = 8;
constexpr int kProgressHeight = 3;

gfx::FontList FontOfSize(int size, gfx::Font::Weight weight) {
  const gfx::FontList base;
  return base.DeriveWithSizeDelta(size - base.GetFontSize())
      .DeriveWithWeight(weight);
}

class DownloadRow : public views::Button, public views::ContextMenuController {
  METADATA_HEADER(DownloadRow, views::Button)

 public:
  DownloadRow(DownloadUIModel* model,
              DownloadRowMenu menu,
              base::RepeatingClosure close)
      : views::Button(base::BindRepeating(&DownloadRow::Open,
                                          base::Unretained(this))),
        model_(model),
        menu_(std::move(menu)),
        close_(std::move(close)) {
    set_context_menu_controller(this);
    SetFocusBehavior(FocusBehavior::ALWAYS);
    views::FocusRing::Install(this);
    GetViewAccessibility().SetRole(ax::mojom::Role::kListItem);
    Refresh();
  }
  DownloadRow(const DownloadRow&) = delete;
  DownloadRow& operator=(const DownloadRow&) = delete;
  ~DownloadRow() override = default;

  void Refresh() {
    name_ = model_->GetFileNameToReportUser().LossyDisplayName();
    status_ = model_->GetStatusTextForLabel(
        FontOfSize(12, gfx::Font::Weight::NORMAL), kWidth - 2 * kPad - 56);
    percent_ = model_->GetState() == download::DownloadItem::IN_PROGRESS
                   ? model_->PercentComplete()
                   : -1;
    GetViewAccessibility().SetName(name_ + u", " + status_);
    SetTooltipText(name_);
    SchedulePaint();
  }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(kWidth - 2 * kPad, kRowHeight);
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    const ui::ColorProvider* provider = GetColorProvider();
    if (!provider) {
      return;
    }
    const bool hot = GetState() == STATE_HOVERED ||
                     GetState() == STATE_PRESSED || HasFocus();
    if (hot) {
      cc::PaintFlags flags;
      flags.setAntiAlias(true);
      flags.setColor(sidebar::RowFillColor(*provider, sidebar::RowState::kActive,
                                           1.0));
      canvas->DrawRoundRect(gfx::RectF(GetLocalBounds()),
                            tokens::kRadiusControl, flags);
    }
    canvas->DrawImageInt(
        gfx::CreateVectorIcon(kKitTextXGenericIcon, kGlyph,
                              provider->GetColor(ui::kColorAccent)),
        12, (height() - kGlyph) / 2);
    const int x = 12 + kGlyph + 12;
    const int w = std::max(0, width() - x - 12);
    canvas->DrawStringRectWithFlags(
        name_, FontOfSize(14, gfx::Font::Weight::SEMIBOLD),
        sidebar::RowTitleColor(*provider), gfx::Rect(x, 7, w, 20),
        gfx::Canvas::TEXT_ALIGN_LEFT | gfx::Canvas::NO_SUBPIXEL_RENDERING);
    canvas->DrawStringRectWithFlags(
        status_, FontOfSize(12, gfx::Font::Weight::NORMAL),
        sidebar::DimTextColor(*provider), gfx::Rect(x, 27, w, 18),
        gfx::Canvas::TEXT_ALIGN_LEFT | gfx::Canvas::NO_SUBPIXEL_RENDERING);
    if (percent_ >= 0) {
      cc::PaintFlags track;
      track.setColor(sidebar::LineColor(*provider));
      canvas->DrawRect(gfx::Rect(x, height() - kProgressHeight - 4, w,
                                 kProgressHeight),
                       track);
      cc::PaintFlags bar;
      bar.setColor(provider->GetColor(ui::kColorAccent));
      canvas->DrawRect(gfx::Rect(x, height() - kProgressHeight - 4,
                                 w * std::clamp(percent_, 0, 100) / 100,
                                 kProgressHeight),
                       bar);
    }
  }

  void StateChanged(ButtonState old_state) override {
    views::Button::StateChanged(old_state);
    SchedulePaint();
  }
  void OnFocus() override {
    views::Button::OnFocus();
    SchedulePaint();
  }
  void OnBlur() override {
    views::Button::OnBlur();
    SchedulePaint();
  }

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(
      views::View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override {
    if (menu_ && GetWidget()) {
      keepalive_ = menu_.Run(model_->GetWeakPtr(), GetWidget()->GetTopLevelWidget(),
                             gfx::Rect(point, gfx::Size()));
    }
  }

 private:
  void Open() {
    // A finished download opens (the engine's command); one in progress has
    // nothing to open yet.
    if (model_->GetState() == download::DownloadItem::COMPLETE) {
      DownloadCommands(model_->GetWeakPtr())
          .ExecuteCommand(DownloadCommands::OPEN_WHEN_COMPLETE);
      if (close_) {
        close_.Run();
      }
    }
  }

  const raw_ptr<DownloadUIModel> model_;
  DownloadRowMenu menu_;
  base::RepeatingClosure close_;
  std::shared_ptr<void> keepalive_;
  std::u16string name_;
  std::u16string status_;
  int percent_ = -1;
};

BEGIN_METADATA(DownloadRow)
END_METADATA

// The card: header, the rows (scrolled after kMaxRows), a timer that refreshes
// the status lines of downloads under way.
class DownloadsCard : public views::View {
  METADATA_HEADER(DownloadsCard, views::View)

 public:
  DownloadsCard(std::vector<DownloadUIModel::DownloadUIModelPtr> models,
                DownloadRowMenu menu,
                base::RepeatingClosure close)
      : models_(std::move(models)) {
    GetViewAccessibility().SetRole(ax::mojom::Role::kList);
    GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(IDS_ATLAS_DOWNLOADS_HEADER));
    header_ = base::i18n::ToUpper(
        l10n_util::GetStringUTF16(IDS_ATLAS_DOWNLOADS_HEADER));
    auto rows = std::make_unique<views::View>();
    rows->SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));
    for (const auto& model : models_) {
      rows_.push_back(rows->AddChildView(
          std::make_unique<DownloadRow>(model.get(), menu, close)));
    }
    const int visible = std::min<int>(kMaxRows, models_.size());
    auto scroll = std::make_unique<views::ScrollView>();
    scroll->SetContents(std::move(rows));
    scroll->ClipHeightTo(visible * kRowHeight, visible * kRowHeight);
    scroll->SetDrawOverflowIndicator(false);
    scroll->SetBackgroundColor(std::nullopt);
    AddChildView(std::move(scroll));
    timer_.Start(FROM_HERE, base::Milliseconds(500), this,
                 &DownloadsCard::Refresh);
  }
  DownloadsCard(const DownloadsCard&) = delete;
  DownloadsCard& operator=(const DownloadsCard&) = delete;
  ~DownloadsCard() override = default;

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(kWidth, kHeaderHeight +
                                children().front()->GetPreferredSize().height() +
                                kPad);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    if (const ui::ColorProvider* provider = GetColorProvider()) {
      canvas->DrawStringRectWithFlags(
          header_, FontOfSize(11, gfx::Font::Weight::SEMIBOLD),
          sidebar::DimTextColor(*provider),
          gfx::Rect(kPad + 4, 0, width() - 2 * kPad, kHeaderHeight),
          gfx::Canvas::TEXT_ALIGN_LEFT);
    }
  }

  void Layout(PassKey) override {
    // The header takes the top; the layout manager works below it.
    views::View* scroll = children().front();
    scroll->SetBounds(kPad, kHeaderHeight, width() - 2 * kPad,
                      std::max(0, height() - kHeaderHeight - kPad));
  }

 private:
  void Refresh() {
    for (DownloadRow* row : rows_) {
      row->Refresh();
    }
  }

  std::vector<DownloadUIModel::DownloadUIModelPtr> models_;
  std::vector<raw_ptr<DownloadRow>> rows_;
  std::u16string header_;
  base::RepeatingTimer timer_;
};

BEGIN_METADATA(DownloadsCard)
END_METADATA

}  // namespace

bool NeedsSecurityPage(const DownloadUIModel& model) {
  return model.IsDangerous() || model.IsInsecure() || model.IsMalicious() ||
         model.GetDangerType() !=
             download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS;
}

std::unique_ptr<views::View> MakeKitDownloadsView(
    std::vector<DownloadUIModel::DownloadUIModelPtr>& models,
    DownloadRowMenu menu,
    base::RepeatingClosure close) {
  if (models.empty()) {
    return nullptr;
  }
  for (const auto& model : models) {
    if (NeedsSecurityPage(*model)) {
      return nullptr;
    }
  }
  return std::make_unique<DownloadsCard>(std::move(models), std::move(menu),
                                         std::move(close));
}

}  // namespace atlas::kit
