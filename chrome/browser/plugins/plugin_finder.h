// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGIN_FINDER_H_
#define CHROME_BROWSER_PLUGINS_PLUGIN_FINDER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/synchronization/lock.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/string16.h"
#include "webkit/plugins/webplugininfo.h"

namespace base {
class DictionaryValue;
}

class GURL;
class PluginMetadata;

#if defined(ENABLE_PLUGIN_INSTALLATION)
class PluginInstaller;
#endif

// This class should be created and initialized by calling
// |GetInstance()| and |Init()| on the UI thread.
// After that it can be safely used on any other thread.
class PluginFinder {
 public:
  static PluginFinder* GetInstance();

  // It should be called on the UI thread.
  void Init();

#if defined(ENABLE_PLUGIN_INSTALLATION)
  void ReinitializePlugins(const base::DictionaryValue& json_metadata);

  // Finds a plug-in for the given MIME type and language (specified as an IETF
  // language tag, i.e. en-US). If found, sets |installer| to the
  // corresponding PluginInstaller and |plugin_metadata| to a copy of the
  // corresponding PluginMetadata.
  bool FindPlugin(const std::string& mime_type,
                  const std::string& language,
                  PluginInstaller** installer,
                  scoped_ptr<PluginMetadata>* plugin_metadata);

  // Finds the plug-in with the given identifier. If found, sets |installer|
  // to the corresponding PluginInstaller and |plugin_metadata| to a copy
  // of the corresponding PluginMetadata.
  bool FindPluginWithIdentifier(const std::string& identifier,
                                PluginInstaller** installer,
                                scoped_ptr<PluginMetadata>* plugin_metadata);
#endif

  // Returns the plug-in name with the given identifier.
  string16 FindPluginNameWithIdentifier(const std::string& identifier);

  // Gets plug-in metadata using |plugin|.
  scoped_ptr<PluginMetadata> GetPluginMetadata(
      const webkit::WebPluginInfo& plugin);

 private:
  friend struct DefaultSingletonTraits<PluginFinder>;
  friend class Singleton<PluginFinder>;
  FRIEND_TEST_ALL_PREFIXES(PluginFinderTest, JsonSyntax);
  FRIEND_TEST_ALL_PREFIXES(PluginFinderTest, PluginGroups);

  PluginFinder();
  ~PluginFinder();

  // Loads the plug-in information from the browser resources and parses it.
  // Returns NULL if the plug-in list couldn't be parsed.
  static base::DictionaryValue* LoadPluginList();

  void InitInternal();

  scoped_ptr<base::DictionaryValue> plugin_list_;
#if defined(ENABLE_PLUGIN_INSTALLATION)
  std::map<std::string, PluginInstaller*> installers_;
#endif

  std::map<std::string, PluginMetadata*> identifier_plugin_;

  // Synchronization for the above member variables is
  // required since multiple threads can be accessing them concurrently.
  base::Lock mutex_;

  DISALLOW_COPY_AND_ASSIGN(PluginFinder);
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGIN_FINDER_H_
