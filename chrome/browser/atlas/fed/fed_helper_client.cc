// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/atlas/fed/fed_helper_client.h"

#include <errno.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>

#include <atomic>
#include <memory>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "net/base/net_errors.h"

namespace atlas_fed {

namespace {

// How long to wait for a helper that was just started to answer.
constexpr int kStartTries = 60;
constexpr base::TimeDelta kStartPause = base::Milliseconds(50);
// A helper is started at most this often (a missing or failing binary must not
// make every subresource fork a process).
constexpr base::TimeDelta kStartEvery = base::Seconds(2);

base::ScopedFD ConnectTo(const base::FilePath& path) {
  const std::string& p = path.value();
  sockaddr_un addr = {};
  if (p.empty() || p.size() >= sizeof(addr.sun_path)) {
    return base::ScopedFD();
  }
  addr.sun_family = AF_UNIX;
  base::span(addr.sun_path).first(p.size()).copy_from(base::span(p));
  base::ScopedFD fd(socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0));
  if (!fd.is_valid()) {
    return base::ScopedFD();
  }
  if (HANDLE_EINTR(connect(fd.get(), reinterpret_cast<sockaddr*>(&addr),
                           sizeof(addr))) != 0) {
    return base::ScopedFD();
  }
  return fd;
}

void SetTimeouts(int fd, base::TimeDelta timeout) {
  timeval tv = {};
  tv.tv_sec = static_cast<time_t>(timeout.InSeconds());
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  timeval send_tv = {};
  send_tv.tv_sec = 10;
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &send_tv, sizeof(send_tv));
}

bool SendAll(int fd, base::span<const char> data) {
  while (!data.empty()) {
    ssize_t n = HANDLE_EINTR(send(fd, data.data(), data.size(), MSG_NOSIGNAL));
    if (n <= 0) {
      return false;
    }
    data = data.subspan(static_cast<size_t>(n));
  }
  return true;
}

// The net error of a receive that returned `n` (0: the helper closed early).
int ReceiveError(ssize_t n) {
  if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
    return net::ERR_TIMED_OUT;
  }
  return net::ERR_CONNECTION_CLOSED;
}

// What the start of a helper came to. One start at a time, and a helper that
// did not answer is not started (or waited for) again for kFailureMemory:
// 300 subresources of one page must not hold 300 threads for 3 s each.
enum class Start { kLaunched, kInFlight, kMissing, kFailed, kRecentFailure };

struct StartMemory {
  base::Lock lock;
  base::TimeTicks last_start;
  base::TimeTicks last_failure;
  bool logged_missing = false;
  bool logged_failed = false;
};

StartMemory& Memory() {
  static base::NoDestructor<StartMemory> memory;
  return *memory;
}

constexpr base::TimeDelta kFailureMemory = base::Seconds(5);

Start StartHelper(const base::FilePath& socket,
                  const base::FilePath& helper,
                  base::Process* launched) {
  StartMemory& m = Memory();
  base::AutoLock guard(m.lock);
  if (helper.empty() || !base::PathExists(helper)) {
    if (!m.logged_missing) {
      m.logged_missing = true;
      LOG(ERROR) << "atlas-fed: the Federate helper is not at "
                 << (helper.empty() ? "<unknown path>" : helper.value())
                 << "; Federate pages cannot be shown";
    }
    return Start::kMissing;
  }
  base::TimeTicks now = base::TimeTicks::Now();
  if (!m.last_failure.is_null() && now - m.last_failure < kFailureMemory) {
    return Start::kRecentFailure;
  }
  if (!m.last_start.is_null() && now - m.last_start < kStartEvery) {
    // Started a moment ago (by another load): wait for that one.
    return Start::kInFlight;
  }
  m.last_start = now;
  std::vector<std::string> argv = {helper.value(), "--socket",
                                   socket.value()};
  base::LaunchOptions options;
  // Its own process group: a signal to the browser's group does not stop it
  // mid-reply; it leaves by itself when idle.
  options.new_process_group = true;
  base::Process process = base::LaunchProcess(argv, options);
  if (!process.IsValid()) {
    if (!m.logged_failed) {
      m.logged_failed = true;
      LOG(ERROR) << "atlas-fed: could not start " << helper.value();
    }
    m.last_failure = now;
    return Start::kFailed;
  }
  *launched = std::move(process);
  return Start::kLaunched;
}

