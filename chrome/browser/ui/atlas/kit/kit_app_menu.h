// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_KIT_KIT_APP_MENU_H_
#define CHROME_BROWSER_UI_ATLAS_KIT_KIT_APP_MENU_H_

class AppMenuIconController;
class BrowserWindowInterface;

namespace ui {
class AcceleratorProvider;
}

namespace views {
class View;
}

namespace atlas::kit {

// The app menu as a Kit popover (reel scene R07, tokens.md section 8): the
// engine's AppMenuModel is the source of the entries, their order, enabled
// state, checks and shortcuts (so extensions, enterprise policy and the
// engine's own gating keep working); the Kit draws them. Submenus open as a
// popover of their own with a back row; the zoom row keeps its buttons.
// Returns false when the Kit chrome is off (the engine draws its own menu).
bool ShowKitAppMenu(views::View* anchor,
                    BrowserWindowInterface* browser,
                    ui::AcceleratorProvider* provider,
                    AppMenuIconController* icon_controller);

}  // namespace atlas::kit

#endif  // CHROME_BROWSER_UI_ATLAS_KIT_KIT_APP_MENU_H_
