// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/atlas_host/atlas_host.h"

#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <string.h>

#include <functional>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "base/files/file_descriptor_watcher_posix.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/atlas_host/protocol.h"
#include "components/atlas_host/test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace atlas_host {

namespace {

constexpr int32_t kPid = 4242;

// A bridge on a socket of the test's own, run on the test's thread. It
// answers by what the test tells it; nothing of WolOS is in it.
class FakeBridge {
 public:
  struct Connection {
    base::ScopedFD fd;
    std::unique_ptr<base::FileDescriptorWatcher::Controller> watch;
    LineReader reader;
  };

  // Called with each line a browser sends on a connection.
  using OnLine = std::function<void(FakeBridge&, size_t connection,
                                    const std::string& line)>;
  // Called when a browser connects.
  using OnAccept = std::function<void(FakeBridge&, size_t connection)>;

  explicit FakeBridge(const base::FilePath& path) {
    listener_.reset(socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0));
    sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    EXPECT_LT(path.value().size(), sizeof(addr.sun_path));
    memcpy(addr.sun_path, path.value().data(), path.value().size());
    EXPECT_EQ(bind(listener_.get(), reinterpret_cast<sockaddr*>(&addr),
                   sizeof(addr)),
              0);
    EXPECT_EQ(listen(listener_.get(), 8), 0);
    watch_ = base::FileDescriptorWatcher::WatchReadable(
        listener_.get(),
        base::BindRepeating(&FakeBridge::Accept, base::Unretained(this)));
  }

  void set_on_line(OnLine f) { on_line_ = std::move(f); }
  void set_on_accept(OnAccept f) { on_accept_ = std::move(f); }
  size_t accepted() const { return connections_.size(); }
  const std::vector<std::string>& lines() const { return lines_; }

  void Send(size_t connection, const std::string& line) {
    ASSERT_LT(connection, connections_.size());
    int fd = connections_[connection].fd.get();
    if (fd < 0) {
      return;
    }
    std::string data = line + "\n";
    ASSERT_EQ(send(fd, data.data(), data.size(), MSG_NOSIGNAL),
              static_cast<ssize_t>(data.size()));
  }

  void SendReply(size_t connection, const Reply& reply) {
    std::string line = EncodeReply(reply);
    line.pop_back();
    Send(connection, line);
  }

  void Close(size_t connection) {
    ASSERT_LT(connection, connections_.size());
    connections_[connection].watch.reset();
    connections_[connection].fd.reset();
  }

 private:
  void Accept() {
    int fd = accept4(listener_.get(), nullptr, nullptr, SOCK_NONBLOCK);
    if (fd < 0) {
      return;
    }
    size_t index = connections_.size();
    connections_.emplace_back();
    connections_[index].fd.reset(fd);
    connections_[index].watch = base::FileDescriptorWatcher::WatchReadable(
        fd, base::BindRepeating(&FakeBridge::Read, base::Unretained(this),
                                index));
    if (on_accept_) {
      on_accept_(*this, index);
    }
  }

  void Read(size_t index) {
    char buffer[4096];
    int fd = connections_[index].fd.get();
    while (fd >= 0) {
      ssize_t n = read(fd, buffer, sizeof(buffer));
      if (n <= 0) {
        if (n == 0) {
          connections_[index].watch.reset();
        }
        return;
      }
      connections_[index].reader.Append(
          std::string_view(buffer, static_cast<size_t>(n)));
      while (auto line = connections_[index].reader.Next()) {
        lines_.push_back(*line);
        if (on_line_) {
          on_line_(*this, index, *line);
        }
      }
      fd = connections_[index].fd.get();
    }
  }

  base::ScopedFD listener_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> watch_;
  std::vector<Connection> connections_;
  std::vector<std::string> lines_;
  OnLine on_line_;
  OnAccept on_accept_;
};

class Availability : public AtlasHost::Observer {
 public:
  explicit Availability(AtlasHost* host) : host_(host) {
    host_->AddObserver(this);
  }
  ~Availability() override { host_->RemoveObserver(this); }

