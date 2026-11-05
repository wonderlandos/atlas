// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_ui_context_menu.h"
#include "components/atlas_host/atlas_host.h"
#include "components/atlas_host/download_verbs.h"
#include "components/download/public/common/mock_download_item.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/menus/simple_menu_model.h"

using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

namespace {

// A host that is told what it is, and records what it is asked. It answers a
// verb with what the test says.
class FakeHost : public atlas_host::AtlasHost {
 public:
  struct Asked {
    atlas_host::Verb verb;
    std::string path;
  };

  bool IsAvailable() const override { return available_; }
  void EnsureConnected() override {}
  void Request(atlas_host::Verb verb,
               const base::FilePath& path,
               atlas_host::ResultCallback callback) override {
    asked_.push_back({verb, path.value()});
    pending_.push_back(std::move(callback));
  }
  void AddObserver(Observer*) override {}
  void RemoveObserver(Observer*) override {}

  bool available_ = true;
  std::vector<Asked> asked_;
  std::vector<atlas_host::ResultCallback> pending_;
};

}  // namespace

class AtlasDownloadUiContextMenuTest : public testing::Test {
 protected:
  void SetUp() override {
    item_ = std::make_unique<NiceMock<download::MockDownloadItem>>();
    full_path_ = base::FilePath("/home/ana/Downloads/report.pdf");
    ON_CALL(*item_, GetState())
        .WillByDefault(Return(download::DownloadItem::COMPLETE));
    ON_CALL(*item_, IsDone()).WillByDefault(Return(true));
    ON_CALL(*item_, CanShowInFolder()).WillByDefault(Return(true));
    ON_CALL(*item_, CanOpenDownload()).WillByDefault(Return(true));
    ON_CALL(*item_, GetFullPath()).WillByDefault(ReturnRef(full_path_));
    ON_CALL(*item_, GetFileNameToReportUser())
        .WillByDefault(Return(base::FilePath("report.pdf")));
    model_ = DownloadItemModel::Wrap(item_.get());
    // Don't use std::make_unique because it needs friend.
    menu_ = base::WrapUnique(new DownloadUiContextMenu(model_->GetWeakPtr()));
  }

  // The menu's delegate functions are protected; this fixture is its friend.
  ui::SimpleMenuModel* GetMenuModel() { return menu_->GetMenuModel(); }
  bool Enabled(int command) { return menu_->IsCommandIdEnabled(command); }
  bool Visible(int command) { return menu_->IsCommandIdVisible(command); }
  bool Checked(int command) { return menu_->IsCommandIdChecked(command); }
  void Execute(int command) { menu_->ExecuteCommand(command, 0); }

