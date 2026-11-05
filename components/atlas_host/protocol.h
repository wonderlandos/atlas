// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATLAS_HOST_PROTOCOL_H_
#define COMPONENTS_ATLAS_HOST_PROTOCOL_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>
#include <variant>

// The Atlas Host Protocol, version 2: what the browser says to the program on
// the other end of $XDG_RUNTIME_DIR/atlas/bridge.sock, and what it hears back.
// The protocol is a published, versioned wire and this file is its browser
// side; the other side is documented with the protocol and tested with the
// same fixtures (components/atlas_host/testdata/, identical byte for byte).
//
// The bridge speaks first (ADR 0033): as soon as it accepts the connection it
// writes `hello <nonce>`, and the browser writes the same line back before its
// own hello. The kernel stamps every byte of that echo with the writing
// process, so only the process that connected can answer it. See
// EchoOfGreeting().
//
// One JSON object per line, UTF-8, at most kMaxLine bytes with the newline.
// Fields are written in a fixed order. A request other than the hello has an
// `id` of 1 or more and gets exactly one reply with that id.
namespace atlas_host {

// The protocol version this side speaks.
inline constexpr uint32_t kVersion = 2;
// The longest line either side sends, newline included.
inline constexpr size_t kMaxLine = 32 * 1024;
// The longest greeting line, newline included.
inline constexpr size_t kMaxGreeting = 64;
// The longest path of a request, in bytes.
inline constexpr size_t kMaxPath = 4096;
// Requests one connection may have waiting at once.
inline constexpr size_t kMaxInFlight = 4;
// The socket: $XDG_RUNTIME_DIR/<dir>/<name>.
inline constexpr char kSocketDir[] = "atlas";
inline constexpr char kSocketName[] = "bridge.sock";

enum class Verb { kShare, kOpenWith, kTrash };

// What happened, as far as the browser may know.
enum class Outcome {
  kCopied,     // Share: copied to the clipboard.
  kSaved,      // Share: saved to a place the person chose.
  kOpened,     // Share or Open With: opened in an app.
  kCancelled,  // The person closed the sheet.
  kTrashed,    // Trash: moved to the Trash.
};

// Why a request did not run. Values are the strings of the wire.
enum class ErrorCode {
  kInvalidRequest,
  kVersion,
  kNotBrowser,
  kPermissionDenied,
  kUnavailable,
  kNotFound,
  kBusy,
  kFailed,
};

struct Hello {
  uint32_t version = kVersion;
  int32_t pid = 0;
  bool operator==(const Hello&) const = default;
};

struct VerbRequest {
  Verb verb = Verb::kShare;
  uint32_t id = 0;
  std::string path;
  bool operator==(const VerbRequest&) const = default;
};

using Request = std::variant<Hello, VerbRequest>;

struct HelloReply {
  uint32_t version = 0;
  bool operator==(const HelloReply&) const = default;
};

struct OkReply {
  uint32_t id = 0;
  Outcome outcome = Outcome::kCancelled;
  bool operator==(const OkReply&) const = default;
};

struct ErrorReply {
  uint32_t id = 0;  // 0: the error of no request (a bad line).
  ErrorCode code = ErrorCode::kFailed;
  std::string text;  // Shown to the person as it is.
  bool operator==(const ErrorReply&) const = default;
};

using Reply = std::variant<HelloReply, OkReply, ErrorReply>;

// True if |path| is a full path as the protocol takes it: from '/', no empty,
// '.' or '..' part, no trailing '/', no control character, valid UTF-8, at
// most kMaxPath bytes. "/" alone is not.
bool IsFullPath(std::string_view path);

// One line, newline included. An empty string if the request is not one the
// protocol allows (a path that is not a full path, id 0, version 0, pid 0).
std::string EncodeRequest(const Request& request);
std::string EncodeReply(const Reply& reply);

// Reads one line, with or without its newline. nullopt if the line is not
// exactly a request (reply) of this version.
std::optional<Request> DecodeRequest(std::string_view line);
std::optional<Reply> DecodeReply(std::string_view line);

// The bridge's greeting, without its newline: `hello ` and a nonce of ASCII
// letters and digits. Returns the line to write back (the same line, newline
// included), or nullopt if |line| is not a greeting (a JSON reply, anything
// else, a nonce that is empty or not alphanumeric, a line over kMaxGreeting).
std::optional<std::string> EchoOfGreeting(std::string_view line);

// The names on the wire.
const char* VerbName(Verb verb);
const char* OutcomeName(Outcome outcome);
const char* ErrorCodeName(ErrorCode code);

// Collects bytes from the socket and hands out whole lines.
class LineReader {
 public:
  // Adds bytes read from the connection.
  void Append(std::string_view bytes);

  // The next whole line, without its newline. nullopt if there is none yet.
  std::optional<std::string> Next();

  // True once more than kMaxLine bytes arrived without a line ending: the
  // other side is not speaking the protocol and the connection must close.
  bool overflowed() const { return overflowed_; }

 private:
  std::string buffer_;
  bool overflowed_ = false;
};

// What the person reads when the answer does not come from the other end.
// Same voice as the texts of the other end: the person is the subject, no
// path, a way forward.
inline constexpr char kTextUnreachable[] =
    "WolOS could not be reached. Try again in a moment.";
inline constexpr char kTextNoAnswer[] =
    "WolOS did not answer in time. Try again in a moment.";
inline constexpr char kTextLost[] =
    "The connection to WolOS was lost. Try again in a moment.";
inline constexpr char kTextBadAnswer[] =
    "WolOS sent an answer Atlas does not understand. Try again in a moment.";
inline constexpr char kTextBadName[] =
    "The name of this file cannot be sent to WolOS.";
inline constexpr char kTextTooMany[] =
    "Something else is waiting for you to choose. Finish it, then try again.";

}  // namespace atlas_host

#endif  // COMPONENTS_ATLAS_HOST_PROTOCOL_H_
