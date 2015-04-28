// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Shared cloud importer namespace
var importer = importer || {};

/** @enum {string} */
importer.ScanEvent = {
  FINALIZED: 'finalized',
  INVALIDATED: 'invalidated',
  UPDATED: 'updated'
};

/**
 * Storage keys for settings saved by importer.
 * @enum {string}
 */
importer.Setting = {
  HAS_COMPLETED_IMPORT: 'importer-has-completed-import',
  MACHINE_ID: 'importer-machine-id',
  PHOTOS_APP_ENABLED: 'importer-photo-app-enabled',
  LAST_KNOWN_LOG_ID: 'importer-last-known-log-id'
};

/**
 * @typedef {function(
 *     !importer.ScanEvent, importer.ScanResult)}
 */
importer.ScanObserver;

/**
 * Volume types eligible for the affections of Cloud Import.
 * @private @const {!Array.<!VolumeManagerCommon.VolumeType>}
 */
importer.ELIGIBLE_VOLUME_TYPES_ = [
  VolumeManagerCommon.VolumeType.MTP,
  VolumeManagerCommon.VolumeType.REMOVABLE
];

/**
 * @enum {string}
 */
importer.Destination = {
  // locally copied, but not imported to cloud as of yet.
  DEVICE: 'device',
  GOOGLE_DRIVE: 'google-drive'
};

/**
 * Returns true if the entry is a media file (and a descendant of a DCIM dir).
 *
 * @param {Entry} entry
 * @return {boolean}
 */
importer.isMediaEntry = function(entry) {
  return !!entry &&
      entry.isFile &&
      FileType.isImageOrVideo(entry) &&
      importer.isBeneathMediaDir(entry);
};

/**
 * Returns true if the entry is a media file (and a descendant of a DCIM dir).
 *
 * @param {Entry} entry
 * @return {boolean}
 */
importer.isBeneathMediaDir = function(entry) {
  var path = entry.fullPath.toUpperCase();
  return path.indexOf('/DCIM/') === 0 ||
      path.indexOf('/MISSINGNO/') >= 0;
};

/**
 * Returns true if the volume is eligible for Cloud Import.
 *
 * @param {VolumeInfo} volumeInfo
 * @return {boolean}
 */
importer.isEligibleVolume = function(volumeInfo) {
  return !!volumeInfo &&
      importer.ELIGIBLE_VOLUME_TYPES_.indexOf(volumeInfo.volumeType) !== -1;
};

/**
 * Returns true if the entry is cloud import eligible.
 *
 * @param {VolumeManagerCommon.VolumeInfoProvider} volumeInfoProvider
 * @param {Entry} entry
 * @return {boolean}
 */
importer.isEligibleEntry = function(volumeInfoProvider, entry) {
  console.assert(volumeInfoProvider !== null);
  if (importer.isMediaEntry(entry)) {
    // MissingNo knows no bounds....like volume type checks.
    if (entry.fullPath.toUpperCase().indexOf('/MISSINGNO/') >= 0) {
      return true;
    } else {
      var volumeInfo = volumeInfoProvider.getVolumeInfo(entry);
      return importer.isEligibleVolume(volumeInfo);
    }
  }
  return false;
};

/**
 * Returns true if the entry represents a media directory for the purposes
 * of Cloud Import.
 *
 * @param {Entry} entry
 * @param  {VolumeManagerCommon.VolumeInfoProvider} volumeInfoProvider
 * @return {boolean}
 */
importer.isMediaDirectory = function(entry, volumeInfoProvider) {
  if (!entry || !entry.isDirectory || !entry.fullPath) {
    return false;
  }

  var path = entry.fullPath.toUpperCase();
  if (path.indexOf('/MISSINGNO') !== -1) {
    return true;
  } else if (path !== '/DCIM' && path !== '/DCIM/') {
    return false;
  }

  console.assert(volumeInfoProvider !== null);
  var volumeInfo = volumeInfoProvider.getVolumeInfo(entry);
  return importer.isEligibleVolume(volumeInfo);
};

