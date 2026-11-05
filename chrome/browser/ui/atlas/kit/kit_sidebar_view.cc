// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/kit/kit_sidebar_view.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/atlas/chrome/atlas_tokens.h"
#include "chrome/browser/ui/atlas/kit/kit_layout.h"
#include "chrome/browser/ui/atlas/kit/kit_tab_menu.h"
#include "chrome/browser/ui/atlas/kit/vector_icons/vector_icons.h"
#include "chrome/browser/ui/atlas/sidebar/row_style.h"
#include "chrome/browser/ui/atlas/sidebar/sidebar_sections.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/tabs/public/tab_group.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/color/color_id.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "base/task/single_thread_task_runner.h"

namespace atlas::kit {

namespace {

// Rows (tokens.md section 3).
constexpr int kRow = tokens::kRowHeight;
constexpr int kInset = tokens::kSidebarRowInset;
constexpr int kGlyph = 16;
// Glyph to title (--space-2) and the content inset inside the row.
constexpr int kGap = 8;
constexpr int kContentInset = 8;
// Type scale: body 13, secondary 11 (hig.md section 2).
constexpr int kBody = 13;
constexpr int kSecondary = 11;
// Below this width the rows show the glyph alone.
constexpr int kMinWidthForText = 120;

gfx::FontList FontOfSize(int size, gfx::Font::Weight weight) {
  const gfx::FontList base;
  return base.DeriveWithSizeDelta(size - base.GetFontSize())
      .DeriveWithWeight(weight);
}

// One row: a tab, a collapsed group's summary or the footer's "New tab".
class Row : public views::Button, public views::ContextMenuController {
  METADATA_HEADER(Row, views::Button)

 public:
  // What a tab row reports to the sidebar besides being pressed.
  enum class Drag { kMove, kEnd };
  struct TabHooks {
    // Close the tab (the glyph on hover, a middle click, Delete).
    base::RepeatingClosure close;
    // The context menu at a screen point.
    base::RepeatingCallback<void(const gfx::Point&)> menu;
    // Reorder by dragging: the pointer at `point` in the row's own space.
    base::RepeatingCallback<void(Drag, const gfx::Point&)> drag;
  };

  Row(PressedCallback callback,
      std::u16string title,
      bool active,
      bool dim_title)
      : views::Button(std::move(callback)),
        title_(std::move(title)),
        active_(active),
        dim_title_(dim_title) {
    SetTooltipText(title_);
    GetViewAccessibility().SetName(title_);
    GetViewAccessibility().SetRole(ax::mojom::Role::kTab);
    if (active_) {
      GetViewAccessibility().SetIsSelected(true);
    }
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                  tokens::kRadiusControl);
    views::FocusRing::Install(this);
    SetFocusBehavior(FocusBehavior::ALWAYS);
  }

  // Makes this row a tab row: hover glyph, middle click, context menu, drag.
  void SetTabHooks(TabHooks hooks) {
    hooks_ = std::move(hooks);
    SetTriggerableEventFlags(ui::EF_LEFT_MOUSE_BUTTON |
                                ui::EF_MIDDLE_MOUSE_BUTTON);
    set_context_menu_controller(this);
  }
  void set_dragged(bool dragged) {
    if (dragged_ != dragged) {
      dragged_ = dragged;
      SchedulePaint();
    }
  }
  Row(const Row&) = delete;
  Row& operator=(const Row&) = delete;
  ~Row() override = default;

  void set_favicon(gfx::ImageSkia favicon) { favicon_ = std::move(favicon); }
  void set_glyph(const gfx::VectorIcon* glyph) { glyph_ = glyph; }
  void set_trailing(std::u16string trailing) {
    trailing_ = std::move(trailing);
  }
  void set_rule_above(bool rule) { rule_above_ = rule; }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(0, kRow + (rule_above_ ? kInset + 1 : 0));
  }

