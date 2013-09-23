// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/media_file_system_mount_point_provider.h"

#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/media_galleries/fileapi/device_media_async_file_util.h"
#include "chrome/browser/media_galleries/fileapi/itunes_file_util.h"
#include "chrome/browser/media_galleries/fileapi/media_file_validator_factory.h"
#include "chrome/browser/media_galleries/fileapi/media_path_filter.h"
#include "chrome/browser/media_galleries/fileapi/native_media_file_util.h"
#include "chrome/browser/media_galleries/fileapi/picasa/picasa_file_util.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/browser/blob/local_file_stream_reader.h"
#include "webkit/browser/fileapi/async_file_util_adapter.h"
#include "webkit/browser/fileapi/copy_or_move_file_validator.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_file_stream_reader.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_task_runners.h"
#include "webkit/browser/fileapi/isolated_context.h"
#include "webkit/browser/fileapi/isolated_file_util.h"
#include "webkit/browser/fileapi/local_file_stream_writer.h"
#include "webkit/browser/fileapi/local_file_system_operation.h"
#include "webkit/browser/fileapi/native_file_util.h"
#include "webkit/common/fileapi/file_system_types.h"
#include "webkit/common/fileapi/file_system_util.h"

using fileapi::FileSystemContext;
using fileapi::FileSystemURL;

namespace chrome {

const char MediaFileSystemMountPointProvider::kMediaTaskRunnerName[] =
    "media-task-runner";
const char MediaFileSystemMountPointProvider::kMediaPathFilterKey[] =
    "MediaPathFilterKey";
const char MediaFileSystemMountPointProvider::kMTPDeviceDelegateURLKey[] =
    "MTPDeviceDelegateKey";

MediaFileSystemMountPointProvider::MediaFileSystemMountPointProvider(
    const base::FilePath& profile_path,
    base::SequencedTaskRunner* media_task_runner)
    : profile_path_(profile_path),
      media_task_runner_(media_task_runner),
      media_path_filter_(new MediaPathFilter),
      media_copy_or_move_file_validator_factory_(new MediaFileValidatorFactory),
      native_media_file_util_(new NativeMediaFileUtil()),
      device_media_async_file_util_(
          DeviceMediaAsyncFileUtil::Create(profile_path_)),
      picasa_file_util_(new picasa::PicasaFileUtil()),
      itunes_file_util_(new itunes::ItunesFileUtil()) {
}

MediaFileSystemMountPointProvider::~MediaFileSystemMountPointProvider() {
}

// static
bool MediaFileSystemMountPointProvider::CurrentlyOnMediaTaskRunnerThread() {
  base::SequencedWorkerPool* pool = content::BrowserThread::GetBlockingPool();
  base::SequencedWorkerPool::SequenceToken media_sequence_token =
      pool->GetNamedSequenceToken(kMediaTaskRunnerName);
  return pool->IsRunningSequenceOnCurrentThread(media_sequence_token);
}

// static
scoped_refptr<base::SequencedTaskRunner>
MediaFileSystemMountPointProvider::MediaTaskRunner() {
  base::SequencedWorkerPool* pool = content::BrowserThread::GetBlockingPool();
  base::SequencedWorkerPool::SequenceToken media_sequence_token =
      pool->GetNamedSequenceToken(kMediaTaskRunnerName);
  return pool->GetSequencedTaskRunner(media_sequence_token);
}

bool MediaFileSystemMountPointProvider::CanHandleType(
    fileapi::FileSystemType type) const {
  switch (type) {
    case fileapi::kFileSystemTypeNativeMedia:
    case fileapi::kFileSystemTypeDeviceMedia:
    case fileapi::kFileSystemTypePicasa:
    case fileapi::kFileSystemTypeItunes:
      return true;
    default:
      return false;
  }
}

void MediaFileSystemMountPointProvider::OpenFileSystem(
    const GURL& origin_url,
    fileapi::FileSystemType type,
    fileapi::OpenFileSystemMode mode,
    const OpenFileSystemCallback& callback) {
  // We never allow opening a new isolated FileSystem via usual OpenFileSystem.
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, base::PLATFORM_FILE_ERROR_SECURITY));
}

