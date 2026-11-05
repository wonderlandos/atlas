// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/atlas_host/atlas_host.h"

#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>
#include <deque>
#include <map>
#include <utility>

#include "base/containers/span.h"
#include "base/environment.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"

namespace atlas_host {

namespace {

AtlasHost* g_host = nullptr;
// The one Start() made, so Stop() can delete it even while overridden.
AtlasHost* g_started = nullptr;

class BridgeClient final : public AtlasHost {
 public:
  explicit BridgeClient(Options options) : options_(std::move(options)) {
    if (options_.pid == 0) {
      options_.pid = static_cast<int32_t>(base::GetCurrentProcId());
    }
    if (options_.socket_path.empty()) {
      options_.socket_path = DefaultSocketPath();
    }
  }

  ~BridgeClient() override {
    // Callbacks of requests still waiting are dropped, not run: whoever asked
    // may be gone with us.
    CloseConnection();
  }

  bool IsAvailable() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return state_ == State::kReady;
  }

  void EnsureConnected() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (state_ != State::kIdle || reconnect_timer_.IsRunning()) {
      return;
    }
    base::TimeTicks now = base::TimeTicks::Now();
    if (!last_attempt_.is_null() &&
        now - last_attempt_ < options_.min_retry_interval) {
      return;
    }
    attempt_ = 0;
    Connect();
  }

