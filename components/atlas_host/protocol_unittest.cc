// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/atlas_host/protocol.h"

#include <string>
#include <variant>

#include "base/files/file_enumerator.h"
#include "components/atlas_host/test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace atlas_host {

namespace {

base::FilePath Fixture(const char* name) {
  return TestDataDir().AppendASCII(name);
}

}  // namespace

TEST(AtlasHostProtocolTest, ValidRequestsDecodeAndEncodeToTheSameBytes) {
  auto lines = ReadLines(Fixture("requests.valid.jsonl"));
  ASSERT_FALSE(lines.empty());
  for (const std::string& line : lines) {
    auto request = DecodeRequest(line);
    ASSERT_TRUE(request.has_value()) << line;
    EXPECT_EQ(EncodeRequest(*request), line + "\n");
  }
}

TEST(AtlasHostProtocolTest, InvalidRequestsAreRefused) {
  auto lines = ReadLines(Fixture("requests.invalid.jsonl"));
  ASSERT_FALSE(lines.empty());
  for (const std::string& line : lines) {
    EXPECT_FALSE(DecodeRequest(line).has_value()) << line;
  }
}

TEST(AtlasHostProtocolTest, ValidRepliesDecodeAndEncodeToTheSameBytes) {
  auto lines = ReadLines(Fixture("replies.valid.jsonl"));
  ASSERT_FALSE(lines.empty());
  for (const std::string& line : lines) {
    auto reply = DecodeReply(line);
    ASSERT_TRUE(reply.has_value()) << line;
    EXPECT_EQ(EncodeReply(*reply), line + "\n");
  }
}

TEST(AtlasHostProtocolTest, InvalidRepliesAreRefused) {
  auto lines = ReadLines(Fixture("replies.invalid.jsonl"));
  ASSERT_FALSE(lines.empty());
  for (const std::string& line : lines) {
    EXPECT_FALSE(DecodeReply(line).has_value()) << line;
  }
}

TEST(AtlasHostProtocolTest, EveryLineOfEverySessionIsSpokenAsWritten) {
  base::FileEnumerator files(Fixture("sessions"), false,
                             base::FileEnumerator::FILES, "*.txt");
  int sessions = 0;
  for (base::FilePath f = files.Next(); !f.empty(); f = files.Next()) {
    ++sessions;
    std::string name = f.BaseName().MaybeAsASCII();
    Session session = ReadSession(name, "4242", "/tmp/scratch/files");
    ASSERT_FALSE(session.steps.empty()) << name;
    for (const auto& step : session.steps) {
      if (!step.answered.empty()) {
        auto reply = DecodeReply(step.answered);
        ASSERT_TRUE(reply.has_value()) << name << ": " << step.answered;
        EXPECT_EQ(EncodeReply(*reply), step.answered + "\n") << name;
      }
      auto request = DecodeRequest(step.sent);
      if (request) {
        EXPECT_EQ(EncodeRequest(*request), step.sent + "\n") << name;
      } else {
        // A line the browser never sends: it is answered as invalid, or the
        // connection is refused.
        ASSERT_TRUE(!step.answered.empty()) << name << ": " << step.sent;
        auto reply = DecodeReply(step.answered);
        const auto* error = std::get_if<ErrorReply>(&*reply);
        ASSERT_NE(error, nullptr) << name << ": " << step.sent;
        EXPECT_TRUE(error->code == ErrorCode::kInvalidRequest ||
                    error->code == ErrorCode::kNotBrowser)
            << name << ": " << step.sent;
      }
    }
  }
  EXPECT_EQ(sessions, 12);
}

TEST(AtlasHostProtocolTest, FullPathsAreTheOnesTheProtocolTakes) {
  EXPECT_TRUE(IsFullPath("/a"));
  EXPECT_TRUE(IsFullPath("/home/ana/Downloads/résumé.pdf"));
  EXPECT_TRUE(IsFullPath("/home/ana/.hidden/..dots"));
  EXPECT_FALSE(IsFullPath(""));
  EXPECT_FALSE(IsFullPath("/"));
  EXPECT_FALSE(IsFullPath("a/b"));
  EXPECT_FALSE(IsFullPath("/a/"));
  EXPECT_FALSE(IsFullPath("/a//b"));
  EXPECT_FALSE(IsFullPath("/a/./b"));
  EXPECT_FALSE(IsFullPath("/a/../b"));
  EXPECT_FALSE(IsFullPath(std::string("/a\0b", 4)));
  EXPECT_FALSE(IsFullPath("/a\nb"));
  EXPECT_FALSE(IsFullPath("/a\x7f"));
  EXPECT_FALSE(IsFullPath("/a\xc2\x85"));  // U+0085, a control character.
  EXPECT_FALSE(IsFullPath("/a\xff"));      // Not UTF-8.
  EXPECT_TRUE(IsFullPath("/" + std::string(kMaxPath - 1, 'x')));
  EXPECT_FALSE(IsFullPath("/" + std::string(kMaxPath, 'x')));
}

