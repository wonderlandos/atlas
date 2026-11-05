// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/atlas/fed/fed_helper_client.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <string>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/posix/eintr_wrapper.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace atlas_fed {

namespace {

// A stand-in helper: accepts one connection, reads the request line, answers
// with `reply` (raw bytes) and closes.
class FakeHelper : public base::SimpleThread {
 public:
  FakeHelper(const base::FilePath& path, std::string reply)
      : base::SimpleThread("FakeFedHelper"), reply_(std::move(reply)) {
    sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    base::span(addr.sun_path)
        .first(path.value().size())
        .copy_from(base::span(path.value()));
    listener_.reset(socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0));
    EXPECT_EQ(0, bind(listener_.get(), reinterpret_cast<sockaddr*>(&addr),
                      sizeof(addr)));
    EXPECT_EQ(0, listen(listener_.get(), 4));
  }

  const std::string& request() const { return request_; }

  void Run() override {
    base::ScopedFD conn(HANDLE_EINTR(accept(listener_.get(), nullptr, nullptr)));
    if (!conn.is_valid()) {
      return;
    }
    char c;
    while (HANDLE_EINTR(recv(conn.get(), &c, 1, 0)) == 1) {
      request_.push_back(c);
      if (c == '\n') {
        break;
      }
    }
    send(conn.get(), reply_.data(), reply_.size(), MSG_NOSIGNAL);
  }

 private:
  base::ScopedFD listener_;
  std::string reply_;
  std::string request_;
};

}  // namespace

TEST(AtlasFedHelperClientTest, AnAnswerComesBackWithStatusTypeAndBody) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath socket = dir.GetPath().AppendASCII("fed.sock");
  FakeHelper helper(socket,
                    "FED/1 200 5 text/html; charset=utf-8\n<p>x</p>"
                    "ignored after the length");
  helper.Start();
  FedResult r = FetchFromHelper(socket, base::FilePath(), "fed://home.fed/",
                                LoadKind::kDocument, base::Seconds(10));
  helper.Join();
  EXPECT_EQ(r.net_error, net::OK);
  EXPECT_EQ(r.status, 200);
  EXPECT_EQ(r.mime, "text/html; charset=utf-8");
  EXPECT_EQ(r.body, "<p>x<");
  EXPECT_EQ(helper.request(), "FED/1 D fed://home.fed/\n");
}

TEST(AtlasFedHelperClientTest, ARefusalPageIsAnAnswerWithItsStatus) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath socket = dir.GetPath().AppendASCII("fed.sock");
  FakeHelper helper(socket, "FED/1 502 3 text/html\nbad");
  helper.Start();
  FedResult r = FetchFromHelper(socket, base::FilePath(), "fed://a.fed/x.css",
                                LoadKind::kSubresource, base::Seconds(10));
  helper.Join();
  EXPECT_EQ(r.net_error, net::OK);
  EXPECT_EQ(r.status, 502);
  EXPECT_EQ(r.body, "bad");
  EXPECT_EQ(helper.request(), "FED/1 S fed://a.fed/x.css\n");
}

TEST(AtlasFedHelperClientTest, ABodyCutShortIsAConnectionError) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath socket = dir.GetPath().AppendASCII("fed.sock");
  FakeHelper helper(socket, "FED/1 200 100 text/html\nshort");
  helper.Start();
  FedResult r = FetchFromHelper(socket, base::FilePath(), "fed://a.fed/",
                                LoadKind::kDocument, base::Seconds(10));
  helper.Join();
  EXPECT_EQ(r.net_error, net::ERR_CONNECTION_CLOSED);
  EXPECT_TRUE(r.body.empty());
}

TEST(AtlasFedHelperClientTest, ANonsenseReplyIsAnInvalidResponse) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath socket = dir.GetPath().AppendASCII("fed.sock");
  FakeHelper helper(socket, "HTTP/1.1 200 OK\n");
  helper.Start();
  FedResult r = FetchFromHelper(socket, base::FilePath(), "fed://a.fed/",
                                LoadKind::kDocument, base::Seconds(10));
  helper.Join();
  EXPECT_EQ(r.net_error, net::ERR_INVALID_RESPONSE);
}

