// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATLAS_HOST_ATLAS_HOST_H_
#define COMPONENTS_ATLAS_HOST_ATLAS_HOST_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "components/atlas_host/protocol.h"

namespace atlas_host {

// What came of one request.
struct Result {
  // True if the verb ran; |outcome| says what the person did.
  bool ok = false;
  Outcome outcome = Outcome::kCancelled;
  // If !ok: why, and the words to show the person as they are (they come from
  // the other end of the bridge, or from this file when no answer came).
  ErrorCode code = ErrorCode::kFailed;
  std::string text;

  static Result Ok(Outcome outcome);
  static Result Error(ErrorCode code, std::string text);
};

using ResultCallback = base::OnceCallback<void(Result)>;

// The browser's side of the bridge to the desktop: it lets the person Share a
// file, Open With another app and Move to the Trash, the verbs only the
// desktop can do. It speaks the Atlas Host Protocol (protocol.h) over
// $XDG_RUNTIME_DIR/atlas/bridge.sock. It holds no logic of the desktop: it
// sends a verb with a path and returns what the other end answers.
//
// Everything runs on the sequence that created it (the UI thread in the
// browser). It makes no network connection and opens no file; it only talks
// to that one socket. Where the socket is missing, or the other end does not
// answer the hello, IsAvailable() is false and a menu offers nothing.
class AtlasHost {
 public:
  struct Options {
    // Empty: $XDG_RUNTIME_DIR/atlas/bridge.sock.
    base::FilePath socket_path;
    // 0: this process.
    int32_t pid = 0;
    // The other end must answer the hello within this.
    base::TimeDelta hello_timeout = base::Seconds(5);
    // How long Trash may wait for an answer, and how long Share and Open With
    // may (the person is choosing in a sheet).
    base::TimeDelta trash_timeout = base::Seconds(30);
    base::TimeDelta choose_timeout = base::Minutes(10);
    // The pause before reconnect attempt 1, 2, ... after a connection was
    // lost; attempt 0 is made at once. Doubles up to |max_backoff|.
    base::TimeDelta first_backoff = base::Seconds(1);
    base::TimeDelta max_backoff = base::Seconds(16);
    // After this many attempts in a row, wait for EnsureConnected().
    int max_attempts = 6;
    // EnsureConnected() tries again at most this often.
    base::TimeDelta min_retry_interval = base::Seconds(5);
  };

  class Observer : public base::CheckedObserver {
   public:
    // The other end answered the hello (true), or the connection is gone.
    virtual void OnAvailabilityChanged(bool available) = 0;
  };

  virtual ~AtlasHost();

  // The host of this browser; null until Start() and after Stop().
  static AtlasHost* Get();

  // Called from the browser's main: connects (once, without waiting) and
  // keeps the connection. No-op if started. Returns the new host.
  static AtlasHost* Start();
  // Closes the connection; requests that wait are dropped.
  static void Stop();

  // A host that is not the browser's, for tests and for code that owns its
  // own. Does not connect: call EnsureConnected().
  static std::unique_ptr<AtlasHost> Create(Options options);

  // Replaces what Get() returns until the returned object is destroyed.
  class ScopedOverride {
   public:
    explicit ScopedOverride(AtlasHost* host);
    ~ScopedOverride();
    ScopedOverride(const ScopedOverride&) = delete;
    ScopedOverride& operator=(const ScopedOverride&) = delete;

   private:
    raw_ptr<AtlasHost> previous_;
  };

  // True once the other end has answered the hello and while the connection
  // lives. A menu item for a verb is shown only when this is true.
  virtual bool IsAvailable() const = 0;

  // Tries to connect if there is no connection and none is on its way; cheap
  // and rate limited, so a menu may call it when it opens.
  virtual void EnsureConnected() = 0;

  // Sends a verb for |path|. |callback| runs once, on this sequence, with what
  // the person did or why nothing happened; never from inside this call.
  // |callback| is not run if this object is destroyed first.
  virtual void Request(Verb verb,
                       const base::FilePath& path,
                       ResultCallback callback) = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

 protected:
  AtlasHost();
};

}  // namespace atlas_host

#endif  // COMPONENTS_ATLAS_HOST_ATLAS_HOST_H_
