// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "webkit/media/crypto/key_systems.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "net/base/mime_util.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"

#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

namespace webkit_media {

// Convert a WebString to ASCII, falling back on an empty string in the case
// of a non-ASCII string.
static std::string ToASCIIOrEmpty(const WebKit::WebString& string) {
  return IsStringASCII(string) ? UTF16ToASCII(string) : std::string();
}

static const char kClearKeyKeySystem[] = "webkit-org.w3.clearkey";
static const char kExternalClearKeyKeySystem[] =
    "org.chromium.externalclearkey";

struct MediaFormatAndKeySystem {
  const char* mime_type;
  const char* codecs_list;
  const char* key_system;
};

struct KeySystemPluginTypePair {
  const char* key_system;
  const char* plugin_type;
};

// Specifies the container and codec combinations supported by individual
// key systems. Each line is a container-codecs combination and the key system
// that supports it. Multiple codecs can be listed. A trailing commas in
// the |codecs_list| allows the container to be specified without a codec.
// This list is converted at runtime into individual container-codec-key system
// entries in KeySystems::key_system_map_.
static const MediaFormatAndKeySystem
supported_format_key_system_combinations[] = {
  // Clear Key.
  { "video/webm", "vorbis,vp8,vp8.0,", kClearKeyKeySystem },
  { "audio/webm", "vorbis,", kClearKeyKeySystem },

  // External Clear Key (used for testing).
  { "video/webm", "vorbis,vp8,vp8.0,", kExternalClearKeyKeySystem },
  { "audio/webm", "vorbis,", kExternalClearKeyKeySystem },

#if defined(WIDEVINE_CDM_AVAILABLE)
  // Widevine.
  { "video/webm", "vorbis,vp8,vp8.0,", kWidevineKeySystem },
  { "audio/webm", "vorbis,", kWidevineKeySystem },
#endif  // WIDEVINE_CDM_AVAILABLE
};

static const KeySystemPluginTypePair key_system_to_plugin_type_mapping[] = {
  // TODO(xhwang): Update this with the real plugin name.
  { kExternalClearKeyKeySystem, "application/x-ppapi-clearkey-cdm" },
#if defined(WIDEVINE_CDM_AVAILABLE)
  { kWidevineKeySystem, kWidevineCdmPluginMimeType }
#endif  // WIDEVINE_CDM_AVAILABLE
};

class KeySystems {
 public:
  bool IsSupportedKeySystem(const std::string& key_system);

  bool IsSupportedKeySystemWithMediaMimeType(
      const std::string& mime_type,
      const std::vector<std::string>& codecs,
      const std::string& key_system);

 private:
  friend struct base::DefaultLazyInstanceTraits<KeySystems>;

  typedef base::hash_set<std::string> CodecMappings;
  typedef std::map<std::string, CodecMappings> MimeTypeMappings;
  typedef std::map<std::string, MimeTypeMappings> KeySystemMappings;

  KeySystems();

  bool IsSupportedKeySystemWithContainerAndCodec(
      const std::string& mime_type,
      const std::string& codec,
      const std::string& key_system);

  KeySystemMappings key_system_map_;
};

static base::LazyInstance<KeySystems> g_key_systems = LAZY_INSTANCE_INITIALIZER;

KeySystems::KeySystems() {
  // Initialize the supported media type/key system combinations.
  for (size_t i = 0;
       i < arraysize(supported_format_key_system_combinations);
       ++i) {
    const MediaFormatAndKeySystem& combination =
        supported_format_key_system_combinations[i];
    std::vector<std::string> mime_type_codecs;
    net::ParseCodecString(combination.codecs_list,
                          &mime_type_codecs,
                          false);

    CodecMappings codecs;
    for (size_t j = 0; j < mime_type_codecs.size(); ++j)
      codecs.insert(mime_type_codecs[j]);

    // Key systems can be repeated, so there may already be an entry.
    KeySystemMappings::iterator key_system_iter =
        key_system_map_.find(combination.key_system);
    if (key_system_iter == key_system_map_.end()) {
      MimeTypeMappings mime_types_map;
      mime_types_map[combination.mime_type] = codecs;
      key_system_map_[combination.key_system] = mime_types_map;
    } else {
      MimeTypeMappings& mime_types_map = key_system_iter->second;
      // mime_types_map may not be repeated for a given key system.
      DCHECK(mime_types_map.find(combination.mime_type) ==
             mime_types_map.end());
      mime_types_map[combination.mime_type] = codecs;
    }
  }
}

bool KeySystems::IsSupportedKeySystem(const std::string& key_system) {
  bool is_supported = key_system_map_.find(key_system) != key_system_map_.end();

  DCHECK_EQ(is_supported,
            (CanUseAesDecryptor(key_system) ||
             !GetPluginType(key_system).empty()))
      << "key_system_map_ & key_system_to_plugin_type_mapping are inconsistent";
  return is_supported;
}

bool KeySystems::IsSupportedKeySystemWithContainerAndCodec(
    const std::string& mime_type,
    const std::string& codec,
    const std::string& key_system) {

  KeySystemMappings::const_iterator key_system_iter =
      key_system_map_.find(key_system);
  if (key_system_iter == key_system_map_.end())
    return false;

  const MimeTypeMappings& mime_types_map = key_system_iter->second;
  MimeTypeMappings::const_iterator mime_iter = mime_types_map.find(mime_type);
  if (mime_iter == mime_types_map.end())
    return false;

  const CodecMappings& codecs = mime_iter->second;
  return (codecs.find(codec) != codecs.end());
}

bool KeySystems::IsSupportedKeySystemWithMediaMimeType(
    const std::string& mime_type,
    const std::vector<std::string>& codecs,
    const std::string& key_system) {
  if (codecs.empty())
    return IsSupportedKeySystemWithContainerAndCodec(mime_type, "", key_system);

  for (size_t i = 0; i < codecs.size(); ++i) {
    if (!IsSupportedKeySystemWithContainerAndCodec(
            mime_type, codecs[i], key_system))
      return false;
  }

  return true;
}

bool IsSupportedKeySystem(const WebKit::WebString& key_system) {
  return g_key_systems.Get().IsSupportedKeySystem(ToASCIIOrEmpty(key_system));
}

bool IsSupportedKeySystemWithMediaMimeType(
    const std::string& mime_type,
    const std::vector<std::string>& codecs,
    const std::string& key_system) {
  return g_key_systems.Get().IsSupportedKeySystemWithMediaMimeType(
      mime_type, codecs, key_system);
}

bool CanUseAesDecryptor(const std::string& key_system) {
  return key_system == kClearKeyKeySystem;
}

std::string GetPluginType(const std::string& key_system) {
  for (size_t i = 0; i < arraysize(key_system_to_plugin_type_mapping); ++i) {
    if (key_system_to_plugin_type_mapping[i].key_system == key_system)
      return key_system_to_plugin_type_mapping[i].plugin_type;
  }

  return std::string();
}

}  // namespace webkit_media
