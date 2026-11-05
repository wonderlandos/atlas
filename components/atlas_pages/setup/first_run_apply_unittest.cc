// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2026 The Atlas Authors.

#include "components/atlas_pages/setup/first_run_apply.h"

#include <map>
#include <string>
#include <vector>

#if defined(ATLAS_STANDALONE_TEST)
#include "components/atlas_pages/setup/mini_gtest.h"
#else
#include "testing/gtest/include/gtest/gtest.h"
#endif

namespace atlas_pages::setup {
namespace {

// A fake prefs service and blocker: records every write in order.
class FakeEnvironment : public Environment {
 public:
  void SetDoNotTrack(bool on) override { Write("dnt", on); }
  void SetGlobalPrivacyControl(bool on) override { Write("gpc", on); }
  void SetTrackerBlocking(bool on) override { Write("blocker", on); }
  void MarkCompleted() override { Write("completed", true); }

  bool Has(const std::string& key) const { return values_.count(key) != 0; }
  bool Get(const std::string& key) const { return values_.at(key); }
  const std::vector<std::string>& order() const { return order_; }

 private:
  void Write(const std::string& key, bool value) {
    values_[key] = value;
    order_.push_back(key);
  }

  std::map<std::string, bool> values_;
  std::vector<std::string> order_;
};

TEST(AtlasFirstRunApplyTest, DefaultsFinishWritesThreeChoicesAndCompletion) {
  FakeEnvironment env;
  EXPECT_EQ(Apply(Choices(), env), NextPage::kNewTab);
  EXPECT_TRUE(env.Get("dnt"));
  EXPECT_TRUE(env.Get("gpc"));
  EXPECT_TRUE(env.Get("blocker"));
  EXPECT_TRUE(env.Get("completed"));
}

TEST(AtlasFirstRunApplyTest, ChoicesAreWrittenAsGiven) {
  FakeEnvironment env;
  Choices c;
  c.do_not_track = false;
  c.global_privacy_control = true;
  c.tracker_blocking = false;
  Apply(c, env);
  EXPECT_FALSE(env.Get("dnt"));
  EXPECT_TRUE(env.Get("gpc"));
  EXPECT_FALSE(env.Get("blocker"));
}

TEST(AtlasFirstRunApplyTest, EachSwitchIsIndependent) {
  for (int mask = 0; mask < 8; ++mask) {
    FakeEnvironment env;
    Choices c;
    c.do_not_track = mask & 1;
    c.global_privacy_control = mask & 2;
    c.tracker_blocking = mask & 4;
    Apply(c, env);
    EXPECT_EQ(env.Get("dnt"), static_cast<bool>(mask & 1));
    EXPECT_EQ(env.Get("gpc"), static_cast<bool>(mask & 2));
    EXPECT_EQ(env.Get("blocker"), static_cast<bool>(mask & 4));
  }
}

TEST(AtlasFirstRunApplyTest, SkipKeepsThePrivacySwitchesAsShownAndImportsNothing) {
  FakeEnvironment env;
  Choices c;
  c.skipped = true;
  // The switches the person turned off before skipping stay off: the page says a skip
  // "keeps the privacy settings you chose".
  c.do_not_track = false;
  c.global_privacy_control = true;
  c.tracker_blocking = false;
  // An import choice does not survive a skip.
  c.start = Choices::Start::kImport;
  EXPECT_EQ(Apply(c, env), NextPage::kNewTab);
  EXPECT_FALSE(env.Get("dnt"));
  EXPECT_TRUE(env.Get("gpc"));
  EXPECT_FALSE(env.Get("blocker"));
  EXPECT_TRUE(env.Get("completed"));
}

TEST(AtlasFirstRunApplyTest, CompletionIsRecordedAfterTheChoices) {
  FakeEnvironment env;
  Apply(Choices(), env);
  EXPECT_EQ(env.order().size(), static_cast<size_t>(4));
  EXPECT_EQ(env.order().back(), std::string("completed"));
}

TEST(AtlasFirstRunApplyTest, ImportOpensTheImportPageOtherwiseTheNewTab) {
  FakeEnvironment a;
  Choices c;
  c.start = Choices::Start::kImport;
  EXPECT_EQ(Apply(c, a), NextPage::kImportData);
  FakeEnvironment b;
  c.start = Choices::Start::kClean;
  EXPECT_EQ(Apply(c, b), NextPage::kNewTab);
}

}  // namespace
}  // namespace atlas_pages::setup

#if defined(ATLAS_STANDALONE_TEST)
int main() {
  return mini_gtest::RunAll();
}
#endif
