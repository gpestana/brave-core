/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/wip/ad_targeting/resources/purchase_intent/purchase_intent_resource.h"

#include "base/json/json_reader.h"
#include "brave/components/l10n/common/locale_util.h"
#include "bat/ads/internal/ads_client_helper.h"
#include "bat/ads/internal/logging.h"
#include "bat/ads/result.h"

namespace ads {
namespace ad_targeting {
namespace resource {

namespace {
const uint16_t kCurrentModelVersion = 1;
}  // namespace

PurchaseIntent::PurchaseIntent() = default;

PurchaseIntent::~PurchaseIntent() = default;

bool PurchaseIntent::IsInitialized() {
  return is_initialized_;
}

void PurchaseIntent::LoadForLocale(
    const std::string& locale) {
  const std::string country_code = brave_l10n::GetCountryCode(locale);

  const auto iter = kPurchaseIntentCountryCodes.find(country_code);
  if (iter == kPurchaseIntentCountryCodes.end()) {
    BLOG(1, country_code << " does not support purchase intent");
    is_initialized_ = false;
    return;
  }

  LoadForId(iter->second);
}

void PurchaseIntent::LoadForId(
    const std::string& id) {
  AdsClientHelper::Get()->LoadUserModelForId(id, [](
      const Result result,
      const std::string& json) {
    if (result != SUCCESS) {
      BLOG(1, "Failed to load " << id << " purchase intent resource");
      is_initialized_ = false;
      return;
    }

    BLOG(1, "Successfully loaded " << id << " purchase intent resource");

    if (!FromJson(json)) {
      BLOG(1, "Failed to initialize " << id << " purchase intent resource");
      is_initialized_ = false;
      return;
    }

    is_initialized_ = true;

    BLOG(1, "Successfully initialized " << id << " purchase intent resource");
  }
}

PurchaseIntentInfo PurchaseIntent::Get() const {
  return purchase_intent_;
}

///////////////////////////////////////////////////////////////////////////////

bool PurchaseIntent::FromJson(
    const std::string& json) {
  version_ = 0;
  signal_level_ = 0;
  classification_threshold_ = 0;
  signal_decay_time_window_in_seconds_ = 0;

  base::Optional<base::Value> root = base::JSONReader::Read(json);
  if (!root) {
    BLOG(1, "Failed to load from JSON, root missing");
    return false;
  }

  if (base::Optional<int> version = root->FindIntPath("version")) {
    if (kCurrentModelVersion != *version) {
      BLOG(1, "Failed to load from JSON, version missing");
      return false;
    }

    version_ = *version;
  }

  // Parsing field: "parameters"
  base::Value* parameters = root->FindKey("parameters");
  if (!parameters->is_dict()) {
    BLOG(1, "Failed to load from JSON, parameters missing");
    return false;
  }

  if (base::Optional<int> signal_level =
      parameters->FindIntPath("signal_level")) {
    signal_level_ = *signal_level;
  }

  if (base::Optional<int> classification_threshold =
      parameters->FindIntPath("classification_threshold")) {
    classification_threshold_ = *classification_threshold;
  }

  if (base::Optional<int> signal_decay_time_window_in_seconds =
      parameters->FindIntPath("signal_decay_time_window_in_seconds")) {
    signal_decay_time_window_in_seconds_ =
        *signal_decay_time_window_in_seconds;
  }

  // Parsing field: "segments"
  base::Value* incoming_segemnts = root->FindListPath("segments");
  if (!incoming_segemnts) {
    BLOG(1, "Failed to load from JSON, segments missing");
    return false;
  }

  if (!incoming_segemnts->is_list()) {
    BLOG(1, "Failed to load from JSON, segments is not of type list");
    return false;
  }

  base::ListValue* list3;
  if (!incoming_segemnts->GetAsList(&list3)) {
    BLOG(1, "Failed to load from JSON, get segments as list");
    return false;
  }

  std::vector<std::string> segments;
  for (auto& segment : *list3) {
    segments.push_back(segment.GetString());
  }

  // Parsing field: "segment_keywords"
  base::Value* incoming_segment_keywords =
      root->FindDictPath("segment_keywords");
  if (!incoming_segment_keywords) {
    BLOG(1, "Failed to load from JSON, segment keywords missing");
    return false;
  }

  if (!incoming_segment_keywords->is_dict()) {
    BLOG(1, "Failed to load from JSON, segment keywords not of type dict");
    return false;
  }

  base::DictionaryValue* dict2;
  if (!incoming_segment_keywords->GetAsDictionary(&dict2)) {
    BLOG(1, "Failed to load from JSON, get segment keywords as dict");
    return false;
  }

  segment_keywords_.clear();
  for (base::DictionaryValue::Iterator it(*dict2); !it.IsAtEnd();
      it.Advance()) {
    SegmentKeywordInfo info;
    info.keywords = it.key();
    for (const auto& segment_ix : it.value().GetList()) {
      info.segments.push_back(segments.at(segment_ix.GetInt()));
    }

    segment_keywords_.push_back(info);
  }

  // Parsing field: "funnel_keywords"
  base::Value* incoming_funnel_keywords =
      root->FindDictPath("funnel_keywords");
  if (!incoming_funnel_keywords) {
    BLOG(1, "Failed to load from JSON, funnel keywords missing");
    return false;
  }

  if (!incoming_funnel_keywords->is_dict()) {
    BLOG(1, "Failed to load from JSON, funnel keywords not of type dict");
    return false;
  }

  base::DictionaryValue* dict;
  if (!incoming_funnel_keywords->GetAsDictionary(&dict)) {
    BLOG(1, "Failed to load from JSON, get funnel keywords as dict");
    return false;
  }

  funnel_keywords_.clear();
  for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd();
      it.Advance()) {
    FunnelKeywordInfo info;
    info.keywords = it.key();
    info.weight = it.value().GetInt();
    funnel_keywords_.push_back(info);
  }

  // Parsing field: "funnel_sites"
  base::Value* incoming_funnel_sites = root->FindListPath("funnel_sites");
  if (!incoming_funnel_sites) {
    BLOG(1, "Failed to load from JSON, sites missing");
    return false;
  }

  if (!incoming_funnel_sites->is_list()) {
    BLOG(1, "Failed to load from JSON, sites not of type dict");
    return false;
  }

  base::ListValue* list1;
  if (!incoming_funnel_sites->GetAsList(&list1)) {
    BLOG(1, "Failed to load from JSON, get sites as dict");
    return false;
  }

  // For each set of sites and segments
  sites_.clear();

  for (auto& set : *list1) {
    if (!set.is_dict()) {
      BLOG(1, "Failed to load from JSON, site set not of type dict");
      return false;
    }

    // Get all segments...
    base::ListValue* seg_list;
    base::Value* seg_value = set.FindListPath("segments");
    if (!seg_value->GetAsList(&seg_list)) {
      BLOG(1, "Failed to load from JSON, get site segment list as dict");
      return false;
    }

    std::vector<std::string> site_segments;
    for (auto& seg : *seg_list) {
      site_segments.push_back(segments.at(seg.GetInt()));
    }

    // ...and for each site create info with appended segments
    base::ListValue* site_list;
    base::Value* site_value = set.FindListPath("sites");
    if (!site_value->GetAsList(&site_list)) {
      BLOG(1, "Failed to load from JSON, get site list as dict");
      return false;
    }

    for (const auto& site : *site_list) {
      SiteInfo info;
      info.segments = site_segments;
      info.url_netloc = site.GetString();
      info.weight = 1;
      sites_.push_back(info);
    }
  }

  BLOG(1, "Parsed purchase intent user model version " << version_
      << " with a signal level of " << signal_level_ << ", classification "
          "threshold of " << classification_threshold_ << " and a signal "
              "decay time window of " << signal_decay_time_window_in_seconds_
                  << " seconds");

  return true;
}

}  // namespace resource
}  // namespace ad_targeting
}  // namespace ads
