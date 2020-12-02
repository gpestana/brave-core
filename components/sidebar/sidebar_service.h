/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_SIDEBAR_SIDEBAR_SERVICE_H_
#define BRAVE_COMPONENTS_SIDEBAR_SIDEBAR_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

class PrefRegistrySimple;
class PrefService;

namespace sidebar {

// This manages per-context sidebar data such as item list and more.
class SidebarService : public KeyedService {
 public:
  class Observer {
   public:
    virtual void OnItemsUpdated() = 0;

   protected:
    virtual ~Observer() = default;
  };

  static void RegisterPrefs(PrefRegistrySimple* registry);

  explicit SidebarService(PrefService* prefs);
  ~SidebarService() override;

  void AddItem();
  void RemoveItem();

  SidebarService(const SidebarService&) = delete;
  SidebarService& operator=(const SidebarService&) = delete;

 private:
  PrefService* prefs_ = nullptr;
};

}  // namespace sidebar

#endif  // BRAVE_COMPONENTS_SIDEBAR_SIDEBAR_SERVICE_H_
