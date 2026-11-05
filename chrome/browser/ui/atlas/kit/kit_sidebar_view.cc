// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/kit/kit_sidebar_view.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/atlas/chrome/atlas_tokens.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/atlas/kit/kit_layout.h"
#include "chrome/browser/ui/atlas/kit/kit_metrics.h"
#include "chrome/browser/ui/atlas/kit/kit_tab_menu.h"
#include "chrome/browser/ui/atlas/kit/kit_text.h"
#include "chrome/browser/ui/atlas/rest/rest_mark.h"
#include "chrome/browser/ui/atlas/kit/vector_icons/vector_icons.h"
#include "chrome/browser/ui/atlas/sidebar/row_style.h"
#include "chrome/browser/ui/atlas/sidebar/sidebar_sections.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_muted_utils.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/tabs/public/tab_alert.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
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
#include "ui/views/border.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
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
// A tab inside a group is indented by this much under the group's header.
constexpr int kGroupIndent = 12;

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
    // Mute or unmute the tab (the speaker glyph of a tab that has sound).
    base::RepeatingClosure toggle_mute;
  };
  // The sound state of a tab, drawn as a glyph left of the close glyph.
  enum class Audio { kNone, kPlaying, kMuted };

  Row(PressedCallback callback,
      std::u16string title,
      bool active,
      bool dim_title)
      : views::Button(std::move(callback)),
        title_(std::move(title)),
        active_(active),
        dim_title_(dim_title) {
    SetTooltipText(WrapToLineChars(title_, metrics::kRichTooltipLineChars));
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
  // A tab that gave its memory back (REST): dim title and favicon, and the
  // frost mark where the close glyph is.
  void set_resting(bool resting) { resting_ = resting; }
  void set_audio(Audio audio) { audio_ = audio; }
  // A tab inside a group is indented under the group's header.
  void set_indent(int indent) { indent_ = indent; }
  // Takes the changed facts of the row's tab in place: the row, its focus, its
  // drag and its menu stay what they are.
  void Update(std::u16string title,
              gfx::ImageSkia favicon,
              bool resting,
              Audio audio) {
    if (title != title_) {
      title_ = std::move(title);
      SetTooltipText(WrapToLineChars(title_, metrics::kRichTooltipLineChars));
      GetViewAccessibility().SetName(title_);
    }
    favicon_ = std::move(favicon);
    resting_ = resting;
    audio_ = audio;
    SchedulePaint();
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
    const int glyph_x = row.x() + kContentInset + indent_;
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
          glyph_x, glyph_y, resting_ ? 128 : 255);
    }
    if (width() < kMinWidthForText) {
      return;
    }
    int text_right = row.right() - kContentInset;
    if (audio_ != Audio::kNone && hooks_.toggle_mute) {
      // Left of where the close glyph appears, always in the same place.
      const gfx::Rect at = AudioRect();
      canvas->DrawImageInt(
          gfx::CreateVectorIcon(
              audio_ == Audio::kMuted ? kKitAudioVolumeMutedIcon
                                      : kKitAudioVolumeHighIcon,
              kGlyph,
              audio_ == Audio::kMuted ? sidebar::DimTextColor(*provider)
                                      : provider->GetColor(ui::kColorAccent)),
          at.x(), at.y());
      text_right = std::min(text_right, at.x() - kGap);
    }
    if (resting_ && !ShowsClose()) {
      // The mark sits where the close glyph of the tab in front is.
      gfx::Rect mark = atlas::rest::RestMarkBounds(row.width(), row.height());
      mark.Offset(row.x(), row.y());
      atlas::rest::PaintRestMark(canvas, GetMirroredRect(mark),
                                 provider->GetColor(ui::kColorAccent));
      text_right -= atlas::rest::kRestMarkSize + kGap;
    }
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
        dim_title_ || dragged_ || resting_ ? sidebar::DimTextColor(*provider)
                   : sidebar::RowTitleColor(*provider),
        gfx::Rect(text_x, row.y(), std::max(0, text_right - text_x),
                  row.height()),
        gfx::Canvas::TEXT_ALIGN_LEFT | gfx::Canvas::NO_SUBPIXEL_RENDERING);
  }

  void StateChanged(ButtonState old_state) override {
    views::Button::StateChanged(old_state);
    SchedulePaint();
  }

  // The speaker glyph: one glyph and one gap left of the close glyph.
  gfx::Rect AudioRect() const {
    const gfx::Rect close = CloseRect();
    return gfx::Rect(close.x() - kGap - kGlyph, close.y(), kGlyph, kGlyph);
  }

  // The close glyph is where the trailing text of other rows is.
  gfx::Rect CloseRect() const {
    const gfx::Rect row = RowRect();
    return gfx::Rect(row.right() - kContentInset - kGlyph,
                     row.y() + (row.height() - kGlyph) / 2, kGlyph, kGlyph);
  }

  void NotifyClick(const ui::Event& event) override {
    if (audio_ != Audio::kNone && hooks_.toggle_mute && event.IsMouseEvent() &&
        width() >= kMinWidthForText &&
        AudioRect().Contains(event.AsMouseEvent()->location())) {
      hooks_.toggle_mute.Run();
      return;
    }
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

  std::u16string title_;
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
  bool resting_ = false;
  Audio audio_ = Audio::kNone;
  int indent_ = 0;
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
                bool collapsed,
                int tab_count,
                base::RepeatingClosure toggle,
                base::RepeatingClosure edit) {
    color_ = color;
    tab_count_ = tab_count;
    collapsed_ = collapsed;
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
    int right = width() - kContentInset;
    if (color_) {
      // The fold state: a chevron at the end, down while the group is open.
      canvas->DrawImageInt(
          gfx::CreateVectorIcon(collapsed_ ? kKitPanEndIcon : kKitPanDownIcon,
                                kGlyph, sidebar::DimTextColor(*provider)),
          right - kGlyph, (height() - kGlyph) / 2);
      right -= kGlyph + kGap;
      // How many tabs it holds, so a group with no name still says what it is.
      const std::u16string count = base::NumberToString16(tab_count_);
      const gfx::FontList small =
          FontOfSize(kSecondary, gfx::Font::Weight::NORMAL);
      const int w = gfx::GetStringWidth(count, small);
      canvas->DrawStringRectWithFlags(
          count, small, sidebar::DimTextColor(*provider),
          gfx::Rect(right - w, 0, w, height()), gfx::Canvas::TEXT_ALIGN_RIGHT);
      right -= w + kGap;
    }
    canvas->DrawStringRectWithFlags(
        text_, FontOfSize(kSecondary, gfx::Font::Weight::SEMIBOLD),
        sidebar::DimTextColor(*provider),
        gfx::Rect(x, 0, std::max(0, right - x), height()),
        gfx::Canvas::TEXT_ALIGN_LEFT);
  }

 private:
  static constexpr int kDot = 8;

  const std::u16string text_;
  std::optional<ui::ColorId> color_;
  int tab_count_ = 0;
  bool collapsed_ = false;
  base::RepeatingClosure toggle_;
  base::RepeatingClosure edit_;
};