  void OnPaintBackground(gfx::Canvas* canvas) override {
    const ui::ColorProvider* provider = GetColorProvider();
    if (!provider) {
      return;
    }
    if (rule_above_) {
      canvas->FillRect(gfx::Rect(0, 0, width(), 1),
                       sidebar::LineColor(*provider));
    }
    const sidebar::RowState state =
        active_ ? sidebar::RowState::kActive : sidebar::RowState::kIdle;
    const double hover = GetState() == STATE_HOVERED ||
                                 GetState() == STATE_PRESSED
                             ? 1.0
                             : 0.0;
    const SkColor fill = sidebar::RowFillColor(*provider, state, hover);
    if (SkColorGetA(fill) == 0) {
      return;
    }
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(fill);
    canvas->DrawRoundRect(gfx::RectF(RowRect()), tokens::kRadiusControl,
                          flags);
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    const ui::ColorProvider* provider = GetColorProvider();
    if (!provider) {
      return;
    }
    const gfx::Rect row = RowRect();
    const sidebar::RowState state =
        active_ ? sidebar::RowState::kActive : sidebar::RowState::kIdle;
    const int glyph_x = row.x() + kContentInset;
    const int glyph_y = row.y() + (row.height() - kGlyph) / 2;
    if (glyph_) {
      canvas->DrawImageInt(
          gfx::CreateVectorIcon(*glyph_, kGlyph,
                                sidebar::RowIconColor(*provider, state)),
          glyph_x, glyph_y);
    } else if (!favicon_.isNull()) {
      canvas->DrawImageInt(
          gfx::ImageSkiaOperations::CreateResizedImage(
              favicon_, skia::ImageOperations::RESIZE_BEST,
              gfx::Size(kGlyph, kGlyph)),
          glyph_x, glyph_y);
    }
    if (width() < kMinWidthForText) {
      return;
    }
    int text_right = row.right() - kContentInset;
    if (ShowsClose()) {
      canvas->DrawImageInt(
          gfx::CreateVectorIcon(kKitWindowCloseIcon, kGlyph,
                                sidebar::DimTextColor(*provider)),
          CloseRect().x(), CloseRect().y());
      text_right -= kGlyph + kGap;
    }
    if (!trailing_.empty()) {
      const gfx::FontList small =
          FontOfSize(kSecondary, gfx::Font::Weight::NORMAL);
      const int w = gfx::GetStringWidth(trailing_, small);
      canvas->DrawStringRectWithFlags(
          trailing_, small, sidebar::DimTextColor(*provider),
          gfx::Rect(text_right - w, row.y(), w, row.height()),
          gfx::Canvas::TEXT_ALIGN_RIGHT);
      text_right -= w + kGap;
    }
    const int text_x = glyph_x + kGlyph + kGap;
    const gfx::FontList font = FontOfSize(
        kBody, active_ ? gfx::Font::Weight::MEDIUM : gfx::Font::Weight::NORMAL);
    canvas->DrawStringRectWithFlags(
        title_, font,
        dim_title_ || dragged_ ? sidebar::DimTextColor(*provider)
                   : sidebar::RowTitleColor(*provider),
        gfx::Rect(text_x, row.y(), std::max(0, text_right - text_x),
                  row.height()),
        gfx::Canvas::TEXT_ALIGN_LEFT | gfx::Canvas::NO_SUBPIXEL_RENDERING);
  }

  void StateChanged(ButtonState old_state) override {
    views::Button::StateChanged(old_state);
    SchedulePaint();
  }

  // The close glyph is where the trailing text of other rows is.
  gfx::Rect CloseRect() const {
    const gfx::Rect row = RowRect();
    return gfx::Rect(row.right() - kContentInset - kGlyph,
                     row.y() + (row.height() - kGlyph) / 2, kGlyph, kGlyph);
  }

  void NotifyClick(const ui::Event& event) override {
    if (hooks_.close) {
      const bool middle = event.IsMouseEvent() &&
                          (event.AsMouseEvent()->changed_button_flags() &
                           ui::EF_MIDDLE_MOUSE_BUTTON);
      const bool on_glyph = event.IsMouseEvent() && ShowsClose() &&
                            CloseRect().Contains(event.AsMouseEvent()->location());
      if (middle || on_glyph) {
        hooks_.close.Run();
        return;
      }
    }
    views::Button::NotifyClick(event);
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    press_ = event.location();
    drag_started_ = false;
    return views::Button::OnMousePressed(event);
  }

  bool OnMouseDragged(const ui::MouseEvent& event) override {
    if (!hooks_.drag || !(event.flags() & ui::EF_LEFT_MOUSE_BUTTON)) {
      return views::Button::OnMouseDragged(event);
    }
    if (!drag_started_ &&
        std::abs(event.y() - press_.y()) < kDragThreshold) {
      return true;
    }
    drag_started_ = true;
    hooks_.drag.Run(Drag::kMove, event.location());
    return true;
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    if (drag_started_) {
      drag_started_ = false;
      hooks_.drag.Run(Drag::kEnd, event.location());
      // The press became a drag, not a click.
      views::Button::OnMouseCaptureLost();
      return;
    }
    views::Button::OnMouseReleased(event);
  }

