/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BAT_ADS_INTERNAL_WIP_AD_TARGETING_PROCESSORS_TEXT_CLASSIFICATION_PROCESSOR_H_  // NOLINT
#define BAT_ADS_INTERNAL_WIP_AD_TARGETING_PROCESSORS_TEXT_CLASSIFICATION_PROCESSOR_H_  // NOLINT

#include "bat/ads/internal/wip/ad_targeting/processor.h"

#include <map>
#include <string>

namespace usermodel {
class UserModel;
}  // namespace usermodel

namespace ads {
namespace ad_targeting {
namespace processor {

using TextClassificationProbabilitiesMap = std::map<std::string, double>;

class TextClassification : public Processor<std::string> {
 public:
  TextClassification(
      usermodel::UserModel* user_model);

  ~TextClassification() override;

  void Process(
      const std::string& text) override;

 private:
  usermodel::UserModel* user_model_;  // NOT OWNED
};

}  // namespace processor
}  // namespace ad_targeting
}  // namespace ads

#endif  // BAT_ADS_INTERNAL_WIP_AD_TARGETING_PROCESSORS_TEXT_CLASSIFICATION_PROCESSOR_H_  // NOLINT
