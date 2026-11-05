// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/kit/kit_popover.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/atlas/chrome/atlas_tokens.h"
#include "chrome/browser/ui/atlas/kit/kit_layout_provider.h"
#include "chrome/browser/ui/atlas/sidebar/row_style.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_utils.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_utils.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace atlas::kit {

namespace {

constexpr int kPad = 8;
constexpr int kGlyph = 16;
constexpr int kMinWidth = 200;

gfx::FontList FontOfSize(int size, gfx::Font::Weight weight) {
  const gfx::FontList base;
  return base.DeriveWithSizeDelta(size - base.GetFontSize())
      .DeriveWithWeight(weight);
}

class Item : public views::Button {
  METADATA_HEADER(Item, views::Button)

 public:
  Item(KitPopover* owner,
       KitPopover::ItemSpec spec,
       base::OnceClosure action)
      : label_(gfx::RemoveAccelerator(spec.label)),
        trailing_(std::move(spec.trailing)),
        icon_(std::move(spec.icon)),
        end_glyph_(spec.end_glyph) {
    SetCallback(base::BindOnce(
        [](KitPopover* owner, base::OnceClosure action) {
          owner->CloseAndRun(std::move(action));
        },
        base::Unretained(owner), std::move(action)));
    GetViewAccessibility().SetRole(ax::mojom::Role::kMenuItem);
    GetViewAccessibility().SetName(label_);
    SetFocusBehavior(FocusBehavior::ALWAYS);
    // The accent row is the focus indicator (tokens.md section 8): no ring.
    SetInstallFocusRingOnFocus(false);
  }
  Item(const Item&) = delete;
  Item& operator=(const Item&) = delete;
  ~Item() override = default;

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    const gfx::FontList body = FontOfSize(13, gfx::Font::Weight::NORMAL);
    const gfx::FontList small = FontOfSize(11, gfx::Font::Weight::NORMAL);
    int w = kPad + gfx::GetStringWidth(label_, body) + kPad;
    if (!icon_.IsEmpty()) {
      w += kGlyph + kPad;
    }
    if (end_glyph_) {
      w += kGlyph + kPad;
    }
    if (!trailing_.empty()) {
      w += 3 * kPad + gfx::GetStringWidth(trailing_, small);
    }
    return gfx::Size(std::max(kMinWidth, w), MenuItemHeight(CurrentDensity()));
  }

  void OnPaintBackground(gfx::Canvas* canvas) override {
    const ui::ColorProvider* provider = GetColorProvider();
    if (!provider || !Highlighted()) {
      return;
    }
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(provider->GetColor(ui::kColorMenuItemBackgroundSelected));
    canvas->DrawRoundRect(gfx::RectF(GetLocalBounds()), tokens::kRadiusControl,
                          flags);
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    const ui::ColorProvider* provider = GetColorProvider();
    if (!provider) {
      return;
    }
    const bool hi = Highlighted();
    const SkColor fg =
        !GetEnabled()
            ? sidebar::DimTextColor(*provider)
            : provider->GetColor(hi ? ui::kColorMenuItemForegroundSelected
                                    : ui::kColorMenuItemForeground);
    const SkColor dim =
        hi ? fg : sidebar::DimTextColor(*provider);
    int x = kPad;
    if (!icon_.IsEmpty()) {
      gfx::ImageSkia image;
      if (icon_.IsVectorIcon() && icon_.GetVectorIcon().vector_icon()) {
        image = gfx::CreateVectorIcon(*icon_.GetVectorIcon().vector_icon(),
                                      kGlyph, hi ? fg : dim);
      } else {
        image = icon_.Rasterize(provider);
      }
      canvas->DrawImageInt(image, x, (height() - image.height()) / 2);
      x += kGlyph + kPad;
    }
    int right = width() - kPad;
    if (end_glyph_) {
      canvas->DrawImageInt(gfx::CreateVectorIcon(*end_glyph_, kGlyph, dim),
                           right - kGlyph, (height() - kGlyph) / 2);
      right -= kGlyph + kPad;
    }
    if (!trailing_.empty()) {
      const gfx::FontList small = FontOfSize(11, gfx::Font::Weight::NORMAL);
      const int w = gfx::GetStringWidth(trailing_, small);
      canvas->DrawStringRectWithFlags(trailing_, small, dim,
                                      gfx::Rect(right - w, 0, w, height()),
                                      gfx::Canvas::TEXT_ALIGN_RIGHT);
      right -= w + 3 * kPad;
    }
    canvas->DrawStringRectWithFlags(
        label_, FontOfSize(13, gfx::Font::Weight::NORMAL), fg,
        gfx::Rect(x, 0, std::max(0, right - x), height()),
        gfx::Canvas::TEXT_ALIGN_LEFT);
  }