  bool OnKeyPressed(const ui::KeyEvent& event) override {
    // Up and Down walk the list (the rows are one tab list), Delete closes.
    if (event.key_code() == ui::VKEY_UP || event.key_code() == ui::VKEY_DOWN) {
      if (GetFocusManager()) {
        GetFocusManager()->AdvanceFocus(event.key_code() == ui::VKEY_UP);
        return true;
      }
    }
    if (event.key_code() == ui::VKEY_DELETE && hooks_.close) {
      hooks_.close.Run();
      return true;
    }
    return views::Button::OnKeyPressed(event);
  }

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(
      views::View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override {
    if (hooks_.menu) {
      hooks_.menu.Run(point);
    }
  }

 private:
  static constexpr int kDragThreshold = 4;

  bool ShowsClose() const {
    return hooks_.close && width() >= kMinWidthForText &&
           (GetState() == STATE_HOVERED || GetState() == STATE_PRESSED);
  }

  gfx::Rect RowRect() const {
    const int top = rule_above_ ? kInset + 1 : 0;
    return gfx::Rect(0, top, width(), kRow);
  }

  const std::u16string title_;
  const bool active_;
  const bool dim_title_;
  gfx::ImageSkia favicon_;
  raw_ptr<const gfx::VectorIcon> glyph_ = nullptr;
  std::u16string trailing_;
  bool rule_above_ = false;
  TabHooks hooks_;
  gfx::Point press_;
  bool drag_started_ = false;
  bool dragged_ = false;
};

BEGIN_METADATA(Row)
END_METADATA

// A section header: 11 px, 600, small caps, `dim` (tokens.md section 3).
class Header : public views::View {
  METADATA_HEADER(Header, views::View)

 public:
  explicit Header(std::u16string text)
      : text_(base::i18n::ToUpper(text)) {
    GetViewAccessibility().SetRole(ax::mojom::Role::kHeading);
    GetViewAccessibility().SetName(text);
  }
  Header(const Header&) = delete;
  Header& operator=(const Header&) = delete;
  ~Header() override = default;

  // A group's header: its colour as a dot, a click folds or unfolds the group,
  // a right click opens the engine's group editor.
  void SetGroup(ui::ColorId color,
                base::RepeatingClosure toggle,
                base::RepeatingClosure edit) {
    color_ = color;
    toggle_ = std::move(toggle);
    edit_ = std::move(edit);
    GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    return toggle_ ? true : views::View::OnMousePressed(event);
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    if (!toggle_ || !HitTestPoint(event.location())) {
      return;
    }
    // The header is rebuilt by the change: after this handler returns.
    base::RepeatingClosure run =
        event.IsRightMouseButton() ? edit_ : toggle_;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run);
  }

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(0, sidebar::kHeaderHeight);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    const ui::ColorProvider* provider = GetColorProvider();
    if (!provider || width() < kMinWidthForText) {
      return;
    }
    int x = kContentInset;
    if (color_) {
      cc::PaintFlags flags;
      flags.setAntiAlias(true);
      flags.setColor(provider->GetColor(*color_));
      canvas->DrawCircle(gfx::PointF(x + kDot / 2.0f, height() / 2.0f),
                         kDot / 2.0f, flags);
      x += kDot + kGap / 2;
    }
    canvas->DrawStringRectWithFlags(
        text_, FontOfSize(kSecondary, gfx::Font::Weight::SEMIBOLD),
        sidebar::DimTextColor(*provider),
        gfx::Rect(x, 0, width() - x - kContentInset, height()),
        gfx::Canvas::TEXT_ALIGN_LEFT);
  }

 private:
  static constexpr int kDot = 8;

  const std::u16string text_;
  std::optional<ui::ColorId> color_;
  base::RepeatingClosure toggle_;
  base::RepeatingClosure edit_;
};

BEGIN_METADATA(Header)
END_METADATA

