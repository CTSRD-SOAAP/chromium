// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/scoped_mtp_device_map_entry.h"

#include "base/bind.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_mount_point_provider.h"
#include "chrome/browser/media_galleries/fileapi/mtp_device_map_service.h"
#include "chrome/browser/media_galleries/mtp_device_delegate_impl.h"
#include "content/public/browser/browser_thread.h"

namespace chrome {

namespace {

bool IsMediaTaskRunnerThread() {
  base::SequencedWorkerPool* pool = content::BrowserThread::GetBlockingPool();
  base::SequencedWorkerPool::SequenceToken media_sequence_token =
      pool->GetNamedSequenceToken(
          MediaFileSystemMountPointProvider::kMediaTaskRunnerName);
  return pool->IsRunningSequenceOnCurrentThread(media_sequence_token);
}

scoped_refptr<base::SequencedTaskRunner> GetSequencedTaskRunner() {
  base::SequencedWorkerPool* pool = content::BrowserThread::GetBlockingPool();
  base::SequencedWorkerPool::SequenceToken media_sequence_token =
      pool->GetNamedSequenceToken(
          MediaFileSystemMountPointProvider::kMediaTaskRunnerName);
  return pool->GetSequencedTaskRunner(media_sequence_token);
}

void OnDeviceAsyncDelegateDestroyed(
    const base::FilePath::StringType& device_location) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  MTPDeviceMapService::GetInstance()->RemoveAsyncDelegate(
      device_location);
}

void RemoveDeviceDelegate(const base::FilePath::StringType& device_location) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&OnDeviceAsyncDelegateDestroyed, device_location));
}

}  // namespace

ScopedMTPDeviceMapEntry::ScopedMTPDeviceMapEntry(
    const base::FilePath::StringType& device_location,
    const base::Closure& on_destruction_callback)
    : device_location_(device_location),
      on_destruction_callback_(on_destruction_callback) {
}

void ScopedMTPDeviceMapEntry::Init() {
#if !defined(OS_BSD)
  CreateMTPDeviceAsyncDelegateCallback callback =
      base::Bind(&ScopedMTPDeviceMapEntry::OnMTPDeviceAsyncDelegateCreated,
                 this);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&CreateMTPDeviceAsyncDelegate,
                 device_location_,
                 callback));
#endif
}

ScopedMTPDeviceMapEntry::~ScopedMTPDeviceMapEntry() {
  RemoveDeviceDelegate(device_location_);
  on_destruction_callback_.Run();
}

void ScopedMTPDeviceMapEntry::OnMTPDeviceAsyncDelegateCreated(
    MTPDeviceAsyncDelegate* delegate) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  MTPDeviceMapService::GetInstance()->AddAsyncDelegate(
      device_location_, delegate);
}

}  // namespace chrome
