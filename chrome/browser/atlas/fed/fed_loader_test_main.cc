// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The main of atlas_fed_loader_unittests: a message loop, a thread pool and
// mojo, nothing else (//base/test:test_support needs test fonts the pruned
// tree lacks).
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/gtest/include/gtest/gtest.h"

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);
  testing::InitGoogleTest(&argc, argv);
  base::SingleThreadTaskExecutor main_task_executor;
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("AtlasFedLoader");
  mojo::core::Init();
  int result = RUN_ALL_TESTS();
  base::ThreadPoolInstance::Get()->Shutdown();
  return result;
}