gfx::ImageSkia FaviconOf(content::WebContents* contents) {
  if (auto* driver =
          favicon::ContentFaviconDriver::FromWebContents(contents)) {
    const gfx::Image image = driver->GetFavicon();
    if (!image.IsEmpty()) {
      return *image.ToImageSkia();
    }
  }
  return gfx::ImageSkia();
}

}  // namespace

KitSidebarView::KitSidebarView(BrowserWindowInterface* browser) : browser_(browser) {
  list_ = AddChildView(std::make_unique<views::View>());
  list_->GetViewAccessibility().SetRole(ax::mojom::Role::kTabList);

  auto footer = std::make_unique<Row>(
      base::BindRepeating(
          [](BrowserWindowInterface* browser) {
            chrome::ExecuteCommand(browser, IDC_NEW_TAB);
          },
          base::Unretained(browser_)),
      l10n_util::GetStringUTF16(IDS_ATLAS_SIDEBAR_NEW_TAB), /*active=*/false,
      /*dim_title=*/false);
  footer->set_glyph(&kKitListAddIcon);
  footer->set_trailing(
      l10n_util::GetStringUTF16(IDS_ATLAS_SIDEBAR_NEW_TAB_SHORTCUT));
  footer->set_rule_above(true);
  footer->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
  footer_ = AddChildView(std::move(footer));
  // Where a dragged tab will land (shown only while dragging).
  auto line = std::make_unique<views::View>();
  line->SetVisible(false);
  drop_line_ = AddChildView(std::move(line));

  browser_->GetTabStripModel()->AddObserver(this);
  Rebuild();
}

KitSidebarView::~KitSidebarView() = default;

void KitSidebarView::TakeOver(views::View* content_area) {
  for (views::View* child : content_area->children()) {
    if (child != this) {
      child->SetVisible(false);
    }
  }
  content_area_.Observe(content_area);
}

void KitSidebarView::OnChildViewAdded(views::View* observed_view,
                                      views::View* child) {
  if (child != this) {
    child->SetVisible(false);
  }
}

void KitSidebarView::OnViewIsDeleting(views::View* observed_view) {
  content_area_.Reset();
}

void KitSidebarView::Layout(PassKey) {
  // The footer sits at the bottom, the sections from the top; rows are inset
  // 8 px on both sides.
  const int w = std::max(0, width() - 2 * kInset);
  const int footer_h = footer_->GetPreferredSize().height();
  const int footer_y = height() - kInset - footer_h;
  footer_->SetBounds(kInset, footer_y, w, footer_h);

  list_->SetBounds(kInset, kInset, w, std::max(0, footer_y - 2 * kInset));
  int y = 0;
  for (views::View* child : list_->children()) {
    const int h = child->GetPreferredSize().height();
    child->SetBounds(0, y, w, h);
    y += h;
  }
}

void KitSidebarView::OnPaintBackground(gfx::Canvas* canvas) {
  const ui::ColorProvider* provider = GetColorProvider();
  if (!provider) {
    return;
  }
  sidebar::PaintSidebarEdge(canvas, GetLocalBounds(), /*on_right=*/true,
                            *provider);
}

void KitSidebarView::OnThemeChanged() {
  views::View::OnThemeChanged();
  SchedulePaint();
}

void KitSidebarView::AddHeader(const std::u16string& text) {
  list_->AddChildView(std::make_unique<Header>(text));
}