/**
 * @return {!Promise.<boolean>} Resolves with true when Cloud Import feature
 *     is enabled.
 */
importer.importEnabled = function() {
  return new Promise(
      function(resolve, reject) {
        chrome.commandLinePrivate.hasSwitch(
            'disable-cloud-import',
            /** @param {boolean} disabled */
            function(disabled) {
              // TODO(smckay): For M42 only, we dropped the ball on
              // decent RTL formatting. Disable it for the duration of M42 only.
              var rtl = ['ar', 'iw', 'he', 'fa'].indexOf(
                  chrome.i18n.getUILanguage().toLowerCase()) !== -1;
              resolve(!disabled && !rtl);
            });
      });
};

/**
 * Handles a message from Pulsar...in which we presume we are being
 * informed of its "Automatically import stuff." state.
 *
 * While the runtime message system is loosey goosey about types,
 * we fully expect message to be a boolean value.
 *
 * @param {*} message
 *
 * @return {!Promise} Resolves once the message has been handled.
 */
importer.handlePhotosAppMessage = function(message) {
  if (typeof message !== 'boolean') {
    console.error(
        'Unrecognized message type received from photos app: ' + message);
    return Promise.reject();
  }

  var storage = importer.ChromeLocalStorage.getInstance();
  return storage.set(importer.Setting.PHOTOS_APP_ENABLED, message);
};

/**
 * @return {!Promise.<boolean>} Resolves with true when Cloud Import feature
 *     is enabled.
 */
importer.isPhotosAppImportEnabled = function() {
  var storage = importer.ChromeLocalStorage.getInstance();
  return storage.get(importer.Setting.PHOTOS_APP_ENABLED, false);
};

/**
 * @param {!Date} date
 * @return {string} The current date, in YYYY-MM-DD format.
 */
importer.getDirectoryNameForDate = function(date) {
  var padAndConvert = function(i) {
    return (i < 10 ? '0' : '') + i.toString();
  };

  var year = date.getFullYear().toString();
  // Months are 0-based, but days aren't.
  var month = padAndConvert(date.getMonth() + 1);
  var day = padAndConvert(date.getDate());

  // NOTE: We use YYYY-MM-DD since it sorts numerically.
  // Ideally this would be localized and appropriate sorting would
  // be done behind the scenes.
  return year + '-' + month + '-' + day;
};

/**
 * @return {!Promise.<number>} Resolves with an integer that is probably
 *     relatively unique to this machine (among a users machines).
 */
importer.getMachineId = function() {
  var storage = importer.ChromeLocalStorage.getInstance();
  return storage.get(importer.Setting.MACHINE_ID)
      .then(
          function(id) {
            if (id) {
              return id;
            }
            var id = importer.generateMachineId_();
            return storage.set(importer.Setting.MACHINE_ID, id)
                .then(
                    function() {
                      return id;
                    });
          });
};

/**
 * @return {!Promise.<string>} Resolves with the filename of this
 *     machines history file.
 */
importer.getHistoryFilename = function() {
  return importer.getMachineId().then(
      function(machineId) {
        return machineId + '-import-history.log';
      });
};

/**
 * @param {number} logId
 * @return {!Promise.<string>} Resolves with the filename of this
 *     machines debug log file.
 */
importer.getDebugLogFilename = function(logId) {
  return importer.getMachineId().then(
      function(machineId) {
        return machineId + '-import-debug-' + logId + '.log';
      });
};

/**
 * @return {number} A relatively unique six digit integer that is most likely
 *     unique to this machine among a user's machines. Used only to segregate
 *     log files on sync storage.
 */
importer.generateMachineId_ = function() {
  return Math.floor(Math.random() * 899999) + 100000;
};

