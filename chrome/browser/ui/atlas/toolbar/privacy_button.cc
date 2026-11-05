// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/toolbar/privacy_button.h"

#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/ui/atlas/toolbar/privacy_state.h"
#include "chrome/browser/ui/atlas/toolbar/toolbar_ids.h"
#include "chrome/browser/ui/atlas/toolbar/toolbar_prefs.h"
#include "chrome/browser/ui/atlas/toolbar/vector_icons/vector_icons.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_class_properties.h"

namespace atlas::toolbar {

PrivacyButton::PrivacyButton(PrefService* prefs,
                             base::RepeatingClosure open_settings)
    : ToolbarButton(base::BindRepeating(
          [](PrivacyButton* button, const ui::Event&) { button->Pressed(); },
          base::Unretained(this))),
      prefs_(prefs),
      open_settings_(std::move(open_settings)) {
  SetProperty(views::kElementIdentifierKey, kPrivacyButtonElementId);
  SetVectorIcon(kAtlasPrivacyIcon);

  pref_registrar_.Init(prefs_);
  pref_registrar_.Add(
      ::prefs::kEnableDoNotTrack,
      base::BindRepeating(&PrivacyButton::Update, base::Unretained(this)));
  pref_registrar_.Add(kShowPrivacyButton,
                      base::BindRepeating(&PrivacyButton::UpdateVisibility,
                                          base::Unretained(this)));
  Update();
  UpdateVisibility();
}

PrivacyButton::~PrivacyButton() = default;

// static
std::u16string PrivacyButton::TextFor(bool on) {
  return l10n_util::GetStringUTF16(on ? IDS_ATLAS_TOOLBAR_PRIVACY_TOOLTIP_ON
                                      : IDS_ATLAS_TOOLBAR_PRIVACY_TOOLTIP_OFF);
}

SkColor PrivacyButton::GetForegroundColor(ButtonState state) const {
  const ui::ColorProvider* color_provider = GetColorProvider();
  if (!color_provider) {
    return ToolbarButton::GetForegroundColor(state);
  }
  // accent while Do Not Track is on, dim when the person turned it off; a disabled button is dim.
  const bool accent = is_on_ && state != ButtonState::STATE_DISABLED;
  return color_provider->GetColor(accent ? ui::kColorAccent
                                         : ui::kColorSecondaryForeground);
}

void PrivacyButton::Update() {
  is_on_ = prefs_->GetBoolean(::prefs::kEnableDoNotTrack);
  const std::u16string text = TextFor(is_on_);
  SetTooltipText(text);
  GetViewAccessibility().SetName(text);
  if (GetColorProvider()) {
    UpdateIcon();
  }
}

void PrivacyButton::Pressed() {
  bubble_.Toggle(this, ReadPrivacyState(*prefs_), open_settings_);
}

void PrivacyButton::UpdateVisibility() {
  SetVisible(prefs_->GetBoolean(kShowPrivacyButton));
}

BEGIN_METADATA(PrivacyButton)
END_METADATA

}  // namespace atlas::toolbar
