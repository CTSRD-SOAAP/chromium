// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_metrics.h"

#include "base/metrics/histogram.h"
#include "base/metrics/user_metrics_action.h"
#include "content/public/browser/user_metrics.h"

namespace content {

void ServiceWorkerMetrics::CountInitDiskCacheResult(bool result) {
  UMA_HISTOGRAM_BOOLEAN("ServiceWorker.DiskCache.InitResult", result);
}

void ServiceWorkerMetrics::CountReadResponseResult(
    ServiceWorkerMetrics::ReadResponseResult result) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.DiskCache.ReadResponseResult",
                            result, NUM_READ_RESPONSE_RESULT_TYPES);
}

void ServiceWorkerMetrics::CountWriteResponseResult(
    ServiceWorkerMetrics::WriteResponseResult result) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.DiskCache.WriteResponseResult",
                            result, NUM_WRITE_RESPONSE_RESULT_TYPES);
}

void ServiceWorkerMetrics::CountOpenDatabaseResult(
    ServiceWorkerDatabase::Status status) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.Database.OpenResult",
                            status, ServiceWorkerDatabase::STATUS_ERROR_MAX);
}

void ServiceWorkerMetrics::CountReadDatabaseResult(
    ServiceWorkerDatabase::Status status) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.Database.ReadResult",
                            status, ServiceWorkerDatabase::STATUS_ERROR_MAX);
}

void ServiceWorkerMetrics::CountWriteDatabaseResult(
    ServiceWorkerDatabase::Status status) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.Database.WriteResult",
                            status, ServiceWorkerDatabase::STATUS_ERROR_MAX);
}

void ServiceWorkerMetrics::RecordDestroyDatabaseResult(
    ServiceWorkerDatabase::Status status) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.Database.DestroyDatabaseResult",
                            status, ServiceWorkerDatabase::STATUS_ERROR_MAX);
}

void ServiceWorkerMetrics::RecordDeleteAndStartOverResult(
    DeleteAndStartOverResult result) {
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.Storage.DeleteAndStartOverResult",
                            result, NUM_DELETE_AND_START_OVER_RESULT_TYPES);
}

void ServiceWorkerMetrics::CountControlledPageLoad() {
  RecordAction(base::UserMetricsAction("ServiceWorker.ControlledPageLoad"));
}

}  // namespace content
