// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliation_service.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "components/password_manager/core/browser/affiliation_backend.h"
#include "net/url_request/url_request_context_getter.h"

namespace password_manager {

AffiliationService::AffiliationService(
    scoped_refptr<base::SingleThreadTaskRunner> backend_task_runner)
    : backend_(nullptr),
      backend_task_runner_(backend_task_runner),
      weak_ptr_factory_(this) {
}

AffiliationService::~AffiliationService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (backend_) {
    backend_task_runner_->DeleteSoon(FROM_HERE, backend_);
    backend_ = nullptr;
  }
}

void AffiliationService::Initialize(
    net::URLRequestContextGetter* request_context_getter,
    const base::FilePath& db_path) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!backend_);
  backend_ = new AffiliationBackend(request_context_getter,
                                    make_scoped_ptr(new base::DefaultClock));
  backend_task_runner_->PostTask(
      FROM_HERE, base::Bind(&AffiliationBackend::Initialize,
                            base::Unretained(backend_), db_path));
}

void AffiliationService::GetAffiliations(
    const FacetURI& facet_uri,
    bool cached_only,
    const ResultCallback& result_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(backend_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&AffiliationBackend::GetAffiliations,
                 base::Unretained(backend_), facet_uri, cached_only,
                 result_callback, base::ThreadTaskRunnerHandle::Get()));
}

AffiliationService::CancelPrefetchingHandle AffiliationService::Prefetch(
    const FacetURI& facet_uri,
    const base::Time& keep_fresh_until) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(backend_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&AffiliationBackend::Prefetch, base::Unretained(backend_),
                 facet_uri, keep_fresh_until));
  return base::Bind(&AffiliationService::CancelPrefetch,
                    weak_ptr_factory_.GetWeakPtr(), facet_uri,
                    keep_fresh_until);
}

void AffiliationService::CancelPrefetch(const FacetURI& facet_uri,
                                        const base::Time& keep_fresh_until) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(backend_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&AffiliationBackend::CancelPrefetch,
                 base::Unretained(backend_), facet_uri, keep_fresh_until));
}

void AffiliationService::TrimCache() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(backend_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&AffiliationBackend::TrimCache, base::Unretained(backend_)));
}

}  // namespace password_manager