void KitSidebarView::Rebuild() {
  ClearDrag();
  tab_rows_.clear();
  list_->RemoveAllChildViews();
  TabStripModel* model = browser_->GetTabStripModel();

  // The model's facts, groups numbered in the order they first appear.
  std::vector<tab_groups::TabGroupId> groups;
  std::vector<TabFacts> facts;
  for (int i = 0; i < model->count(); ++i) {
    TabFacts f;
    f.pinned = model->IsTabPinned(i);
    f.active = i == model->active_index();
    f.last_active = model->GetWebContentsAt(i)->GetLastActiveTime();
    if (const std::optional<tab_groups::TabGroupId> id =
            model->GetTabGroupForTab(i)) {
      auto it = std::find(groups.begin(), groups.end(), *id);
      if (it == groups.end()) {
        groups.push_back(*id);
        it = groups.end() - 1;
      }
      f.group = static_cast<int>(it - groups.begin());
      const TabGroup* group = model->group_model()->GetTabGroup(*id);
      f.group_collapsed = group->visual_data()->is_collapsed();
      f.group_tab_count = group->tab_count();
    }
    facts.push_back(f);
  }

  for (const SidebarEntry& e : BuildSidebar(facts, base::Time::Now())) {
    switch (e.kind) {
      case EntryKind::kHeader: {
        if (!e.group) {
          AddHeader(sidebar::SectionLabel(*e.section));
          break;
        }
        const tab_groups::TabGroupId id = groups[*e.group];
        const tab_groups::TabGroupVisualData* visual =
            model->group_model()->GetTabGroup(id)->visual_data();
        auto header = std::make_unique<Header>(visual->title());
        header->SetGroup(
            GetTabGroupDialogColorId(visual->color()),
            base::BindRepeating(&KitSidebarView::ToggleGroup,
                                weak_factory_.GetWeakPtr(), id),
            base::BindRepeating(&KitSidebarView::EditGroup,
                                weak_factory_.GetWeakPtr(), id));
        list_->AddChildView(std::move(header));
        break;
      }
      case EntryKind::kGroupSummary: {
        auto summary = std::make_unique<Row>(
            base::BindRepeating(
                [](TabStripModel* model, int index) {
                  model->ActivateTabAt(index);
                },
                base::Unretained(model), e.tab),
            l10n_util::GetPluralStringFUTF16(IDS_ATLAS_SIDEBAR_GROUP_TAB_COUNT,
                                             e.group_tab_count),
            /*active=*/false, /*dim_title=*/true);
        summary->set_glyph(&vector_icons::kFolderFlippableIcon);
        list_->AddChildView(std::move(summary));
        break;
      }
      case EntryKind::kTab: {
        content::WebContents* contents = model->GetWebContentsAt(e.tab);
        auto row = std::make_unique<Row>(
            base::BindRepeating(
                [](TabStripModel* model, content::WebContents* contents) {
                  const int index = model->GetIndexOfWebContents(contents);
                  if (index != TabStripModel::kNoTab) {
                    model->ActivateTabAt(index);
                  }
                },
                base::Unretained(model), base::Unretained(contents)),
            contents->GetTitle(), e.tab == model->active_index(),
            /*dim_title=*/false);
        row->set_favicon(FaviconOf(contents));
        Row::TabHooks hooks;
        hooks.close = base::BindRepeating(&KitSidebarView::CloseTab,
                                          weak_factory_.GetWeakPtr(), base::Unretained(contents));
        hooks.menu = base::BindRepeating(&KitSidebarView::ShowTabMenu,
                                         weak_factory_.GetWeakPtr(), base::Unretained(contents));
        Row* raw = row.get();
        hooks.drag = base::BindRepeating(
            [](base::WeakPtr<KitSidebarView> self,
               content::WebContents* contents, Row* row, Row::Drag phase,
               const gfx::Point& point) {
              if (self) {
                self->DragTab(contents, row, phase == Row::Drag::kEnd, point);
              }
            },
            weak_factory_.GetWeakPtr(), base::Unretained(contents),
            base::Unretained(raw));
        row->SetTabHooks(std::move(hooks));
        tab_rows_.push_back({row.get(), e.tab});
        list_->AddChildView(std::move(row));
        break;
      }
    }
  }
  for (size_t i = 0; i < tab_rows_.size(); ++i) {
    auto& a11y = tab_rows_[i].view->GetViewAccessibility();
    a11y.SetPosInSet(static_cast<int>(i) + 1);
    a11y.SetSetSize(static_cast<int>(tab_rows_.size()));
  }
  InvalidateLayout();
  SchedulePaint();
}

void KitSidebarView::ToggleGroup(tab_groups::TabGroupId id) {
  TabStripModel* model = browser_->GetTabStripModel();
  if (!model->group_model()->ContainsTabGroup(id)) {
    return;
  }
  const tab_groups::TabGroupVisualData* now =
      model->group_model()->GetTabGroup(id)->visual_data();
  model->ChangeTabGroupVisuals(
      id, tab_groups::TabGroupVisualData(now->title(), now->color(),
                                         !now->is_collapsed()));
}

void KitSidebarView::EditGroup(tab_groups::TabGroupId id) {
  TabStripModel* model = browser_->GetTabStripModel();
  if (model->group_model()->ContainsTabGroup(id)) {
    model->OpenTabGroupEditor(id);
  }
}

