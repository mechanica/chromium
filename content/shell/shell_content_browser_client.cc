// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_content_browser_client.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/shell/geolocation/shell_access_token_store.h"
#include "content/shell/shell.h"
#include "content/shell/shell_browser_context.h"
#include "content/shell/shell_browser_main_parts.h"
#include "content/shell/shell_devtools_delegate.h"
#include "content/shell/shell_resource_dispatcher_host_delegate.h"
#include "content/shell/shell_switches.h"
#include "content/shell/shell_web_contents_view_delegate_creator.h"
#include "content/shell/webkit_test_runner_host.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/webpreferences.h"

#if defined(OS_ANDROID)
#include "base/android/path_utils.h"
#include "base/path_service.h"
#include "base/platform_file.h"
#include "content/shell/android/shell_descriptors.h"
#endif

namespace content {

ShellContentBrowserClient::ShellContentBrowserClient()
    : shell_browser_main_parts_(NULL) {
}

ShellContentBrowserClient::~ShellContentBrowserClient() {
}

BrowserMainParts* ShellContentBrowserClient::CreateBrowserMainParts(
    const MainFunctionParams& parameters) {
  shell_browser_main_parts_ = new ShellBrowserMainParts(parameters);
  return shell_browser_main_parts_;
}

void ShellContentBrowserClient::RenderViewHostCreated(
    RenderViewHost* render_view_host) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree))
    return;
  new WebKitTestRunnerHost(render_view_host);
}

void ShellContentBrowserClient::AppendExtraCommandLineSwitches(
    CommandLine* command_line, int child_process_id) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree))
    command_line->AppendSwitch(switches::kDumpRenderTree);
}

void ShellContentBrowserClient::OverrideWebkitPrefs(
    RenderViewHost* render_view_host,
    const GURL& url,
    webkit_glue::WebPreferences* prefs) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree))
    return;
  prefs->dom_paste_enabled = true;
  prefs->javascript_can_access_clipboard = true;
  prefs->allow_universal_access_from_file_urls = true;
}

void ShellContentBrowserClient::ResourceDispatcherHostCreated() {
  resource_dispatcher_host_delegate_.reset(
      new ShellResourceDispatcherHostDelegate());
  ResourceDispatcherHost::Get()->SetDelegate(
      resource_dispatcher_host_delegate_.get());
}

std::string ShellContentBrowserClient::GetDefaultDownloadName() {
  return "download";
}

WebContentsViewDelegate* ShellContentBrowserClient::GetWebContentsViewDelegate(
    WebContents* web_contents) {
#if defined(TOOLKIT_GTK) || defined(OS_WIN) || defined(OS_MACOSX)
  return CreateShellWebContentsViewDelegate(web_contents);
#endif
  NOTIMPLEMENTED();
  return NULL;
}

#if defined(OS_ANDROID)
void ShellContentBrowserClient::GetAdditionalMappedFilesForChildProcess(
    const CommandLine& command_line,
    std::vector<content::FileDescriptorInfo>* mappings) {
  int flags = base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ;
  FilePath pak_file;
  bool r = PathService::Get(base::DIR_ANDROID_APP_DATA, &pak_file);
  CHECK(r);
  pak_file = pak_file.Append(FILE_PATH_LITERAL("paks"));
  pak_file = pak_file.Append(FILE_PATH_LITERAL("content_shell.pak"));

  base::PlatformFile f =
      base::CreatePlatformFile(pak_file, flags, NULL, NULL);
  if (f == base::kInvalidPlatformFileValue) {
    NOTREACHED() << "Failed to open file when creating renderer process: "
                 << "content_shell.pak";
  }
  mappings->push_back(
      content::FileDescriptorInfo(kShellPakDescriptor,
                                  base::FileDescriptor(f, true)));
}
#endif

ShellBrowserContext* ShellContentBrowserClient::browser_context() {
  return shell_browser_main_parts_->browser_context();
}

ShellBrowserContext*
    ShellContentBrowserClient::off_the_record_browser_context() {
  return shell_browser_main_parts_->off_the_record_browser_context();
}

AccessTokenStore* ShellContentBrowserClient::CreateAccessTokenStore() {
  return new ShellAccessTokenStore(browser_context()->GetRequestContext());
}

}  // namespace content
