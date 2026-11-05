// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/toolbar/toolbar_buttons.h"

#include <memory>

#include "base/callback_list.h"
#include "chrome/browser/ui/atlas/toolbar/bookmark_button.h"
#include "chrome/browser/ui/atlas/toolbar/privacy_button.h"
#include "chrome/browser/ui/atlas/toolbar/toolbar_prefs.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace atlas::toolbar {

namespace {

using ::testing::_;
using ::testing::Return;

class AtlasToolbarButtonsTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    profile_ = std::make_unique<TestingProfile>();
    // TestingProfile registers every profile pref of the browser, the Atlas ones included.

    ON_CALL(browser_, GetProfile())
        .WillByDefault(Return(static_cast<Profile*>(profile_.get())));
    ON_CALL(browser_, RegisterActiveTabDidChange(_))
        .WillByDefault([](BrowserWindowInterface::ActiveTabChangeCallback) {
          return base::CallbackListSubscription();
        });
    ON_CALL(browser_, GetActiveTabInterface()).WillByDefault(Return(nullptr));

    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    toolbar_ = widget_->SetContentsView(std::make_unique<views::View>());
  }

  void TearDown() override {
    toolbar_ = nullptr;
    widget_.reset();
    profile_.reset();
    ChromeViewsTestBase::TearDown();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  testing::NiceMock<MockBrowserWindowInterface> browser_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::View> toolbar_ = nullptr;
};

TEST_F(AtlasToolbarButtonsTest, AddsPrivacyThenBookmark) {
  AddToolbarButtons(toolbar_, &browser_);
  ASSERT_EQ(2u, toolbar_->children().size());
  EXPECT_TRUE(views::IsViewClass<PrivacyButton>(toolbar_->children()[0]));
  EXPECT_TRUE(views::IsViewClass<BookmarkButton>(toolbar_->children()[1]));
}

TEST_F(AtlasToolbarButtonsTest, FindsTheBookmarkButtonOnlyWhileVisible) {
  AddToolbarButtons(toolbar_, &browser_);
  views::View* bookmark = FindVisibleBookmarkButton(toolbar_);
  ASSERT_TRUE(bookmark);
  EXPECT_EQ(toolbar_->children()[1].get(), bookmark);

  profile_->GetPrefs()->SetBoolean(kShowBookmarkButton, false);
  EXPECT_FALSE(bookmark->GetVisible());
  EXPECT_EQ(nullptr, FindVisibleBookmarkButton(toolbar_));
}

TEST_F(AtlasToolbarButtonsTest, TheOmniboxStarStepsAsideForTheButton) {
  EXPECT_TRUE(BookmarkButtonReplacesStar(profile_->GetPrefs()));
  profile_->GetPrefs()->SetBoolean(kShowBookmarkButton, false);
  EXPECT_FALSE(BookmarkButtonReplacesStar(profile_->GetPrefs()));
}

TEST_F(AtlasToolbarButtonsTest, BookmarkButtonNamesItsStateWithoutATab) {
  AddToolbarButtons(toolbar_, &browser_);
  auto* bookmark = views::AsViewClass<BookmarkButton>(toolbar_->children()[1]);
  ASSERT_TRUE(bookmark);
  EXPECT_FALSE(bookmark->is_starred());
}

}  // namespace

}  // namespace atlas::toolbar