BEGIN_METADATA(Header)
END_METADATA

// What the row says for a tab: the engine's own title (a tab that crashed or
// has not loaded has none), else the host, else the product's word.
std::u16string TitleOf(tabs::TabInterface* tab, content::WebContents* contents) {
  std::u16string title;
  if (const TabUIHelper* helper = TabUIHelper::From(tab)) {
    title = helper->GetTitle();
  }
  if (title.empty()) {
    title = contents->GetTitle();
  }
  if (title.empty()) {
    const GURL& url = contents->GetVisibleURL();
    if (url.has_host()) {
      title = base::UTF8ToUTF16(url.host());
    } else if (!url.is_empty()) {
      title = base::UTF8ToUTF16(url.spec());
    } else {
      title = l10n_util::GetStringUTF16(IDS_ATLAS_SIDEBAR_UNTITLED_TAB);
    }
  }
  return title;
}

Row::Audio AudioOf(tabs::TabInterface* tab, content::WebContents* contents) {
  if (contents->IsAudioMuted()) {
    // Only a tab that has made sound (or is making it) shows the glyph, so a
    // muted tab can be unmuted; a silent one shows nothing.
    const tabs::TabAlertController* alerts = tabs::TabAlertController::From(tab);
    if (!alerts || alerts->IsAlertActive(tabs::TabAlert::kAudioMuting)) {
      return Row::Audio::kMuted;
    }
  }
  if (contents->IsCurrentlyAudible()) {
    return Row::Audio::kPlaying;
  }
  return Row::Audio::kNone;
}

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
  // The rows scroll when there are more than fit above the footer: a window
  // with 30 tabs has all of them reachable by pointer and wheel.
  auto scroll = std::make_unique<views::ScrollView>();
  scroll->SetBackgroundColor(std::nullopt);
  scroll->SetDrawOverflowIndicator(false);
  // A bounded scroll view sizes its contents to the rows (the sidebar sets the
  // view's own height): without this the rows get no size at all.
  scroll->ClipHeightTo(0, std::numeric_limits<int>::max());
  scroll->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  auto list = std::make_unique<views::View>();
  list->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  list->GetViewAccessibility().SetRole(ax::mojom::Role::kTabList);
  // The rows are inset 8 px on every side INSIDE the scroll view, so a focus
  // ring (outside its row) is not cut by the viewport.
  list->SetBorder(views::CreateEmptyBorder(gfx::Insets(kInset)));
  list_ = list.get();
  scroll->SetContents(std::move(list));
  scroll_ = AddChildView(std::move(scroll));

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

  scroll_->SetBounds(0, 0, width(), std::max(0, footer_y - kInset));
  // The scroll view lays out its own rows (this override does not do it).
  LayoutSuperclass<views::View>(this);
}

