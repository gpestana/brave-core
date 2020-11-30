/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/ads_impl.h"

#include <utility>

#include "base/time/time.h"
#include "bat/ads/ad_history_info.h"
#include "bat/ads/ad_info.h"
#include "bat/ads/ad_notification_info.h"
#include "bat/ads/ads_client.h"
#include "bat/ads/confirmation_type.h"
#include "bat/ads/internal/account/account.h"
#include "bat/ads/internal/ad_events/ad_events.h"
#include "bat/ads/internal/ad_server/ad_server.h"
#include "bat/ads/internal/ad_serving/ad_notifications/ad_notification_serving.h"
#include "bat/ads/internal/ad_targeting/ad_targeting.h"
#include "bat/ads/internal/ad_targeting/behavioral/purchase_intent_classifier/purchase_intent_classifier.h"
#include "bat/ads/internal/ad_targeting/behavioral/purchase_intent_classifier/purchase_intent_classifier_user_models.h"
#include "bat/ads/internal/ad_targeting/contextual/contextual_util.h"
#include "bat/ads/internal/ad_targeting/geographic/subdivision/subdivision_targeting.h"
#include "bat/ads/internal/ad_transfer/ad_transfer.h"
#include "bat/ads/internal/ads/ad_notifications/ad_notification.h"
#include "bat/ads/internal/ads/ad_notifications/ad_notifications.h"
#include "bat/ads/internal/ads/new_tab_page_ads/new_tab_page_ad.h"
#include "bat/ads/internal/ads_client_helper.h"
#include "bat/ads/internal/ads_history/ads_history.h"
#include "bat/ads/internal/catalog/catalog_issuers_info.h"
#include "bat/ads/internal/catalog/catalog_util.h"
#include "bat/ads/internal/client/client.h"
#include "bat/ads/internal/confirmations/confirmation_info.h"
#include "bat/ads/internal/confirmations/confirmations.h"
#include "bat/ads/internal/confirmations/confirmations_state.h"
#include "bat/ads/internal/conversions/conversions.h"
#include "bat/ads/internal/database/database_initialize.h"
#include "bat/ads/internal/features/features.h"
#include "bat/ads/internal/logging.h"
#include "bat/ads/internal/platform/platform_helper.h"
#include "bat/ads/internal/privacy/tokens/token_generator.h"
#include "bat/ads/internal/privacy/unblinded_tokens/unblinded_tokens.h"
#include "bat/ads/internal/tab_manager/tab_info.h"
#include "bat/ads/internal/tab_manager/tab_manager.h"
#include "bat/ads/internal/user_activity/user_activity.h"
#include "bat/ads/internal/wip/ad_targeting/processors/text_classification_processor.h"
#include "bat/ads/internal/wip/ad_targeting/resources/text_classification/text_classification_resource.h"
#include "bat/ads/internal/wip/ad_targeting/resources/text_classification/text_classification_resources.h"
#include "bat/ads/new_tab_page_ad_info.h"
#include "bat/ads/pref_names.h"
#include "bat/ads/statement_info.h"