void NoteNotAnswering(const base::FilePath& socket) {
  StartMemory& m = Memory();
  base::AutoLock guard(m.lock);
  m.last_failure = base::TimeTicks::Now();
  LOG(ERROR) << "atlas-fed: the Federate helper does not answer at "
             << socket.value();
}

// Waits for the helper to listen on `socket`. `launched` is the process this
// load started (invalid when another load did): when it has already left, the
// socket gets one last try, since a second helper leaves at once when the
// first one holds the socket.
base::ScopedFD WaitForHelper(const base::FilePath& socket,
                             base::Process* launched) {
  base::ScopedFD fd;
  for (int i = 0; i < kStartTries && !fd.is_valid(); ++i) {
    base::PlatformThread::Sleep(kStartPause);
    fd = ConnectTo(socket);
    if (!fd.is_valid() && launched->IsValid()) {
      int exit_code = 0;
      if (launched->WaitForExitWithTimeout(base::TimeDelta(), &exit_code)) {
        base::PlatformThread::Sleep(kStartPause);
        fd = ConnectTo(socket);
        launched->Close();
        return fd;
      }
    }
  }
  return fd;
}

FedResult Exchange(int fd, const std::string& url, LoadKind kind) {
  FedResult result;
  std::optional<std::string> line = RequestLine(url, kind);
  if (!line) {
    result.net_error = net::ERR_INVALID_URL;
    return result;
  }
  if (!SendAll(fd, base::span(*line))) {
    result.net_error = net::ERR_CONNECTION_RESET;
    return result;
  }
  std::string head_line;
  while (true) {
    char c = 0;
    ssize_t n = HANDLE_EINTR(recv(fd, &c, 1, 0));
    if (n != 1) {
      result.net_error = ReceiveError(n);
      return result;
    }
    if (c == '\n') {
      break;
    }
    head_line.push_back(c);
    if (head_line.size() > kMaxHead) {
      result.net_error = net::ERR_INVALID_RESPONSE;
      return result;
    }
  }
  std::optional<ReplyHead> head = ParseReplyHead(head_line);
  if (!head) {
    result.net_error = net::ERR_INVALID_RESPONSE;
    return result;
  }
  result.body.resize(static_cast<size_t>(head->length));
  base::span<char> out = base::span(result.body);
  while (!out.empty()) {
    ssize_t n = HANDLE_EINTR(recv(fd, out.data(), out.size(), 0));
    if (n <= 0) {
      result.body.clear();
      result.net_error = ReceiveError(n);
      return result;
    }
    out = out.subspan(static_cast<size_t>(n));
  }
  result.net_error = net::OK;
  result.status = head->status;
  result.mime = std::move(head->mime);
  return result;
}

}  // namespace

base::FilePath HelperSocketPath() {
  std::unique_ptr<base::Environment> env = base::Environment::Create();
  std::optional<std::string> dir = env->GetVar("XDG_RUNTIME_DIR");
  if (!dir || dir->empty() || (*dir)[0] != '/') {
    return base::FilePath();
  }
  return base::FilePath(*dir).Append("atlas").Append("fed.sock");
}

base::FilePath HelperBinaryPath() {
  base::FilePath dir;
  if (!base::PathService::Get(base::DIR_EXE, &dir)) {
    return base::FilePath();
  }
  return dir.Append("atlas-fed");
}

