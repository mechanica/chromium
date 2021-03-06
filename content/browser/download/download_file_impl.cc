// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_file_impl.h"

#include <string>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/message_loop_proxy.h"
#include "base/time.h"
#include "content/browser/download/byte_stream.h"
#include "content/browser/download/download_create_info.h"
#include "content/browser/download/download_interrupt_reasons_impl.h"
#include "content/browser/download/download_net_log_parameters.h"
#include "content/browser/power_save_blocker.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_destination_observer.h"
#include "content/browser/download/download_stats.h"
#include "net/base/io_buffer.h"

using content::BrowserThread;
using content::DownloadId;
using content::DownloadManager;

const int kUpdatePeriodMs = 500;
const int kMaxTimeBlockingFileThreadMs = 1000;

int content::DownloadFile::number_active_objects_ = 0;

DownloadFileImpl::DownloadFileImpl(
    scoped_ptr<content::DownloadSaveInfo> save_info,
    const FilePath& default_download_directory,
    const GURL& url,
    const GURL& referrer_url,
    int64 received_bytes,
    bool calculate_hash,
    scoped_ptr<content::ByteStreamReader> stream,
    const net::BoundNetLog& bound_net_log,
    scoped_ptr<content::PowerSaveBlocker> power_save_blocker,
    base::WeakPtr<content::DownloadDestinationObserver> observer)
        : file_(save_info->file_path,
                url,
                referrer_url,
                received_bytes,
                calculate_hash,
                save_info->hash_state,
                save_info->file_stream.Pass(),
                bound_net_log),
          default_download_directory_(default_download_directory),
          stream_reader_(stream.Pass()),
          bytes_seen_(0),
          bound_net_log_(bound_net_log),
          observer_(observer),
          weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
          power_save_blocker_(power_save_blocker.Pass()) {
}

DownloadFileImpl::~DownloadFileImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  --number_active_objects_;
}

void DownloadFileImpl::Initialize(const InitializeCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  update_timer_.reset(new base::RepeatingTimer<DownloadFileImpl>());
  net::Error net_result = file_.Initialize(default_download_directory_);
  if (net_result != net::OK) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE, base::Bind(
            callback, content::ConvertNetErrorToInterruptReason(
                net_result, content::DOWNLOAD_INTERRUPT_FROM_DISK)));
    return;
  }

  stream_reader_->RegisterCallback(
      base::Bind(&DownloadFileImpl::StreamActive, weak_factory_.GetWeakPtr()));

  download_start_ = base::TimeTicks::Now();

  // Initial pull from the straw.
  StreamActive();

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE, base::Bind(
          callback, content::DOWNLOAD_INTERRUPT_REASON_NONE));

  ++number_active_objects_;
}

content::DownloadInterruptReason DownloadFileImpl::AppendDataToFile(
    const char* data, size_t data_len) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (!update_timer_->IsRunning()) {
    update_timer_->Start(FROM_HERE,
                         base::TimeDelta::FromMilliseconds(kUpdatePeriodMs),
                         this, &DownloadFileImpl::SendUpdate);
  }
  return content::ConvertNetErrorToInterruptReason(
      file_.AppendDataToFile(data, data_len),
      content::DOWNLOAD_INTERRUPT_FROM_DISK);
}

void DownloadFileImpl::Rename(const FilePath& full_path,
                              bool overwrite_existing_file,
                              const RenameCompletionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  FilePath new_path(full_path);
  if (!overwrite_existing_file) {
    // Make the file unique if requested.
    int uniquifier =
        file_util::GetUniquePathNumber(new_path, FILE_PATH_LITERAL(""));
    if (uniquifier > 0) {
      new_path = new_path.InsertBeforeExtensionASCII(
          StringPrintf(" (%d)", uniquifier));
    }
  }

  net::Error rename_error = file_.Rename(new_path);
  content::DownloadInterruptReason reason(
      content::DOWNLOAD_INTERRUPT_REASON_NONE);
  if (net::OK != rename_error) {
    // Make sure our information is updated, since we're about to
    // error out.
    SendUpdate();

    // Null out callback so that we don't do any more stream processing.
    stream_reader_->RegisterCallback(base::Closure());

    reason =
        content::ConvertNetErrorToInterruptReason(
            rename_error,
            content::DOWNLOAD_INTERRUPT_FROM_DISK);
    new_path.clear();
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(callback, reason, new_path));
}

void DownloadFileImpl::Detach(base::Closure callback) {
  // Doing the annotation here leaves a small window during
  // which the file has the final name but hasn't been marked with the
  // Mark Of The Web.  However, it allows anti-virus scanners on Windows
  // to actually see the data (http://crbug.com/127999), and the Window
  // is pretty small (round trip to the UI thread).
  AnnotateWithSourceInformation();

  file_.Detach();

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, callback);
}

void DownloadFileImpl::Cancel() {
  file_.Cancel();
}

