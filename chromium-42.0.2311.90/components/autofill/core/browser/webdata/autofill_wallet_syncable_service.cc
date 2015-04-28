// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_syncable_service.h"

#include <set>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "sync/api/sync_error_factory.h"
#include "sync/protocol/sync.pb.h"

namespace autofill {

namespace {

void* UserDataKey() {
  // Use the address of a static so that COMDAT folding won't ever fold
  // with something else.
  static int user_data_key = 0;
  return reinterpret_cast<void*>(&user_data_key);
}

const char* CardTypeFromWalletCardType(
    sync_pb::WalletMaskedCreditCard::WalletCardType type) {
  switch (type) {
    case sync_pb::WalletMaskedCreditCard::AMEX:
      return kAmericanExpressCard;
    case sync_pb::WalletMaskedCreditCard::DISCOVER:
      return kDiscoverCard;
    case sync_pb::WalletMaskedCreditCard::JCB:
      return kJCBCard;
    case sync_pb::WalletMaskedCreditCard::MASTER_CARD:
      return kMasterCard;
    case sync_pb::WalletMaskedCreditCard::VISA:
      return kVisaCard;

    // These aren't supported by the client, so just declare a generic card.
    case sync_pb::WalletMaskedCreditCard::MAESTRO:
    case sync_pb::WalletMaskedCreditCard::SOLO:
    case sync_pb::WalletMaskedCreditCard::SWITCH:
    default:
      return kGenericCard;
  }
}

CreditCard::ServerStatus ServerToLocalStatus(
    sync_pb::WalletMaskedCreditCard::WalletCardStatus status) {
  switch (status) {
    case sync_pb::WalletMaskedCreditCard::VALID:
      return CreditCard::OK;
    case sync_pb::WalletMaskedCreditCard::EXPIRED:
    default:
      DCHECK_EQ(sync_pb::WalletMaskedCreditCard::EXPIRED, status);
      return CreditCard::EXPIRED;
  }
}

CreditCard CardFromSpecifics(const sync_pb::WalletMaskedCreditCard& card) {
  CreditCard result(CreditCard::MASKED_SERVER_CARD, card.id());
  result.SetNumber(base::UTF8ToUTF16(card.last_four()));
  result.SetServerStatus(ServerToLocalStatus(card.status()));
  result.SetTypeForMaskedCard(CardTypeFromWalletCardType(card.type()));
  result.SetRawInfo(CREDIT_CARD_NAME, base::UTF8ToUTF16(card.name_on_card()));
  result.SetExpirationMonth(card.exp_month());
  result.SetExpirationYear(card.exp_year());
  return result;
}

AutofillProfile ProfileFromSpecifics(
    const sync_pb::WalletPostalAddress& address) {
  AutofillProfile profile(AutofillProfile::SERVER_PROFILE, address.id());

  // AutofillProfile stores multi-line addresses with newline separators.
  std::vector<std::string> street_address(address.street_address().begin(),
                                          address.street_address().end());
  profile.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS,
                     base::UTF8ToUTF16(JoinString(street_address, '\n')));

  profile.SetRawInfo(COMPANY_NAME, base::UTF8ToUTF16(address.company_name()));
  profile.SetRawInfo(ADDRESS_HOME_STATE,
                     base::UTF8ToUTF16(address.address_1()));
  profile.SetRawInfo(ADDRESS_HOME_CITY,
                     base::UTF8ToUTF16(address.address_2()));
  profile.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY,
                     base::UTF8ToUTF16(address.address_3()));
  // AutofillProfile doesn't support address_4 ("sub dependent locality").
  profile.SetRawInfo(ADDRESS_HOME_ZIP,
                     base::UTF8ToUTF16(address.postal_code()));
  profile.SetRawInfo(ADDRESS_HOME_SORTING_CODE,
                     base::UTF8ToUTF16(address.sorting_code()));
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY,
                     base::UTF8ToUTF16(address.country_code()));
  profile.set_language_code(address.language_code());

  return profile;
}

// Implements operator< for two objects given their pointers.
template<class Data> struct AutofillDataPtrLessThan {
  bool operator()(const Data* a, const Data* b) const {
    return a->Compare(*b) < 0;
  }
};