/**
 * A Promise wrapper that provides public access to resolve and reject methods.
 *
 * @constructor
 * @struct
 * @template T
 */
importer.Resolver = function() {
  /** @private {boolean} */
  this.settled_ = false;

  /** @private {function(T=)} */
  this.resolve_;

  /** @private {function(*=)} */
  this.reject_;

  /** @private {!Promise.<T>} */
  this.promise_ = new Promise(
      function(resolve, reject) {
        this.resolve_ = resolve;
        this.reject_ = reject;
      }.bind(this));

  var settler = function() {
    this.settled_ = true;
  }.bind(this);

  this.promise_.then(settler, settler);
};

importer.Resolver.prototype = /** @struct */ {
  /**
   * @return {function(T=)}
   * @template T
   */
  get resolve() {
    return this.resolve_;
  },
  /**
   * @return {function(*=)}
   * @template T
   */
  get reject() {
    return this.reject_;
  },
  /**
   * @return {!Promise.<T>}
   * @template T
   */
  get promise() {
    return this.promise_;
  },
  /** @return {boolean} */
  get settled() {
    return this.settled_;
  }
};

/**
 * Returns the directory, creating it if necessary.
 *
 * @param {!DirectoryEntry} parent
 * @param {string} name
 *
 * @return {!Promise<!DirectoryEntry>}
 */
importer.demandChildDirectory = function(parent, name) {
  return new Promise(
      function(resolve, reject) {
        parent.getDirectory(
            name,
            {
              create: true,
              exclusive: false
            },
            resolve,
            reject);
      });
};

/**
 * A wrapper for FileEntry that provides Promises.
 *
 * @constructor
 * @struct
 *
 * @param {!FileEntry} fileEntry
 */
importer.PromisingFileEntry = function(fileEntry) {
  /** @private {!FileEntry} */
  this.fileEntry_ = fileEntry;
};

/**
 * A "Promisary" wrapper around entry.getWriter.
 * @return {!Promise.<!FileWriter>}
 */
importer.PromisingFileEntry.prototype.createWriter = function() {
  return new Promise(this.fileEntry_.createWriter.bind(this.fileEntry_));
};

/**
 * A "Promisary" wrapper around entry.file.
 * @return {!Promise.<!File>}
 */
importer.PromisingFileEntry.prototype.file = function() {
  return new Promise(this.fileEntry_.file.bind(this.fileEntry_));
};

/**
 * @return {!Promise.<!Object>}
 */
importer.PromisingFileEntry.prototype.getMetadata = function() {
  return new Promise(this.fileEntry_.getMetadata.bind(this.fileEntry_));
};

/**
 * This prefix is stripped from URL used in import history. It is stripped
 * to same on disk space, parsing time, and runtime memory.
 * @private @const {string}
 */
importer.APP_URL_PREFIX_ =
    'filesystem:chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/external';

/**
 * Strips non-unique information from the URL. The resulting
 * value can be reconstituted using {@code importer.inflateAppUrl}.
 *
 * @param {string} url
 * @return {string}
 */
importer.deflateAppUrl = function(url) {
  if (url.substring(0, importer.APP_URL_PREFIX_.length) ===
      importer.APP_URL_PREFIX_) {
    return '$' + url.substring(importer.APP_URL_PREFIX_.length);
  }

  return url;
};

/**
 * Reconstitutes a url previous deflated by {@code deflateAppUrl}.
 * Returns the original string if it can't be inflated.
 *
 * @param {string} deflated
 * @return {string}
 */
importer.inflateAppUrl = function(deflated) {
  if (deflated.substring(0, 1) === '$') {
    return importer.APP_URL_PREFIX_ + deflated.substring(1);
  }
  return deflated;
};

/**
 * @param {!FileEntry} fileEntry
 * @return {!Promise.<string>} Resolves with a "hashcode" consisting of
 *     just the last modified time and the file size.
 */
