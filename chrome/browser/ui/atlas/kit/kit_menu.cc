// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/kit/kit_layout_provider.h"
#include "chrome/browser/ui/atlas/kit/kit_menu.h"
#include "chrome/browser/ui/atlas/kit/kit_text.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/atlas/chrome/atlas_tokens.h"
#include "chrome/browser/ui/atlas/kit/kit_chrome.h"
#include "chrome/browser/ui/atlas/kit/kit_icon_button.h"
#include "chrome/browser/ui/atlas/kit/kit_popover.h"
#include "chrome/browser/ui/atlas/kit/vector_icons/vector_icons.h"
#include "chrome/browser/ui/atlas/sidebar/row_style.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/button_menu_item_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/controls/label.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"

namespace atlas::kit {

namespace {

// What a context menu can need at most beside the pointer.
constexpr int kContextMenuWidth = 300;

void ShowModel(MenuAnchor anchor,
               base::WeakPtr<ui::MenuModel> model,
               std::shared_ptr<void> keepalive,
               std::u16string back_to);

// The popover is closed by now; the model may be gone (a row of the downloads
// list closed with its bubble).
void Activate(base::WeakPtr<ui::MenuModel> model,
              std::shared_ptr<void> keepalive,
              size_t i) {
  if (model) {
    model->ActivatedAt(i, /*event_flags=*/0);
  }
}

const gfx::VectorIcon* GlyphOfButton(int command_id) {
  switch (command_id) {
    case IDC_ZOOM_MINUS:
      return &kKitListRemoveIcon;
    case IDC_ZOOM_PLUS:
      return &kKitListAddIcon;
    case IDC_FULLSCREEN:
      return &kKitViewFullscreenIcon;
    default:
      return nullptr;
  }
}

// A row of small buttons after a label (the zoom row: minus, the percentage,
// plus, full screen). The buttons are the model's; a label that changes (the
// percentage) is read again after a button runs.
class ButtonRow : public views::View {
  METADATA_HEADER(ButtonRow, views::View)

 public:
  ButtonRow(std::shared_ptr<void> keepalive,
            ui::ButtonMenuItemModel* model,
            base::RepeatingCallback<std::u16string()> zoom_text)
      : keepalive_(std::move(keepalive)),
        model_(model),
        zoom_text_(std::move(zoom_text)) {
    title_ = AddChildView(std::make_unique<views::Label>(
        DoctrineClean(gfx::RemoveAccelerator(model->label())), views::style::CONTEXT_LABEL,
        views::style::STYLE_PRIMARY));
    title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    for (size_t i = 0; i < model->GetItemCount(); ++i) {
      switch (model->GetTypeAt(i)) {
        case ui::ButtonMenuItemModel::TYPE_SPACE:
          break;
        case ui::ButtonMenuItemModel::TYPE_BUTTON_LABEL: {
          auto* label = AddChildView(std::make_unique<views::Label>(
              model->GetLabelAt(i), views::style::CONTEXT_LABEL,
              views::style::STYLE_SECONDARY));
          labels_.emplace_back(i * 2, label);
          break;
        }
        case ui::ButtonMenuItemModel::TYPE_BUTTON: {
          const gfx::VectorIcon* glyph = GlyphOfButton(model->GetCommandIdAt(i));
          if (glyph == &kKitListAddIcon && zoom_text_) {
            // The percentage sits between minus and plus.
            zoom_ = AddChildView(std::make_unique<views::Label>(
                zoom_text_.Run(), views::style::CONTEXT_LABEL,
                views::style::STYLE_SECONDARY));
            labels_.emplace_back(i * 2 - 1, zoom_.get());
          }
          if (!glyph) {
            // The percentage of the zoom row is a button without a glyph.
            if (!model->GetLabelAt(i).empty()) {
              auto* label = AddChildView(std::make_unique<views::Label>(
                  model->GetLabelAt(i), views::style::CONTEXT_LABEL,
                  views::style::STYLE_SECONDARY));
              labels_.emplace_back(i * 2, label);
            }
            break;
          }
          auto* button = AddChildView(std::make_unique<KitIconButton>(
              base::BindRepeating(&ButtonRow::Run, base::Unretained(this), i),
              *glyph, model->GetLabelAt(i)));
          button->SetEnabled(model->IsEnabledAt(i));
          buttons_.emplace_back(i * 2, button);
          break;
        }
      }
    }
  }
  ButtonRow(const ButtonRow&) = delete;
  ButtonRow& operator=(const ButtonRow&) = delete;
  ~ButtonRow() override = default;

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    int w = 8 + title_->GetPreferredSize().width() + 16;
    for (const auto& l : labels_) {
      w += l.second->GetPreferredSize().width() + 8;
    }
    w += static_cast<int>(buttons_.size()) * tokens::kToolbarButtonHitArea;
    return gfx::Size(std::max(200, w), MenuItemHeight(CurrentDensity()));
  }