FedResult FaultPageFor(HelperFault fault) {
  const char* title = "WolOS cannot check Federate pages on this computer";
  const char* line = "The part of WolOS that checks Federate pages is not answering.";
  switch (fault) {
    case HelperFault::kNoRuntimeDir:
      line =
          "There is no runtime folder for this session (XDG_RUNTIME_DIR is not set), so the "
          "program that checks Federate pages cannot be reached. Sign out and in again.";
      break;
    case HelperFault::kHelperMissing:
      line =
          "The program that checks Federate pages (atlas-fed) is missing. Reinstall the "
          "browser from WolOS.";
      break;
    case HelperFault::kStartFailed:
      line =
          "The program that checks Federate pages could not be started. Restart the browser; "
          "if this stays, reinstall it from WolOS.";
      break;
    case HelperFault::kNotAnswering:
    case HelperFault::kNone:
      line =
          "The program that checks Federate pages started but did not answer. Load the page "
          "again in a few seconds.";
      break;
  }
  FedResult result;
  result.net_error = net::OK;
  result.status = 503;
  result.mime = "text/html; charset=utf-8";
  result.body = base::StrCat(
      {"<!doctype html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n<title>", title,
       "</title>\n<style>:root{color-scheme:light dark}body{font:16px/1.5 system-ui,sans-serif;"
       "max-width:40rem;margin:12vh auto;padding:0 1.5rem}h1{font-size:1.4rem}</style>\n</head>\n<body>\n<h1>",
       title, "</h1>\n<p>", line, "</p>\n<p>Nothing from the page was shown.</p>\n</body>\n</html>\n"});
  return result;
}

void ResetHelperStartMemoryForTesting() {
  StartMemory& m = Memory();
  base::AutoLock guard(m.lock);
  m.last_start = base::TimeTicks();
  m.last_failure = base::TimeTicks();
  m.logged_missing = false;
  m.logged_failed = false;
}

FedResult FetchFromHelper(const base::FilePath& socket,
                          const base::FilePath& helper,
                          const std::string& url,
                          LoadKind kind,
                          base::TimeDelta timeout) {
  base::ScopedBlockingCall blocking(FROM_HERE, base::BlockingType::WILL_BLOCK);
  FedResult result;
  if (socket.empty()) {
    static std::atomic<bool> logged{false};
    if (!logged.exchange(true)) {
      LOG(ERROR) << "atlas-fed: XDG_RUNTIME_DIR is not set; there is no place "
                    "for the Federate helper's socket";
    }
    result.net_error = net::ERR_ACCESS_DENIED;
    result.fault = HelperFault::kNoRuntimeDir;
    return result;
  }
  base::ScopedFD fd = ConnectTo(socket);
  if (!fd.is_valid()) {
    base::Process launched;
    Start started = StartHelper(socket, helper, &launched);
    result.net_error = net::ERR_CONNECTION_REFUSED;
    switch (started) {
      case Start::kMissing:
        result.fault = HelperFault::kHelperMissing;
        return result;
      case Start::kFailed:
        result.fault = HelperFault::kStartFailed;
        return result;
      case Start::kRecentFailure:
        result.fault = HelperFault::kNotAnswering;
        return result;
      case Start::kLaunched:
      case Start::kInFlight:
        break;
    }
    fd = WaitForHelper(socket, &launched);
    if (launched.IsValid()) {
      base::EnsureProcessGetsReaped(std::move(launched));
    }
    if (!fd.is_valid()) {
      NoteNotAnswering(socket);
      result.fault = HelperFault::kNotAnswering;
      return result;
    }
    result = FedResult();
  }
  SetTimeouts(fd.get(), timeout);
  return Exchange(fd.get(), url, kind);
}

FedResult Fetch(const std::string& url, LoadKind kind) {
  return FetchFromHelper(HelperSocketPath(), HelperBinaryPath(), url, kind,
                         base::Seconds(120));
}

}  // namespace atlas_fed
