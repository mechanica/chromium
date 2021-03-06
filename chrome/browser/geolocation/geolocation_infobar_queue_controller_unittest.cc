// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/waitable_event.h"
#include "chrome/browser/geolocation/geolocation_infobar_queue_controller.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

class GeolocationInfoBarQueueControllerTests
    : public ChromeRenderViewHostTestHarness {
 public:
  GeolocationInfoBarQueueControllerTests();

 protected:
  int ProcessId();
  int RenderId();

 private:
  // ChromeRenderViewHostTestHarness:
  virtual void SetUp() OVERRIDE;

  content::TestBrowserThread ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationInfoBarQueueControllerTests);
};

GeolocationInfoBarQueueControllerTests::GeolocationInfoBarQueueControllerTests()
    : ui_thread_(content::BrowserThread::UI, MessageLoop::current()) {
}

int GeolocationInfoBarQueueControllerTests::ProcessId() {
  return web_contents()->GetRenderProcessHost()->GetID();
}

int GeolocationInfoBarQueueControllerTests::RenderId() {
  return web_contents()->GetRenderViewHost()->GetRoutingID();
}

void GeolocationInfoBarQueueControllerTests::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  InfoBarTabHelper::CreateForWebContents(web_contents());
}

class ObservationCountingQueueController :
    public GeolocationInfoBarQueueController {
 public:
  explicit ObservationCountingQueueController(Profile* profile);

  int call_count() const { return call_count_; }

 private:
  int call_count_;

  // GeolocationInfoBarQueueController
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

  static void NotifyPermissionSet(int render_process_id,
                                  int render_view_id,
                                  int bridge_id,
                                  const GURL& requesting_frame,
                                  base::Callback<void(bool)> callback,
                                  bool allowed);
};

ObservationCountingQueueController::ObservationCountingQueueController(
    Profile* profile)
    : GeolocationInfoBarQueueController(
          profile),
      call_count_(0) {
}

void ObservationCountingQueueController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  ++call_count_;
  GeolocationInfoBarQueueController::Observe(type, source, details);
}

void ObservationCountingQueueController::NotifyPermissionSet(
    int render_process_id,
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame,
    base::Callback<void(bool)> callback,
    bool allowed) {
}


TEST_F(GeolocationInfoBarQueueControllerTests,
       OneObservationPerInfoBarCancelled) {
  // When an infobar is cancelled, the infobar helper sends a notification to
  // the controller. If the controller has another infobar queued, it should
  // maintain its registration for notifications with the helper, but on the
  // last infobar cancellation it should unregister for notifications.
  //
  // What we don't want is for the controller to unregister and then re-register
  // for notifications, which can lead to getting notified multiple times.  This
  // test checks that in the case where the controller should remain registered
  // for notifications, it gets notified exactly once."

  ObservationCountingQueueController infobar_queue_controller(profile());
  GURL url("http://www.example.com/geolocation");
  base::Callback<void(bool)> callback;
  infobar_queue_controller.CreateInfoBarRequest(ProcessId(), RenderId(), 1,
                                                url, url, callback);
  infobar_queue_controller.CreateInfoBarRequest(ProcessId(), RenderId(), 2,
                                                url, url, callback);
  infobar_queue_controller.CancelInfoBarRequest(ProcessId(), RenderId(), 1);
  EXPECT_EQ(1, infobar_queue_controller.call_count());
};
