// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/toolbar/privacy_button.h"

#include <memory>

#include "base/test/bind.h"
#include "chrome/browser/ui/atlas/toolbar/toolbar_ids.h"
#include "chrome/browser/ui/atlas/toolbar/toolbar_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace atlas::toolbar {

namespace {

class AtlasToolbarPrivacyButtonTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    prefs_.registry()->RegisterBooleanPref(::prefs::kEnableDoNotTrack, true);
    prefs_.registry()->RegisterBooleanPref(::prefs::kHeliumGlobalPrivacyControl,
                                           true);
    prefs_.registry()->RegisterBooleanPref(::prefs::kHeliumNoiseEnabled, true);
    prefs_.registry()->RegisterBooleanPref(kShowPrivacyButton, true);

    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    button_ = widget_->SetContentsView(std::make_unique<PrivacyButton>(
        &prefs_, base::BindRepeating(
                     [](int* count) { ++*count; }, &settings_opened_)));
    widget_->Show();
  }

  void TearDown() override {
    button_ = nullptr;
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  SkColor Color(ui::ColorId id) const {
    return button_->GetColorProvider()->GetColor(id);
  }

  TestingPrefServiceSimple prefs_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<PrivacyButton> button_ = nullptr;
  int settings_opened_ = 0;
};

TEST_F(AtlasToolbarPrivacyButtonTest, OnByDefaultWithTooltipAndName) {
  EXPECT_TRUE(button_->is_on());
  EXPECT_EQ(u"Do Not Track is on", PrivacyButton::TextFor(true));
  EXPECT_EQ(PrivacyButton::TextFor(true),
            button_->GetRenderedTooltipText(gfx::Point()));

  ui::AXNodeData data;
  button_->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(PrivacyButton::TextFor(true),
            data.GetString16Attribute(ax::mojom::StringAttribute::kName));
  EXPECT_EQ(ax::mojom::Role::kButton, data.role);
}

TEST_F(AtlasToolbarPrivacyButtonTest, GlyphIsAccentWhileDoNotTrackIsOn) {
  EXPECT_EQ(Color(ui::kColorAccent),
            button_->GetForegroundColor(views::Button::STATE_NORMAL));
  EXPECT_EQ(Color(ui::kColorAccent),
            button_->GetForegroundColor(views::Button::STATE_HOVERED));
}

TEST_F(AtlasToolbarPrivacyButtonTest, GlyphIsDimWhenThePersonTurnedItOff) {
  prefs_.SetBoolean(::prefs::kEnableDoNotTrack, false);
  EXPECT_FALSE(button_->is_on());
  EXPECT_EQ(u"Do Not Track is off", PrivacyButton::TextFor(false));
  EXPECT_EQ(PrivacyButton::TextFor(false),
            button_->GetRenderedTooltipText(gfx::Point()));
  EXPECT_EQ(Color(ui::kColorSecondaryForeground),
            button_->GetForegroundColor(views::Button::STATE_NORMAL));
  EXPECT_NE(Color(ui::kColorAccent),
            button_->GetForegroundColor(views::Button::STATE_NORMAL));

  // And back on.
  prefs_.SetBoolean(::prefs::kEnableDoNotTrack, true);
  EXPECT_TRUE(button_->is_on());
  EXPECT_EQ(Color(ui::kColorAccent),
            button_->GetForegroundColor(views::Button::STATE_NORMAL));
}

TEST_F(AtlasToolbarPrivacyButtonTest, WordsNeverSayBlocked) {
  EXPECT_EQ(std::u16string::npos, PrivacyButton::TextFor(true).find(u"block"));
  EXPECT_EQ(std::u16string::npos, PrivacyButton::TextFor(false).find(u"block"));
}

TEST_F(AtlasToolbarPrivacyButtonTest, HiddenWhenThePrefSaysSo) {
  EXPECT_TRUE(button_->GetVisible());
  prefs_.SetBoolean(kShowPrivacyButton, false);
  EXPECT_FALSE(button_->GetVisible());
  prefs_.SetBoolean(kShowPrivacyButton, true);
  EXPECT_TRUE(button_->GetVisible());
}

TEST_F(AtlasToolbarPrivacyButtonTest, CarriesItsElementIdentifier) {
  EXPECT_EQ(kPrivacyButtonElementId,
            button_->GetProperty(views::kElementIdentifierKey));
}

TEST_F(AtlasToolbarPrivacyButtonTest, SpaceOpensThePopoverAndEscapeClosesIt) {
  button_->RequestFocus();
  EXPECT_FALSE(button_->bubble().IsShowing());

  ui::KeyEvent space_press(ui::EventType::kKeyPressed, ui::VKEY_SPACE,
                           ui::EF_NONE);
  ui::KeyEvent space_release(ui::EventType::kKeyReleased, ui::VKEY_SPACE,
                             ui::EF_NONE);
  button_->OnKeyPressed(space_press);
  button_->OnKeyReleased(space_release);
  EXPECT_TRUE(button_->bubble().IsShowing());

  button_->bubble().Close();
  EXPECT_FALSE(button_->bubble().IsShowing());
}

}  // namespace

}  // namespace atlas::toolbar
