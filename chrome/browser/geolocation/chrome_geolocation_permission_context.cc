// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/chrome_geolocation_permission_context.h"

#include <functional>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/geolocation/geolocation_infobar_queue_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/view_type_utils.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"

using WebKit::WebSecurityOrigin;
using content::BrowserThread;
using content::WebContents;

// GeolocationPermissionContext -----------------------------------------------

ChromeGeolocationPermissionContext::ChromeGeolocationPermissionContext(
    Profile* profile)
    : profile_(profile) {
}

ChromeGeolocationPermissionContext::~ChromeGeolocationPermissionContext() {
}

void ChromeGeolocationPermissionContext::RequestGeolocationPermission(
    int render_process_id, int render_view_id, int bridge_id,
    const GURL& requesting_frame, base::Callback<void(bool)> callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            &ChromeGeolocationPermissionContext::RequestGeolocationPermission,
            this, render_process_id, render_view_id, bridge_id,
            requesting_frame, callback));
    return;
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  WebContents* web_contents =
      tab_util::GetWebContentsByID(render_process_id, render_view_id);
  if (chrome::GetViewType(web_contents) != chrome::VIEW_TYPE_TAB_CONTENTS) {
    // The tab may have gone away, or the request may not be from a tab at all.
    // TODO(mpcomplete): the request could be from a background page or
    // extension popup (tab_contents will have a different ViewType). But why do
    // we care? Shouldn't we still put an infobar up in the current tab?
    LOG(WARNING) << "Attempt to use geolocation tabless renderer: "
                 << render_process_id << "," << render_view_id << ","
                 << bridge_id << " (can't prompt user without a visible tab)";
    NotifyPermissionSet(render_process_id, render_view_id, bridge_id,
                        requesting_frame, callback, false);
    return;
  }

  GURL embedder = web_contents->GetURL();
  if (!requesting_frame.is_valid() || !embedder.is_valid()) {
    LOG(WARNING) << "Attempt to use geolocation from an invalid URL: "
                 << requesting_frame << "," << embedder
                 << " (geolocation is not supported in popups)";
    NotifyPermissionSet(render_process_id, render_view_id, bridge_id,
                        requesting_frame, callback, false);
    return;
  }

  DecidePermission(render_process_id, render_view_id, bridge_id,
                   requesting_frame, embedder, callback);
}

void ChromeGeolocationPermissionContext::CancelGeolocationPermissionRequest(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame) {
  CancelPendingInfoBarRequest(render_process_id, render_view_id, bridge_id);
}

void ChromeGeolocationPermissionContext::DecidePermission(
    int render_process_id, int render_view_id, int bridge_id,
    const GURL& requesting_frame, const GURL& embedder,
    base::Callback<void(bool)> callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ExtensionService* extension_service = profile_->GetExtensionService();
  if (extension_service) {
    const extensions::Extension* extension =
        extension_service->extensions()->GetExtensionOrAppByURL(
            ExtensionURLInfo(
                WebSecurityOrigin::createFromString(
                    UTF8ToUTF16(requesting_frame.spec())),
                requesting_frame));
    if (extension &&
        extension->HasAPIPermission(extensions::APIPermission::kGeolocation)) {
      // Make sure the extension is in the calling process.
      if (extension_service->process_map()->Contains(
              extension->id(), render_process_id)) {
        PermissionDecided(render_process_id, render_view_id, bridge_id,
                          requesting_frame, embedder, callback, true);
        return;
      }
    }
  }

  ContentSetting content_setting =
     profile_->GetHostContentSettingsMap()->GetContentSetting(
          requesting_frame,
          embedder,
          CONTENT_SETTINGS_TYPE_GEOLOCATION,
          std::string());
  switch (content_setting) {
    case CONTENT_SETTING_BLOCK:
      PermissionDecided(render_process_id, render_view_id, bridge_id,
                        requesting_frame, embedder, callback, false);
      break;
    case CONTENT_SETTING_ALLOW:
      PermissionDecided(render_process_id, render_view_id, bridge_id,
                        requesting_frame, embedder, callback, true);
      break;
    default:
      // setting == ask. Prompt the user.
      QueueController()->CreateInfoBarRequest(
          render_process_id, render_view_id, bridge_id, requesting_frame,
          embedder, base::Bind(
              &ChromeGeolocationPermissionContext::NotifyPermissionSet,
              base::Unretained(this),
              render_process_id, render_view_id, bridge_id, requesting_frame,
              callback));
  }
}

void ChromeGeolocationPermissionContext::PermissionDecided(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame,
    const GURL& embedder,
    base::Callback<void(bool)> callback,
    bool allowed) {
  NotifyPermissionSet(render_process_id, render_view_id, bridge_id,
                      requesting_frame, callback, allowed);
}

void ChromeGeolocationPermissionContext::NotifyPermissionSet(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame,
    base::Callback<void(bool)> callback,
    bool allowed) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // WebContents may have gone away (or not exists for extension).
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::Get(render_process_id, render_view_id);
  if (content_settings) {
    content_settings->OnGeolocationPermissionSet(requesting_frame.GetOrigin(),
                                                 allowed);
  }

  callback.Run(allowed);
}

GeolocationInfoBarQueueController*
ChromeGeolocationPermissionContext::QueueController() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!geolocation_infobar_queue_controller_)
    geolocation_infobar_queue_controller_.reset(CreateQueueController());
  return geolocation_infobar_queue_controller_.get();
}

GeolocationInfoBarQueueController*
ChromeGeolocationPermissionContext::CreateQueueController() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return new GeolocationInfoBarQueueController(profile());
}

void ChromeGeolocationPermissionContext::CancelPendingInfoBarRequest(
    int render_process_id,
    int render_view_id,
    int bridge_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(
            &ChromeGeolocationPermissionContext::CancelPendingInfoBarRequest,
            this, render_process_id, render_view_id, bridge_id));
     return;
  }
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  QueueController()->CancelInfoBarRequest(
      render_process_id,
      render_view_id,
      bridge_id);
}
