// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/brand/wolos_release.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace atlas::brand {

namespace {

// The value of KEY in an os-release text: `KEY=value` or `KEY="value"`, one per
// line.
std::string OsReleaseValue(std::string_view os_release, std::string_view key) {
  for (std::string_view line :
       base::SplitStringPiece(os_release, "\n", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    std::vector<std::string_view> parts = base::SplitStringPiece(
        line, "=", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (parts.size() < 2 || parts[0] != key) {
      continue;
    }
    std::string_view value = line.substr(line.find('=') + 1);
    value = base::TrimWhitespaceASCII(value, base::TRIM_ALL);
    if (value.size() >= 2 && (value.front() == '"' || value.front() == '\'') &&
        value.back() == value.front()) {
      value = value.substr(1, value.size() - 2);
    }
    return std::string(value);
  }
  return std::string();
}

std::string ReadWolosRelease() {
  std::string text;
  if (!base::ReadFileToString(base::FilePath("/etc/os-release"), &text) &&
      !base::ReadFileToString(base::FilePath("/usr/lib/os-release"), &text)) {
    return std::string();
  }
  return ParseWolosRelease(text);
}

}  // namespace

std::string ParseWolosRelease(std::string_view os_release) {
  if (!base::EqualsCaseInsensitiveASCII(OsReleaseValue(os_release, "ID"),
                                        "wolos")) {
    return std::string();
  }
  std::string release = OsReleaseValue(os_release, "VERSION_ID");
  if (release.empty()) {
    release = OsReleaseValue(os_release, "VERSION");
  }
  return release;
}

const std::string& GetWolosRelease() {
  static const base::NoDestructor<std::string> release(ReadWolosRelease());
  return *release;
}

}  // namespace atlas::brand
