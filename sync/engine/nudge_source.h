// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_ENGINE_NUDGE_SOURCE_H_
#define SYNC_ENGINE_NUDGE_SOURCE_H_

namespace syncer {

enum NudgeSource {
  NUDGE_SOURCE_UNKNOWN = 0,
  // We received an invalidation message and are nudging to check for updates.
  NUDGE_SOURCE_NOTIFICATION,
  // A local change occurred (e.g. bookmark moved).
  NUDGE_SOURCE_LOCAL,
  // A previous sync cycle did not fully complete (e.g. HTTP error).
  NUDGE_SOURCE_CONTINUATION,
  // A local event is triggering an optimistic datatype refresh.
  NUDGE_SOURCE_LOCAL_REFRESH,
};

const char* GetNudgeSourceString(NudgeSource nudge_source);

}  // namespace syncer

#endif  // SYNC_ENGINE_NUDGE_SOURCE_H_
