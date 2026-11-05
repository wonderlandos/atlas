// Copyright 2026 The Atlas Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/atlas_host/download_verbs.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"

namespace atlas_host {

bool IsDownloadVerbCommand(int command_id) {
  return command_id == kCommandOpenWith || command_id == kCommandShare ||
         command_id == kCommandMoveToTrash;
}

bool DownloadVerbsAvailable() {
  AtlasHost* host = AtlasHost::Get();
  if (!host) {
    return false;
  }
  if (!host->IsAvailable()) {
    host->EnsureConnected();
    return false;
  }
  return true;
}

void RunDownloadVerb(int command_id,
                     const base::FilePath& path,
                     ResultCallback callback) {
  AtlasHost* host = AtlasHost::Get();
  if (!host || !IsDownloadVerbCommand(command_id)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       Result::Error(ErrorCode::kUnavailable,
                                     std::string(kTextUnreachable))));
    return;
  }
  Verb verb = command_id == kCommandShare      ? Verb::kShare
              : command_id == kCommandOpenWith ? Verb::kOpenWith
                                               : Verb::kTrash;
  host->Request(verb, path, std::move(callback));
}

}  // namespace atlas_host
