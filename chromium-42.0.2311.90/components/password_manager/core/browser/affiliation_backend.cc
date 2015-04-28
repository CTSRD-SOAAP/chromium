// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliation_backend.h"

#include <stdint.h>

#include "base/task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/affiliation_database.h"
#include "components/password_manager/core/browser/affiliation_fetcher.h"
#include "components/password_manager/core/browser/facet_manager.h"
#include "net/url_request/url_request_context_getter.h"

namespace password_manager {

AffiliationBackend::AffiliationBackend(
    const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
    scoped_ptr<base::Clock> time_source)
    : request_context_getter_(request_context_getter),
      clock_(time_source.Pass()),
      weak_ptr_factory_(this) {
  DCHECK_LT(base::Time(), clock_->Now());
}

AffiliationBackend::~AffiliationBackend() {
}

void AffiliationBackend::Initialize(const base::FilePath& db_path) {
  thread_checker_.reset(new base::ThreadChecker);
  cache_.reset(new AffiliationDatabase());
  if (!cache_->Init(db_path)) {
    // TODO(engedy): Implement this. crbug.com/437865.
    NOTIMPLEMENTED();
  }
}

void AffiliationBackend::GetAffiliations(
    const FacetURI& facet_uri,
    bool cached_only,
    const AffiliationService::ResultCallback& callback,
    const scoped_refptr<base::TaskRunner>& callback_task_runner) {
  DCHECK(thread_checker_ && thread_checker_->CalledOnValidThread());
  if (!facet_managers_.contains(facet_uri)) {
    scoped_ptr<FacetManager> new_manager(new FacetManager(this, facet_uri));
    facet_managers_.add(facet_uri, new_manager.Pass());
  }

  FacetManager* facet_manager = facet_managers_.get(facet_uri);
  DCHECK(facet_manager);
  facet_manager->GetAffiliations(cached_only, callback, callback_task_runner);

  if (facet_manager->CanBeDiscarded())
    facet_managers_.erase(facet_uri);
}

void AffiliationBackend::Prefetch(const FacetURI& facet_uri,
                                  const base::Time& keep_fresh_until) {
  DCHECK(thread_checker_ && thread_checker_->CalledOnValidThread());

  // TODO(engedy): Implement this. crbug.com/437865.
  NOTIMPLEMENTED();
}

void AffiliationBackend::CancelPrefetch(const FacetURI& facet_uri,
                                        const base::Time& keep_fresh_until) {
  DCHECK(thread_checker_ && thread_checker_->CalledOnValidThread());

  // TODO(engedy): Implement this. crbug.com/437865.
  NOTIMPLEMENTED();
}

void AffiliationBackend::TrimCache() {
  DCHECK(thread_checker_ && thread_checker_->CalledOnValidThread());

  // TODO(engedy): Implement this. crbug.com/437865.
  NOTIMPLEMENTED();
}

void AffiliationBackend::SendNetworkRequest() {
  DCHECK(!fetcher_);

  std::vector<FacetURI> requested_facet_uris;
  for (const auto& facet_manager_pair : facet_managers_) {
    if (facet_manager_pair.second->DoesRequireFetch())
      requested_facet_uris.push_back(facet_manager_pair.first);
  }
  DCHECK(!requested_facet_uris.empty());
  fetcher_.reset(AffiliationFetcher::Create(request_context_getter_.get(),
                                            requested_facet_uris, this));
  fetcher_->StartRequest();
}

base::Time AffiliationBackend::GetCurrentTime() {
  return clock_->Now();
}

base::Time AffiliationBackend::ReadLastUpdateTimeFromDatabase(
    const FacetURI& facet_uri) {
  AffiliatedFacetsWithUpdateTime affiliation;
  return ReadAffiliationsFromDatabase(facet_uri, &affiliation)
             ? affiliation.last_update_time
             : base::Time();
}

bool AffiliationBackend::ReadAffiliationsFromDatabase(
    const FacetURI& facet_uri,
    AffiliatedFacetsWithUpdateTime* affiliations) {
  return cache_->GetAffiliationsForFacet(facet_uri, affiliations);
}

void AffiliationBackend::SignalNeedNetworkRequest() {
  // TODO(engedy): Add more sophisticated throttling logic. crbug.com/437865.
  if (fetcher_)
    return;
  SendNetworkRequest();
}

void AffiliationBackend::OnFetchSucceeded(
    scoped_ptr<AffiliationFetcherDelegate::Result> result) {
  DCHECK(thread_checker_ && thread_checker_->CalledOnValidThread());
  fetcher_.reset();

  for (const AffiliatedFacets& affiliated_facets : *result) {
    AffiliatedFacetsWithUpdateTime affiliation;
    affiliation.facets = affiliated_facets;
    affiliation.last_update_time = clock_->Now();

    std::vector<AffiliatedFacetsWithUpdateTime> obsoleted_affiliations;
    cache_->StoreAndRemoveConflicting(affiliation, &obsoleted_affiliations);

    // Cached data in contradiction with newly stored data automatically gets
    // removed from the DB, and will be stored into |obsoleted_affiliations|.
    // Let facet managers know if data is removed from under them.
    // TODO(engedy): Implement this. crbug.com/437865.
    if (!obsoleted_affiliations.empty())
      NOTIMPLEMENTED();

    for (const auto& facet_uri : affiliated_facets) {
      if (!facet_managers_.contains(facet_uri))
        continue;
      FacetManager* facet_manager = facet_managers_.get(facet_uri);
      facet_manager->OnFetchSucceeded(affiliation);
      if (facet_manager->CanBeDiscarded())
        facet_managers_.erase(facet_uri);
    }
  }

  // A subsequent fetch may be needed if any additional GetAffiliations()
  // requests came in while the current fetch was in flight.
  for (const auto& facet_manager_pair : facet_managers_) {
    if (facet_manager_pair.second->DoesRequireFetch()) {
      SendNetworkRequest();
      return;
    }
  }
}

void AffiliationBackend::OnFetchFailed() {
  DCHECK(thread_checker_ && thread_checker_->CalledOnValidThread());

  // TODO(engedy): Implement this. crbug.com/437865.
  NOTIMPLEMENTED();
}

void AffiliationBackend::OnMalformedResponse() {
  DCHECK(thread_checker_ && thread_checker_->CalledOnValidThread());

  // TODO(engedy): Implement this. crbug.com/437865.
  NOTIMPLEMENTED();
}

}  // namespace password_manager