void DownloadFileImpl::AnnotateWithSourceInformation() {
  bound_net_log_.BeginEvent(net::NetLog::TYPE_DOWNLOAD_FILE_ANNOTATED);
  file_.AnnotateWithSourceInformation();
  bound_net_log_.EndEvent(net::NetLog::TYPE_DOWNLOAD_FILE_ANNOTATED);
}

FilePath DownloadFileImpl::FullPath() const {
  return file_.full_path();
}

bool DownloadFileImpl::InProgress() const {
  return file_.in_progress();
}

int64 DownloadFileImpl::BytesSoFar() const {
  return file_.bytes_so_far();
}

int64 DownloadFileImpl::CurrentSpeed() const {
  return file_.CurrentSpeed();
}

bool DownloadFileImpl::GetHash(std::string* hash) {
  return file_.GetHash(hash);
}

std::string DownloadFileImpl::GetHashState() {
  return file_.GetHashState();
}

void DownloadFileImpl::StreamActive() {
  base::TimeTicks start(base::TimeTicks::Now());
  base::TimeTicks now;
  scoped_refptr<net::IOBuffer> incoming_data;
  size_t incoming_data_size = 0;
  size_t total_incoming_data_size = 0;
  size_t num_buffers = 0;
  content::ByteStreamReader::StreamState state(
      content::ByteStreamReader::STREAM_EMPTY);
  content::DownloadInterruptReason reason =
      content::DOWNLOAD_INTERRUPT_REASON_NONE;
  base::TimeDelta delta(
      base::TimeDelta::FromMilliseconds(kMaxTimeBlockingFileThreadMs));

  // Take care of any file local activity required.
  do {
    state = stream_reader_->Read(&incoming_data, &incoming_data_size);

    switch (state) {
      case content::ByteStreamReader::STREAM_EMPTY:
        break;
      case content::ByteStreamReader::STREAM_HAS_DATA:
        {
          ++num_buffers;
          base::TimeTicks write_start(base::TimeTicks::Now());
          reason = AppendDataToFile(
              incoming_data.get()->data(), incoming_data_size);
          disk_writes_time_ += (base::TimeTicks::Now() - write_start);
          bytes_seen_ += incoming_data_size;
          total_incoming_data_size += incoming_data_size;
        }
        break;
      case content::ByteStreamReader::STREAM_COMPLETE:
        {
          reason = stream_reader_->GetStatus();
          SendUpdate();
          base::TimeTicks close_start(base::TimeTicks::Now());
          file_.Finish();
          base::TimeTicks now(base::TimeTicks::Now());
          disk_writes_time_ += (now - close_start);
          download_stats::RecordFileBandwidth(
              bytes_seen_, disk_writes_time_, now - download_start_);
          update_timer_.reset();
        }
        break;
      default:
        NOTREACHED();
        break;
    }
    now = base::TimeTicks::Now();
  } while (state == content::ByteStreamReader::STREAM_HAS_DATA &&
           reason == content::DOWNLOAD_INTERRUPT_REASON_NONE &&
           now - start <= delta);

  // If we're stopping to yield the thread, post a task so we come back.
  if (state == content::ByteStreamReader::STREAM_HAS_DATA &&
      now - start > delta) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&DownloadFileImpl::StreamActive,
                   weak_factory_.GetWeakPtr()));
  }

  if (total_incoming_data_size)
    download_stats::RecordFileThreadReceiveBuffers(num_buffers);

  download_stats::RecordContiguousWriteTime(now - start);

  // Take care of communication with our observer.
  if (reason != content::DOWNLOAD_INTERRUPT_REASON_NONE) {
    // Error case for both upstream source and file write.
    // Shut down processing and signal an error to our observer.
    // Our observer will clean us up.
    stream_reader_->RegisterCallback(base::Closure());
    weak_factory_.InvalidateWeakPtrs();
    SendUpdate();                       // Make info up to date before error.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&content::DownloadDestinationObserver::DestinationError,
                   observer_, reason));
  } else if (state == content::ByteStreamReader::STREAM_COMPLETE) {
    // Signal successful completion and shut down processing.
    stream_reader_->RegisterCallback(base::Closure());
    weak_factory_.InvalidateWeakPtrs();
    std::string hash;
    if (!GetHash(&hash) || file_.IsEmptyHash(hash))
      hash.clear();
    SendUpdate();
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            &content::DownloadDestinationObserver::DestinationCompleted,
            observer_, hash));
  }
  if (bound_net_log_.IsLoggingAllEvents()) {
    bound_net_log_.AddEvent(
        net::NetLog::TYPE_DOWNLOAD_STREAM_DRAINED,
        base::Bind(&download_net_logs::FileStreamDrainedCallback,
                   total_incoming_data_size, num_buffers));
  }
}

void DownloadFileImpl::SendUpdate() {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&content::DownloadDestinationObserver::DestinationUpdate,
                 observer_, BytesSoFar(), CurrentSpeed(), GetHashState()));
}

// static
int content::DownloadFile::GetNumberOfDownloadFiles() {
  return number_active_objects_;
}