  void StateChanged(ButtonState old_state) override {
    views::Button::StateChanged(old_state);
    SchedulePaint();
  }
  // Up and Down move between the rows (the bubble's own arrow traversal does
  // not reach rows of a custom contents view); Enter and Space run one, Esc
  // closes (the bubble).
  bool OnKeyPressed(const ui::KeyEvent& event) override {
    if ((event.key_code() == ui::VKEY_UP || event.key_code() == ui::VKEY_DOWN) &&
        GetFocusManager()) {
      GetFocusManager()->AdvanceFocus(event.key_code() == ui::VKEY_UP);
      return true;
    }
    return views::Button::OnKeyPressed(event);
  }
  // The row under the pointer is the row (as in the engine's menus): the
  // accent follows the pointer and the keyboard, never two rows at once.
  void OnMouseEntered(const ui::MouseEvent& event) override {
    views::Button::OnMouseEntered(event);
    if (GetEnabled()) {
      RequestFocus();
    }
  }
  void OnFocus() override {
    views::Button::OnFocus();
    SchedulePaint();
  }
  void OnBlur() override {
    views::Button::OnBlur();
    SchedulePaint();
  }

 private:
  bool Highlighted() const {
    return GetEnabled() && (HasFocus() || GetState() == STATE_HOVERED ||
                            GetState() == STATE_PRESSED);
  }

  const std::u16string label_;
  const std::u16string trailing_;
  const ui::ImageModel icon_;
  raw_ptr<const gfx::VectorIcon> end_glyph_;
};

BEGIN_METADATA(Item)
END_METADATA

class Header : public views::View {
  METADATA_HEADER(Header, views::View)

 public:
  explicit Header(const std::u16string& text)
      : text_(base::i18n::ToUpper(text)) {
    GetViewAccessibility().SetRole(ax::mojom::Role::kHeading);
    GetViewAccessibility().SetName(text);
  }
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(kMinWidth, 24);
  }
  void OnPaint(gfx::Canvas* canvas) override {
    if (const ui::ColorProvider* provider = GetColorProvider()) {
      canvas->DrawStringRectWithFlags(
          text_, FontOfSize(11, gfx::Font::Weight::SEMIBOLD),
          sidebar::DimTextColor(*provider),
          gfx::Rect(kPad, 0, width() - 2 * kPad, height()),
          gfx::Canvas::TEXT_ALIGN_LEFT);
    }
  }

 private:
  const std::u16string text_;
};

BEGIN_METADATA(Header)
END_METADATA

class Separator : public views::View {
  METADATA_HEADER(Separator, views::View)

 public:
  Separator() { GetViewAccessibility().SetRole(ax::mojom::Role::kSplitter); }
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(0, 2 * 4 + 1);
  }
  void OnPaint(gfx::Canvas* canvas) override {
    if (const ui::ColorProvider* provider = GetColorProvider()) {
      canvas->FillRect(gfx::Rect(kPad, 4, width() - 2 * kPad, 1),
                       sidebar::LineColor(*provider));
    }
  }
};

BEGIN_METADATA(Separator)
END_METADATA

}  // namespace

KitPopover::KitPopover(views::View* anchor, views::BubbleBorder::Arrow arrow)
    : views::BubbleDialogDelegate(anchor,
                                  arrow,
                                  views::BubbleBorder::DIALOG_SHADOW,
                                  /*autosize=*/true) {
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  SetShowCloseButton(false);
  SetShowTitle(false);
  set_margins(gfx::Insets(6));
  set_corner_radius(tokens::kRadiusCard);
  SetBackgroundColor(ui::kColorMenuBackground);
  SetEnableArrowKeyTraversal(true);
  auto contents = std::make_unique<views::View>();
  contents->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  contents->GetViewAccessibility().SetRole(ax::mojom::Role::kMenu);
  contents_ = contents.get();
  SetContentsView(std::move(contents));
}

KitPopover::~KitPopover() = default;

void KitPopover::AddHeader(const std::u16string& text) {
  contents_->AddChildView(std::make_unique<Header>(text));
}

views::View* KitPopover::AddItem(ItemSpec spec, base::OnceClosure action) {
  const bool enabled = spec.enabled;
  auto* item = contents_->AddChildView(
      std::make_unique<Item>(this, std::move(spec), std::move(action)));
  item->SetEnabled(enabled);
  return item;
}

void KitPopover::AddSeparator() {
  contents_->AddChildView(std::make_unique<Separator>());
}

views::View* KitPopover::AddCustom(std::unique_ptr<views::View> view) {
  return contents_->AddChildView(std::move(view));
}

// static
views::Widget* KitPopover::Show(std::unique_ptr<KitPopover> popover) {
  KitPopover* p = popover.get();
  for (views::View* child : p->contents_->children()) {
    if (views::IsViewClass<Item>(child) && child->GetEnabled()) {
      p->SetInitiallyFocusedView(child);
      break;
    }
  }
  views::Widget* widget = views::BubbleDialogDelegate::CreateBubbleDeprecated(
      std::move(popover), views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET);
  widget->Show();
  return widget;
}

void KitPopover::CloseAndRun(base::OnceClosure action) {
  // The action is queued before the close: whoever hears of the close (the
  // menu model) hears of it after the action ran.
  if (action) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(action));
  }
  GetWidget()->Close();
}

}  // namespace atlas::kit