  void OnAvailabilityChanged(bool available) override {
    changes_.push_back(available);
    if (quit_) {
      std::move(quit_).Run();
    }
  }

  // Waits for the next change not yet seen and returns it.
  bool Next() {
    while (changes_.size() <= seen_) {
      base::RunLoop loop;
      quit_ = loop.QuitClosure();
      loop.Run();
    }
    return changes_[seen_++];
  }

  const std::vector<bool>& changes() const { return changes_; }

 private:
  raw_ptr<AtlasHost> host_;
  std::vector<bool> changes_;
  size_t seen_ = 0;
  base::OnceClosure quit_;
};

class AtlasHostTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    socket_ = dir_.GetPath().AppendASCII("bridge.sock");
  }

  AtlasHost::Options FastOptions() {
    AtlasHost::Options o;
    o.socket_path = socket_;
    o.pid = kPid;
    o.hello_timeout = base::Milliseconds(300);
    o.trash_timeout = base::Milliseconds(300);
    o.choose_timeout = base::Milliseconds(300);
    o.first_backoff = base::Milliseconds(20);
    o.max_backoff = base::Milliseconds(80);
    o.min_retry_interval = base::Milliseconds(0);
    return o;
  }

  // The bridge as it behaves: answers the hello, and every verb by |answer|.
  void Behave(
      FakeBridge& bridge,
      std::function<void(FakeBridge&, size_t, const VerbRequest&)> answer) {
    bridge.set_on_line([answer](FakeBridge& b, size_t c,
                                const std::string& line) {
      auto request = DecodeRequest(line);
      ASSERT_TRUE(request.has_value()) << line;
      if (std::holds_alternative<Hello>(*request)) {
        b.SendReply(c, HelloReply{1});
      } else if (answer) {
        answer(b, c, std::get<VerbRequest>(*request));
      }
    });
  }

  Result Ask(AtlasHost& host, Verb verb, const std::string& path) {
    base::test::TestFuture<Result> future;
    host.Request(verb, base::FilePath(path), future.GetCallback());
    return future.Take();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  base::ScopedTempDir dir_;
  base::FilePath socket_;
};

}  // namespace

// The session transcripts of the bridge, played from the browser's side: the
// bytes the host sends are the ones in the transcript and the answers become
// the results the person's code sees.
TEST_F(AtlasHostTest, PlaysTheSessionsOfTheBridge) {
  struct Case {
    const char* name;
    // What each verb answers, in the order of the transcript: "ok:<outcome>"
    // or "error:<code>".
    std::vector<std::string> results;
  };
  const Case cases[] = {
      {"share.txt", {"ok:copied", "ok:saved", "ok:opened", "ok:cancelled"}},
      {"open-with.txt", {"ok:opened", "ok:cancelled", "error:failed"}},
      {"trash.txt", {"ok:trashed", "ok:trashed", "error:failed"}},
      {"denied.txt",
       {"error:permission-denied", "error:permission-denied",
        "error:permission-denied"}},
      {"desktop.txt", {"error:busy", "error:unavailable", "error:unavailable"}},
      {"older-wolos.txt",
       {"error:unavailable", "error:unavailable", "ok:opened"}},
      {"files.txt",
       {"error:not-found", "error:not-found", "error:failed", "error:failed"}},
      {"wold-down.txt", {"error:unavailable"}},
  };
  for (const Case& c : cases) {
    SCOPED_TRACE(c.name);
    base::FilePath socket = dir_.GetPath().AppendASCII(c.name);
    Session session = ReadSession(c.name, "4242", "/tmp/atlas-scratch/files");
    ASSERT_EQ(session.steps.size(), c.results.size() + 1);

    FakeBridge bridge(socket);
    size_t step = 0;
    bridge.set_on_line([&](FakeBridge& b, size_t conn, const std::string& line) {
      ASSERT_LT(step, session.steps.size());
      EXPECT_EQ(line, session.steps[step].sent);
      b.Send(conn, session.steps[step].answered);
      ++step;
    });
    AtlasHost::Options options = FastOptions();
    options.socket_path = socket;
    auto host = AtlasHost::Create(options);
    Availability availability(host.get());
    host->EnsureConnected();
    ASSERT_TRUE(availability.Next());
    EXPECT_TRUE(host->IsAvailable());

    for (size_t i = 1; i < session.steps.size(); ++i) {
      auto request = DecodeRequest(session.steps[i].sent);
      ASSERT_TRUE(request.has_value());
      const auto& verb = std::get<VerbRequest>(*request);
      Result r = Ask(*host, verb.verb, verb.path);
      auto reply = DecodeReply(session.steps[i].answered);
      ASSERT_TRUE(reply.has_value());
      std::string got = r.ok ? std::string("ok:") + OutcomeName(r.outcome)
                             : std::string("error:") + ErrorCodeName(r.code);
      EXPECT_EQ(got, c.results[i - 1]);
      if (const auto* error = std::get_if<ErrorReply>(&*reply)) {
        EXPECT_EQ(r.text, error->text);  // Shown as it came.
      }
    }
    EXPECT_EQ(step, session.steps.size());
  }
}