void KitSidebarView::CloseTab(content::WebContents* contents) {
  // The row that asked is deleted by the rebuild that follows: after its
  // event handler returns.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WeakPtr<KitSidebarView> self,
                        base::WeakPtr<content::WebContents> contents) {
                       if (!self || !contents) {
                         return;
                       }
                       TabStripModel* model = self->browser_->GetTabStripModel();
                       const int index =
                           model->GetIndexOfWebContents(contents.get());
                       if (index != TabStripModel::kNoTab) {
                         model->CloseWebContentsAt(
                             index, TabCloseTypes::CLOSE_USER_GESTURE |
                                        TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
                       }
                     },
                     weak_factory_.GetWeakPtr(), contents->GetWeakPtr()));
}

void KitSidebarView::ShowTabMenu(content::WebContents* contents,
                                 const gfx::Point& screen) {
  const int index =
      browser_->GetTabStripModel()->GetIndexOfWebContents(contents);
  if (index != TabStripModel::kNoTab) {
    ShowKitTabMenu(browser_, index, GetWidget(), gfx::Rect(screen, gfx::Size()));
  }
}

void KitSidebarView::DragTab(content::WebContents* contents,
                             views::View* row,
                             bool end,
                             const gfx::Point& point_in_row) {
  TabStripModel* model = browser_->GetTabStripModel();
  const int from = model->GetIndexOfWebContents(contents);
  if (from == TabStripModel::kNoTab || tab_rows_.empty()) {
    ClearDrag();
    return;
  }
  gfx::Point p = point_in_row;
  views::View::ConvertPointToTarget(row, list_, &p);
  // The tab is dropped before the first tab row whose middle is below the
  // pointer; pinned tabs stay among the pinned and the others among the rest.
  int before = model->count();
  int line_y = tab_rows_.back().view->bounds().bottom();
  for (const TabRow& r : tab_rows_) {
    if (p.y() < r.view->bounds().CenterPoint().y()) {
      before = r.index;
      line_y = r.view->y();
      break;
    }
  }
  const int pinned = model->IndexOfFirstNonPinnedTab();
  const int clamped = model->IsTabPinned(from) ? std::min(before, pinned)
                                                : std::max(before, pinned);
  if (clamped != before) {
    before = clamped;
    line_y = before >= model->count()
                 ? tab_rows_.back().view->bounds().bottom()
                 : list_->height();
    for (const TabRow& r : tab_rows_) {
      if (r.index == before) {
        line_y = r.view->y();
      }
    }
  }
  if (end) {
    ClearDrag();
    const int to = before > from ? before - 1 : before;
    if (to != from) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&KitSidebarView::MoveTab,
                                    weak_factory_.GetWeakPtr(), from, to));
    }
    return;
  }
  if (dragged_row_ != row) {
    ClearDrag();
    dragged_row_ = row;
    static_cast<Row*>(row)->set_dragged(true);
  }
  drop_before_ = before;
  if (const ui::ColorProvider* provider = GetColorProvider()) {
    drop_line_->SetBackground(views::CreateSolidBackground(
        provider->GetColor(ui::kColorSysPrimary)));
  }
  drop_line_->SetBounds(list_->x(), list_->y() + line_y - 1, list_->width(), 2);
  drop_line_->SetVisible(true);
}

void KitSidebarView::MoveTab(int from, int to) {
  TabStripModel* model = browser_->GetTabStripModel();
  if (from >= 0 && from < model->count() && to >= 0 && to < model->count()) {
    model->MoveWebContentsAt(from, to, /*select_after_move=*/false);
  }
}

void KitSidebarView::ClearDrag() {
  if (dragged_row_) {
    static_cast<Row*>(dragged_row_.get())->set_dragged(false);
    dragged_row_ = nullptr;
  }
  drop_before_ = -1;
  if (drop_line_) {
    drop_line_->SetVisible(false);
  }
}

void KitSidebarView::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  Rebuild();
}

void KitSidebarView::OnTabChangedAt(tabs::TabInterface* tab,
                                    TabChangeType change_type) {
  Rebuild();
}

void KitSidebarView::OnTabPinnedStateChanged(tabs::TabInterface* tab,
                                             int index) {
  Rebuild();
}

void KitSidebarView::OnTabGroupChanged(const TabGroupChange& change) {
  Rebuild();
}

BEGIN_METADATA(KitSidebarView)
END_METADATA

}  // namespace atlas::kit
