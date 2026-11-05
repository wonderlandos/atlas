// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/atlas/fed/fed_protocol.h"

#include <algorithm>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"

namespace atlas_fed {

std::optional<std::string> RequestLine(std::string_view url, LoadKind kind) {
  if (url.empty() || url.size() > kMaxUrl) {
    return std::nullopt;
  }
  if (!std::ranges::all_of(url, [](char c) { return c > 0x20 && c < 0x7f; })) {
    return std::nullopt;
  }
  std::string line(kProtocol);
  line += kind == LoadKind::kDocument ? " D " : " S ";
  line += url;
  line += '\n';
  return line;
}

std::optional<ReplyHead> ParseReplyHead(std::string_view line) {
  if (line.size() > kMaxHead) {
    return std::nullopt;
  }
  std::string_view rest = line;
  auto next = [&rest]() -> std::optional<std::string_view> {
    size_t sp = rest.find(' ');
    if (sp == std::string_view::npos) {
      return std::nullopt;
    }
    std::string_view word = rest.substr(0, sp);
    rest = rest.substr(sp + 1);
    return word;
  };
  std::optional<std::string_view> proto = next();
  std::optional<std::string_view> status = next();
  std::optional<std::string_view> length = next();
  if (!proto || *proto != kProtocol || !status || !length) {
    return std::nullopt;
  }
  ReplyHead head;
  if (!base::StringToInt(*status, &head.status) || head.status < 100 ||
      head.status > 599) {
    return std::nullopt;
  }
  if (!base::StringToUint64(*length, &head.length) ||
      head.length > kMaxBody) {
    return std::nullopt;
  }
  if (!std::ranges::all_of(rest, [](char c) { return c >= 0x20 && c < 0x7f; })) {
    return std::nullopt;
  }
  head.mime = std::string(base::TrimWhitespaceASCII(rest, base::TRIM_ALL));
  if (head.mime.empty()) {
    head.mime = "application/octet-stream";
  }
  return head;
}

}  // namespace atlas_fed
