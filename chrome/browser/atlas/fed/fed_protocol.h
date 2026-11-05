// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ATLAS_FED_FED_PROTOCOL_H_
#define CHROME_BROWSER_ATLAS_FED_FED_PROTOCOL_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// The helper's protocol FED/1 (crates/atlas-fed of WolOS, src/lib.rs), one
// request per connection:
//   request  "FED/1 D <url>\n" (a document) or "FED/1 S <url>\n" (a subresource)
//   reply    "FED/1 <status> <length> <mime>\n" and exactly <length> body bytes
// Pure functions, so the codec is tested without a socket.
namespace atlas_fed {

inline constexpr char kProtocol[] = "FED/1";
// Longest URL a request carries (the helper refuses longer ones).
inline constexpr size_t kMaxUrl = 4096;
// Longest reply header line.
inline constexpr size_t kMaxHead = 512;
// Largest body: the protocol's block cap for a document (64 MiB).
inline constexpr uint64_t kMaxBody = uint64_t{64} << 20;

enum class LoadKind { kDocument, kSubresource };

struct ReplyHead {
  int status = 0;
  uint64_t length = 0;
  std::string mime;
};

// The request line for `url`, or nullopt when it cannot travel (too long, a
// space or a control character: GURL's spelling never has them).
std::optional<std::string> RequestLine(std::string_view url, LoadKind kind);

// The reply header line (without its newline), or nullopt when it is not one.
std::optional<ReplyHead> ParseReplyHead(std::string_view line);

}  // namespace atlas_fed

#endif  // CHROME_BROWSER_ATLAS_FED_FED_PROTOCOL_H_
