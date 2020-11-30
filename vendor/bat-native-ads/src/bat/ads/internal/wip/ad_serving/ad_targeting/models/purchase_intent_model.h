/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BAT_ADS_INTERNAL_WIP_AD_SERVING_AD_TARGETING_MODELS_PURCHASE_INTENT_CLASSIFICATION_MODEL_H_  // NOLINT
#define BAT_ADS_INTERNAL_WIP_AD_SERVING_AD_TARGETING_MODELS_PURCHASE_INTENT_CLASSIFICATION_MODEL_H_  // NOLINT

#include "bat/ads/internal/wip/ad_serving/ad_targeting/model.h"

#include <string>
#include <vector>

namespace ads {
namespace ad_targeting {
namespace model {

class PurchaseIntent : public Model {
 public:
  PurchaseIntent();

  ~PurchaseIntent() override;

  SegmentList GetSegments() const override;
};

}  // namespace model
}  // namespace ad_targeting
}  // namespace ads

#endif  // BAT_ADS_INTERNAL_WIP_AD_SERVING_AD_TARGETING_MODELS_PURCHASE_INTENT_CLASSIFICATION_MODEL_H_  // NOLINT
