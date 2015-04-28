// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Namespace
var importer = importer || {};

/**
 * Interface providing access to information about active import processes.
 *
 * @interface
 */
importer.ImportRunner = function() {};

/**
 * Imports all media identified by scanResult.
 *
 * @param {!importer.ScanResult} scanResult
 * @param {!importer.Destination} destination
 * @param {!Promise<!DirectoryEntry>} directoryPromise
 *
 * @return {!importer.MediaImportHandler.ImportTask} The resulting import task.
 */
importer.ImportRunner.prototype.importFromScanResult;

/**
 * Handler for importing media from removable devices into the user's Drive.
 *
 * @constructor
 * @implements {importer.ImportRunner}
 * @struct
 *
 * @param {!ProgressCenter} progressCenter
 * @param {!importer.HistoryLoader} historyLoader
 * @param {!importer.DuplicateFinder.Factory} duplicateFinderFactory
 * @param {!analytics.Tracker} tracker
 */
importer.MediaImportHandler =
    function(progressCenter, historyLoader, duplicateFinderFactory, tracker) {
  /** @private {!ProgressCenter} */
  this.progressCenter_ = progressCenter;

  /** @private {!importer.HistoryLoader} */
  this.historyLoader_ = historyLoader;

  /** @private {!importer.TaskQueue} */
  this.queue_ = new importer.TaskQueue();

  // Prevent the system from sleeping while imports are active.
  this.queue_.setActiveCallback(function() {
    chrome.power.requestKeepAwake('system');
  });
  this.queue_.setIdleCallback(function() {
    chrome.power.releaseKeepAwake();
  });

  /** @private {!importer.DuplicateFinder.Factory} */
  this.duplicateFinderFactory_ = duplicateFinderFactory;

  /** @private {!analytics.Tracker} */
  this.tracker_ = tracker;

  /** @private {number} */
  this.nextTaskId_ = 0;
};

/** @override */
importer.MediaImportHandler.prototype.importFromScanResult =
    function(scanResult, destination, directoryPromise) {

  var task = new importer.MediaImportHandler.ImportTask(
      this.generateTaskId_(),
      this.historyLoader_,
      scanResult,
      directoryPromise,
      this.duplicateFinderFactory_.create(),
      destination,
      this.tracker_);

  task.addObserver(this.onTaskProgress_.bind(this, task));

  this.queue_.queueTask(task);

  return task;
};

/**
 * Generates unique task IDs.
 * @private
 */
importer.MediaImportHandler.prototype.generateTaskId_ = function() {
  return 'media-import' + this.nextTaskId_++;
};

/**
 * Sends updates to the ProgressCenter when an import is happening.
 *
 * @param {!importer.TaskQueue.Task} task
 * @param {string} updateType
 * @private
 */
importer.MediaImportHandler.prototype.onTaskProgress_ =
    function(task, updateType) {
  var UpdateType = importer.TaskQueue.UpdateType;

  var item = this.progressCenter_.getItemById(task.taskId);
  if (!item) {
    item = new ProgressCenterItem();
    item.id = task.taskId;
    // TODO(kenobi): Might need a different progress item type here.
    item.type = ProgressItemType.COPY;
    item.progressMax = task.totalBytes;
    item.cancelCallback = function() {
      task.requestCancel();
    };
  }

  switch (updateType) {
    case UpdateType.PROGRESS:
      item.message =
          strf('CLOUD_IMPORT_ITEMS_REMAINING', task.remainingFilesCount);
      item.progressValue = task.processedBytes;
      item.state = ProgressItemState.PROGRESSING;
    break;
    case UpdateType.COMPLETE:
      item.message = '';
      item.progressValue = item.progressMax;
      item.state = ProgressItemState.COMPLETED;
      break;
    case UpdateType.ERROR:
      item.message =
          strf('CLOUD_IMPORT_ITEMS_REMAINING', task.remainingFilesCount);
      item.progressValue = task.processedBytes;
      item.state = ProgressItemState.ERROR;
      break;
    case UpdateType.CANCELED:
      item.message = '';
      item.state = ProgressItemState.CANCELED;
      break;
  }

  this.progressCenter_.updateItem(item);
};

