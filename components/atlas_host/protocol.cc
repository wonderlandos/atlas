// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/atlas_host/protocol.h"

#include <cmath>
#include <limits>
#include <utility>

#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/values.h"

namespace atlas_host {

namespace {

constexpr char kHex[] = "0123456789abcdef";

// A JSON string the way the other end writes it, so that a line is the same
// bytes on both sides: only '"', '\\' and the control characters below U+0020
// are escaped, everything else (the person's file names included) is written
// as it is.
void AppendString(std::string& out, std::string_view s) {
  out.push_back('"');
  for (char c : s) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          out += "\\u00";
          out.push_back(kHex[(c >> 4) & 0xf]);
          out.push_back(kHex[c & 0xf]);
        } else {
          out.push_back(c);
        }
    }
  }
  out.push_back('"');
}

// A number the wire carries as an unsigned 32-bit integer. The JSON reader
// returns a double for a number that does not fit its int.
std::optional<uint32_t> GetU32(const base::Value* v) {
  if (!v) {
    return std::nullopt;
  }
  if (v->is_int()) {
    int i = v->GetInt();
    if (i < 0) {
      return std::nullopt;
    }
    return static_cast<uint32_t>(i);
  }
  if (v->is_double()) {
    double d = v->GetDouble();
    if (d >= 0 && d <= std::numeric_limits<uint32_t>::max() &&
        d == std::floor(d)) {
      return static_cast<uint32_t>(d);
    }
  }
  return std::nullopt;
}

// The line as a dictionary; nullopt if it is anything else than an object.
// The other end refuses a line that does not start with '{'.
std::optional<base::DictValue> ReadObject(std::string_view line) {
  if (line.ends_with('\n')) {
    line.remove_suffix(1);
  }
  if (line.size() + 1 > kMaxLine || !line.starts_with('{')) {
    return std::nullopt;
  }
  return base::JSONReader::ReadDict(line, base::JSON_PARSE_RFC);
}

std::optional<Verb> VerbOfName(std::string_view name) {
  if (name == "share") {
    return Verb::kShare;
  }
  if (name == "open_with") {
    return Verb::kOpenWith;
  }
  if (name == "trash") {
    return Verb::kTrash;
  }
  return std::nullopt;
}

constexpr std::pair<Outcome, const char*> kOutcomes[] = {
    {Outcome::kCopied, "copied"},   {Outcome::kSaved, "saved"},
    {Outcome::kOpened, "opened"},   {Outcome::kCancelled, "cancelled"},
    {Outcome::kTrashed, "trashed"},
};

constexpr std::pair<ErrorCode, const char*> kErrorCodes[] = {
    {ErrorCode::kInvalidRequest, "invalid-request"},
    {ErrorCode::kVersion, "version"},
    {ErrorCode::kNotBrowser, "not-browser"},
    {ErrorCode::kPermissionDenied, "permission-denied"},
    {ErrorCode::kUnavailable, "unavailable"},
    {ErrorCode::kNotFound, "not-found"},
    {ErrorCode::kBusy, "busy"},
    {ErrorCode::kFailed, "failed"},
};

}  // namespace

bool IsFullPath(std::string_view path) {
  if (path.size() < 2 || path.size() > kMaxPath || path.front() != '/' ||
      path.back() == '/') {
    return false;
  }
  // Valid UTF-8 without a control character (U+0000-U+001F, U+007F-U+009F).
  for (size_t i = 0; i < path.size(); ++i) {
    base_icu::UChar32 c;
    if (!base::ReadUnicodeCharacter(path, &i, &c)) {
      return false;
    }
    if (c < 0x20 || (c >= 0x7f && c <= 0x9f)) {
      return false;
    }
  }
  std::string_view rest = path.substr(1);
  while (true) {
    size_t slash = rest.find('/');
    std::string_view part = rest.substr(0, slash);
    if (part.empty() || part == "." || part == "..") {
      return false;
    }
    if (slash == std::string_view::npos) {
      return true;
    }
    rest.remove_prefix(slash + 1);
  }
}

const char* VerbName(Verb verb) {
  switch (verb) {
    case Verb::kShare:
      return "share";
    case Verb::kOpenWith:
      return "open_with";
    case Verb::kTrash:
      return "trash";
  }
  return "";
}

const char* OutcomeName(Outcome outcome) {
  for (const auto& [o, name] : kOutcomes) {
    if (o == outcome) {
      return name;
    }
  }
  return "";
}

const char* ErrorCodeName(ErrorCode code) {
  for (const auto& [c, name] : kErrorCodes) {
    if (c == code) {
      return name;
    }
  }
  return "";
}

