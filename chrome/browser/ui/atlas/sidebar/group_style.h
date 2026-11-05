// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_SIDEBAR_GROUP_STYLE_H_
#define CHROME_BROWSER_UI_ATLAS_SIDEBAR_GROUP_STYLE_H_

#include <string>

#include "base/functional/callback.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/font_list.h"
#include "ui/views/view.h"

namespace atlas::sidebar {

// A named group in the sidebar looks like a section (tokens.md section 3): its
// name is a header in small caps, 11 px, weight 600, in the dim colour, with no
// coloured chip; while the group is collapsed one row under it says how many
// tabs it holds.

inline constexpr int kGroupSummaryRowHeight = 28;

// The name of a group as drawn in its header (small caps: upper case). The
// group's real name, used by assistive technology and by the editor, is not
// changed.
std::u16string GroupHeaderText(const std::u16string& title);

// The font of a group header: 11 px, weight 600.
gfx::FontList GroupHeaderFont(const gfx::FontList& current);

// The "N tabs" row under the header of a collapsed group: a bookmark glyph and
// the count, in the dim colour. Activating it opens the group.
class GroupSummaryRow : public views::View {
  METADATA_HEADER(GroupSummaryRow, views::View)

 public:
  // `tab_count` is asked each time the row paints, so it never shows a stale
  // number; `on_activate` expands the group.
  GroupSummaryRow(base::RepeatingCallback<int()> tab_count,
                  base::RepeatingClosure on_activate);
  GroupSummaryRow(const GroupSummaryRow&) = delete;
  GroupSummaryRow& operator=(const GroupSummaryRow&) = delete;
  ~GroupSummaryRow() override;

  // The count in words, as drawn ("3 tabs").
  std::u16string GetSummaryText() const;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnPaint(gfx::Canvas* canvas) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  void UpdateAccessibleName();

  base::RepeatingCallback<int()> tab_count_;
  base::RepeatingClosure on_activate_;
  std::u16string accessible_name_;
  bool hovered_ = false;
};

}  // namespace atlas::sidebar

#endif  // CHROME_BROWSER_UI_ATLAS_SIDEBAR_GROUP_STYLE_H_