/**
 * Note that this isn't an actual FileOperationManager.Task.  It currently uses
 * the FileOperationManager (and thus *spawns* an associated
 * FileOperationManager.CopyTask) but this is a temporary state of affairs.
 *
 * @constructor
 * @extends {importer.TaskQueue.BaseTask}
 * @struct
 *
 * @param {string} taskId
 * @param {!importer.HistoryLoader} historyLoader
 * @param {!importer.ScanResult} scanResult
 * @param {!Promise<!DirectoryEntry>} directoryPromise
 * @param {!importer.DuplicateFinder} duplicateFinder A duplicate-finder linked
 *     to the import destination, that will be used to deduplicate imports.
 * @param {!importer.Destination} destination The logical destination.
 * @param {!analytics.Tracker} tracker
 */
importer.MediaImportHandler.ImportTask = function(
    taskId,
    historyLoader,
    scanResult,
    directoryPromise,
    duplicateFinder,
    destination,
    tracker) {

  importer.TaskQueue.BaseTask.call(this, taskId);
  /** @private {string} */
  this.taskId_ = taskId;

  /** @private {!importer.Destination} */
  this.destination_ = destination;

  /** @private {!Promise<!DirectoryEntry>} */
  this.directoryPromise_ = directoryPromise;

  /** @private {!importer.DuplicateFinder} */
  this.deduplicator_ = duplicateFinder;

  /** @private {!importer.ScanResult} */
  this.scanResult_ = scanResult;

  /** @private {!importer.HistoryLoader} */
  this.historyLoader_ = historyLoader;

  /** @private {!analytics.Tracker} */
  this.tracker_ = tracker;

  /** @private {number} */
  this.totalBytes_ = 0;

  /** @private {number} */
  this.processedBytes_ = 0;

  /** @private {number} */
  this.remainingFilesCount_ = 0;

  /** @private {?function()} */
  this.cancelCallback_ = null;

  /** @private {boolean} Indicates whether this task was canceled. */
  this.canceled_ = false;

  /** @private {number} Number of files deduped by content dedupe. */
  this.dedupeCount_ = 0;

  /** @private {number} */
  this.errorCount_ = 0;
};

/** @struct */
importer.MediaImportHandler.ImportTask.prototype = {
  /** @return {number} Number of imported bytes */
  get processedBytes() { return this.processedBytes_; },

  /** @return {number} Total number of bytes to import */
  get totalBytes() { return this.totalBytes_; },

  /** @return {number} Number of files left to import */
  get remainingFilesCount() { return this.remainingFilesCount_; }
};

/**
 * Update types that are specific to ImportTask.  Clients can add Observers to
 * ImportTask to listen for these kinds of updates.
 * @enum {string}
 */
importer.MediaImportHandler.ImportTask.UpdateType = {
  ENTRY_CHANGED: 'ENTRY_CHANGED'
};

/**
 * Auxilliary info for ENTRY_CHANGED notifications.
 * @typedef {{
 *   sourceUrl: string,
 *   destination: !Entry
 * }}
 */
importer.MediaImportHandler.ImportTask.EntryChangedInfo;

/**
 * Extends importer.TaskQueue.Task
 */
importer.MediaImportHandler.ImportTask.prototype.__proto__ =
    importer.TaskQueue.BaseTask.prototype;

/** @override */
importer.MediaImportHandler.ImportTask.prototype.run = function() {
  // Wait for the scan to finish, then get the destination entry, then start the
  // import.
  this.scanResult_.whenFinal()
      .then(this.initialize_.bind(this))
      .then(this.importScanEntries_.bind(this))
      .catch(importer.getLogger().catcher('import-task-run'));
};

/**
 * Request cancellation of this task.  An update will be sent to observers once
 * the task is actually cancelled.
 */
importer.MediaImportHandler.ImportTask.prototype.requestCancel = function() {
  this.canceled_ = true;
  if (this.cancelCallback_) {
    // Reset the callback before calling it, as the callback might do anything
    // (including calling #requestCancel again).
    var cancelCallback = this.cancelCallback_;
    this.cancelCallback_ = null;
    cancelCallback();
  }
};