void KitSidebarView::RequestFocus() {
  TabStripModel* model = browser_->GetTabStripModel();
  views::View* first = nullptr;
  for (const TabRow& r : tab_rows_) {
    if (!first) {
      first = r.view;
    }
    if (r.index == model->active_index()) {
      r.view->RequestFocus();
      return;
    }
  }
  if (first) {
    first->RequestFocus();
  }
}

void KitSidebarView::OnPaintBackground(gfx::Canvas* canvas) {
  const ui::ColorProvider* provider = GetColorProvider();
  if (!provider) {
    return;
  }
  // The 1 px `line` is the content area's: along its left edge (the column
  // right of the sidebar) and along its top, which starts at the window's left.
  // The region that hosts the sidebar paints a line of its own at its last
  // column (the default chrome's), which would make the edge 2 px thick: the
  // sidebar covers it with its fill, and draws only the top part of the line,
  // over itself, in the content area's own colour and row (E2E-11).
  canvas->DrawColor(provider->GetColor(kColorToolbar));
  canvas->FillRect(gfx::Rect(0, 0, width(), 1),
                   provider->GetColor(kColorToolbarContentAreaSeparator));
}

void KitSidebarView::OnThemeChanged() {
  views::View::OnThemeChanged();
  SchedulePaint();
}

void KitSidebarView::AddHeader(const std::u16string& text) {
  list_->AddChildView(std::make_unique<Header>(text));
}

