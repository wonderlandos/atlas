// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_TOOLBAR_PRIVACY_BUTTON_H_
#define CHROME_BROWSER_UI_ATLAS_TOOLBAR_PRIVACY_BUTTON_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/atlas/toolbar/privacy_bubble.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace atlas::toolbar {

// The privacy button of the toolbar. Its glyph is the accent colour while Do Not Track is on and
// the dim colour when the person turned it off; its tooltip and accessible name say which. Pressing
// it (mouse, or Space and Enter from the keyboard) opens the popover with the privacy facts
// (privacy_bubble.h); Esc closes it.
class PrivacyButton : public ToolbarButton {
  METADATA_HEADER(PrivacyButton, ToolbarButton)

 public:
  // `prefs` must outlive the button. `open_settings` runs when the person follows the link
  // "Privacy settings" in the popover.
  PrivacyButton(PrefService* prefs, base::RepeatingClosure open_settings);
  PrivacyButton(const PrivacyButton&) = delete;
  PrivacyButton& operator=(const PrivacyButton&) = delete;
  ~PrivacyButton() override;

  // Whether the button currently shows the "on" state (Do Not Track is on).
  bool is_on() const { return is_on_; }

  // The popover, for tests.
  PrivacyBubbleController& bubble() { return bubble_; }

  // The tooltip and accessible name of each state.
  static std::u16string TextFor(bool on);

  // ToolbarButton:
  SkColor GetForegroundColor(ButtonState state) const override;

 private:
  // Re-reads Do Not Track and refreshes the glyph colour, tooltip and name.
  void Update();
  // Shows or hides the button as kShowPrivacyButton says.
  void UpdateVisibility();

  // Opens the popover, or closes it when it is open.
  void Pressed();

  const raw_ptr<PrefService> prefs_;
  const base::RepeatingClosure open_settings_;
  PrivacyBubbleController bubble_;
  PrefChangeRegistrar pref_registrar_;
  bool is_on_ = true;
};

}  // namespace atlas::toolbar

#endif  // CHROME_BROWSER_UI_ATLAS_TOOLBAR_PRIVACY_BUTTON_H_
