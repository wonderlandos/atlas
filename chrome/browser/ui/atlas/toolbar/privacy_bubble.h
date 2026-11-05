// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ATLAS_TOOLBAR_PRIVACY_BUBBLE_H_
#define CHROME_BROWSER_UI_ATLAS_TOOLBAR_PRIVACY_BUBBLE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/atlas/toolbar/privacy_state.h"

namespace views {
class View;
class Widget;
}  // namespace views

namespace atlas::toolbar {

// Builds the contents of the popover: for each row of PrivacyRows() its name and its value (On or
// Off), one line under it that says what the setting does, and at the bottom the link "Privacy
// settings", which runs `open_settings`. The view only reports `state`; it changes no pref.
std::unique_ptr<views::View> BuildPrivacyContents(
    const PrivacyState& state,
    base::RepeatingClosure open_settings);

// The popover. Open() shows it under `anchor`; Toggle() closes it when it is open. One instance
// per button; it never outlives the anchor view.
class PrivacyBubbleController {
 public:
  PrivacyBubbleController();
  PrivacyBubbleController(const PrivacyBubbleController&) = delete;
  PrivacyBubbleController& operator=(const PrivacyBubbleController&) = delete;
  ~PrivacyBubbleController();

  // Shows the popover under `anchor` and returns its widget. The default is the engine-styled
  // bubble below; the Kit chrome (ADR 0062) sets one that draws the same rows as a Kit popover.
  // Process-wide, set before the first button is pressed; an empty callback restores the default.
  using Presenter =
      base::RepeatingCallback<views::Widget*(views::View* anchor,
                                             const PrivacyState& state,
                                             base::RepeatingClosure open_settings)>;
  static void SetPresenter(Presenter presenter);

  bool IsShowing() const;

  // Opens the popover, or closes it when it is already open. `open_settings` runs when the person
  // follows "Privacy settings"; the popover closes first.
  void Toggle(views::View* anchor,
              const PrivacyState& state,
              base::RepeatingClosure open_settings);

  // Closes the popover if it is open.
  void Close();

 private:
  base::WeakPtr<views::Widget> widget_;
  base::WeakPtrFactory<PrivacyBubbleController> weak_factory_{this};
};

}  // namespace atlas::toolbar

#endif  // CHROME_BROWSER_UI_ATLAS_TOOLBAR_PRIVACY_BUBBLE_H_