TEST_F(AtlasHostTest, NoSocketMeansNotAvailableAndNoRetry) {
  auto host = AtlasHost::Create(FastOptions());
  host->EnsureConnected();
  EXPECT_FALSE(host->IsAvailable());
  Result r = Ask(*host, Verb::kShare, "/tmp/x");
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(r.code, ErrorCode::kUnavailable);
  EXPECT_EQ(r.text, kTextUnreachable);

  // The bridge shows up later (installed, or the session started it): the
  // next verb finds it.
  FakeBridge bridge(socket_);
  Behave(bridge, [](FakeBridge& b, size_t c, const VerbRequest& v) {
    b.SendReply(c, OkReply{v.id, Outcome::kCopied});
  });
  Result again = Ask(*host, Verb::kShare, "/tmp/x");
  EXPECT_TRUE(again.ok);
  EXPECT_EQ(again.outcome, Outcome::kCopied);
}

TEST_F(AtlasHostTest, NoRuntimeDirectoryMeansNothingToConnectTo) {
  AtlasHost::Options options = FastOptions();
  options.socket_path = base::FilePath(std::string(200, 'x'));  // Too long.
  auto host = AtlasHost::Create(options);
  host->EnsureConnected();
  EXPECT_FALSE(host->IsAvailable());
  Result r = Ask(*host, Verb::kTrash, "/tmp/x");
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(r.text, kTextUnreachable);
}

TEST_F(AtlasHostTest, ASilentBridgeIsNotAvailableAndAnswersWhatWaits) {
  FakeBridge bridge(socket_);  // Accepts, never answers.
  auto host = AtlasHost::Create(FastOptions());
  base::test::TestFuture<Result> future;
  host->Request(Verb::kShare, base::FilePath("/tmp/x"), future.GetCallback());
  EXPECT_FALSE(host->IsAvailable());
  Result r = future.Take();
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(r.code, ErrorCode::kUnavailable);
  EXPECT_EQ(r.text, kTextNoAnswer);
  EXPECT_FALSE(host->IsAvailable());
}

TEST_F(AtlasHostTest, ConnectsAgainWhenTheBridgeLeavesBeforeTheHello) {
  FakeBridge bridge(socket_);
  // The first connection finds a bridge that is just leaving.
  bridge.set_on_accept([](FakeBridge& b, size_t c) {
    if (c == 0) {
      b.Close(c);
    }
  });
  Behave(bridge, nullptr);
  auto host = AtlasHost::Create(FastOptions());
  Availability availability(host.get());
  host->EnsureConnected();
  EXPECT_TRUE(availability.Next());
  EXPECT_TRUE(host->IsAvailable());
  EXPECT_EQ(bridge.accepted(), 2u);
}