  std::vector<std::u16string> Labels(ui::SimpleMenuModel* model) {
    std::vector<std::u16string> labels;
    for (size_t i = 0; i < model->GetItemCount(); ++i) {
      labels.push_back(model->GetTypeAt(i) == ui::MenuModel::TYPE_SEPARATOR
                           ? u"-"
                           : model->GetLabelAt(i));
    }
    return labels;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::FilePath full_path_;
  std::unique_ptr<download::MockDownloadItem> item_;
  DownloadUIModel::DownloadUIModelPtr model_;
  std::unique_ptr<DownloadUiContextMenu> menu_;
};

// The reel's menu: Open, Open With..., Share..., Show in Files, a separator,
// Move to Trash; the destructive item last. (The & marks the mnemonic letter;
// the menu shows it as an underline only while Alt is held.)
TEST_F(AtlasDownloadUiContextMenuTest, FinishedMenuIsTheReelsWhenTheBridgeIsThere) {
  FakeHost host;
  atlas_host::AtlasHost::ScopedOverride use(&host);
  ui::SimpleMenuModel* model = GetMenuModel();
  ASSERT_NE(model, nullptr);
  EXPECT_EQ(Labels(model),
            (std::vector<std::u16string>{u"&Open", u"Open &With…", u"&Share…",
                                         u"Show in &Files", u"-",
                                         u"&Move to Trash"}));
  EXPECT_EQ(model->GetCommandIdAt(0), DownloadCommands::OPEN_WHEN_COMPLETE);
  EXPECT_EQ(model->GetCommandIdAt(1), atlas_host::kCommandOpenWith);
  EXPECT_EQ(model->GetCommandIdAt(2), atlas_host::kCommandShare);
  EXPECT_EQ(model->GetCommandIdAt(3), DownloadCommands::SHOW_IN_FOLDER);
  EXPECT_EQ(model->GetCommandIdAt(5), atlas_host::kCommandMoveToTrash);
  for (int command : {atlas_host::kCommandOpenWith, atlas_host::kCommandShare,
                      atlas_host::kCommandMoveToTrash}) {
    EXPECT_TRUE(Enabled(command));
    EXPECT_TRUE(Visible(command));
    EXPECT_FALSE(Checked(command));
  }
}

TEST_F(AtlasDownloadUiContextMenuTest, ItemsHaveNamesThatSayWhichFile) {
  FakeHost host;
  atlas_host::AtlasHost::ScopedOverride use(&host);
  ui::SimpleMenuModel* model = GetMenuModel();
  ASSERT_NE(model, nullptr);
  EXPECT_EQ(model->GetAccessibleNameAt(1), u"Open report.pdf with another app");
  EXPECT_EQ(model->GetAccessibleNameAt(2), u"Share report.pdf");
  EXPECT_EQ(model->GetAccessibleNameAt(3), u"Show report.pdf in Files");
  EXPECT_EQ(model->GetAccessibleNameAt(5), u"Move report.pdf to the Trash");
}

// No bridge: the engine's own menu, no entry that would do nothing.
TEST_F(AtlasDownloadUiContextMenuTest, WithoutTheBridgeTheEnginesMenuIsKept) {
  ui::SimpleMenuModel* model = GetMenuModel();
  ASSERT_NE(model, nullptr);
  for (int command : {atlas_host::kCommandOpenWith, atlas_host::kCommandShare,
                      atlas_host::kCommandMoveToTrash}) {
    EXPECT_FALSE(model->GetIndexOfCommandId(command).has_value());
    EXPECT_FALSE(Enabled(command));
    EXPECT_FALSE(Visible(command));
  }
  EXPECT_TRUE(model->GetIndexOfCommandId(DownloadCommands::PLATFORM_OPEN)
                  .has_value());
}

TEST_F(AtlasDownloadUiContextMenuTest, TheMenuFollowsTheBridgeAtEachShowing) {
  FakeHost host;
  host.available_ = false;
  atlas_host::AtlasHost::ScopedOverride use(&host);
  ui::SimpleMenuModel* without = GetMenuModel();
  EXPECT_FALSE(without->GetIndexOfCommandId(atlas_host::kCommandShare));
  host.available_ = true;
  ui::SimpleMenuModel* with = GetMenuModel();
  EXPECT_NE(with, without);
  EXPECT_TRUE(with->GetIndexOfCommandId(atlas_host::kCommandShare));
}

// A row whose file is gone offers nothing to send.
TEST_F(AtlasDownloadUiContextMenuTest, VerbsNeedTheFile) {
  FakeHost host;
  atlas_host::AtlasHost::ScopedOverride use(&host);
  ON_CALL(*item_, CanShowInFolder()).WillByDefault(Return(false));
  EXPECT_FALSE(Enabled(atlas_host::kCommandShare));
  EXPECT_FALSE(Enabled(atlas_host::kCommandMoveToTrash));
}

TEST_F(AtlasDownloadUiContextMenuTest, EachVerbAsksTheHostForTheFile) {
  FakeHost host;
  atlas_host::AtlasHost::ScopedOverride use(&host);
  Execute(atlas_host::kCommandShare);
  Execute(atlas_host::kCommandOpenWith);
  Execute(atlas_host::kCommandMoveToTrash);
  ASSERT_EQ(host.asked_.size(), 3u);
  EXPECT_EQ(host.asked_[0].verb, atlas_host::Verb::kShare);
  EXPECT_EQ(host.asked_[1].verb, atlas_host::Verb::kOpenWith);
  EXPECT_EQ(host.asked_[2].verb, atlas_host::Verb::kTrash);
  for (const auto& asked : host.asked_) {
    EXPECT_EQ(asked.path, "/home/ana/Downloads/report.pdf");
  }
}

// The person closes the sheet: nothing to say, nothing removed.
TEST_F(AtlasDownloadUiContextMenuTest, ACancelledSheetIsSilent) {
  FakeHost host;
  atlas_host::AtlasHost::ScopedOverride use(&host);
  Execute(atlas_host::kCommandShare);
  ASSERT_EQ(host.pending_.size(), 1u);
  std::move(host.pending_[0])
      .Run(atlas_host::Result::Ok(atlas_host::Outcome::kCancelled));
  task_environment_.RunUntilIdle();
}

// The menu may be gone when the answer comes.
TEST_F(AtlasDownloadUiContextMenuTest, AnAnswerAfterTheMenuIsGoneIsIgnored) {
  FakeHost host;
  atlas_host::AtlasHost::ScopedOverride use(&host);
  Execute(atlas_host::kCommandShare);
  menu_.reset();
  ASSERT_EQ(host.pending_.size(), 1u);
  std::move(host.pending_[0])
      .Run(atlas_host::Result::Ok(atlas_host::Outcome::kCopied));
  task_environment_.RunUntilIdle();
}

// A row that is gone does not ask.
TEST_F(AtlasDownloadUiContextMenuTest, ADestroyedDownloadAsksNothing) {
  FakeHost host;
  atlas_host::AtlasHost::ScopedOverride use(&host);
  model_.reset();
  Execute(atlas_host::kCommandShare);
  EXPECT_TRUE(host.asked_.empty());
  EXPECT_FALSE(Enabled(atlas_host::kCommandShare));
}