importer.createMetadataHashcode = function(fileEntry) {
  var entry = new importer.PromisingFileEntry(fileEntry);
  return new Promise(
      /**
       * @param {function()} resolve
       * @param {function()} reject
       * @this {importer.PersistentImportHistory}
       */
      function(resolve, reject) {
        entry.getMetadata()
            .then(
                /**
                 * @param {!Object} metadata
                 * @return {!Promise.<string>}
                 * @this {importer.PersistentImportHistory}
                 */
                function(metadata) {
                  if (!('modificationTime' in metadata)) {
                    reject('File entry missing "modificationTime" field.');
                  } else if (!('size' in metadata)) {
                    reject('File entry missing "size" field.');
                  } else {
                    var secondsSinceEpoch =
                        importer.toSecondsFromEpoch(metadata.modificationTime);
                    resolve(secondsSinceEpoch + '_' + metadata.size);
                  }
                }.bind(this));
      }.bind(this))
      .catch(importer.getLogger().catcher('importer-common-create-hashcode'));
};

/**
 * @param {string} date A date string in the form
 *     expected by Date.parse.
 * @return {string} The number of seconds from epoch to the date...as a string.
 */
importer.toSecondsFromEpoch = function(date) {
  // Since we're parsing a value that only has
  // precision to the second, our last three digits
  // will always be 000. We strip them and end up
  // with seconds.
  var milliseconds = String(Date.parse(date));
  return milliseconds.substring(0, milliseconds.length - 3);
};

/**
 * Factory interface for creating/accessing synced {@code FileEntry}
 * instances and listening to sync events on those files.
 *
 * @interface
 */
importer.SyncFileEntryProvider = function() {};

/**
 * Provides accsess to the sync FileEntry owned/managed by this class.
 *
 * @return {!Promise.<!FileEntry>}
 */
importer.SyncFileEntryProvider.prototype.getSyncFileEntry;

/**
 * Factory for synchronized files based on chrome.syncFileSystem.
 *
 * @constructor
 * @implements {importer.SyncFileEntryProvider}
 * @struct
 *
 * @param {string} fileName
 */
importer.ChromeSyncFileEntryProvider = function(fileName) {

  /** @private {string} */
  this.fileName_ = fileName;

  /** @private {!Array.<function()>} */
  this.syncListeners_ = [];

  /** @private {Promise.<!FileEntry>} */
  this.fileEntryPromise_ = null;
};

/**
 * Returns a sync FileEntry. Convenience method for class that just want
 * a file, but don't need to monitor changes.
 * @param {!Promise.<string>} fileNamePromise
 * @return {!Promise.<!FileEntry>}
 */
importer.ChromeSyncFileEntryProvider.getFileEntry =
    function(fileNamePromise) {
  return fileNamePromise.then(
      function(fileName) {
        return new importer.ChromeSyncFileEntryProvider(fileName)
          .getSyncFileEntry();
      });
};

/** @override */
importer.ChromeSyncFileEntryProvider.prototype.getSyncFileEntry = function() {
  if (this.fileEntryPromise_) {
    return /** @type {!Promise.<!FileEntry>} */ (this.fileEntryPromise_);
  };

  this.fileEntryPromise_ = this.getFileSystem_()
      .then(
          /**
           * @param {!FileSystem} fileSystem
           * @return {!Promise.<!FileEntry>}
           * @this {importer.ChromeSyncFileEntryProvider}
           */
          function(fileSystem) {
            return this.getFileEntry_(fileSystem);
          }.bind(this));

  return /** @type {!Promise.<!FileEntry>} */ (this.fileEntryPromise_);
};

/**
 * Wraps chrome.syncFileSystem in a Promise.
 *
 * @return {!Promise.<!FileSystem>}
 * @private
 */
