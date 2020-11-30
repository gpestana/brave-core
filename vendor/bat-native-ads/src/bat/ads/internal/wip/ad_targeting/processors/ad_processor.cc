/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/wip/ad_targeting/processors/ad_processor.h"

#include "bat/ads/ad_info.h"

namespace ads {
namespace ad_targeting {
namespace processor {

Ad::Ad() = default;

Ad::~Ad() = default;

void Ad::Process(
    const AdInfo& ad) {
}

}  // namespace processor
}  // namespace ad_targeting
}  // namespace ads
