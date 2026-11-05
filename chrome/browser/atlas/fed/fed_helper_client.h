// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ATLAS_FED_FED_HELPER_CLIENT_H_
#define CHROME_BROWSER_ATLAS_FED_FED_HELPER_CLIENT_H_

#include <string>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "chrome/browser/atlas/fed/fed_protocol.h"

// The browser's end of the Federate helper (atlas-fed, a WolOS program beside
// the browser binary). One connection per load to
// $XDG_RUNTIME_DIR/atlas/fed.sock; when nothing answers there, the helper is
// started from beside the browser binary and asked again. Blocking: call on
// a thread pool sequence that may block, never on the UI or IO thread.
namespace atlas_fed {

// Why the helper could not be reached, when it could not (QA-A9-WEB-ATLAS-9):
// the person gets a page that says so instead of the engine's "refused to
// connect", and the log names the path that was tried.
enum class HelperFault {
  kNone,
  kNoRuntimeDir,   // XDG_RUNTIME_DIR is not set: there is no place for the socket
  kHelperMissing,  // atlas-fed is not beside the browser binary
  kStartFailed,    // the helper could not be launched
  kNotAnswering,   // it was launched (or was being launched) and never answered
};

struct FedResult {
  // net::OK when the helper answered; then `status`, `mime` and `body` are
  // its reply (verified content, or the page that says what failed).
  int net_error = 0;
  // Not kNone only when net_error is a connection error caused by the helper
  // being unreachable.
  HelperFault fault = HelperFault::kNone;
  int status = 0;
  std::string mime;
  std::string body;
};

// $XDG_RUNTIME_DIR/atlas/fed.sock, or empty without a runtime directory.
base::FilePath HelperSocketPath();

// <directory of the browser binary>/atlas-fed.
base::FilePath HelperBinaryPath();

// One load through the helper at `socket`, starting `helper` when nothing
// answers (an empty `helper` never starts one). `timeout` bounds each read.
FedResult FetchFromHelper(const base::FilePath& socket,
                          const base::FilePath& helper,
                          const std::string& url,
                          LoadKind kind,
                          base::TimeDelta timeout);

// The reply that tells a person why a Federate page cannot be shown now (status
// 503, an HTML page in the helper's own style; net_error net::OK). For the
// document of a navigation; a subresource just fails.
FedResult FaultPageFor(HelperFault fault);

// Forgets that a helper was started or failed a moment ago (tests).
void ResetHelperStartMemoryForTesting();

// FetchFromHelper with the paths above and a 120 s read timeout.
FedResult Fetch(const std::string& url, LoadKind kind);

}  // namespace atlas_fed

#endif  // CHROME_BROWSER_ATLAS_FED_FED_HELPER_CLIENT_H_