namespace ads {

namespace {
const int kIdleThresholdInSeconds = 15;
}  // namespace

AdsImpl::AdsImpl(
    AdsClient* ads_client)
    : ads_client_helper_(std::make_unique<AdsClientHelper>(ads_client)),
      token_generator_(std::make_unique<privacy::TokenGenerator>()) {
  set(token_generator_.get());
}

AdsImpl::~AdsImpl() {
  account_->RemoveObserver(this);
  ad_notification_->RemoveObserver(this);
  ad_server_->RemoveObserver(this);
  ad_transfer_->RemoveObserver(this);
  conversions_->RemoveObserver(this);
  new_tab_page_ad_->RemoveObserver(this);
}

void AdsImpl::set_for_testing(
    privacy::TokenGeneratorInterface* token_generator) {
  DCHECK(token_generator);

  token_generator_.release();

  set(token_generator);
}

bool AdsImpl::IsInitialized() {
  if (!is_initialized_ ||
      !AdsClientHelper::Get()->GetBooleanPref(prefs::kEnabled)) {
    return false;
  }

  return true;
}

void AdsImpl::Initialize(
    InitializeCallback callback) {
  BLOG(1, "Initializing ads");

  if (IsInitialized()) {
    BLOG(1, "Already initialized ads");
    callback(FAILED);
    return;
  }

  const auto initialize_step_2_callback = std::bind(&AdsImpl::InitializeStep2,
      this, std::placeholders::_1, std::move(callback));
  database_->CreateOrOpen(initialize_step_2_callback);
}

void AdsImpl::Shutdown(
    ShutdownCallback callback) {
  if (!is_initialized_) {
    BLOG(0, "Shutdown failed as not initialized");

    callback(FAILED);
    return;
  }

  ad_notifications_->RemoveAll(true);

  callback(SUCCESS);
}

void AdsImpl::ChangeLocale(
    const std::string& locale) {
  subdivision_targeting_->MaybeFetchForLocale(locale);
  text_classification_resource_->LoadForLocale(locale);
  purchase_intent_classifier_->LoadUserModelForLocale(locale);
}

void AdsImpl::OnAdsSubdivisionTargetingCodeHasChanged() {
  subdivision_targeting_->MaybeFetchForCurrentLocale();
}

void AdsImpl::OnPageLoaded(
    const int32_t tab_id,
    const std::string& original_url,
    const std::string& url,
    const std::string& content) {
  DCHECK(!original_url.empty());
  DCHECK(!url.empty());

  if (!IsInitialized()) {
    return;
  }

  ad_transfer_->MaybeTransferAd(tab_id, original_url);

  conversions_->MaybeConvert(url);

  const base::Optional<TabInfo> last_visible_tab =
      TabManager::Get()->GetLastVisible();

  std::string last_visible_tab_url;
  if (last_visible_tab) {
    last_visible_tab_url = last_visible_tab->url;
  }

  purchase_intent_classifier_->MaybeExtractIntentSignal(url,
      last_visible_tab_url);


  const std::string stripped_text =
      ad_targeting::contextual::StripHtmlTagsAndNonAlphaCharacters(content);
  text_classification_processor_->Process(stripped_text);
}

void AdsImpl::OnIdle() {
  BLOG(1, "Browser state changed to idle");
}

void AdsImpl::OnUnIdle() {
  if (!IsInitialized()) {
    return;
  }

  BLOG(1, "Browser state changed to unidle");

  MaybeUpdateCatalog();

  MaybeServeAdNotification();
}

void AdsImpl::OnForeground() {
  TabManager::Get()->OnForegrounded();

  MaybeUpdateCatalog();

  MaybeServeAdNotificationsAtRegularIntervals();
}

void AdsImpl::OnBackground() {
  TabManager::Get()->OnBackgrounded();

  MaybeServeAdNotificationsAtRegularIntervals();
}

void AdsImpl::OnMediaPlaying(
    const int32_t tab_id) {
  TabManager::Get()->OnMediaPlaying(tab_id);
}

void AdsImpl::OnMediaStopped(
    const int32_t tab_id) {
  TabManager::Get()->OnMediaStopped(tab_id);
}

void AdsImpl::OnTabUpdated(
    const int32_t tab_id,
    const std::string& url,
    const bool is_active,
    const bool is_browser_active,
    const bool is_incognito) {
  const bool is_visible = is_active && is_browser_active;
  TabManager::Get()->OnUpdated(tab_id, url, is_visible, is_incognito);
}

void AdsImpl::OnTabClosed(
    const int32_t tab_id) {
  TabManager::Get()->OnClosed(tab_id);

  ad_transfer_->Cancel(tab_id);
}

void AdsImpl::OnWalletUpdated(
    const std::string& id,
    const std::string& seed) {
  if (!account_->SetWallet(id, seed)) {
    BLOG(0, "Failed to set wallet");
    return;
  }

  BLOG(1, "Successfully set wallet");
}

void AdsImpl::OnUserModelUpdated(
    const std::string& id) {
  if (ad_targeting::resource::kTextClassificationIds.find(id) !=
      ad_targeting::resource::kTextClassificationIds.end()) {
    text_classification_resource_->LoadForId(id);
  } else if (kPurchaseIntentUserModelIds.find(id) !=
      kPurchaseIntentUserModelIds.end()) {
    purchase_intent_classifier_->LoadUserModelForId(id);
  } else {
    BLOG(0, "Unknown " << id << " user model");
  }
}

bool AdsImpl::GetAdNotification(
    const std::string& uuid,
    AdNotificationInfo* notification) {
  DCHECK(notification);
  return ad_notifications_->Get(uuid, notification);
}

void AdsImpl::OnAdNotificationEvent(
    const std::string& uuid,
    const AdNotificationEventType event_type) {
  ad_notification_->FireEvent(uuid, event_type);
}

void AdsImpl::OnNewTabPageAdEvent(
    const std::string& wallpaper_id,
    const std::string& creative_instance_id,
    const NewTabPageAdEventType event_type) {
  new_tab_page_ad_->FireEvent(wallpaper_id, creative_instance_id, event_type);
}

void AdsImpl::RemoveAllHistory(
    RemoveAllHistoryCallback callback) {
  Client::Get()->RemoveAllHistory();

  callback(SUCCESS);
}

void AdsImpl::ReconcileAdRewards() {
  if (!IsInitialized()) {
    return;
  }

  account_->Reconcile();
}

AdsHistoryInfo AdsImpl::GetAdsHistory(
    const AdsHistoryInfo::FilterType filter_type,
    const AdsHistoryInfo::SortType sort_type,
    const uint64_t from_timestamp,
    const uint64_t to_timestamp) {
  return history::Get(filter_type, sort_type, from_timestamp, to_timestamp);
}

void AdsImpl::GetStatement(
    GetStatementCallback callback) {
  StatementInfo statement_of_account;

  if (!IsInitialized()) {
    callback(/* success */ false, statement_of_account);
    return;
  }

  const int64_t to_timestamp =
      static_cast<int64_t>(base::Time::Now().ToDoubleT());

  statement_of_account = account_->GetStatement(0, to_timestamp);

  callback(/* success */ true, statement_of_account);
}

AdContentInfo::LikeAction AdsImpl::ToggleAdThumbUp(
    const std::string& creative_instance_id,
    const std::string& creative_set_id,
    const AdContentInfo::LikeAction& action) {
  auto like_action = Client::Get()->ToggleAdThumbUp(creative_instance_id,
      creative_set_id, action);
  if (like_action == AdContentInfo::LikeAction::kThumbsUp) {
    confirmations_->ConfirmAd(creative_instance_id, ConfirmationType::kUpvoted);
  }

  return like_action;
}

AdContentInfo::LikeAction AdsImpl::ToggleAdThumbDown(
    const std::string& creative_instance_id,
    const std::string& creative_set_id,
    const AdContentInfo::LikeAction& action) {
  auto like_action = Client::Get()->ToggleAdThumbDown(creative_instance_id,
      creative_set_id, action);
  if (like_action == AdContentInfo::LikeAction::kThumbsDown) {
    confirmations_->ConfirmAd(creative_instance_id,
        ConfirmationType::kDownvoted);
  }

  return like_action;
}

CategoryContentInfo::OptAction AdsImpl::ToggleAdOptInAction(
    const std::string& category,
    const CategoryContentInfo::OptAction& action) {
  return Client::Get()->ToggleAdOptInAction(category, action);
}

CategoryContentInfo::OptAction AdsImpl::ToggleAdOptOutAction(
    const std::string& category,
    const CategoryContentInfo::OptAction& action) {
  return Client::Get()->ToggleAdOptOutAction(category, action);
}

bool AdsImpl::ToggleSaveAd(
    const std::string& creative_instance_id,
    const std::string& creative_set_id,
    const bool saved) {
  return Client::Get()->ToggleSaveAd(creative_instance_id,
      creative_set_id, saved);
}

bool AdsImpl::ToggleFlagAd(
    const std::string& creative_instance_id,
    const std::string& creative_set_id,
    const bool flagged) {
  auto flag_ad = Client::Get()->ToggleFlagAd(creative_instance_id,
      creative_set_id, flagged);
  if (flag_ad) {
    confirmations_->ConfirmAd(creative_instance_id, ConfirmationType::kFlagged);
  }

  return flag_ad;
}

///////////////////////////////////////////////////////////////////////////////

void AdsImpl::set(
    privacy::TokenGeneratorInterface* token_generator) {
  DCHECK(token_generator);

  confirmations_ = std::make_unique<Confirmations>(token_generator_.get());

  account_ = std::make_unique<Account>(confirmations_.get(),
      token_generator_.get());
  account_->AddObserver(this);

  text_classification_resource_ =
      std::make_unique<ad_targeting::resource::TextClassification>();
  text_classification_processor_ =
      std::make_unique<ad_targeting::processor::TextClassification>(
          text_classification_resource_->Get());

  purchase_intent_classifier_ =
      std::make_unique<ad_targeting::behavioral::PurchaseIntentClassifier>();
  subdivision_targeting_ =
      std::make_unique<ad_targeting::geographic::SubdivisionTargeting>();
  ad_targeting_ = std::make_unique<AdTargeting>(
      purchase_intent_classifier_.get());

  ad_notification_serving_ = std::make_unique<ad_notifications::AdServing>(
      ad_targeting_.get(), subdivision_targeting_.get());
  ad_notification_ = std::make_unique<AdNotification>();
  ad_notification_->AddObserver(this);
  ad_notifications_ = std::make_unique<AdNotifications>();

  ad_server_ = std::make_unique<AdServer>();
  ad_server_->AddObserver(this);

  ad_transfer_ = std::make_unique<AdTransfer>();
  ad_transfer_->AddObserver(this);

  client_ = std::make_unique<Client>();

  conversions_ = std::make_unique<Conversions>();
  conversions_->AddObserver(this);

  database_ = std::make_unique<database::Initialize>();

  new_tab_page_ad_ = std::make_unique<NewTabPageAd>();
  new_tab_page_ad_->AddObserver(this);

  tab_manager_ = std::make_unique<TabManager>();

  user_activity_ = std::make_unique<UserActivity>();
}

void AdsImpl::InitializeStep2(
    const Result result,
    InitializeCallback callback) {
  if (result != SUCCESS) {
    BLOG(0, "Failed to initialize database: " << database_->get_last_message());
    callback(FAILED);
    return;
  }

  const auto initialize_step_3_callback = std::bind(&AdsImpl::InitializeStep3,
      this, std::placeholders::_1, std::move(callback));
  Client::Get()->Initialize(initialize_step_3_callback);
}

void AdsImpl::InitializeStep3(
    const Result result,
    InitializeCallback callback) {
  if (result != SUCCESS) {
    callback(FAILED);
    return;
  }

  const auto initialize_step_4_callback = std::bind(&AdsImpl::InitializeStep4,
      this, std::placeholders::_1, std::move(callback));
  ConfirmationsState::Get()->Initialize(initialize_step_4_callback);
}

void AdsImpl::InitializeStep4(
    const Result result,
    InitializeCallback callback) {
  if (result != SUCCESS) {
    callback(FAILED);
    return;
  }

  const auto initialize_step_5_callback = std::bind(&AdsImpl::InitializeStep5,
      this, std::placeholders::_1, std::move(callback));
  ad_notifications_->Initialize(initialize_step_5_callback);
}

void AdsImpl::InitializeStep5(
    const Result result,
    InitializeCallback callback) {
  if (result != SUCCESS) {
    callback(FAILED);
    return;
  }

  const auto initialize_step_6_callback = std::bind(&AdsImpl::InitializeStep6,
      this, std::placeholders::_1, std::move(callback));
  conversions_->Initialize(initialize_step_6_callback);
}

void AdsImpl::InitializeStep6(
    const Result result,
    InitializeCallback callback) {
  if (result != SUCCESS) {
    callback(FAILED);
    return;
  }

  is_initialized_ = true;

  BLOG(1, "Successfully initialized ads");

  AdsClientHelper::Get()->SetIntegerPref(prefs::kIdleThreshold,
      kIdleThresholdInSeconds);

  callback(SUCCESS);

#if defined(OS_ANDROID)
    // Ad notifications do not sustain a reboot or update, so we should remove
    // orphaned ad notifications
    ad_notifications_->RemoveAllAfterReboot();
    ad_notifications_->RemoveAllAfterUpdate();
#endif

  CleanupAdEvents();

  account_->Reconcile();
  account_->ProcessUnclearedTransactions();

  confirmations_->RetryAfterDelay();

  subdivision_targeting_->MaybeFetchForCurrentLocale();

  conversions_->StartTimerIfReady();

  ad_server_->MaybeFetch();

  features::LogPageProbabilitiesStudy();

  MaybeServeAdNotificationsAtRegularIntervals();
}

void AdsImpl::CleanupAdEvents() {
  PurgeExpiredAdEvents([](
      const Result result) {
    if (result != Result::SUCCESS) {
      BLOG(1, "Failed to purge expired ad events");
      return;
    }

    BLOG(6, "Successfully purged expired ad events");
  });
}

void AdsImpl::MaybeUpdateCatalog() {
  if (!HasCatalogExpired()) {
    return;
  }

  ad_server_->MaybeFetch();
}

void AdsImpl::MaybeServeAdNotification() {
  if (PlatformHelper::GetInstance()->IsMobile()) {
    return;
  }

  ad_notification_serving_->MaybeServe();
}

void AdsImpl::MaybeServeAdNotificationsAtRegularIntervals() {
  if (!PlatformHelper::GetInstance()->IsMobile()) {
    return;
  }

  if (TabManager::Get()->IsForegrounded()) {
    ad_notification_serving_->ServeAtRegularIntervals();
  } else {
    ad_notification_serving_->StopServing();
  }
}

void AdsImpl::OnAdRewardsChanged() {
  AdsClientHelper::Get()->OnAdRewardsChanged();
}

void AdsImpl::OnTransactionsChanged() {
  AdsClientHelper::Get()->OnAdRewardsChanged();
}

void AdsImpl::OnAdNotificationViewed(
    const AdNotificationInfo& ad) {
  confirmations_->ConfirmAd(ad.creative_instance_id, ConfirmationType::kViewed);
}

void AdsImpl::OnAdNotificationClicked(
    const AdNotificationInfo& ad) {
  ad_transfer_->set_last_clicked_ad(ad);

  confirmations_->ConfirmAd(ad.creative_instance_id,
      ConfirmationType::kClicked);
}

void AdsImpl::OnAdNotificationDismissed(
    const AdNotificationInfo& ad) {
  confirmations_->ConfirmAd(ad.creative_instance_id,
      ConfirmationType::kDismissed);
}

void AdsImpl::OnCatalogUpdated(
    const CatalogIssuersInfo& catalog_issuers) {
  confirmations_->SetCatalogIssuers(catalog_issuers);

  account_->TopUpUnblindedTokens();
}

void AdsImpl::OnAdTransfer(
    const AdInfo& ad) {
  confirmations_->ConfirmAd(ad.creative_instance_id,
      ConfirmationType::kTransferred);
}

void AdsImpl::OnConversion(
    const std::string& creative_instance_id) {
  confirmations_->ConfirmAd(creative_instance_id,
      ConfirmationType::kConversion);
}

void AdsImpl::OnNewTabPageAdViewed(
    const NewTabPageAdInfo& ad) {
  confirmations_->ConfirmAd(ad.creative_instance_id,
      ConfirmationType::kViewed);
}

void AdsImpl::OnNewTabPageAdClicked(
    const NewTabPageAdInfo& ad) {
  ad_transfer_->set_last_clicked_ad(ad);

  confirmations_->ConfirmAd(ad.creative_instance_id,
      ConfirmationType::kClicked);
}

}  // namespace ads