  void Request(Verb verb,
               const base::FilePath& path,
               ResultCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!IsFullPath(path.value())) {
      Fail(std::move(callback), ErrorCode::kInvalidRequest, kTextBadName);
      return;
    }
    if (in_flight_.size() + waiting_.size() >= kMaxInFlight) {
      Fail(std::move(callback), ErrorCode::kBusy, kTextTooMany);
      return;
    }
    if (state_ == State::kIdle) {
      reconnect_timer_.Stop();
      attempt_ = 0;
      Connect();
      if (state_ == State::kIdle) {
        Fail(std::move(callback), ErrorCode::kUnavailable, kTextUnreachable);
        return;
      }
    }
    Waiting w;
    w.verb = verb;
    w.path = path.value();
    w.callback = std::move(callback);
    if (state_ == State::kReady) {
      Send(std::move(w));
    } else {
      waiting_.push_back(std::move(w));
    }
  }

  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }
  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

 private:
  enum class State {
    kIdle,   // No connection.
    kHello,  // Connected, the hello is not answered yet.
    kReady,  // The hello is answered.
  };

  struct Waiting {
    Verb verb = Verb::kShare;
    std::string path;
    ResultCallback callback;
  };

  struct InFlight {
    ResultCallback callback;
    std::unique_ptr<base::OneShotTimer> timer;
  };

  static base::FilePath DefaultSocketPath() {
    auto dir = base::Environment::Create()->GetVar("XDG_RUNTIME_DIR");
    if (!dir || dir->empty() || (*dir)[0] != '/') {
      return base::FilePath();
    }
    return base::FilePath(*dir).Append(kSocketDir).Append(kSocketName);
  }

  // Answers |callback| from a task, so no callback runs inside a call.
  void Fail(ResultCallback callback, ErrorCode code, const char* text) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  Result::Error(code, std::string(text))));
  }

  // Opens the connection and sends the hello. Leaves state_ at kIdle if there
  // is no one to talk to; when a retry makes sense one is scheduled.
  void Connect() {
    last_attempt_ = base::TimeTicks::Now();
    CloseConnection();
    const std::string& path = options_.socket_path.value();
    sockaddr_un addr = {};
    if (path.empty() || path.size() >= sizeof(addr.sun_path)) {
      return;
    }
    base::ScopedFD fd(
        socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0));
    if (!fd.is_valid()) {
      return;
    }
    addr.sun_family = AF_UNIX;
    memcpy(addr.sun_path, path.data(), path.size());
    int rv;
    do {
      rv = connect(fd.get(), reinterpret_cast<sockaddr*>(&addr),
                   sizeof(addr));
    } while (rv < 0 && errno == EINTR);
    if (rv < 0) {
      // No socket: the bridge is not installed, nothing to retry. Anything
      // else (refused, backlog full): the bridge is on its way or busy.
      if (errno != ENOENT && errno != ENOTDIR && errno != EACCES) {
        ScheduleReconnect();
      }
      return;
    }
    fd_ = std::move(fd);
    reader_ = LineReader();
    watch_ = base::FileDescriptorWatcher::WatchReadable(
        fd_.get(), base::BindRepeating(&BridgeClient::OnReadable,
                                       base::Unretained(this)));
    state_ = State::kHello;
    hello_timer_.Start(FROM_HERE, options_.hello_timeout,
                       base::BindOnce(&BridgeClient::OnHelloTimeout,
                                      base::Unretained(this)));
    if (!Write(EncodeRequest(Hello{kVersion, options_.pid}))) {
      Drop(kTextUnreachable, ErrorCode::kUnavailable, /*retry=*/true);
    }
  }

  // Closes without telling anyone; requests that wait stay as they are.
  void CloseConnection() {
    hello_timer_.Stop();
    watch_.reset();
    fd_.reset();
    state_ = State::kIdle;
  }

  void ScheduleReconnect() {
    if (attempt_ >= options_.max_attempts) {
      return;
    }
    base::TimeDelta delay;
    if (attempt_ > 0) {
      delay = std::min(options_.first_backoff * (1 << (attempt_ - 1)),
                       options_.max_backoff);
    }
    ++attempt_;
    reconnect_timer_.Start(
        FROM_HERE, delay,
        base::BindOnce(&BridgeClient::Connect, base::Unretained(this)));
  }

  // Writes the whole line or fails. Lines are short and the other end reads
  // at once; a full buffer means it is stuck, which is a lost connection.
  bool Write(const std::string& line) {
    if (line.empty() || !fd_.is_valid()) {
      return false;
    }
    base::span<const char> rest(line);
    while (!rest.empty()) {
      ssize_t n = send(fd_.get(), rest.data(), rest.size(), MSG_NOSIGNAL);
      if (n < 0 && errno == EINTR) {
        continue;
      }
      if (n <= 0) {
        return false;
      }
      rest = rest.subspan(static_cast<size_t>(n));
    }
    return true;
  }

  // Ends the connection and answers everything that waits with |text|.
  void Drop(const char* text, ErrorCode code, bool retry) {
    bool was_ready = state_ == State::kReady;
    CloseConnection();
    std::map<uint32_t, InFlight> in_flight = std::move(in_flight_);
    in_flight_.clear();
    std::deque<Waiting> waiting = std::move(waiting_);
    waiting_.clear();
    if (retry) {
      ScheduleReconnect();
    }
    if (was_ready) {
      for (auto& observer : observers_) {
        observer.OnAvailabilityChanged(false);
      }
    }
    for (auto& entry : in_flight) {
      if (entry.second.callback) {
        std::move(entry.second.callback).Run(Result::Error(code, text));
      }
    }
    for (auto& w : waiting) {
      std::move(w.callback).Run(Result::Error(code, text));
    }
  }

  void OnHelloTimeout() {
    VLOG(1) << "atlas_host: the bridge did not answer the hello";
    Drop(kTextNoAnswer, ErrorCode::kUnavailable, /*retry=*/true);
  }

  void OnReadable() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto weak = weak_factory_.GetWeakPtr();
    char buffer[4096];
    while (weak && fd_.is_valid()) {
      ssize_t n = read(fd_.get(), buffer, sizeof(buffer));
      if (n < 0 && errno == EINTR) {
        continue;
      }
      if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return;
      }
      if (n <= 0) {
        // The other end is gone: before the hello it left as we arrived (it
        // exits when idle and the socket starts a new one), so we connect
        // again at once.
        Drop(kTextLost, ErrorCode::kUnavailable, /*retry=*/true);
        return;
      }
      reader_.Append(std::string_view(buffer, static_cast<size_t>(n)));
      while (weak && fd_.is_valid()) {
        auto line = reader_.Next();
        if (!line) {
          break;
        }
        HandleLine(*line);
      }
      if (weak && fd_.is_valid() && reader_.overflowed()) {
        Drop(kTextBadAnswer, ErrorCode::kUnavailable, /*retry=*/false);
        return;
      }
    }
  }

  void HandleLine(const std::string& line) {
    std::optional<Reply> reply = DecodeReply(line);
    if (!reply) {
      Drop(kTextBadAnswer, ErrorCode::kUnavailable, /*retry=*/false);
      return;
    }
    if (state_ == State::kHello) {
      if (const auto* hello = std::get_if<HelloReply>(&*reply)) {
        if (hello->version > kVersion) {
          Drop(kTextBadAnswer, ErrorCode::kUnavailable, /*retry=*/false);
          return;
        }
        BecomeReady();
        return;
      }
      if (const auto* error = std::get_if<ErrorReply>(&*reply)) {
        // Refused: the peer was not accepted, the versions do not match, or
        // there are too many connections. Only the last may pass.
        bool again = error->code == ErrorCode::kBusy;
        Drop(error->text.c_str(), error->code, again);
        return;
      }
      Drop(kTextBadAnswer, ErrorCode::kUnavailable, /*retry=*/false);
      return;
    }
    if (const auto* ok = std::get_if<OkReply>(&*reply)) {
      Complete(ok->id, Result::Ok(ok->outcome));
    } else if (const auto* error = std::get_if<ErrorReply>(&*reply)) {
      if (error->id != 0) {
        Complete(error->id, Result::Error(error->code, error->text));
      } else if (error->code == ErrorCode::kNotBrowser ||
                 error->code == ErrorCode::kVersion) {
        // The connection is closed by the other end; do not knock again.
        Drop(error->text.c_str(), error->code, /*retry=*/false);
      }
      // Any other error of no request is about a line we did not send.
    } else {
      Drop(kTextBadAnswer, ErrorCode::kUnavailable, /*retry=*/false);
    }
  }

  void BecomeReady() {
    hello_timer_.Stop();
    reconnect_timer_.Stop();
    attempt_ = 0;
    state_ = State::kReady;
    auto weak = weak_factory_.GetWeakPtr();
    for (auto& observer : observers_) {
      observer.OnAvailabilityChanged(true);
    }
    std::deque<Waiting> waiting = std::move(waiting_);
    waiting_.clear();
    while (weak && state_ == State::kReady && !waiting.empty()) {
      Send(std::move(waiting.front()));
      waiting.pop_front();
    }
  }

  // Sends one verb. The answer, or a failure, reaches its callback later.
  void Send(Waiting w) {
    uint32_t id = NextId();
    VerbRequest request{w.verb, id, std::move(w.path)};
    InFlight entry;
    entry.callback = std::move(w.callback);
    entry.timer = std::make_unique<base::OneShotTimer>();
    entry.timer->Start(
        FROM_HERE,
        w.verb == Verb::kTrash ? options_.trash_timeout
                               : options_.choose_timeout,
        base::BindOnce(&BridgeClient::OnVerbTimeout, base::Unretained(this)));
    in_flight_.emplace(id, std::move(entry));
    if (!Write(EncodeRequest(request))) {
      Drop(kTextLost, ErrorCode::kUnavailable, /*retry=*/true);
    }
  }

  uint32_t NextId() {
    do {
      if (next_id_ == 0 || next_id_ == UINT32_MAX) {
        next_id_ = 0;
      }
      ++next_id_;
    } while (in_flight_.contains(next_id_));
    return next_id_;
  }

  void Complete(uint32_t id, Result result) {
    auto it = in_flight_.find(id);
    if (it == in_flight_.end()) {
      return;
    }
    ResultCallback callback = std::move(it->second.callback);
    in_flight_.erase(it);
    if (callback) {
      std::move(callback).Run(std::move(result));
    }
  }

  // Nobody answered a verb in time. Closing the connection abandons what the
  // other end holds for us (a sheet already on screen stays until the person
  // answers it) and the next verb starts clean.
  void OnVerbTimeout() {
    Drop(kTextNoAnswer, ErrorCode::kUnavailable, /*retry=*/true);
  }

  SEQUENCE_CHECKER(sequence_checker_);
  Options options_;
  State state_ = State::kIdle;
  base::ScopedFD fd_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> watch_;
  LineReader reader_;
  std::deque<Waiting> waiting_;
  std::map<uint32_t, InFlight> in_flight_;
  uint32_t next_id_ = 0;
  int attempt_ = 0;
  base::TimeTicks last_attempt_;
  base::OneShotTimer hello_timer_;
  base::OneShotTimer reconnect_timer_;
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<BridgeClient> weak_factory_{this};
};

}  // namespace

