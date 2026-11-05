// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/atlas/fed/fed_protocol.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace atlas_fed {

TEST(AtlasFedProtocolTest, RequestLineCarriesKindAndUrl) {
  EXPECT_EQ(RequestLine("fed://home.fed/", LoadKind::kDocument),
            "FED/1 D fed://home.fed/\n");
  EXPECT_EQ(RequestLine("fed://home.fed/a.css", LoadKind::kSubresource),
            "FED/1 S fed://home.fed/a.css\n");
}

TEST(AtlasFedProtocolTest, RequestLineRefusesWhatCannotTravel) {
  EXPECT_FALSE(RequestLine("", LoadKind::kDocument));
  EXPECT_FALSE(RequestLine("fed://home.fed/a b", LoadKind::kDocument));
  EXPECT_FALSE(RequestLine("fed://home.fed/\n", LoadKind::kDocument));
  EXPECT_FALSE(RequestLine("fed://home.fed/\x7f", LoadKind::kDocument));
  std::string too_long = "fed://home.fed/" + std::string(kMaxUrl, 'a');
  EXPECT_FALSE(RequestLine(too_long, LoadKind::kDocument));
}

TEST(AtlasFedProtocolTest, ReplyHeadParses) {
  auto head = ParseReplyHead("FED/1 200 4096 text/html; charset=utf-8");
  ASSERT_TRUE(head);
  EXPECT_EQ(head->status, 200);
  EXPECT_EQ(head->length, 4096u);
  EXPECT_EQ(head->mime, "text/html; charset=utf-8");
}

TEST(AtlasFedProtocolTest, ReplyHeadWithoutMimeIsOctetStream) {
  auto head = ParseReplyHead("FED/1 404 0 ");
  ASSERT_TRUE(head);
  EXPECT_EQ(head->status, 404);
  EXPECT_EQ(head->mime, "application/octet-stream");
}

TEST(AtlasFedProtocolTest, ReplyHeadRefusesAnythingElse) {
  EXPECT_FALSE(ParseReplyHead(""));
  EXPECT_FALSE(ParseReplyHead("HTTP/1.1 200 1 text/html"));
  EXPECT_FALSE(ParseReplyHead("FED/1 99 1 text/html"));
  EXPECT_FALSE(ParseReplyHead("FED/1 600 1 text/html"));
  EXPECT_FALSE(ParseReplyHead("FED/1 200 x text/html"));
  EXPECT_FALSE(ParseReplyHead("FED/1 200"));
  // A body over the cap (64 MiB).
  EXPECT_FALSE(ParseReplyHead("FED/1 200 67108865 text/html"));
  EXPECT_TRUE(ParseReplyHead("FED/1 200 67108864 text/html"));
  // A control character in the type.
  EXPECT_FALSE(ParseReplyHead("FED/1 200 1 text/html\x01"));
}

}  // namespace atlas_fed
