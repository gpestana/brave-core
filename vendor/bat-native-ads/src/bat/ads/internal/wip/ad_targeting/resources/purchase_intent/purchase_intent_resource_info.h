/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BAT_ADS_INTERNAL_WIP_AD_TARGETING_RESOURCES_PURCHASE_INTENT_PURCHASE_INTENT_RESOURCE_INFO_H_  // NOLINT
#define BAT_ADS_INTERNAL_WIP_AD_TARGETING_RESOURCES_PURCHASE_INTENT_PURCHASE_INTENT_RESOURCE_INFO_H_  // NOLINT

#include <stdint.h>

#include <vector>

#include "bat/ads/internal/ad_targeting/behavioral/purchase_intent_classifier/site_info.h"
#include "bat/ads/internal/ad_targeting/behavioral/purchase_intent_classifier/segment_keyword_info.h"
#include "bat/ads/internal/ad_targeting/behavioral/purchase_intent_classifier/funnel_keyword_info.h"

namespace ads {
namespace ad_targeting {
namespace resource {

struct PurchaseIntentInfo {
 public:
  PurchaseIntentInfo();
  PurchaseIntentInfo(
      const PurchaseIntentInfo& info);
  ~PurchaseIntentInfo();

  uint16_t version = 0;
  uint16_t signal_level = 0;
  uint16_t classification_threshold = 0;
  uint64_t signal_decay_time_window_in_seconds = 0;
  std::vector<SiteInfo> sites;
  std::vector<SegmentKeywordInfo> segment_keywords;
  std::vector<FunnelKeywordInfo> funnel_keywords;
};

}  // namespace resource
}  // namespace ad_targeting
}  // namespace ads

#endif  // BAT_ADS_INTERNAL_WIP_AD_TARGETING_RESOURCES_PURCHASE_INTENT_PURCHASE_INTENT_RESOURCE_INFO_H_  // NOLINT
