// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_KIT_KIT_PRIVACY_POPOVER_H_
#define CHROME_BROWSER_UI_ATLAS_KIT_KIT_PRIVACY_POPOVER_H_

#include "base/functional/callback.h"
#include "chrome/browser/ui/atlas/toolbar/privacy_state.h"

namespace views {
class View;
class Widget;
}  // namespace views

namespace atlas::kit {

// The privacy popover as a Kit popover (reel scene R02): the same rows as the
// engine-styled bubble of atlas/toolbar (name, On or Off, one line of what it
// does) in the card with radius 12, and "Privacy settings" as a popover row.
// Returns the popover's widget.
views::Widget* ShowKitPrivacyPopover(views::View* anchor,
                                     const atlas::toolbar::PrivacyState& state,
                                     base::RepeatingClosure open_settings);

// Makes atlas::toolbar's privacy button show its popover this way (Kit chrome
// on).
void InstallKitPrivacyPresenter();

}  // namespace atlas::kit

#endif  // CHROME_BROWSER_UI_ATLAS_KIT_KIT_PRIVACY_POPOVER_H_
