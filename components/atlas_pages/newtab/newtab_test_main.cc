// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The main of atlas_newtab_unittests: gtest and an at-exit manager
// (//base/test:test_support needs test fonts the pruned tree lacks).
#include "base/at_exit.h"
#include "base/command_line.h"
#include "testing/gtest/include/gtest/gtest.h"

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