importer.ChromeSyncFileEntryProvider.prototype.getFileSystem_ = function() {
  return new Promise(
      /**
       * @param {function()} resolve
       * @param {function()} reject
       * @this {importer.ChromeSyncFileEntryProvider}
       */
      function(resolve, reject) {
        chrome.syncFileSystem.requestFileSystem(
            /**
              * @param {FileSystem} fileSystem
              * @this {importer.ChromeSyncFileEntryProvider}
              */
            function(fileSystem) {
              if (chrome.runtime.lastError) {
                reject(chrome.runtime.lastError.message);
              } else {
                resolve(/** @type {!FileSystem} */ (fileSystem));
              }
            });
      }.bind(this));
};

/**
 * @param {!FileSystem} fileSystem
 * @return {!Promise.<!FileEntry>}
 * @private
 */
importer.ChromeSyncFileEntryProvider.prototype.getFileEntry_ =
    function(fileSystem) {
  return new Promise(
      /**
       * @param {function()} resolve
       * @param {function()} reject
       * @this {importer.ChromeSyncFileEntryProvider}
       */
      function(resolve, reject) {
        fileSystem.root.getFile(
            this.fileName_,
            {
              create: true,
              exclusive: false
            },
            resolve,
            reject);
      }.bind(this));
};

/**
 * Handles sync events. Checks to see if the event is for the file
 * we track, and sync-direction, and if so, notifies syncListeners.
 *
 * @see https://developer.chrome.com/apps/syncFileSystem
 *     #event-onFileStatusChanged
 *
 * @param {!Object} event Having a structure not unlike: {
 *     fileEntry: Entry,
 *     status: string,
 *     action: (string|undefined),
 *     direction: (string|undefined)}
 *
 * @private
 */
importer.ChromeSyncFileEntryProvider.prototype.handleSyncEvent_ =
    function(event) {
  if (!this.fileEntryPromise_) {
    return;
  }

  this.fileEntryPromise_.then(
      /**
       * @param {!FileEntry} fileEntry
       * @this {importer.ChromeSyncFileEntryProvider}
       */
      function(fileEntry) {
        if (event['fileEntry'].fullPath !== fileEntry.fullPath) {
          return;
        }

        if (event.direction && event.direction !== 'remote_to_local') {
          return;
        }

        if (event.action && event.action !== 'updated') {
          console.warn(
              'Unusual sync event action for sync file: ' + event.action);
          return;
        }

        this.syncListeners_.forEach(
            /**
             * @param {function()} listener
             * @this {importer.ChromeSyncFileEntryProvider}
             */
            function(listener) {
              // Notify by way of a promise so that it is fully asynchronous
              // (which can rationalize testing).
              Promise.resolve().then(listener);
            }.bind(this));
      }.bind(this));
};

/**
 * A basic logging mechanism.
 *
 * @interface
 */
importer.Logger = function() {};

/**
 * Writes an error message to the logger followed by a new line.
 *
 * @param {string} message
 */
importer.Logger.prototype.info;

/**
 * Writes an error message to the logger followed by a new line.
 *
 * @param {string} message
 */
importer.Logger.prototype.error;

/**
 * Returns a function suitable for use as an argument to
 * Promise#catch.
 *
 * @param {string} context
 */
importer.Logger.prototype.catcher;

/**
 * A {@code importer.Logger} that persists data in a {@code FileEntry}.
 *
 * @constructor
 * @implements {importer.Logger}
 * @struct
 * @final
 *
 * @param {!Promise.<!FileEntry>} fileEntryPromise
 * @param {!Promise.<!analytics.Tracker>} trackerPromise
 */
importer.RuntimeLogger = function(fileEntryPromise, trackerPromise) {

  /** @private {!Promise.<!importer.PromisingFileEntry>} */
  this.fileEntryPromise_ = fileEntryPromise.then(
      /** @param {!FileEntry} fileEntry */
      function(fileEntry) {
        return new importer.PromisingFileEntry(fileEntry);
      });

  /** @private {!Promise.<!analytics.Tracker>} */
  this.trackerPromise_ = trackerPromise;
};

