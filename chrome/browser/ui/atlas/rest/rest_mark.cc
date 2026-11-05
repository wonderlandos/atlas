// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/rest/rest_mark.h"

#include "chrome/browser/ui/atlas/chrome/atlas_tokens.h"
#include "chrome/browser/ui/atlas/rest/vector_icons/vector_icons.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/paint_vector_icon.h"

namespace atlas::rest {

bool IsResting(bool discarded, bool active) {
  return discarded && !active;
}

SkColor RestingTitleColor(SkColor text, SkColor fill) {
  return color_utils::AlphaBlend(text, fill, tokens::kDimAlpha);
}

gfx::Rect RestMarkBounds(int row_width, int row_height) {
  return gfx::Rect(row_width - kRestMarkTrailingInset - kRestMarkSize,
                   (row_height - kRestMarkSize) / 2, kRestMarkSize,
                   kRestMarkSize);
}

void PaintRestMark(gfx::Canvas* canvas, const gfx::Rect& bounds, SkColor color) {
  canvas->Save();
  canvas->Translate(bounds.OffsetFromOrigin());
  gfx::PaintVectorIcon(canvas, kAtlasRestIcon, bounds.width(), color);
  canvas->Restore();
}

}  // namespace atlas::rest
