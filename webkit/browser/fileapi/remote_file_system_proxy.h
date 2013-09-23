// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_REMOTE_FILE_SYSTEM_PROXY_H_
#define WEBKIT_BROWSER_FILEAPI_REMOTE_FILE_SYSTEM_PROXY_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "webkit/browser/fileapi/file_system_operation.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace webkit_blob {
class FileStreamReader;
}  // namespace webkit_blob

namespace fileapi {

typedef base::Callback<
    void(base::PlatformFileError result,
         const base::FilePath& platform_path,
         const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref)>
    WritableSnapshotFile;

// The interface class for remote file system proxy.
class RemoteFileSystemProxyInterface :
    public base::RefCountedThreadSafe<RemoteFileSystemProxyInterface> {
 public:
  // Used for OpenFile(). |result| is the return code of the operation.
  typedef base::Callback<
      void(base::PlatformFileError result,
           base::PlatformFile file,
           base::ProcessHandle peer_handle)> OpenFileCallback;


  // Gets the file or directory info for given|path|.
  virtual void GetFileInfo(const FileSystemURL& url,
      const FileSystemOperation::GetMetadataCallback& callback) = 0;

  // Copies a file or directory from |src_url| to |dest_url|. If
  // |src_url| is a directory, the contents of |src_url| are copied to
  // |dest_url| recursively. A new file or directory is created at
  // |dest_url| as needed.
  virtual void Copy(
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
      const FileSystemOperation::StatusCallback& callback) = 0;

  // Moves a file or directory from |src_url| to |dest_url|. A new file
  // or directory is created at |dest_url| as needed.
  virtual void Move(
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
      const FileSystemOperation::StatusCallback& callback) = 0;

  // Reads contents of a directory at |url|.
  virtual void ReadDirectory(const FileSystemURL& url,
      const FileSystemOperation::ReadDirectoryCallback& callback) = 0;

  // Removes a file or directory at |url|. If |recursive| is true, remove
  // all files and directories under the directory at |url| recursively.
  virtual void Remove(const FileSystemURL& url, bool recursive,
      const FileSystemOperation::StatusCallback& callback) = 0;

  // Creates a directory at |url|. If |exclusive| is true, an error is
  // raised in case a directory is already present at the URL. If
  // |recursive| is true, create parent directories as needed just like
  // mkdir -p does.
  virtual void CreateDirectory(
      const FileSystemURL& url,
      bool exclusive,
      bool recursive,
      const FileSystemOperation::StatusCallback& callback) = 0;

  // Creates a file at |url|. If the flag |is_exclusive| is true, an
  // error is raised when a file already exists at the path. It is
  // an error if a directory or a hosted document is already present at the
  // path, or the parent directory of the path is not present yet.
  virtual void CreateFile(
      const FileSystemURL& url,
      bool exclusive,
      const FileSystemOperation::StatusCallback& callback) = 0;

  // Changes the length of an existing file at |url| to |length|. If |length|
  // is negative, an error is raised. If |length| is more than the current size
  // of the file, zero is padded for the extended part.
  virtual void Truncate(
      const FileSystemURL& url,
      int64 length,
      const FileSystemOperation::StatusCallback& callback) = 0;

  // Creates a local snapshot file for a given |url| and returns the
  // metadata and platform path of the snapshot file via |callback|.
  // See also FileSystemOperation::CreateSnapshotFile().
  virtual void CreateSnapshotFile(
      const FileSystemURL& url,
      const FileSystemOperation::SnapshotFileCallback& callback) = 0;

  // Creates a local snapshot file for a given |url| and marks it for
  // modification. A webkit_blob::ShareableFileReference is passed to
  // |callback|, and when the reference is released, modification to the
  // snapshot is marked for uploading to the remote file system.
  virtual void CreateWritableSnapshotFile(
      const FileSystemURL& url,
      const WritableSnapshotFile& callback) = 0;

  // Opens file for a given |url| with specified |flags| (see
  // base::PlatformFileFlags for details).
  virtual void OpenFile(
      const FileSystemURL& url,
      int flags,
      base::ProcessHandle peer_handle,
      const OpenFileCallback& callback) = 0;

  // Notifies that a file opened by OpenFile (at |path|) is closed.
  virtual void NotifyCloseFile(const FileSystemURL& url) = 0;

  // Modifies the timestamp of a given |url| to |last_access_time| and
  // |last_modified_time|. Note that unlike 'touch' command of Linux, it
  // does not create a new file.
  virtual void TouchFile(
      const FileSystemURL& url,
      const base::Time& last_access_time,
      const base::Time& last_modified_time,
      const FileSystemOperation::StatusCallback& callback) = 0;

  // Creates a new file stream reader for the file at |url| with an |offset|.
  // |expected_modification_time| specifies the expected last modification
  // if it isn't null, and the reader will return ERR_UPLOAD_FILE_CHANGED error
  // if the file has been modified.
  // The error will be notified via error code of FileStreamReader's methods,
  // and this method itself doesn't check if the file exists and is a regular
  // file.
  virtual scoped_ptr<webkit_blob::FileStreamReader> CreateFileStreamReader(
      base::SequencedTaskRunner* file_task_runner,
      const FileSystemURL& url,
      int64 offset,
      const base::Time& expected_modification_time) = 0;

 protected:
  friend class base::RefCountedThreadSafe<RemoteFileSystemProxyInterface>;
  virtual ~RemoteFileSystemProxyInterface() {}
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_REMOTE_FILE_SYSTEM_PROXY_H_