/** @private */
importer.MediaImportHandler.ImportTask.prototype.initialize_ = function() {
  var stats = this.scanResult_.getStatistics();

  this.remainingFilesCount_ = stats.newFileCount;
  this.totalBytes_ = stats.sizeBytes;
  this.notify(importer.TaskQueue.UpdateType.PROGRESS);

  this.tracker_.send(metrics.ImportEvents.STARTED);
  this.tracker_.send(metrics.ImportEvents.HISTORY_DEDUPE_COUNT
                     .value(stats.duplicateFileCount));
};

/**
 * Initiates an import to the given location.  This should only be called once
 * the scan result indicates that it is ready.
 *
 * @private
 */
importer.MediaImportHandler.ImportTask.prototype.importScanEntries_ =
    function() {
  this.directoryPromise_.then(
      /** @this {importer.MediaImportHandler.ImportTask} */
      function(destinationDirectory) {
        AsyncUtil.forEach(
            this.scanResult_.getFileEntries(),
            this.importOne_.bind(this, destinationDirectory),
            this.onSuccess_.bind(this));
      }.bind(this));
};

/**
 * @param {!DirectoryEntry} destinationDirectory
 * @param {function()} completionCallback Called after this operation is
 *     complete.
 * @param {!FileEntry} entry The entry to import.
 * @private
 */
importer.MediaImportHandler.ImportTask.prototype.importOne_ =
    function(destinationDirectory, completionCallback, entry) {
  if (this.canceled_) {
    this.notify(importer.TaskQueue.UpdateType.CANCELED);
    this.tracker_.send(metrics.ImportEvents.CANCELLED);
    this.sendImportStats_();
    return;
  }

  this.deduplicator_.checkDuplicate(entry)
      .then(
          /** @param {boolean} isDuplicate */
          function(isDuplicate) {
            if (isDuplicate) {
              // If the given file is a duplicate, don't import it again.  Just
              // update the progress indicator.
              this.dedupeCount_++;
              this.markAsImported_(entry);
              this.processedBytes_ += entry.size;
              this.notify(importer.TaskQueue.UpdateType.PROGRESS);
              return Promise.resolve();
            } else {
              return this.copy_(entry, destinationDirectory);
            }
          }.bind(this))
      // Regardless of the result of this copy, push on to the next file.
      .then(completionCallback)
      .catch(
          /** @param {*} error */
          function(error) {
            importer.getLogger().catcher('import-task-import-one')(error);
            completionCallback();
          });
};

/**
 * @param {!FileEntry} entry The file to copy.
 * @param {!DirectoryEntry} destinationDirectory The destination directory.
 * @return {!Promise<!FileEntry>} Resolves to the destination file when the copy
 *     is complete.
 * @private
 */
