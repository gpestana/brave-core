/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/wip/ad_targeting/processors/ad_processor.h"

#include "bat/ads/internal/unittest_base.h"
#include "bat/ads/internal/unittest_util.h"

// npm run test -- brave_unit_tests --filter=BatAds*

namespace ads {
namespace ad_targeting {
namespace processor {

class BatAdsAdProcessorTest : public UnitTestBase {
 protected:
  BatAdsAdProcessorTest() = default;

  ~BatAdsAdProcessorTest() override = default;
};

TEST_F(BatAdsAdProcessorTest,
    Process) {
  // Arrange

  // Act
  AdProcessor processor;
  processor.Process();

  // Assert
  FAIL();
}

}  // namespace processor
}  // namespace ad_targeting
}  // namespace ads