std::string EncodeRequest(const Request& request) {
  std::string out;
  if (const auto* hello = std::get_if<Hello>(&request)) {
    if (hello->version == 0 || hello->pid <= 0) {
      return std::string();
    }
    out += "{\"op\":\"hello\",\"version\":";
    out += std::to_string(hello->version);
    out += ",\"pid\":";
    out += std::to_string(hello->pid);
    out += "}\n";
    return out;
  }
  const auto& v = std::get<VerbRequest>(request);
  if (v.id == 0 || !IsFullPath(v.path)) {
    return std::string();
  }
  out += "{\"op\":\"";
  out += VerbName(v.verb);
  out += "\",\"id\":";
  out += std::to_string(v.id);
  out += ",\"path\":";
  AppendString(out, v.path);
  out += "}\n";
  return out;
}

std::string EncodeReply(const Reply& reply) {
  std::string out;
  if (const auto* hello = std::get_if<HelloReply>(&reply)) {
    out += "{\"reply\":\"hello\",\"version\":";
    out += std::to_string(hello->version);
  } else if (const auto* ok = std::get_if<OkReply>(&reply)) {
    out += "{\"reply\":\"ok\",\"id\":";
    out += std::to_string(ok->id);
    out += ",\"outcome\":\"";
    out += OutcomeName(ok->outcome);
    out += "\"";
  } else {
    const auto& e = std::get<ErrorReply>(reply);
    out += "{\"reply\":\"error\",\"id\":";
    out += std::to_string(e.id);
    out += ",\"code\":\"";
    out += ErrorCodeName(e.code);
    out += "\",\"text\":";
    AppendString(out, e.text);
  }
  out += "}\n";
  return out;
}

std::optional<Request> DecodeRequest(std::string_view line) {
  auto dict = ReadObject(line);
  if (!dict) {
    return std::nullopt;
  }
  const std::string* op = dict->FindString("op");
  if (!op) {
    return std::nullopt;
  }
  if (*op == "hello") {
    auto version = GetU32(dict->Find("version"));
    auto pid = GetU32(dict->Find("pid"));
    if (dict->size() != 3 || !version || !pid || *version == 0 || *pid == 0 ||
        *pid > static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
      return std::nullopt;
    }
    return Hello{*version, static_cast<int32_t>(*pid)};
  }
  auto verb = VerbOfName(*op);
  auto id = GetU32(dict->Find("id"));
  const std::string* path = dict->FindString("path");
  if (!verb || dict->size() != 3 || !id || *id == 0 || !path ||
      !IsFullPath(*path)) {
    return std::nullopt;
  }
  return VerbRequest{*verb, *id, *path};
}

std::optional<Reply> DecodeReply(std::string_view line) {
  auto dict = ReadObject(line);
  if (!dict) {
    return std::nullopt;
  }
  const std::string* kind = dict->FindString("reply");
  if (!kind) {
    return std::nullopt;
  }
  if (*kind == "hello") {
    auto version = GetU32(dict->Find("version"));
    if (dict->size() != 2 || !version || *version == 0) {
      return std::nullopt;
    }
    return HelloReply{*version};
  }
  auto id = GetU32(dict->Find("id"));
  if (!id) {
    return std::nullopt;
  }
  if (*kind == "ok") {
    const std::string* name = dict->FindString("outcome");
    if (dict->size() != 3 || !name || *id == 0) {
      return std::nullopt;
    }
    for (const auto& [outcome, wire] : kOutcomes) {
      if (*name == wire) {
        return OkReply{*id, outcome};
      }
    }
    return std::nullopt;
  }
  if (*kind == "error") {
    const std::string* name = dict->FindString("code");
    const std::string* text = dict->FindString("text");
    if (dict->size() != 4 || !name || !text ||
        base::TrimWhitespaceASCII(*text, base::TRIM_ALL).empty()) {
      return std::nullopt;
    }
    for (const auto& [code, wire] : kErrorCodes) {
      if (*name == wire) {
        return ErrorReply{*id, code, *text};
      }
    }
  }
  return std::nullopt;
}

void LineReader::Append(std::string_view bytes) {
  if (overflowed_) {
    return;
  }
  buffer_.append(bytes);
  // A line needs its newline within kMaxLine bytes.
  size_t newline = buffer_.find('\n');
  if (newline == std::string::npos ? buffer_.size() >= kMaxLine
                                   : newline + 1 > kMaxLine) {
    overflowed_ = true;
    buffer_.clear();
  }
}

std::optional<std::string> LineReader::Next() {
  size_t newline = buffer_.find('\n');
  if (overflowed_ || newline == std::string::npos) {
    return std::nullopt;
  }
  std::string line = buffer_.substr(0, newline);
  buffer_.erase(0, newline + 1);
  return line;
}

}  // namespace atlas_host
