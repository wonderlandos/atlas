// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/toolbar/privacy_bubble.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/gesture_event_details.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace atlas::toolbar {

namespace {

// Collects the text of every label, in tree order.
void CollectLabels(views::View* view, std::vector<std::u16string>* out) {
  if (auto* label = views::AsViewClass<views::Label>(view)) {
    if (!views::AsViewClass<views::Link>(view)) {
      out->push_back(std::u16string(label->GetText()));
    }
  }
  for (views::View* child : view->children()) {
    CollectLabels(child, out);
  }
}

views::Link* FindLink(views::View* view) {
  if (auto* link = views::AsViewClass<views::Link>(view)) {
    return link;
  }
  for (views::View* child : view->children()) {
    if (views::Link* found = FindLink(child)) {
      return found;
    }
  }
  return nullptr;
}

class AtlasToolbarPrivacyBubbleTest : public ChromeViewsTestBase {};

TEST_F(AtlasToolbarPrivacyBubbleTest, RowsNameValueAndOneLine) {
  PrivacyState state;
  auto contents = BuildPrivacyContents(state, base::DoNothing());
  std::vector<std::u16string> labels;
  CollectLabels(contents.get(), &labels);

  // Four rows of three labels: name, value, one line of what it does.
  ASSERT_EQ(12u, labels.size());
  EXPECT_EQ(u"Do Not Track", labels[0]);
  EXPECT_EQ(u"On", labels[1]);
  EXPECT_EQ(u"Global Privacy Control", labels[3]);
  EXPECT_EQ(u"On", labels[4]);
  EXPECT_EQ(u"Tracker blocking", labels[6]);
  EXPECT_EQ(u"On", labels[7]);
  EXPECT_EQ(u"Fingerprint noise", labels[9]);
  EXPECT_EQ(u"On", labels[10]);
}

TEST_F(AtlasToolbarPrivacyBubbleTest, OffRowsSayOff) {
  PrivacyState state;
  state.do_not_track = false;
  state.fingerprint_noise = false;
  auto contents = BuildPrivacyContents(state, base::DoNothing());
  std::vector<std::u16string> labels;
  CollectLabels(contents.get(), &labels);
  ASSERT_EQ(12u, labels.size());
  EXPECT_EQ(u"Off", labels[1]);
  EXPECT_EQ(u"On", labels[4]);
  EXPECT_EQ(u"On", labels[7]);
  EXPECT_EQ(u"Off", labels[10]);
}

TEST_F(AtlasToolbarPrivacyBubbleTest, NoLineSaysBlockedForDoNotTrack) {
  PrivacyState state;
  auto contents = BuildPrivacyContents(state, base::DoNothing());
  std::vector<std::u16string> labels;
  CollectLabels(contents.get(), &labels);
  // The Do Not Track line (labels[2]) says sites are asked, and that some ignore it.
  EXPECT_EQ(std::u16string::npos, labels[2].find(u"block"));
  EXPECT_NE(std::u16string::npos, labels[2].find(u"ignore"));
}

TEST_F(AtlasToolbarPrivacyBubbleTest, LinkRunsOpenSettings) {
  int opened = 0;
  auto contents = BuildPrivacyContents(
      PrivacyState(), base::BindRepeating([](int* count) { ++*count; },
                                          &opened));
  views::Link* link = FindLink(contents.get());
  ASSERT_TRUE(link);
  EXPECT_EQ(u"Privacy settings", link->GetText());
  ui::GestureEvent tap(0, 0, 0, base::TimeTicks::Now(),
                       ui::GestureEventDetails(ui::EventType::kGestureTap));
  link->OnGestureEvent(&tap);
  EXPECT_EQ(1, opened);
}

TEST_F(AtlasToolbarPrivacyBubbleTest, ToggleOpensAndCloses) {
  auto anchor_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  views::View* anchor =
      anchor_widget->SetContentsView(std::make_unique<views::View>());

  PrivacyBubbleController controller;
  EXPECT_FALSE(controller.IsShowing());
  controller.Toggle(anchor, PrivacyState(), base::DoNothing());
  EXPECT_TRUE(controller.IsShowing());
  controller.Toggle(anchor, PrivacyState(), base::DoNothing());
  EXPECT_FALSE(controller.IsShowing());
}

}  // namespace

}  // namespace atlas::toolbar
