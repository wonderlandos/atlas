// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/atlas/newtab/newtab_handler.h"

#include <memory>
#include <optional>
#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/atlas/newtab/newtab_prefs.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/atlas_pages/newtab/newtab_state.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/referrer.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace atlas::newtab {

namespace {

// The strings the page asks for, by the key the page uses (newtab.js).
struct StringKey {
  const char* key;
  int id;
  // The string takes one value ($1); the page gets it with {0} where the
  // value goes and fills it in itself (newtab.js).
  bool takes_value = false;
};
constexpr StringKey kStrings[] = {
    {"pinned", IDS_ATLAS_NEWTAB_PINNED},
    {"recent", IDS_ATLAS_NEWTAB_RECENT},
    {"pinnedEmpty", IDS_ATLAS_NEWTAB_PINNED_EMPTY},
    {"emptyTitle", IDS_ATLAS_NEWTAB_EMPTY_TITLE},
    {"emptyHint", IDS_ATLAS_NEWTAB_EMPTY_HINT},
    {"emptyAction", IDS_ATLAS_NEWTAB_EMPTY_ACTION},
    {"dntOn", IDS_ATLAS_DNT_IS_ON},
    {"opened", IDS_ATLAS_NEWTAB_OPENED, true},
    {"openedJustNow", IDS_ATLAS_NEWTAB_OPENED_JUST_NOW},
    {"removeFromPinned", IDS_ATLAS_NEWTAB_REMOVE_FROM_PINNED, true},
    {"removedFromPinned", IDS_ATLAS_NEWTAB_REMOVED_FROM_PINNED, true},
    {"searchPlaceholder", IDS_ATLAS_NEWTAB_SEARCH_PLACEHOLDER},
};

// Windows that already showed the page once (the entrance plays the first
// time a window shows it, motion.md "New tab page appears").
std::set<SessionID::id_type>& WindowsThatShowedThePage() {
  static std::set<SessionID::id_type> windows;
  return windows;
}

bool IsCallId(const base::ListValue& args, size_t count) {
  return args.size() == count && args[0].is_int();
}

}  // namespace

NewTabHandler::NewTabHandler(Profile* profile) : profile_(profile) {}

NewTabHandler::~NewTabHandler() = default;

void NewTabHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getState", base::BindRepeating(&NewTabHandler::HandleGetState,
                                      base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "open", base::BindRepeating(&NewTabHandler::HandleOpen,
                                  base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "submit", base::BindRepeating(&NewTabHandler::HandleSubmit,
                                    base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "pin", base::BindRepeating(&NewTabHandler::HandlePin,
                                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "unpin", base::BindRepeating(&NewTabHandler::HandleUnpin,
                                   base::Unretained(this)));
}

// static
std::vector<RecentPage> NewTabHandler::RecentFrom(
    const sessions::TabRestoreService::Entries& entries) {
  std::vector<RecentPage> candidates;
  auto add_tab = [&](const sessions::tab_restore::Tab& tab, base::Time when) {
    if (tab.navigations.empty()) {
      return;
    }
    const sessions::SerializedNavigationEntry& nav =
        tab.navigations[tab.normalized_navigation_index()];
    candidates.push_back({nav.title(), nav.virtual_url(), when});
  };
  for (const auto& entry : entries) {
    switch (entry->type) {
      case sessions::tab_restore::TAB:
        add_tab(static_cast<const sessions::tab_restore::Tab&>(*entry),
                entry->timestamp);
        break;
      case sessions::tab_restore::WINDOW:
        // The tabs of a closed window carry the window's time.
        for (const auto& tab :
             static_cast<const sessions::tab_restore::Window&>(*entry).tabs) {
          add_tab(*tab, entry->timestamp);
        }
        break;
      case sessions::tab_restore::SPLIT:
      case sessions::tab_restore::GROUP:
        break;
    }
  }
  return PickRecent(candidates);
}

void NewTabHandler::HandleGetState(const base::ListValue& args) {
  if (!IsCallId(args, 1)) {
    return;
  }
  int call_id = args[0].GetInt();
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(profile_);
  if (service && !service->IsLoaded()) {
    // The list of closed tabs is read from disk; answer when it is there.
    waiting_for_restore_list_.push_back(call_id);
    if (!restore_observation_.IsObserving()) {
      restore_observation_.Observe(service);
      service->LoadTabsFromLastSession();
    }
    return;
  }
  Reply(call_id);
}

void NewTabHandler::TabRestoreServiceLoaded(
    sessions::TabRestoreService* service) {
  restore_observation_.Reset();
  std::vector<int> waiting;
  waiting.swap(waiting_for_restore_list_);
  for (int call_id : waiting) {
    Reply(call_id);
  }
}

void NewTabHandler::TabRestoreServiceDestroyed(
    sessions::TabRestoreService* service) {
  restore_observation_.Reset();
}

base::DictValue NewTabHandler::Strings() const {
  base::DictValue strings;
  for (const StringKey& s : kStrings) {
    strings.Set(s.key, s.takes_value
                           ? l10n_util::GetStringFUTF8(s.id, u"{0}")
                           : l10n_util::GetStringUTF8(s.id));
  }
  return strings;
}

bool NewTabHandler::FirstInWindow() {
  const tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_ui()->GetWebContents());
  const BrowserWindowInterface* window =
      tab ? tab->GetBrowserWindowInterface() : nullptr;
  if (!window) {
    return false;
  }
  return WindowsThatShowedThePage().insert(window->GetSessionID().id()).second;
}

