/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/wip/ad_targeting/resources/text_classification/text_classification_resource.h"

#include "base/json/json_reader.h"
#include "brave/components/l10n/common/locale_util.h"
#include "bat/ads/internal/ads_client_helper.h"
#include "bat/ads/internal/logging.h"
#include "bat/ads/internal/wip/ad_targeting/resources/text_classification/text_classification_resources.h"
#include "bat/ads/result.h"
#include "bat/usermodel/user_model.h"

namespace ads {
namespace ad_targeting {
namespace resource {

TextClassification::TextClassification() = default;

TextClassification::~TextClassification() = default;

bool TextClassification::IsInitialized() const {
  return user_model_ && user_model_->IsInitialized();
}

void TextClassification::LoadForLocale(
    const std::string& locale) {
  const std::string language_code = brave_l10n::GetLanguageCode(locale);

  const auto iter = kTextClassificationLanguageCodes.find(language_code);
  if (iter == kTextClassificationLanguageCodes.end()) {
    BLOG(1, locale << " locale does not support page classification");
    user_model_.reset(usermodel::UserModel::CreateInstance());
    return;
  }

  LoadForId(iter->second);
}

void TextClassification::LoadForId(
    const std::string& id) {
  AdsClientHelper::Get()->LoadUserModelForId(id, [&](
      const Result result,
      const std::string& json) {
    user_model_.reset(usermodel::UserModel::CreateInstance());

    if (result != SUCCESS) {
      BLOG(1, "Failed to load " << id << " text classification resource");
      return;
    }

    BLOG(1, "Successfully loaded " << id << " text classification resource");

    if (!user_model_->InitializePageClassifier(json)) {
      BLOG(1, "Failed to initialize " << id << " text classification resource");
      return;
    }

    BLOG(1, "Successfully initialized " << id
        << " text classification resource");
  });
}

usermodel::UserModel* TextClassification::Get() {
  return user_model_.get();
}

}  // namespace resource
}  // namespace ad_targeting
}  // namespace ads