// This function handles conditionally updating the AutofillTable with either
// a set of CreditCards or AutocompleteProfiles only when the existing data
// doesn't match.
//
// It's passed the getter and setter function on the AutofillTable for the
// corresponding data type, and expects the types to implement a server_id()
// and a Compare function.
//
// Returns the previous number of items in the table (for sync tracking).
template<class Data>
size_t SetDataIfChanged(
    AutofillTable* table,
    const std::vector<Data>& data,
    bool (AutofillTable::*getter)(std::vector<Data*>*),
    void (AutofillTable::*setter)(const std::vector<Data>&)) {
  ScopedVector<Data> existing_data;
  (table->*getter)(&existing_data.get());

  bool difference_found = true;
  if (existing_data.size() == data.size()) {
    difference_found = false;

    // Implement this set with pointers using our custom operator that takes
    // pointers which avoids any copies.
    std::set<const Data*, AutofillDataPtrLessThan<Data>> existing_data_set;
    for (const Data* cur : existing_data)
      existing_data_set.insert(cur);

    for (const Data& new_data : data) {
      if (existing_data_set.find(&new_data) == existing_data_set.end()) {
        difference_found = true;
        break;
      }
    }
  }

  if (difference_found)
    (table->*setter)(data);

  return existing_data.size();
}

}  // namespace

AutofillWalletSyncableService::AutofillWalletSyncableService(
    AutofillWebDataBackend* webdata_backend,
    const std::string& app_locale)
    : webdata_backend_(webdata_backend) {
}

AutofillWalletSyncableService::~AutofillWalletSyncableService() {
}

syncer::SyncMergeResult
AutofillWalletSyncableService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
    scoped_ptr<syncer::SyncErrorFactory> sync_error_factory) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::vector<CreditCard> wallet_cards;
  std::vector<AutofillProfile> wallet_addresses;

  for (const syncer::SyncData& data : initial_sync_data) {
    DCHECK_EQ(syncer::AUTOFILL_WALLET_DATA, data.GetDataType());
    const sync_pb::AutofillWalletSpecifics& autofill_specifics =
        data.GetSpecifics().autofill_wallet();
    if (autofill_specifics.type() ==
        sync_pb::AutofillWalletSpecifics::MASKED_CREDIT_CARD) {
      wallet_cards.push_back(
          CardFromSpecifics(autofill_specifics.masked_card()));
    } else {
      DCHECK_EQ(sync_pb::AutofillWalletSpecifics::POSTAL_ADDRESS,
                autofill_specifics.type());
      wallet_addresses.push_back(
          ProfileFromSpecifics(autofill_specifics.address()));
    }
  }

  // In the common case, the database won't have changed. Committing an update
  // to the database will require at least one DB page write and will schedule
  // a fsync. To avoid this I/O, it should be more efficient to do a read and
  // only do the writes if something changed.
  AutofillTable* table =
      AutofillTable::FromWebDatabase(webdata_backend_->GetDatabase());
  size_t prev_card_count =
      SetDataIfChanged(table, wallet_cards,
                       &AutofillTable::GetServerCreditCards,
                       &AutofillTable::SetServerCreditCards);
  size_t prev_address_count =
      SetDataIfChanged(table, wallet_addresses,
                       &AutofillTable::GetAutofillServerProfiles,
                       &AutofillTable::SetAutofillServerProfiles);

  syncer::SyncMergeResult merge_result(type);
  merge_result.set_num_items_before_association(
      static_cast<int>(prev_card_count + prev_address_count));
  merge_result.set_num_items_after_association(
      static_cast<int>(wallet_cards.size() + wallet_addresses.size()));
  return merge_result;
}

void AutofillWalletSyncableService::StopSyncing(syncer::ModelType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(type, syncer::AUTOFILL_WALLET_DATA);
}

syncer::SyncDataList AutofillWalletSyncableService::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  syncer::SyncDataList current_data;
  return current_data;
}

syncer::SyncError AutofillWalletSyncableService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(brettw) handle incremental updates while Chrome is running.
  return syncer::SyncError();
}

// static
void AutofillWalletSyncableService::CreateForWebDataServiceAndBackend(
    AutofillWebDataService* web_data_service,
    AutofillWebDataBackend* webdata_backend,
    const std::string& app_locale) {
  web_data_service->GetDBUserData()->SetUserData(
      UserDataKey(),
      new AutofillWalletSyncableService(webdata_backend, app_locale));
}

// static
AutofillWalletSyncableService*
AutofillWalletSyncableService::FromWebDataService(
    AutofillWebDataService* web_data_service) {
  return static_cast<AutofillWalletSyncableService*>(
      web_data_service->GetDBUserData()->GetUserData(UserDataKey()));
}

void AutofillWalletSyncableService::InjectStartSyncFlare(
    const syncer::SyncableService::StartSyncFlare& flare) {
  flare_ = flare;
}

}  // namespace autofill
