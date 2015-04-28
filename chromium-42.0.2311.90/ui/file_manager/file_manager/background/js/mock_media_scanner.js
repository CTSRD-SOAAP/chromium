// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * importer.MediaScanner and importer.ScanResult test double.
 *
 * @constructor
 * @struct
 * @implements {importer.MediaScanner}
 */
function TestMediaScanner() {

  /** @private {!importer.ScanResult} */
  this.scans_ = [];

  /**
   * List of file entries found while scanning.
   * @type {!Array.<!FileEntry>}
   */
  this.fileEntries = [];

  /** @type {number} */
  this.totalBytes = 100;

  /** @type {number} */
  this.scanDuration = 100;

  /** @private {!Array.<!importer.ScanObserver>} */
  this.observers = [];
}

/** @override */
TestMediaScanner.prototype.addObserver = function(observer) {
  this.observers.push(observer);
};

/** @override */
TestMediaScanner.prototype.removeObserver = function(observer) {
  var index = this.observers.indexOf(observer);
  if (index > -1) {
    this.observers.splice(index, 1);
  } else {
    console.warn('Ignoring request to remove observer that is not registered.');
  }
};

/** @override */
TestMediaScanner.prototype.scan = function(entries) {
  var scan = new TestScanResult(this.fileEntries);
  scan.totalBytes = this.totalBytes;
  scan.scanDuration = this.scanDuration;
  this.scans_.push(scan);
  return scan;
};

/**
 * Finalizes all previously started scans.
 */
TestMediaScanner.prototype.finalizeScans = function() {
  this.scans_.forEach(this.finalize.bind(this));
};

/**
 * Notifies observers that the most recently started scan has been updated.
 * @param {!importer.ScanResult} result
 */
TestMediaScanner.prototype.update = function() {
  assertTrue(this.scans_.length > 0);
  var scan = this.scans_[this.scans_.length - 1];
  this.observers.forEach(
      function(observer) {
        observer(importer.ScanEvent.UPDATED, scan);
      });
};

/**
 * Notifies observers that a scan has finished.
 * @param {!importer.ScanResult} result
 */
TestMediaScanner.prototype.finalize = function(result) {
  result.finalize();
  this.observers.forEach(
      function(observer) {
        observer(importer.ScanEvent.FINALIZED, result);
      });
};

/**
 * @param {number} expected
 */
TestMediaScanner.prototype.assertScanCount = function(expected) {
  assertEquals(expected, this.scans_.length);
};

/**
 * importer.MediaScanner and importer.ScanResult test double.
 *
 * @constructor
 * @struct
 * @implements {importer.ScanResult}
 *
 * @param {!Array.<!FileEntry>} fileEntries
 */
function TestScanResult(fileEntries) {
  /**
   * List of file entries found while scanning.
   * @type {!Array.<!FileEntry>}
   */
  this.fileEntries = fileEntries.slice();

  /** @type {number} */
  this.totalBytes = 100;

  /** @type {number} */
  this.scanDuration = 100;

  /** @type {number} */
  this.duplicateFileCount = 0;

  /** @type {function} */
  this.resolveResult_;

  /** @type {function} */
  this.rejectResult_;

  /** @type {boolean} */
  this.settled_ = false;

  /** @type {!Promise.<!importer.ScanResult>} */
  this.whenFinal_ = new Promise(
      function(resolve, reject) {
        this.resolveResult_ = function(result) {
          this.settled_ = true;
          resolve(result);
        }.bind(this);
        this.rejectResult_ = function() {
          this.settled_ = true;
          reject();
        }.bind(this);
      }.bind(this));
}

/** @override */
TestScanResult.prototype.getFileEntries = function() {
  return this.fileEntries;
};

/** @override */
TestScanResult.prototype.finalize = function() {
  return this.resolveResult_(this);
};

/** @override */
TestScanResult.prototype.whenFinal = function() {
  return this.whenFinal_;
};

/** @override */
TestScanResult.prototype.isFinal = function() {
  return this.settled_;
};

/** @override */
TestScanResult.prototype.isInvalidated = function() {
  return false;
};

/** @override */
TestScanResult.prototype.getStatistics = function() {
  return {
    scanDuration: this.scanDuration,
    newFileCount: this.fileEntries.length,
    duplicateFileCount: this.duplicateFileCount,
    sizeBytes: this.totalBytes
  };
};

/**
 * @constructor
 * @implements {importer.DirectoryWatcher}
 */
function TestDirectoryWatcher(callback) {
  /**
   * @public {function()}
   * @const
   */
  this.callback = callback;

  /**
   * @public {boolean}
   */
  this.triggered = false;
};

/**
 * @override
 */
TestDirectoryWatcher.prototype.addDirectory = function() {
};
