// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/kit/kit_text.h"

#include <string_view>

namespace atlas::kit {

namespace {

bool IsWordChar(char16_t c) {
  return (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z') ||
         (c >= u'0' && c <= u'9') || c == u'_';
}

}  // namespace

std::u16string DoctrineClean(const std::u16string& label) {
  static constexpr std::u16string_view kNames[] = {u"Helium", u"Chromium",
                                                   u"Chrome"};
  std::u16string out = label;
  for (std::u16string_view name : kNames) {
    size_t at = 0;
    while ((at = out.find(name, at)) != std::u16string::npos) {
      const size_t end = at + name.size();
      const bool word_start = at == 0 || !IsWordChar(out[at - 1]);
      const bool word_end = end >= out.size() || !IsWordChar(out[end]);
      if (word_start && word_end) {
        out.replace(at, name.size(), u"Atlas");
        at += 5;
      } else {
        at = end;
      }
    }
  }
  return out;
}

std::u16string WrapToLineChars(const std::u16string& text, size_t max_chars) {
  std::u16string out;
  size_t line = 0;
  size_t i = 0;
  while (i < text.size()) {
    size_t end = text.find(u' ', i);
    if (end == std::u16string::npos) {
      end = text.size();
    }
    const size_t word = end - i;
    if (line > 0 && line + 1 + word > max_chars) {
      out += u'\n';
      line = 0;
    } else if (line > 0) {
      out += u' ';
      ++line;
    }
    out.append(text, i, word);
    line += word;
    i = end + 1;
  }
  return out;
}

}  // namespace atlas::kit