Result Result::Ok(Outcome outcome) {
  Result r;
  r.ok = true;
  r.outcome = outcome;
  return r;
}

Result Result::Error(ErrorCode code, std::string text) {
  Result r;
  r.ok = false;
  r.code = code;
  r.text = std::move(text);
  return r;
}

AtlasHost::AtlasHost() = default;
AtlasHost::~AtlasHost() = default;

// static
AtlasHost* AtlasHost::Get() {
  return g_host;
}

// static
AtlasHost* AtlasHost::Start() {
  if (g_started) {
    return g_started;
  }
  std::unique_ptr<AtlasHost> host = Create(Options());
  host->EnsureConnected();
  g_started = host.release();
  if (!g_host) {
    g_host = g_started;
  }
  return g_started;
}

// static
void AtlasHost::Stop() {
  if (!g_started) {
    return;
  }
  if (g_host == g_started) {
    g_host = nullptr;
  }
  delete g_started;
  g_started = nullptr;
}

// static
std::unique_ptr<AtlasHost> AtlasHost::Create(Options options) {
  return std::make_unique<BridgeClient>(std::move(options));
}

AtlasHost::ScopedOverride::ScopedOverride(AtlasHost* host)
    : previous_(g_host) {
  g_host = host;
}

AtlasHost::ScopedOverride::~ScopedOverride() {
  g_host = previous_;
}

}  // namespace atlas_host