  void Layout(PassKey) override {
    int right = width() - 4;
    // Right to left, in the model's order reversed.
    std::vector<std::pair<size_t, views::View*>> parts;
    for (auto& b : buttons_) {
      parts.emplace_back(b.first, b.second);
    }
    for (auto& l : labels_) {
      parts.emplace_back(l.first, l.second);
    }
    std::sort(parts.begin(), parts.end());
    for (auto it = parts.rbegin(); it != parts.rend(); ++it) {
      const int w = std::max(it->second->GetPreferredSize().width(),
                             tokens::kToolbarButtonHitArea);
      right -= w;
      it->second->SetBounds(right, (height() - MenuItemHeight(CurrentDensity())) / 2,
                            w, MenuItemHeight(CurrentDensity()));
    }
    title_->SetBounds(8, 0, std::max(0, right - 8), height());
  }

 private:
  void Run(size_t i) {
    if (!model_->IsEnabledAt(i)) {
      return;
    }
    model_->ActivatedAt(i);
    // The percentage and the enabled states change a moment later.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ButtonRow::Refresh, weak_factory_.GetWeakPtr()),
        base::Milliseconds(60));
  }

  void Refresh() {
    for (auto& l : labels_) {
      if (l.second == zoom_) {
        l.second->SetText(zoom_text_.Run());
      } else {
        l.second->SetText(model_->GetLabelAt(l.first / 2));
      }
    }
    for (auto& b : buttons_) {
      b.second->SetEnabled(model_->IsEnabledAt(b.first / 2));
    }
    InvalidateLayout();
  }

  std::shared_ptr<void> keepalive_;
  const raw_ptr<ui::ButtonMenuItemModel> model_;
  base::RepeatingCallback<std::u16string()> zoom_text_;
  raw_ptr<views::Label> zoom_ = nullptr;
  raw_ptr<views::Label> title_ = nullptr;
  std::vector<std::pair<size_t, raw_ptr<views::Label>>> labels_;
  std::vector<std::pair<size_t, raw_ptr<KitIconButton>>> buttons_;
  base::WeakPtrFactory<ButtonRow> weak_factory_{this};
};

BEGIN_METADATA(ButtonRow)
END_METADATA