void KitSidebarView::Rebuild() {
  // The focused row's tab, so focus goes back to it: the rows are made again.
  content::WebContents* focused = nullptr;
  if (views::FocusManager* focus = GetFocusManager()) {
    const views::View* view = focus->GetFocusedView();
    for (const TabRow& r : tab_rows_) {
      if (view && r.view == view) {
        focused = r.contents.get();
      }
    }
  }
  ClearDrag();
  tab_rows_.clear();
  alert_subscriptions_.clear();
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
        // A group with no name says so (dimmed like every header) and counts
        // its tabs.
        auto header = std::make_unique<Header>(
            visual->title().empty()
                ? l10n_util::GetStringUTF16(IDS_ATLAS_SIDEBAR_GROUP_UNTITLED)
                : visual->title());
        header->SetGroup(
            GetTabGroupDialogColorId(visual->color()), visual->is_collapsed(),
            model->group_model()->GetTabGroup(id)->tab_count(),
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
        tabs::TabInterface* tab = model->GetTabAtIndex(e.tab);
        auto row = std::make_unique<Row>(
            base::BindRepeating(
                [](TabStripModel* model, content::WebContents* contents) {
                  const int index = model->GetIndexOfWebContents(contents);
                  if (index != TabStripModel::kNoTab) {
                    model->ActivateTabAt(index);
                  }
                },
                base::Unretained(model), base::Unretained(contents)),
            TitleOf(tab, contents), e.tab == model->active_index(),
            /*dim_title=*/false);
        row->set_favicon(FaviconOf(contents));
        row->set_audio(AudioOf(tab, contents));
        if (facts[e.tab].group) {
          row->set_indent(kGroupIndent);
        }
        row->set_resting(atlas::rest::IsResting(contents->WasDiscarded(),
                                                e.tab == model->active_index()));
        Row::TabHooks hooks;
        hooks.close = base::BindRepeating(&KitSidebarView::CloseTab,
                                          weak_factory_.GetWeakPtr(), base::Unretained(contents));
        hooks.toggle_mute = base::BindRepeating(
            &KitSidebarView::ToggleMute, weak_factory_.GetWeakPtr(),
            base::Unretained(contents));
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
        tab_rows_.push_back({row.get(), e.tab, contents->GetWeakPtr()});
        // The sound of this tab starts or stops: its row alone follows.
        if (tabs::TabAlertController* alerts =
                tabs::TabAlertController::From(tab)) {
          alert_subscriptions_.push_back(alerts->AddAlertToShowChangedCallback(
              base::BindRepeating(
                  [](base::WeakPtr<KitSidebarView> self,
                     base::WeakPtr<content::WebContents> contents,
                     std::optional<tabs::TabAlert>) {
                    if (self && contents) {
                      self->RefreshRow(contents.get());
                    }
                  },
                  weak_factory_.GetWeakPtr(), contents->GetWeakPtr())));
        }
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
  for (const TabRow& r : tab_rows_) {
    if (focused && r.contents.get() == focused) {
      r.view->RequestFocus();
    }
  }
  InvalidateLayout();
  SchedulePaint();
  // The tab in front stays in view; the layout of the new rows comes first.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&KitSidebarView::ScrollActiveRowToVisible,
                                weak_factory_.GetWeakPtr()));
}

void KitSidebarView::RefreshRow(content::WebContents* contents) {
  if (!contents) {
    return;
  }
  TabStripModel* model = browser_->GetTabStripModel();
  tabs::TabInterface* tab = model->GetTabForWebContents(contents);
  if (!tab) {
    return;
  }
  const bool active = model->GetIndexOfWebContents(contents) ==
                      model->active_index();
  for (const TabRow& r : tab_rows_) {
    if (r.contents.get() == contents) {
      static_cast<Row*>(r.view.get())
          ->Update(TitleOf(tab, contents), FaviconOf(contents),
                   atlas::rest::IsResting(contents->WasDiscarded(), active),
                   AudioOf(tab, contents));
      return;
    }
  }
}

void KitSidebarView::ScrollActiveRowToVisible() {
  TabStripModel* model = browser_->GetTabStripModel();
  scroll_->DeprecatedLayoutImmediately();
  for (const TabRow& r : tab_rows_) {
    if (r.index == model->active_index()) {
      r.view->ScrollViewToVisible();
      return;
    }
  }
}

void KitSidebarView::ToggleMute(content::WebContents* contents) {
  if (contents) {
    SetTabAudioMuted(contents, !contents->IsAudioMuted(),
                     TabMutedReason::kAudioIndicator, std::string());
  }
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
  // The line is the sidebar's own child, in the list's coordinates scrolled.
  gfx::Rect line(kInset, line_y - 1, list_->width() - 2 * kInset, 2);
  line = views::View::ConvertRectToTarget(list_, this, line);
  line.Intersect(scroll_->bounds());
  drop_line_->SetBoundsRect(line);
  drop_line_->SetVisible(!line.IsEmpty());
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
  // A title, icon, loading or sound change of one tab: its row alone follows
  // (QA-A9-WEB-ATLAS-6). The shape of the list did not change.
  RefreshRow(tab->GetContents());
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
