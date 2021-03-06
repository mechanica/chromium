// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_SYNCABLE_LOCAL_FILE_SYNC_STATUS_H_
#define WEBKIT_FILEAPI_SYNCABLE_LOCAL_FILE_SYNC_STATUS_H_

#include <map>
#include <set>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/threading/non_thread_safe.h"
#include "webkit/fileapi/file_system_url.h"

namespace fileapi {

// Represents local file sync status.
// This class is supposed to run only on IO thread.
//
// This class manages two important synchronization flags: writing (counter)
// and syncing (flag).  Writing counter keeps track of which URL is in
// writing and syncing flag indicates which URL is in syncing.
//
// An invariant of this class is: no FileSystem objects should be both
// in syncing_ and writing_ status, i.e. trying to increment writing
// while the target url is in syncing must fail and vice versa.
class WEBKIT_STORAGE_EXPORT LocalFileSyncStatus : public base::NonThreadSafe {
 public:
  LocalFileSyncStatus();
  ~LocalFileSyncStatus();

  // Increment writing counter for |url|.
  // This should not be called if the |url| is not writable.
  void StartWriting(const FileSystemURL& url);

  // Decrement writing counter for |url|.
  void EndWriting(const FileSystemURL& url);

  // Start syncing for |url| and disable writing.
  // This should not be called if |url| is in writing.
  void StartSyncing(const FileSystemURL& url);

  // Clears the syncing flag for |url| and enable writing.
  void EndSyncing(const FileSystemURL& url);

  // Returns true if the |url| or its parent or child is in writing.
  bool IsWriting(const FileSystemURL& url) const;

  // Returns true if the |url| is enabled for writing (i.e. not in syncing).
  bool IsWritable(const FileSystemURL& url) const;

 private:
  typedef std::map<FileSystemURL, int64, FileSystemURL::Comparator> URLCountMap;
  typedef std::set<FileSystemURL, FileSystemURL::Comparator> URLSet;

  bool IsChildOrParentWriting(const FileSystemURL& url) const;
  bool IsChildOrParentSyncing(const FileSystemURL& url) const;

  // If this count is non-zero positive there're ongoing write operations.
  URLCountMap writing_;

  // If this flag is set sync process is running on the file.
  URLSet syncing_;

  DISALLOW_COPY_AND_ASSIGN(LocalFileSyncStatus);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_SYNCABLE_LOCAL_FILE_SYNC_STATUS_H_
