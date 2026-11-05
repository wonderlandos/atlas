// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_KIT_KIT_TAB_MENU_H_
#define CHROME_BROWSER_UI_ATLAS_KIT_KIT_TAB_MENU_H_

#include "ui/gfx/geometry/rect.h"

class BrowserWindowInterface;

namespace views {
class Widget;
}

namespace atlas::kit {

// The context menu of the tab at `tab_index` of `browser`, as a Kit popover at
// `screen_rect` of `widget`. The entries are the engine's TabMenuModel (so
// the tab commands, the group entries and the extensions' items stay the
// engine's) and run through TabStripModel as the engine's tab strip does.
void ShowKitTabMenu(BrowserWindowInterface* browser,
                    int tab_index,
                    views::Widget* widget,
                    const gfx::Rect& screen_rect);

}  // namespace atlas::kit

#endif  // CHROME_BROWSER_UI_ATLAS_KIT_KIT_TAB_MENU_H_