/**
 * Reports an error to analytics.
 *
 * @param {string} context MUST NOT contain any dynamic error content,
 *    only statically defined string will dooooo.
 */
importer.RuntimeLogger.prototype.reportErrorContext_ = function(context) {
  this.trackerPromise_.then(
      /** @param {!analytics.Tracker} tracker */
      function(tracker) {
        tracker.sendException(
            context,
            false  /* fatal */ );
      });
};

/** @override  */
importer.RuntimeLogger.prototype.info = function(content) {
  this.write_('INFO', content);
  console.log(content);
};

/** @override  */
importer.RuntimeLogger.prototype.error = function(content) {
  this.write_('ERROR', content);
  console.error(content);
};

/** @override  */
importer.RuntimeLogger.prototype.catcher = function(context) {
  var prefix = '(' + context + ') ';
  return function(error) {
    this.reportErrorContext_(context);

    var message = prefix + 'Caught error in promise chain.';
    if (error) {
      // Error can be anything...maybe an Error, maybe a string.
      var error = error.message || error;
      this.error(message + ' Error: ' + error);
      if (error.stack) {
        this.write_('STACK', prefix + error.stack);
      }
    } else {
      this.error(message);
      error = new Error(message);
    }

    throw error;
  }.bind(this);
};

/**
 * Writes a message to the logger followed by a new line.
 *
 * @param {string} type
 * @param {string} message
 */
importer.RuntimeLogger.prototype.write_ = function(type, message) {
  // TODO(smckay): should we make an effort to reuse a file writer?
  return this.fileEntryPromise_
      .then(
          /** @param {!importer.PromisingFileEntry} fileEntry */
          function(fileEntry) {
            return fileEntry.createWriter();
          })
      .then(this.writeLine_.bind(this, type, message));
};

/**
 * Appends a new record to the end of the file.
 *
 * @param {string} type
 * @param {string} line
 * @param {!FileWriter} writer
 * @private
 */
importer.RuntimeLogger.prototype.writeLine_ = function(type, line, writer) {
  var blob = new Blob(
      ['[' + type + ' @ ' + new Date().toString() + '] ' + line + '\n'],
      {type: 'text/plain; charset=UTF-8'});
  return new Promise(
      /**
       * @param {function()} resolve
       * @param {function()} reject
       * @this {importer.RuntimeLogger}
       */
      function(resolve, reject) {
        writer.onwriteend = resolve;
        writer.onerror = reject;

        writer.seek(writer.length);
        writer.write(blob);
      }.bind(this));
};

/** @private {importer.Logger} */
importer.logger_ = null;

/**
 * Creates a new logger instance...all ready to go.
 *
 * @return {!importer.Logger}
 */
importer.getLogger = function() {
  if (!importer.logger_) {
    var nextLogId = importer.getNextDebugLogId_();

    /** @return {!Promise} */
    var rotator = function() {
      return importer.rotateLogs(
          nextLogId,
          importer.ChromeSyncFileEntryProvider.getFileEntry);
    };

    // This is a sligtly odd arrangement in service of two goals.
    //
    // 1) Make a logger available synchronously.
    // 2) Nuke old log files before reusing their names.
    //
    // In support of these goals we push the "rotator" between
    // the call to load the file entry and the method that
    // produces the name of the file to load. That method
    // (getDebugLogFilename) returns promise. We exploit this.
    importer.logger_ = new importer.RuntimeLogger(
        importer.ChromeSyncFileEntryProvider.getFileEntry(
            /** @type {!Promise.<string>} */ (rotator().then(
                importer.getDebugLogFilename.bind(null, nextLogId)))),
        importer.getTracker_());
  }

  return importer.logger_;
};

/**
 * Fetch analytics.Tracker from background page.
 * @return {!Promise.<!analytics.Tracker>}
 * @private
 */
