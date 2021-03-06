// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINTING_MESSAGE_FILTER_H_
#define CHROME_BROWSER_PRINTING_PRINTING_MESSAGE_FILTER_H_

#include <string>

#include "base/compiler_specific.h"
#include "content/public/browser/browser_message_filter.h"

#if defined(OS_WIN)
#include "base/shared_memory.h"
#endif

class FilePath;
struct PrintHostMsg_ScriptedPrint_Params;
class Profile;
class ProfileIOData;

namespace base {
class DictionaryValue;
}

namespace content {
class WebContents;
}

namespace printing {
class PrinterQuery;
class PrintJobManager;
}

// This class filters out incoming printing related IPC messages for the
// renderer process on the IPC thread.
class PrintingMessageFilter : public content::BrowserMessageFilter {
 public:
  PrintingMessageFilter(int render_process_id, Profile* profile);

  // content::BrowserMessageFilter methods.
  virtual void OverrideThreadForMessage(
      const IPC::Message& message,
      content::BrowserThread::ID* thread) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

 private:
  virtual ~PrintingMessageFilter();

#if defined(OS_WIN)
  // Used to pass resulting EMF from renderer to browser in printing.
  void OnDuplicateSection(base::SharedMemoryHandle renderer_handle,
                          base::SharedMemoryHandle* browser_handle);
#endif

#if defined(OS_CHROMEOS)
  // Used to ask the browser allocate a temporary file for the renderer
  // to fill in resulting PDF in renderer.
  void OnAllocateTempFileForPrinting(base::FileDescriptor* temp_file_fd,
                                     int* sequence_number);
  void OnTempFileForPrintingWritten(int render_view_id, int sequence_number);
  void CreatePrintDialogForFile(int render_view_id, const FilePath& path);
#endif

  // Given a render_view_id get the corresponding WebContents.
  // Must be called on the UI thread.
  content::WebContents* GetWebContentsForRenderView(int render_view_id);

  // GetPrintSettingsForRenderView must be called via PostTask and
  // base::Bind.  Collapse the settings-specific params into a
  // struct to avoid running into issues with too many params
  // to base::Bind.
  struct GetPrintSettingsForRenderViewParams;

  // Retrieve print settings.  Uses |render_view_id| to get a parent
  // for any UI created if needed.
  void GetPrintSettingsForRenderView(
      int render_view_id,
      GetPrintSettingsForRenderViewParams params,
      const base::Closure& callback,
      scoped_refptr<printing::PrinterQuery> printer_query);

  // Get the default print setting.
  void OnGetDefaultPrintSettings(IPC::Message* reply_msg);
  void OnGetDefaultPrintSettingsReply(
      scoped_refptr<printing::PrinterQuery> printer_query,
      IPC::Message* reply_msg);

  // The renderer host have to show to the user the print dialog and returns
  // the selected print settings. The task is handled by the print worker
  // thread and the UI thread. The reply occurs on the IO thread.
  void OnScriptedPrint(const PrintHostMsg_ScriptedPrint_Params& params,
                       IPC::Message* reply_msg);
  void OnScriptedPrintReply(
      scoped_refptr<printing::PrinterQuery> printer_query,
      IPC::Message* reply_msg);

  // Modify the current print settings based on |job_settings|. The task is
  // handled by the print worker thread and the UI thread. The reply occurs on
  // the IO thread.
  void OnUpdatePrintSettings(int document_cookie,
                             const base::DictionaryValue& job_settings,
                             IPC::Message* reply_msg);
  void OnUpdatePrintSettingsReply(
      scoped_refptr<printing::PrinterQuery> printer_query,
      IPC::Message* reply_msg);

  // Check to see if print preview has been cancelled.
  void OnCheckForCancel(int32 preview_ui_id,
                        int preview_request_id,
                        bool* cancel);

  printing::PrintJobManager* print_job_manager_;

  ProfileIOData* profile_io_data_;

  int render_process_id_;

  DISALLOW_COPY_AND_ASSIGN(PrintingMessageFilter);
};

#endif  // CHROME_BROWSER_PRINTING_PRINTING_MESSAGE_FILTER_H_