importer.MediaImportHandler.ImportTask.prototype.copy_ =
    function(entry, destinationDirectory) {
  // A count of the current number of processed bytes for this entry.
  var currentBytes = 0;

  var resolver = new importer.Resolver();

  /**
   * Updates the task when the copy code reports progress.
   * @param {string} sourceUrl
   * @param {number} processedBytes
   * @this {importer.MediaImportHandler.ImportTask}
   */
  var onProgress = function(sourceUrl, processedBytes) {
    // Update the running total, then send a progress update.
    this.processedBytes_ -= currentBytes;
    this.processedBytes_ += processedBytes;
    currentBytes = processedBytes;
    this.notify(importer.TaskQueue.UpdateType.PROGRESS);
  };

  /**
   * Updates the task when the new file has been created.
   * @param {string} sourceUrl
   * @param {Entry} destinationEntry
   * @this {importer.MediaImportHandler.ImportTask}
   */
  var onEntryChanged = function(sourceUrl, destinationEntry) {
    this.processedBytes_ -= currentBytes;
    this.processedBytes_ += entry.size;
    destinationEntry.size = entry.size;
    this.notify(
        importer.MediaImportHandler.ImportTask.UpdateType.ENTRY_CHANGED,
        {
          sourceUrl: sourceUrl,
          destination: destinationEntry
        });
    this.notify(importer.TaskQueue.UpdateType.PROGRESS);
  };

  /**
   * @param {Entry} destinationEntry The new destination entry.
   * @this {importer.MediaImportHandler.ImportTask}
   */
  var onComplete = function(destinationEntry) {
    this.cancelCallback_ = null;
    this.markAsCopied_(entry, destinationEntry);
    this.notify(importer.TaskQueue.UpdateType.PROGRESS);
    resolver.resolve(destinationEntry);
  };

  /** @this {importer.MediaImportHandler.ImportTask} */
  var onError = function(error) {
    this.cancelCallback_ = null;
    this.errorCount_++;
    // Log the bytes as processed in spite of the error.  This ensures
    // completion of the progress bar.
    this.processedBytes_ -= currentBytes;
    this.processedBytes_ += entry.size;
    this.notify(importer.TaskQueue.UpdateType.ERROR);
    resolver.reject(error);
  };

  fileOperationUtil.deduplicatePath(destinationDirectory, entry.name)
      .then(
          /**
           * Performs the copy using the given deduped filename.
           * @param {string} destinationFilename
           * @this {importer.MediaImportHandler.ImportTask}
           */
          function(destinationFilename) {
            this.cancelCallback_ = fileOperationUtil.copyTo(
                entry,
                destinationDirectory,
                destinationFilename,
                onEntryChanged.bind(this),
                onProgress.bind(this),
                onComplete.bind(this),
                onError.bind(this));
          }.bind(this),
          resolver.reject)
      .catch(importer.getLogger().catcher('import-task-copy'));

  return resolver.promise;
};

/**
 * @param {!FileEntry} entry
 * @param {!FileEntry} destinationEntry
 */
importer.MediaImportHandler.ImportTask.prototype.markAsCopied_ =
    function(entry, destinationEntry) {
  this.remainingFilesCount_--;
  this.historyLoader_.getHistory().then(
      /**
       * @param {!importer.ImportHistory} history
       * @this {importer.MediaImportHandler.ImportTask}
       */
      function(history) {
        history.markCopied(
            entry,
            this.destination_,
            destinationEntry.toURL());
      }.bind(this))
      .catch(importer.getLogger().catcher('import-task-mark-as-copied'));
};

/**
 * @param {!FileEntry} entry
 * @private
 */
importer.MediaImportHandler.ImportTask.prototype.markAsImported_ =
    function(entry) {
  this.remainingFilesCount_--;
  this.historyLoader_.getHistory().then(
      /** @param {!importer.ImportHistory} history */
      function(history) {
        history.markImported(entry, this.destination_);
      }.bind(this))
      .catch(importer.getLogger().catcher('import-task-mark-as-imported'));
};

/** @private */
importer.MediaImportHandler.ImportTask.prototype.onSuccess_ = function() {
  this.notify(importer.TaskQueue.UpdateType.COMPLETE);
  this.tracker_.send(metrics.ImportEvents.ENDED);
  this.sendImportStats_();
};

/**
 * Sends import statistics to analytics.
 */
importer.MediaImportHandler.ImportTask.prototype.sendImportStats_ = function() {
  this.tracker_.send(
      metrics.ImportEvents.CONTENT_DEDUPE_COUNT
          .value(this.dedupeCount_));
  // TODO(kenobi): Send correct import byte counts.
  var importFileCount = this.scanResult_.getStatistics().newFileCount -
      (this.dedupeCount_ + this.remainingFilesCount_);
  this.tracker_.send(
      metrics.ImportEvents.FILE_COUNT
          .value(importFileCount));

  this.tracker_.send(metrics.ImportEvents.ERROR.value(this.errorCount_));

  // Send aggregate deduplication timings, to avoid flooding analytics with one
  // timing per file.
  var deduplicatorStats = this.deduplicator_.getStatistics();
  this.tracker_.sendTiming(
      metrics.Categories.ACQUISITION,
      metrics.timing.Variables.COMPUTE_HASH,
      deduplicatorStats.computeHashTime,
      'In Place');
  this.tracker_.sendTiming(
      metrics.Categories.ACQUISITION,
      metrics.timing.Variables.SEARCH_BY_HASH,
      deduplicatorStats.searchHashTime);

};
