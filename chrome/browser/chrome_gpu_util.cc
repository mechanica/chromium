// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_gpu_util.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/version.h"
#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"

using content::GpuDataManager;

namespace gpu_util {

// The BrowserMonitor class is used to track the number of currently open
// browser windows, so that the gpu can be notified when they are created or
// destroyed. We only count tabbed windows for this purpose.

// There's no BrowserList on Android/
#if !defined(OS_ANDROID)
class BrowserMonitor : public chrome::BrowserListObserver {
 public:
  static BrowserMonitor* GetInstance() {
    static BrowserMonitor* instance = NULL;
    if (!instance)
      instance = new BrowserMonitor;
    return instance;
  }

  void Install() {
    if (!installed_) {
      BrowserList::AddObserver(this);
      installed_ = true;
    }
  }

  void Uninstall() {
    if (installed_) {
      BrowserList::RemoveObserver(this);
      installed_ = false;
    }
  }

 private:
  BrowserMonitor() : num_browsers_(0), installed_(false) {
  }

  ~BrowserMonitor() {
  }

  // BrowserListObserver implementation.
  virtual void OnBrowserAdded(Browser* browser) OVERRIDE {
    if (browser->type() == Browser::TYPE_TABBED)
      content::GpuDataManager::GetInstance()->SetWindowCount(++num_browsers_);
  }

  virtual void OnBrowserRemoved(Browser* browser) OVERRIDE {
    if (browser->type() == Browser::TYPE_TABBED)
      content::GpuDataManager::GetInstance()->SetWindowCount(--num_browsers_);
  }

  uint32 num_browsers_;
  bool installed_;
};

void InstallBrowserMonitor() {
  BrowserMonitor::GetInstance()->Install();
}

void UninstallBrowserMonitor() {
  BrowserMonitor::GetInstance()->Uninstall();
}

#endif // !defined(OS_ANDROID)

void DisableCompositingFieldTrial() {
  base::FieldTrial* trial =
      base::FieldTrialList::Find(content::kGpuCompositingFieldTrialName);
  if (trial)
    trial->Disable();
}

bool ShouldRunCompositingFieldTrial() {
// Enable the field trial only on desktop OS's.
#if !(defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX))
  return false;
#endif

// Necessary for linux_chromeos build since it defines both OS_LINUX
// and OS_CHROMEOS.
#if defined(OS_CHROMEOS)
  return false;
#endif

#if defined(OS_WIN)
  // Don't run the trial on Windows XP.
  if (base::win::GetVersion() < base::win::VERSION_VISTA)
    return false;
#endif

  const GpuDataManager* gpu_data_manager = GpuDataManager::GetInstance();
  content::GpuFeatureType blacklisted_features =
      gpu_data_manager->GetBlacklistedFeatures();

  // Don't run the field trial if gpu access has been blocked or
  // accelerated compositing is blacklisted.
  if (!gpu_data_manager->GpuAccessAllowed() ||
      blacklisted_features & content::GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING)
    return false;

  // The performance of accelerated compositing is too low with software
  // rendering.
  if (gpu_data_manager->ShouldUseSoftwareRendering())
    return false;

  // Don't activate the field trial if force-compositing-mode has been
  // explicitly disabled from the command line.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableForceCompositingMode) ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableAcceleratedCompositing))
    return false;

  return true;
}

// Note: The compositing field trial may be created at startup time via the
// Finch framework. In that case, all the Groups and probability values are
// set before this function is called and any Field Trial setup calls
// made here are simply ignored.
// Early outs from this function intended to bypass activation of the field
// trial must call DisableCompositingFieldTrial() before returning.
void InitializeCompositingFieldTrial() {
  // Early out in configurations that should not run the compositing
  // field trial.
  if (!ShouldRunCompositingFieldTrial()) {
    DisableCompositingFieldTrial();
    return;
  }

  const base::FieldTrial::Probability kDivisor = 3;
  scoped_refptr<base::FieldTrial> trial(
    base::FieldTrialList::FactoryGetFieldTrial(
        content::kGpuCompositingFieldTrialName, kDivisor,
        "disable", 2012, 12, 31, NULL));

  // Produce the same result on every run of this client.
  trial->UseOneTimeRandomization();

  base::FieldTrial::Probability force_compositing_mode_probability = 0;
  base::FieldTrial::Probability threaded_compositing_probability = 0;

  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel == chrome::VersionInfo::CHANNEL_STABLE ||
      channel == chrome::VersionInfo::CHANNEL_BETA) {
    // Stable and Beta channels: Non-threaded force-compositing-mode on by
    // default (mac and windows only).
#if defined(OS_WIN) || defined(OS_MACOSX)
    force_compositing_mode_probability = 3;
#endif
  } else if (channel == chrome::VersionInfo::CHANNEL_DEV ||
             channel == chrome::VersionInfo::CHANNEL_CANARY) {
    // Dev and Canary channels: force-compositing-mode and
    // threaded-compositing on with 1/3 probability each.
    force_compositing_mode_probability = 1;

#if defined(OS_MACOSX) || defined(OS_LINUX)
    // Threaded compositing mode isn't feature complete on mac or linux yet:
    // http://crbug.com/133602 for mac
    // http://crbug.com/140866 for linux
    threaded_compositing_probability = 0;
#else
    if (!CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kDisableThreadedCompositing))
        threaded_compositing_probability = 1;
#endif
  }

  int force_compositing_group = trial->AppendGroup(
      content::kGpuCompositingFieldTrialForceCompositingEnabledName,
      force_compositing_mode_probability);
  int thread_group = trial->AppendGroup(
      content::kGpuCompositingFieldTrialThreadEnabledName,
      threaded_compositing_probability);

  bool force_compositing = (trial->group() == force_compositing_group);
  bool thread = (trial->group() == thread_group);
  UMA_HISTOGRAM_BOOLEAN("GPU.InForceCompositingModeFieldTrial",
                        force_compositing);
  UMA_HISTOGRAM_BOOLEAN("GPU.InCompositorThreadFieldTrial", thread);
}

}  // namespace gpu_util;

