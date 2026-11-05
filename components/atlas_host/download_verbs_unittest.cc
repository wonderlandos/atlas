// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/atlas_host/download_verbs.h"

#include <string>
#include <vector>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace atlas_host {

namespace {

// A host that is told what it is, and records what it is asked.
class FakeHost : public AtlasHost {
 public:
  struct Asked {
    Verb verb;
    std::string path;
  };

  bool IsAvailable() const override { return available_; }
  void EnsureConnected() override { ++ensure_calls_; }
  void Request(Verb verb,
               const base::FilePath& path,
               ResultCallback callback) override {
    asked_.push_back({verb, path.value()});
    std::move(callback).Run(Result::Ok(Outcome::kCopied));
  }
  void AddObserver(Observer*) override {}
  void RemoveObserver(Observer*) override {}

  bool available_ = false;
  int ensure_calls_ = 0;
  std::vector<Asked> asked_;
};

}  // namespace

TEST(AtlasHostDownloadVerbsTest, IdsAreTheThreeVerbsAndNoEngineCommand) {
  EXPECT_TRUE(IsDownloadVerbCommand(kCommandOpenWith));
  EXPECT_TRUE(IsDownloadVerbCommand(kCommandShare));
  EXPECT_TRUE(IsDownloadVerbCommand(kCommandMoveToTrash));
  // The engine's commands are numbered from 0 and stay below 100 (the hook
  // asserts it against DownloadCommands::kMaxValue).
  for (int engine_command = 0; engine_command < 64; ++engine_command) {
    EXPECT_FALSE(IsDownloadVerbCommand(engine_command));
  }
}

TEST(AtlasHostDownloadVerbsTest, NothingIsOfferedWithoutAHost) {
  base::test::SingleThreadTaskEnvironment task_environment;
  EXPECT_EQ(AtlasHost::Get(), nullptr);
  EXPECT_FALSE(DownloadVerbsAvailable());
  base::test::TestFuture<Result> future;
  RunDownloadVerb(kCommandShare, base::FilePath("/tmp/x"),
                  future.GetCallback());
  Result r = future.Take();
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(r.text, kTextUnreachable);
}

TEST(AtlasHostDownloadVerbsTest, VerbsAreOfferedOnlyWhenTheBridgeAnswered) {
  base::test::SingleThreadTaskEnvironment task_environment;
  FakeHost host;
  AtlasHost::ScopedOverride use(&host);
  EXPECT_FALSE(DownloadVerbsAvailable());
  EXPECT_EQ(host.ensure_calls_, 1);  // Asked for a connection for next time.
  host.available_ = true;
  EXPECT_TRUE(DownloadVerbsAvailable());
  EXPECT_EQ(host.ensure_calls_, 1);
}

TEST(AtlasHostDownloadVerbsTest, EachIdRunsItsVerbOnThePath) {
  base::test::SingleThreadTaskEnvironment task_environment;
  FakeHost host;
  host.available_ = true;
  AtlasHost::ScopedOverride use(&host);
  struct {
    int command;
    Verb verb;
  } cases[] = {
      {kCommandOpenWith, Verb::kOpenWith},
      {kCommandShare, Verb::kShare},
      {kCommandMoveToTrash, Verb::kTrash},
  };
  for (const auto& c : cases) {
    base::test::TestFuture<Result> future;
    RunDownloadVerb(c.command, base::FilePath("/home/ana/Downloads/a.pdf"),
                    future.GetCallback());
    EXPECT_TRUE(future.Take().ok);
  }
  ASSERT_EQ(host.asked_.size(), 3u);
  for (size_t i = 0; i < 3; ++i) {
    EXPECT_EQ(host.asked_[i].verb, cases[i].verb);
    EXPECT_EQ(host.asked_[i].path, "/home/ana/Downloads/a.pdf");
  }
}

TEST(AtlasHostDownloadVerbsTest, ACommandOfTheEngineIsNotAVerb) {
  base::test::SingleThreadTaskEnvironment task_environment;
  FakeHost host;
  host.available_ = true;
  AtlasHost::ScopedOverride use(&host);
  base::test::TestFuture<Result> future;
  RunDownloadVerb(3, base::FilePath("/tmp/x"), future.GetCallback());
  EXPECT_FALSE(future.Take().ok);
  EXPECT_TRUE(host.asked_.empty());
}

}  // namespace atlas_host
