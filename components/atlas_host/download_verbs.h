// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ATLAS_HOST_DOWNLOAD_VERBS_H_
#define COMPONENTS_ATLAS_HOST_DOWNLOAD_VERBS_H_

#include "base/files/file_path.h"
#include "components/atlas_host/atlas_host.h"

// What a downloads menu needs from the bridge, without knowing the bridge: the
// ids of the three verbs, whether to offer them, and how to run one.
namespace atlas_host {

// Menu command ids of the three verbs. They sit above every value of the
// engine's DownloadCommands::Command (the hook in download_ui_context_menu.cc
// asserts it), so a menu delegate tells them apart by range.
inline constexpr int kCommandOpenWith = 100;
inline constexpr int kCommandShare = 101;
inline constexpr int kCommandMoveToTrash = 102;

// True for the three ids above.
bool IsDownloadVerbCommand(int command_id);

// True when the bridge has answered the hello and the connection lives. A menu
// offers the verbs only then, so a machine without the bridge shows the
// engine's own items and no entry that does nothing. When the bridge is not
// there yet this also asks for a connection, so the next menu finds it.
bool DownloadVerbsAvailable();

// Runs the verb of |command_id| on |path|. |callback| runs once with what the
// person did, or with the words to show them if nothing happened; it is not
// run if the host is destroyed first.
void RunDownloadVerb(int command_id,
                     const base::FilePath& path,
                     ResultCallback callback);

}  // namespace atlas_host

#endif  // COMPONENTS_ATLAS_HOST_DOWNLOAD_VERBS_H_