importer.getTracker_ = function() {
  return new Promise(
      function(resolve, reject) {
        chrome.runtime.getBackgroundPage(
          /** @param {Window=} opt_background */
          function(opt_background) {
            if (chrome.runtime.lastError) {
              reject(chrome.runtime.lastError);
            }
            opt_background.background.ready(
                function() {
                  resolve(opt_background.background.tracker);
                });
          });
      });
};

/**
 * Returns the log ID for the next debug log to use.
 * @private
 */
importer.getNextDebugLogId_ = function() {
  // Changes every other month.
  return new Date().getMonth() % 2;
};

/**
 * Deletes the "next" log file if it has just-now become active.
 *
 * Basically we toggle back and forth writing to two log files. At the time
 * we flip from one to another we want to delete the oldest data we have.
 * In this case it will be the "next" log.
 *
 * This function must be run before instantiating the logger.
 *
 * @param {number} nextLogId
 * @param {function(!Promise<string>): !Promise<!FileEntry>} fileFactory
 *     Injected primarily to facilitate testing.
 * @return {!Promise} Resolves when trimming is complete.
 */
importer.rotateLogs = function(nextLogId, fileFactory) {
  var storage = importer.ChromeLocalStorage.getInstance();

  /** @return {!Promise} */
  var rememberLogId = function() {
    return storage.set(
        importer.Setting.LAST_KNOWN_LOG_ID,
        nextLogId);
  };

  return storage.get(importer.Setting.LAST_KNOWN_LOG_ID)
      .then(
          /** @param {number} lastKnownLogId */
          function(lastKnownLogId) {
            if (nextLogId === lastKnownLogId ||
                lastKnownLogId === undefined) {
              return Promise.resolve();
            }

            return fileFactory(importer.getDebugLogFilename(nextLogId))
                .then(
                    /**
                     * @param {!FileEntry} entry
                     * @return {!Promise}
                     * @suppress {checkTypes}
                     */
                    function(entry) {
                      return new Promise(entry.remove.bind(entry));
                    });
          })
          .then(rememberLogId)
          .catch(rememberLogId);
};

/**
 * Friendly wrapper around chrome.storage.local.
 *
 * NOTE: If you want to use this in a test, install MockChromeStorageAPI.
 *
 * @constructor
 */
importer.ChromeLocalStorage = function() {};

/**
 * @param {string} key
 * @param {string|number|boolean} value
 * @return {!Promise} Resolves when operation is complete
 */
importer.ChromeLocalStorage.prototype.set = function(key, value) {
  return new Promise(
      function(resolve, reject) {
        var values = {};
        values[key] = value;
        chrome.storage.local.set(
            values,
            function() {
              if (chrome.runtime.lastError) {
                reject(chrome.runtime.lastError);
              } else {
                resolve(undefined);
              }
            });
      });
};

/**
 * @param {string} key
 * @param {T=} opt_default
 * @return {!Promise.<T>} Resolves with the value, or {@code opt_default} when
 *     no value entry existis, or {@code undefined}.
 * @template T
 */
importer.ChromeLocalStorage.prototype.get = function(key, opt_default) {
  return new Promise(
      function(resolve, reject) {
        chrome.storage.local.get(
            key,
            /** @param {Object.<string, ?>} values */
            function(values) {
              if (chrome.runtime.lastError) {
                reject(chrome.runtime.lastError);
              } else if (key in values) {
                resolve(values[key]);
              } else {
                resolve(opt_default);
              }
            });
      });
};

/** @private @const {!importer.ChromeLocalStorage} */
importer.ChromeLocalStorage.INSTANCE_ = new importer.ChromeLocalStorage();

/** @return {!importer.ChromeLocalStorage} */
importer.ChromeLocalStorage.getInstance = function() {
  return importer.ChromeLocalStorage.INSTANCE_;
};
