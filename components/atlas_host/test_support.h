// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATLAS_HOST_TEST_SUPPORT_H_
#define COMPONENTS_ATLAS_HOST_TEST_SUPPORT_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"

namespace atlas_host {

// components/atlas_host/testdata: the fixtures shared byte for byte with the
// bridge.
base::FilePath TestDataDir();

// The non-empty lines of a fixture file, without their newlines.
std::vector<std::string> ReadLines(const base::FilePath& path);

// A session transcript (testdata/sessions/*.txt): the lines the browser sends
// ("> ") and the lines the bridge answers ("< "), with @PID and @DIR replaced.
struct Session {
  struct Step {
    std::string sent;      // What the browser sends.
    std::string answered;  // What the bridge answers; empty if it closes.
    bool closes = false;
  };
  std::vector<Step> steps;
};
Session ReadSession(const std::string& name,
                    const std::string& pid,
                    const std::string& dir);

}  // namespace atlas_host

#endif  // COMPONENTS_ATLAS_HOST_TEST_SUPPORT_H_
