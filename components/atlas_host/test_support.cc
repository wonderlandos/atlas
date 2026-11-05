// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/atlas_host/test_support.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace atlas_host {

base::FilePath TestDataDir() {
  base::FilePath root;
  EXPECT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &root));
  return root.AppendASCII("components/atlas_host/testdata");
}

std::vector<std::string> ReadLines(const base::FilePath& path) {
  std::string text;
  EXPECT_TRUE(base::ReadFileToString(path, &text)) << path;
  std::vector<std::string> lines;
  // Keep the lines exactly: a fixture line may start with a space.
  for (const std::string& line : base::SplitString(
           text, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    lines.push_back(line);
  }
  return lines;
}

Session ReadSession(const std::string& name,
                    const std::string& pid,
                    const std::string& dir) {
  Session session;
  auto sub = [&](std::string s) {
    base::ReplaceSubstringsAfterOffset(&s, 0, "@PID", pid);
    base::ReplaceSubstringsAfterOffset(&s, 0, "@DIR", dir);
    return s;
  };
  for (const std::string& line :
       ReadLines(TestDataDir().AppendASCII("sessions").AppendASCII(name))) {
    if (line.starts_with("> ")) {
      session.steps.emplace_back();
      session.steps.back().sent = sub(line.substr(2));
    } else if (line.starts_with("< ")) {
      EXPECT_FALSE(session.steps.empty()) << name;
      session.steps.back().answered = sub(line.substr(2));
    } else if (line == "closed") {
      EXPECT_FALSE(session.steps.empty()) << name;
      session.steps.back().closes = true;
    }
    // The other lines set up and check the fake desktop of the bridge's own
    // tests; a fake bridge here ignores them.
  }
  return session;
}

}  // namespace atlas_host