TEST_F(AtlasHostTest, ConnectsAgainWhenAnEstablishedConnectionIsLost) {
  FakeBridge bridge(socket_);
  Behave(bridge, [](FakeBridge& b, size_t c, const VerbRequest& v) {
    // A verb that the bridge never answers because it dies.
    b.Close(c);
  });
  auto host = AtlasHost::Create(FastOptions());
  Availability availability(host.get());
  host->EnsureConnected();
  ASSERT_TRUE(availability.Next());

  Result lost = Ask(*host, Verb::kOpenWith, "/tmp/x");
  EXPECT_FALSE(lost.ok);
  EXPECT_EQ(lost.text, kTextLost);
  EXPECT_FALSE(host->IsAvailable());

  EXPECT_FALSE(availability.Next());  // Seen as lost,
  EXPECT_TRUE(availability.Next());   // and found again.
  EXPECT_TRUE(host->IsAvailable());
  EXPECT_EQ(bridge.accepted(), 2u);
}

TEST_F(AtlasHostTest, AnswersGoToTheVerbTheyBelongTo) {
  FakeBridge bridge(socket_);
  std::vector<VerbRequest> held;
  Behave(bridge, [&](FakeBridge& b, size_t c, const VerbRequest& v) {
    held.push_back(v);
    if (held.size() == 2) {
      // The second is ready first: a sheet waiting for the person does not
      // hold back a Trash.
      b.SendReply(c, OkReply{held[1].id, Outcome::kTrashed});
      b.SendReply(c, OkReply{held[0].id, Outcome::kSaved});
    }
  });
  auto host = AtlasHost::Create(FastOptions());
  Availability availability(host.get());
  host->EnsureConnected();
  ASSERT_TRUE(availability.Next());

  base::test::TestFuture<Result> share;
  base::test::TestFuture<Result> trash;
  host->Request(Verb::kShare, base::FilePath("/tmp/a"), share.GetCallback());
  host->Request(Verb::kTrash, base::FilePath("/tmp/b"), trash.GetCallback());
  Result t = trash.Take();
  Result s = share.Take();
  EXPECT_TRUE(t.ok);
  EXPECT_EQ(t.outcome, Outcome::kTrashed);
  EXPECT_TRUE(s.ok);
  EXPECT_EQ(s.outcome, Outcome::kSaved);
  ASSERT_EQ(held.size(), 2u);
  EXPECT_NE(held[0].id, held[1].id);
  EXPECT_EQ(held[0].verb, Verb::kShare);
  EXPECT_EQ(held[1].path, "/tmp/b");
}

TEST_F(AtlasHostTest, AFifthVerbIsBusyWithoutAskingTheBridge) {
  FakeBridge bridge(socket_);
  Behave(bridge, nullptr);  // Holds every verb.
  auto host = AtlasHost::Create(FastOptions());
  Availability availability(host.get());
  host->EnsureConnected();
  ASSERT_TRUE(availability.Next());

  std::vector<std::unique_ptr<base::test::TestFuture<Result>>> held;
  for (size_t i = 0; i < kMaxInFlight; ++i) {
    held.push_back(std::make_unique<base::test::TestFuture<Result>>());
    host->Request(Verb::kShare, base::FilePath("/tmp/x"),
                  held.back()->GetCallback());
  }
  Result fifth = Ask(*host, Verb::kShare, "/tmp/x");
  EXPECT_FALSE(fifth.ok);
  EXPECT_EQ(fifth.code, ErrorCode::kBusy);
  EXPECT_EQ(fifth.text, kTextTooMany);
  // Hello and four verbs only.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(bridge.lines().size(), 1u + kMaxInFlight);
}

TEST_F(AtlasHostTest, AVerbThatIsNeverAnsweredTimesOut) {
  FakeBridge bridge(socket_);
  Behave(bridge, nullptr);
  auto host = AtlasHost::Create(FastOptions());
  Availability availability(host.get());
  host->EnsureConnected();
  ASSERT_TRUE(availability.Next());
  Result r = Ask(*host, Verb::kTrash, "/tmp/x");
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(r.code, ErrorCode::kUnavailable);
  EXPECT_EQ(r.text, kTextNoAnswer);
}

