// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_CONFIRM_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_CONFIRM_INFOBAR_DELEGATE_H_

#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "googleurl/src/gurl.h"

#include <string>

class GeolocationInfoBarQueueController;
class InfoBarTabHelper;

// GeolocationInfoBarDelegates are created by the
// GeolocationInfoBarQueueController to control the display
// and handling of geolocation permission infobars to the user.
class GeolocationConfirmInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  GeolocationConfirmInfoBarDelegate(
      InfoBarTabHelper* infobar_helper,
      GeolocationInfoBarQueueController* controller,
      int render_process_id,
      int render_view_id,
      int bridge_id,
      const GURL& requesting_frame_url,
      const std::string& display_languages);

  int render_process_id() const { return render_process_id_; }
  int render_view_id() const { return render_view_id_; }

 protected:
  // ConfirmInfoBarDelegate:
  virtual gfx::Image* GetIcon() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual string16 GetLinkText() const OVERRIDE;
  virtual bool LinkClicked(WindowOpenDisposition disposition) OVERRIDE;

  // Call back to the controller, to inform of the user's decision.
  void SetPermission(bool update_content_setting, bool allowed);

 private:
  GeolocationInfoBarQueueController* controller_;
  int render_process_id_;
  int render_view_id_;
  int bridge_id_;

  GURL requesting_frame_url_;
  std::string display_languages_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(GeolocationConfirmInfoBarDelegate);
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_CONFIRM_INFOBAR_DELEGATE_H_