void NewTabHandler::Reply(int call_id) {
  PrefService* pref_service = profile_->GetPrefs();
  std::vector<PinnedPage> pinned =
      ParsePinned(pref_service->GetList(pref_names::kPinned));
  std::vector<RecentPage> recent;
  if (sessions::TabRestoreService* service =
          TabRestoreServiceFactory::GetForProfile(profile_)) {
    recent = RecentFrom(service->entries());
  }
  base::DictValue state = BuildState(
      pinned, recent, pref_service->GetBoolean(::prefs::kEnableDoNotTrack),
      /*status_strip=*/false, FirstInWindow());
  state.Set("strings", Strings());
  web_ui()->CallJavascriptFunctionUnsafe("atlasNewTabHost.resolve",
                                         base::Value(call_id),
                                         base::Value(std::move(state)));
}

void NewTabHandler::ResolveNull(int call_id) {
  web_ui()->CallJavascriptFunctionUnsafe("atlasNewTabHost.resolve",
                                         base::Value(call_id), base::Value());
}

void NewTabHandler::OpenWebUrl(const GURL& url,
                               WindowOpenDisposition disposition) {
  content::OpenURLParams params(url, content::Referrer(), disposition,
                                ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                                /*is_renderer_initiated=*/false);
  web_ui()->GetWebContents()->OpenURL(params, /*navigation_handle_callback=*/{});
}

void NewTabHandler::HandleOpen(const base::ListValue& args) {
  if (args.size() != 2 || !args[0].is_string() || !args[1].is_bool()) {
    return;
  }
  GURL url(args[0].GetString());
  if (!IsWebUrl(url)) {
    return;
  }
  OpenWebUrl(url, args[1].GetBool() ? WindowOpenDisposition::NEW_BACKGROUND_TAB
                                    : WindowOpenDisposition::CURRENT_TAB);
}

void NewTabHandler::HandleSubmit(const base::ListValue& args) {
  if (args.size() != 1 || !args[0].is_string()) {
    return;
  }
  const std::string& text = args[0].GetString();
  if (text.empty() || text.size() > 2048) {
    return;
  }
  // A Federate name (home.fed, lobo.pagina/x) opens as the fed:// page it
  // names, before anything else is asked: it is never a search, whatever the
  // classifier below says (QA-A9-WEB-ATLAS-1; patch 0124 teaches the classifier
  // the same).
  if (std::optional<GURL> fed = FederateAddressFor(text)) {
    OpenWebUrl(*fed, WindowOpenDisposition::CURRENT_TAB);
    return;
  }
  // The same decision the address bar makes, with no suggestion request: an
  // address opens, anything else is a search with the default provider.
  AutocompleteClassifier* classifier =
      AutocompleteClassifierFactory::GetForProfile(profile_);
  if (!classifier) {
    return;
  }
  AutocompleteMatch match;
  classifier->Classify(base::UTF8ToUTF16(text), /*prefer_keyword=*/false,
                       /*allow_exact_keyword_match=*/false,
                       metrics::OmniboxEventProto::NTP, &match, nullptr);
  if (!match.destination_url.is_valid() ||
      match.destination_url.SchemeIs("javascript")) {
    return;
  }
  content::OpenURLParams params(match.destination_url, content::Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                match.transition,
                                /*is_renderer_initiated=*/false);
  web_ui()->GetWebContents()->OpenURL(params, /*navigation_handle_callback=*/{});
}

void NewTabHandler::HandlePin(const base::ListValue& args) {
  if (!IsCallId(args, 3) || !args[1].is_string() || !args[2].is_string()) {
    return;
  }
  PrefService* pref_service = profile_->GetPrefs();
  bool changed = false;
  std::vector<PinnedPage> pages = WithPinned(
      ParsePinned(pref_service->GetList(pref_names::kPinned)), GURL(args[1].GetString()),
      args[2].GetString(), &changed);
  if (changed) {
    pref_service->SetList(pref_names::kPinned, PinnedToList(pages));
  }
  ResolveNull(args[0].GetInt());
}

void NewTabHandler::HandleUnpin(const base::ListValue& args) {
  if (!IsCallId(args, 2) || !args[1].is_string()) {
    return;
  }
  PrefService* pref_service = profile_->GetPrefs();
  bool changed = false;
  std::vector<PinnedPage> pages =
      WithoutPinned(ParsePinned(pref_service->GetList(pref_names::kPinned)),
                    GURL(args[1].GetString()), &changed);
  if (changed) {
    pref_service->SetList(pref_names::kPinned, PinnedToList(pages));
  }
  ResolveNull(args[0].GetInt());
}

}  // namespace atlas::newtab