TEST_F(AtlasHostTest, ARefusedPeerIsNotTriedAgain) {
  FakeBridge bridge(socket_);
  bridge.set_on_line([](FakeBridge& b, size_t c, const std::string&) {
    b.SendReply(c, ErrorReply{0, ErrorCode::kNotBrowser,
                              "Only Atlas can use this service."});
    b.Close(c);
  });
  auto host = AtlasHost::Create(FastOptions());
  Availability availability(host.get());
  host->EnsureConnected();
  base::test::TestFuture<Result> future;
  host->Request(Verb::kShare, base::FilePath("/tmp/x"), future.GetCallback());
  Result r = future.Take();
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(r.code, ErrorCode::kNotBrowser);
  EXPECT_EQ(r.text, "Only Atlas can use this service.");
  EXPECT_FALSE(host->IsAvailable());
  base::RunLoop loop;
  task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE, loop.QuitClosure(), base::Milliseconds(300));
  loop.Run();
  EXPECT_EQ(bridge.accepted(), 1u);
}

TEST_F(AtlasHostTest, ALineThatIsNotAReplyClosesTheConnection) {
  FakeBridge bridge(socket_);
  Behave(bridge, [](FakeBridge& b, size_t c, const VerbRequest&) {
    b.Send(c, "this is not json");
  });
  auto host = AtlasHost::Create(FastOptions());
  Availability availability(host.get());
  host->EnsureConnected();
  ASSERT_TRUE(availability.Next());
  Result r = Ask(*host, Verb::kShare, "/tmp/x");
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(r.text, kTextBadAnswer);
}

TEST_F(AtlasHostTest, ARequestThatIsNotAFullPathIsRefusedBeforeItIsSent) {
  FakeBridge bridge(socket_);
  Behave(bridge, nullptr);
  auto host = AtlasHost::Create(FastOptions());
  Availability availability(host.get());
  host->EnsureConnected();
  ASSERT_TRUE(availability.Next());
  Result r = Ask(*host, Verb::kShare, "Downloads/report.pdf");
  EXPECT_FALSE(r.ok);
  EXPECT_EQ(r.text, kTextBadName);
  EXPECT_EQ(bridge.lines().size(), 1u);  // Only the hello.
}

TEST_F(AtlasHostTest, TheHelloNamesTheBrowserAndTheVersion) {
  FakeBridge bridge(socket_);
  Behave(bridge, nullptr);
  auto host = AtlasHost::Create(FastOptions());
  Availability availability(host.get());
  host->EnsureConnected();
  ASSERT_TRUE(availability.Next());
  ASSERT_EQ(bridge.lines().size(), 1u);
  EXPECT_EQ(bridge.lines()[0], "{\"op\":\"hello\",\"version\":1,\"pid\":4242}");
}

TEST_F(AtlasHostTest, DestroyingTheHostDropsWhatWaitsWithoutCallingBack) {
  FakeBridge bridge(socket_);
  Behave(bridge, nullptr);
  auto host = AtlasHost::Create(FastOptions());
  bool called = false;
  {
    Availability availability(host.get());
    host->EnsureConnected();
    ASSERT_TRUE(availability.Next());
    host->Request(Verb::kShare, base::FilePath("/tmp/x"),
                  base::BindOnce([](bool* called, Result) { *called = true; },
                                 &called));
  }
  host.reset();
  base::RunLoop loop;
  task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE, loop.QuitClosure(), base::Milliseconds(400));
  loop.Run();
  EXPECT_FALSE(called);
}

TEST_F(AtlasHostTest, ScopedOverrideReplacesTheHostOfTheBrowser) {
  EXPECT_EQ(AtlasHost::Get(), nullptr);
  auto host = AtlasHost::Create(FastOptions());
  {
    AtlasHost::ScopedOverride override_host(host.get());
    EXPECT_EQ(AtlasHost::Get(), host.get());
  }
  EXPECT_EQ(AtlasHost::Get(), nullptr);
}

}  // namespace atlas_host
