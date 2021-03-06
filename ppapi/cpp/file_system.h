// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_FILE_SYSTEM_H_
#define PPAPI_CPP_FILE_SYSTEM_H_

#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/resource.h"

/// @file
/// This file defines the API to create a file system associated with a file.

namespace pp {

class CompletionCallback;

/// The <code>FileSystem</code> class identifies the file system type
/// associated with a file.
class FileSystem : public Resource {
 public:
  /// Constructs an is_null() filesystem resource. If you use this constructor,
  /// you will have to assign it to a "real" FileSystem object before you can
  /// use it.
  FileSystem();

  /// This constructor creates a file system object of the given type.
  ///
  /// @param[in] instance The instance with which this resource will be
  /// associated.
  ///
  /// @param[in] type A file system type as defined by
  /// <code>PP_FileSystemType</code> enum.
  FileSystem(const InstanceHandle& instance, PP_FileSystemType type);

  /// Open() opens the file system. A file system must be opened before running
  /// any other operation on it.
  ///
  /// @param[in] expected_size The expected size of the file system. Note that
  /// this does not request quota; to do that, you must either invoke
  /// requestQuota from JavaScript:
  /// http://www.html5rocks.com/en/tutorials/file/filesystem/#toc-requesting-quota
  /// or set the unlimitedStorage permission for Chrome Web Store apps:
  /// http://code.google.com/chrome/extensions/manifest.html#permissions
  ///
  /// @param[in] cc A <code>PP_CompletionCallback</code> to be called upon
  /// completion of Open().
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  int32_t Open(int64_t expected_size, const CompletionCallback& cc);
};

}  // namespace pp

#endif  // PPAPI_CPP_FILE_SYSTEM_H_