TEST(AtlasFedHelperClientTest, NobodyThereAndNoHelperToStartIsRefused) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FedResult r = FetchFromHelper(dir.GetPath().AppendASCII("none.sock"),
                                dir.GetPath().AppendASCII("no-such-helper"),
                                "fed://a.fed/", LoadKind::kDocument,
                                base::Seconds(1));
  EXPECT_EQ(r.net_error, net::ERR_CONNECTION_REFUSED);
}

// QA-A9-WEB-ATLAS-9: a missing helper is answered at once and says what is
// missing (no 3 s of polling per load).
TEST(AtlasFedHelperClientTest, AMissingHelperAnswersAtOnceAndNamesItself) {
  ResetHelperStartMemoryForTesting();
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::TimeTicks start = base::TimeTicks::Now();
  FedResult r = FetchFromHelper(dir.GetPath().AppendASCII("none.sock"),
                                dir.GetPath().AppendASCII("no-such-helper"),
                                "fed://a.fed/", LoadKind::kDocument,
                                base::Seconds(1));
  EXPECT_LT(base::TimeTicks::Now() - start, base::Milliseconds(100));
  EXPECT_EQ(r.net_error, net::ERR_CONNECTION_REFUSED);
  EXPECT_EQ(r.fault, HelperFault::kHelperMissing);
  FedResult page = FaultPageFor(r.fault);
  EXPECT_EQ(page.net_error, net::OK);
  EXPECT_EQ(page.status, 503);
  EXPECT_NE(page.body.find("atlas-fed"), std::string::npos);
  EXPECT_NE(page.body.find("WolOS cannot check Federate pages"),
            std::string::npos);
}

TEST(AtlasFedHelperClientTest, NoRuntimeFolderIsAFaultOfItsOwn) {
  FedResult r = FetchFromHelper(base::FilePath(), base::FilePath(),
                                "fed://a.fed/", LoadKind::kDocument,
                                base::Seconds(1));
  EXPECT_EQ(r.fault, HelperFault::kNoRuntimeDir);
  EXPECT_NE(FaultPageFor(r.fault).body.find("XDG_RUNTIME_DIR"),
            std::string::npos);
}

// A helper that starts and leaves at once (it cannot make its socket) is not
// waited for 3 s, and the next loads do not start it or wait again.
TEST(AtlasFedHelperClientTest, AHelperThatLeavesAtOnceIsNotPolledForThreeSeconds) {
  ResetHelperStartMemoryForTesting();
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath socket = dir.GetPath().AppendASCII("none.sock");
  base::FilePath helper("/bin/false");
  if (!base::PathExists(helper)) {
    GTEST_SKIP() << "no /bin/false";
  }
  base::TimeTicks start = base::TimeTicks::Now();
  FedResult first = FetchFromHelper(socket, helper, "fed://a.fed/",
                                    LoadKind::kDocument, base::Seconds(1));
  EXPECT_LT(base::TimeTicks::Now() - start, base::Seconds(1));
  EXPECT_EQ(first.fault, HelperFault::kNotAnswering);
  start = base::TimeTicks::Now();
  for (int i = 0; i < 50; ++i) {
    FedResult again = FetchFromHelper(socket, helper, "fed://a.fed/x",
                                      LoadKind::kSubresource,
                                      base::Seconds(1));
    EXPECT_EQ(again.fault, HelperFault::kNotAnswering);
  }
  EXPECT_LT(base::TimeTicks::Now() - start, base::Milliseconds(100));
  ResetHelperStartMemoryForTesting();
}

TEST(AtlasFedHelperClientTest, AnAddressThatCannotTravelIsInvalid) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::FilePath socket = dir.GetPath().AppendASCII("fed.sock");
  FakeHelper helper(socket, "");
  helper.Start();
  FedResult r = FetchFromHelper(socket, base::FilePath(), "fed://a.fed/a b",
                                LoadKind::kDocument, base::Seconds(10));
  EXPECT_EQ(r.net_error, net::ERR_INVALID_URL);
  // The client connected before it found the address unfit and closed again,
  // which lets the stand-in finish.
  helper.Join();
}

}  // namespace atlas_fed