void ShowModel(MenuAnchor anchor,
               base::WeakPtr<ui::MenuModel> weak_model,
               std::shared_ptr<void> keepalive,
               std::u16string back_to) {
  ui::MenuModel* model = weak_model.get();
  if (!model) {
    return;
  }
  views::View* anchor_view = anchor.view ? anchor.view->view() : nullptr;
  if (!anchor_view && anchor.parent == gfx::NativeView()) {
    return;
  }
  auto popover =
      std::make_unique<KitPopover>(anchor_view, anchor.arrow);
  if (!anchor_view) {
    popover->SetAnchorRect(anchor.rect);
    popover->set_parent_window(anchor.parent);
  }
  // Models that time the menu (the back and forward lists) or tell their
  // delegate (the downloads row menu) need to hear it opened and closed, as
  // the engine's menu runner tells them. The close is told after the row that
  // ran, which is posted before the popover is closed.
  model->MenuWillShow();
  popover->RegisterWindowClosingCallback(base::BindOnce(
      [](base::WeakPtr<ui::MenuModel> model) {
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(
                           [](base::WeakPtr<ui::MenuModel> model) {
                             if (model) {
                               model->MenuWillClose();
                             }
                           },
                           std::move(model)));
      },
      weak_model));
  if (views::Widget* held = anchor.hold_open.get()) {
    if (views::BubbleDialogDelegate* bubble =
            held->widget_delegate()
                ? held->widget_delegate()->AsBubbleDialogDelegate()
                : nullptr) {
      bubble->set_close_on_deactivate(false);
      popover->RegisterWindowClosingCallback(base::BindOnce(
          [](base::WeakPtr<views::Widget> held) {
            if (held && held->widget_delegate() &&
                held->widget_delegate()->AsBubbleDialogDelegate()) {
              held->widget_delegate()
                  ->AsBubbleDialogDelegate()
                  ->set_close_on_deactivate(true);
            }
          },
          anchor.hold_open));
    }
  }
  if (!back_to.empty() && anchor.root) {
    KitPopover::ItemSpec back;
    back.label = back_to;
    back.icon = ui::ImageModel::FromVectorIcon(kKitGoPreviousIcon);
    MenuAnchor again = anchor;
    popover->AddItem(std::move(back),
                     base::BindOnce(&ShowModel, again, anchor.root,
                                    keepalive, std::u16string()));
    popover->AddSeparator();
  }
  bool separator_pending = false;
  bool any = false;
  for (size_t i = 0; i < model->GetItemCount(); ++i) {
    if (!model->IsVisibleAt(i)) {
      continue;
    }
    const auto type = model->GetTypeAt(i);
    if (type == ui::MenuModel::TYPE_SEPARATOR) {
      separator_pending = any;
      continue;
    }
    if (separator_pending) {
      popover->AddSeparator();
      separator_pending = false;
    }
    any = true;
    if (type == ui::MenuModel::TYPE_BUTTON_ITEM) {
      popover->AddCustom(
          std::make_unique<ButtonRow>(keepalive, model->GetButtonMenuItemAt(i),
                                      anchor.zoom_text));
      continue;
    }
    KitPopover::ItemSpec spec;
    spec.label = DoctrineClean(model->GetLabelAt(i));
    spec.icon = model->GetIconAt(i);
    // A submenu is enabled while the engine shows it (its own rows say more).
    spec.enabled = model->IsEnabledAt(i) ||
                   type == ui::MenuModel::TYPE_SUBMENU ||
                   type == ui::MenuModel::TYPE_ACTIONABLE_SUBMENU;
    ui::Accelerator accelerator;
    if (model->GetAcceleratorAt(i, &accelerator)) {
      spec.trailing = accelerator.GetShortcutText();
    }
    if (model->IsItemCheckedAt(i)) {
      spec.end_glyph = &kKitEmblemOkIcon;
    }
    if (type == ui::MenuModel::TYPE_SUBMENU ||
        type == ui::MenuModel::TYPE_ACTIONABLE_SUBMENU) {
      ui::MenuModel* child = model->GetSubmenuModelAt(i);
      spec.end_glyph = &kKitPanEndIcon;
      const std::u16string title = spec.label;
      MenuAnchor sub = anchor;
      if (!sub.root) {
        sub.root = weak_model;
      }
      popover->AddItem(std::move(spec),
                       base::BindOnce(&ShowModel, sub, child->AsWeakPtr(),
                                      keepalive, title));
      continue;
    }
    popover->AddItem(std::move(spec),
                     base::BindOnce(&Activate, weak_model, keepalive, i));
  }
  KitPopover::Show(std::move(popover));
}

}  // namespace

void ShowMenuModel(MenuAnchor anchor,
                   base::WeakPtr<ui::MenuModel> model,
                   std::shared_ptr<void> keepalive) {
  ShowModel(std::move(anchor), std::move(model), std::move(keepalive),
            std::u16string());
}

bool ShowKitContextMenu(views::Widget* parent,
                        const gfx::Rect& screen_rect,
                        base::WeakPtr<ui::MenuModel> model,
                        std::shared_ptr<void> keepalive) {
  if (!IsKitChromeEnabled() || !parent) {
    return false;
  }
  MenuAnchor anchor;
  anchor.rect = screen_rect;
  anchor.parent = parent->GetNativeView();
  anchor.hold_open = parent->GetWeakPtr();
  // Open towards the middle of the screen: a menu at the right edge opens to
  // the left of the pointer.
  const gfx::Rect area =
      display::Screen::Get()->GetDisplayNearestPoint(screen_rect.origin())
          .work_area();
  anchor.arrow = screen_rect.x() + kContextMenuWidth > area.right()
                     ? views::BubbleBorder::TOP_RIGHT
                     : views::BubbleBorder::TOP_LEFT;
  ShowMenuModel(std::move(anchor), std::move(model), std::move(keepalive));
  return true;
}

}  // namespace atlas::kit
