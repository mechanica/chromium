// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_toolbar_model_delegate.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"

BrowserToolbarModelDelegate::BrowserToolbarModelDelegate(Browser* browser)
    : browser_(browser) {
}

BrowserToolbarModelDelegate::~BrowserToolbarModelDelegate() {
}

content::WebContents*
BrowserToolbarModelDelegate::GetActiveWebContents() const {
  return chrome::GetActiveWebContents(browser_);
}
