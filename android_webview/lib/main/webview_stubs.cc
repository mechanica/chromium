// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"

// This file contains temporary stubs to allow the libwebview target to compile.
// They will be removed once real implementations are written/upstreamed, or
// once other code is refactored to eliminate the need for them.

// static
TabAndroid* TabAndroid::FromWebContents(content::WebContents* web_contents) {
  // We don't need to return a real TabAndroid object yet.
  // Eventually, WebView will need its own TabAndroid implementation.
  return NULL;
}

// static
InfoBar* ConfirmInfoBarDelegate::CreateInfoBar(InfoBarService* owner) {
  return NULL;
}