fileapi::FileSystemFileUtil* MediaFileSystemMountPointProvider::GetFileUtil(
    fileapi::FileSystemType type) {
  NOTREACHED();
  return NULL;
}

fileapi::AsyncFileUtil* MediaFileSystemMountPointProvider::GetAsyncFileUtil(
    fileapi::FileSystemType type) {
  switch (type) {
    case fileapi::kFileSystemTypeNativeMedia:
      return native_media_file_util_.get();
    case fileapi::kFileSystemTypePicasa:
      return picasa_file_util_.get();
    case fileapi::kFileSystemTypeDeviceMedia:
      return device_media_async_file_util_.get();
    case fileapi::kFileSystemTypeItunes:
      return itunes_file_util_.get();
    default:
      NOTREACHED();
  }
  return NULL;
}

fileapi::CopyOrMoveFileValidatorFactory*
MediaFileSystemMountPointProvider::GetCopyOrMoveFileValidatorFactory(
    fileapi::FileSystemType type, base::PlatformFileError* error_code) {
  DCHECK(error_code);
  *error_code = base::PLATFORM_FILE_OK;
  switch (type) {
    case fileapi::kFileSystemTypeNativeMedia:
    case fileapi::kFileSystemTypeDeviceMedia:
    case fileapi::kFileSystemTypeItunes:
      if (!media_copy_or_move_file_validator_factory_) {
        *error_code = base::PLATFORM_FILE_ERROR_SECURITY;
        return NULL;
      }
      return media_copy_or_move_file_validator_factory_.get();
    default:
      NOTREACHED();
  }
  return NULL;
}

fileapi::FilePermissionPolicy
MediaFileSystemMountPointProvider::GetPermissionPolicy(
    const FileSystemURL& url, int permissions) const {
  // Access to media file systems should be checked using per-filesystem
  // access permission.
  return fileapi::FILE_PERMISSION_USE_FILESYSTEM_PERMISSION;
}

fileapi::FileSystemOperation*
MediaFileSystemMountPointProvider::CreateFileSystemOperation(
    const FileSystemURL& url,
    FileSystemContext* context,
    base::PlatformFileError* error_code) const {
  scoped_ptr<fileapi::FileSystemOperationContext> operation_context(
      new fileapi::FileSystemOperationContext(
          context, media_task_runner_.get()));

  operation_context->SetUserValue(kMediaPathFilterKey,
                                  media_path_filter_.get());
  if (url.type() == fileapi::kFileSystemTypeDeviceMedia) {
    operation_context->SetUserValue(kMTPDeviceDelegateURLKey,
                                    url.filesystem_id());
  }

  return new fileapi::LocalFileSystemOperation(url, context,
                                               operation_context.Pass());
}

scoped_ptr<webkit_blob::FileStreamReader>
MediaFileSystemMountPointProvider::CreateFileStreamReader(
    const FileSystemURL& url,
    int64 offset,
    const base::Time& expected_modification_time,
    FileSystemContext* context) const {
  return scoped_ptr<webkit_blob::FileStreamReader>(
      new webkit_blob::LocalFileStreamReader(
          context->task_runners()->file_task_runner(),
          url.path(), offset, expected_modification_time));
}

scoped_ptr<fileapi::FileStreamWriter>
MediaFileSystemMountPointProvider::CreateFileStreamWriter(
    const FileSystemURL& url,
    int64 offset,
    FileSystemContext* context) const {
  return scoped_ptr<fileapi::FileStreamWriter>(
      new fileapi::LocalFileStreamWriter(
          context->task_runners()->file_task_runner(),
          url.path(), offset));
}

fileapi::FileSystemQuotaUtil*
MediaFileSystemMountPointProvider::GetQuotaUtil() {
  // No quota support.
  return NULL;
}

void MediaFileSystemMountPointProvider::DeleteFileSystem(
    const GURL& origin_url,
    fileapi::FileSystemType type,
    FileSystemContext* context,
    const DeleteFileSystemCallback& callback) {
  NOTREACHED();
  callback.Run(base::PLATFORM_FILE_ERROR_INVALID_OPERATION);
}

}  // namespace chrome