TEST(AtlasHostProtocolTest, EncodingEscapesAsTheOtherEndDoes) {
  VerbRequest request{Verb::kTrash, 7, "/tmp/a\"b\\c d"};
  EXPECT_EQ(EncodeRequest(request),
            "{\"op\":\"trash\",\"id\":7,\"path\":\"/tmp/a\\\"b\\\\c d\"}\n");
  ErrorReply error{3, ErrorCode::kFailed, "one\ntwo\x01\t"};
  EXPECT_EQ(EncodeReply(error),
            "{\"reply\":\"error\",\"id\":3,\"code\":\"failed\","
            "\"text\":\"one\\ntwo\\u0001\\t\"}\n");
  auto back = DecodeReply(EncodeReply(error));
  ASSERT_TRUE(back.has_value());
  EXPECT_EQ(*back, Reply(error));
}

TEST(AtlasHostProtocolTest, WhatIsNotARequestOfThisVersionIsNotEncoded) {
  EXPECT_EQ(EncodeRequest(VerbRequest{Verb::kShare, 0, "/a"}), "");
  EXPECT_EQ(EncodeRequest(VerbRequest{Verb::kShare, 1, "a"}), "");
  EXPECT_EQ(EncodeRequest(Hello{0, 5}), "");
  EXPECT_EQ(EncodeRequest(Hello{1, 0}), "");
  EXPECT_EQ(EncodeRequest(Hello{1, 4242}),
            "{\"op\":\"hello\",\"version\":1,\"pid\":4242}\n");
}

TEST(AtlasHostProtocolTest, NumbersTakeTheWholeRangeOfAnId) {
  auto request = DecodeRequest(
      "{\"op\":\"trash\",\"id\":4294967295,\"path\":\"/tmp/x\"}");
  ASSERT_TRUE(request.has_value());
  EXPECT_EQ(std::get<VerbRequest>(*request).id, 4294967295u);
  EXPECT_FALSE(DecodeRequest(
      "{\"op\":\"trash\",\"id\":4294967296,\"path\":\"/tmp/x\"}"));
  EXPECT_FALSE(DecodeRequest("{\"op\":\"trash\",\"id\":1.5,\"path\":\"/x\"}"));
}

TEST(AtlasHostProtocolTest, LineReaderGivesWholeLinesOnly) {
  LineReader reader;
  reader.Append("{\"a\":1}\n{\"b\"");
  EXPECT_EQ(reader.Next(), "{\"a\":1}");
  EXPECT_FALSE(reader.Next().has_value());
  reader.Append(":2}\n\nlast");
  EXPECT_EQ(reader.Next(), "{\"b\":2}");
  EXPECT_EQ(reader.Next(), "");
  EXPECT_FALSE(reader.Next().has_value());
  EXPECT_FALSE(reader.overflowed());
}

TEST(AtlasHostProtocolTest, LineReaderRefusesALineOverTheLimit) {
  LineReader fits;
  fits.Append(std::string(kMaxLine - 1, 'x'));
  EXPECT_FALSE(fits.overflowed());
  fits.Append("\n");
  EXPECT_EQ(fits.Next()->size(), kMaxLine - 1);

  LineReader too_long;
  too_long.Append(std::string(kMaxLine, 'x'));
  EXPECT_TRUE(too_long.overflowed());
  EXPECT_FALSE(too_long.Next().has_value());

  LineReader too_long_with_newline;
  too_long_with_newline.Append(std::string(kMaxLine, 'x') + "\n");
  EXPECT_TRUE(too_long_with_newline.overflowed());
}

TEST(AtlasHostProtocolTest, OurTextsAreSentencesWithoutAPath) {
  for (const char* text :
       {kTextUnreachable, kTextNoAnswer, kTextLost, kTextBadAnswer,
        kTextBadName, kTextTooMany}) {
    std::string t = text;
    EXPECT_EQ(t.find('/'), std::string::npos) << t;
    EXPECT_EQ(t.find('!'), std::string::npos) << t;
    EXPECT_TRUE(t.ends_with('.')) << t;
    EXPECT_TRUE(std::isupper(static_cast<unsigned char>(t[0]))) << t;
  }
}

}  // namespace atlas_host
