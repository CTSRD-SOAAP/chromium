// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {!MetadataProviderCache} cache
 * @param {!MessagePort=} opt_messagePort Message port overriding the default
 *     worker port.
 * @extends {NewMetadataProvider}
 * @constructor
 * @struct
 */
function ContentMetadataProvider(cache, opt_messagePort) {
  NewMetadataProvider.call(
      this,
      cache,
      ContentMetadataProvider.PROPERTY_NAMES);

  /**
   * Pass all URLs to the metadata reader until we have a correct filter.
   * @private {RegExp}
   */
  this.urlFilter_ = /.*/;

  /**
   * @private {!MessagePort}
   * @const
   */
  this.dispatcher_ = opt_messagePort ?
      opt_messagePort :
      new SharedWorker(ContentMetadataProvider.WORKER_SCRIPT).port;
  this.dispatcher_.onmessage = this.onMessage_.bind(this);
  this.dispatcher_.postMessage({verb: 'init'});
  this.dispatcher_.start();

  /**
   * Initialization is not complete until the Worker sends back the
   * 'initialized' message.  See below.
   * @private {boolean}
   */
  this.initialized_ = false;

  /**
   * Map from Entry.toURL() to callback.
   * Note that simultaneous requests for same url are handled in MetadataCache.
   * @private {!Object<!string, !Array<function(Object)>>}
   * @const
   */
  this.callbacks_ = {};
}

/**
 * @const {!Array<string>}
 */
ContentMetadataProvider.PROPERTY_NAMES = [
  'contentThumbnailUrl',
  'contentThumbnailTransform',
  'contentImageTransform',
  'mediaTitle',
  'mediaArtist'
];

/**
 * Path of a worker script.
 * @const {string}
 */
ContentMetadataProvider.WORKER_SCRIPT =
    'chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/' +
    'foreground/js/metadata/metadata_dispatcher.js';

/**
 * Converts content metadata from parsers to the internal format.
 * @param {Object} metadata The content metadata.
 * @return {!MetadataItem} Converted metadata.
 */
ContentMetadataProvider.convertContentMetadata = function(metadata) {
  var item = new MetadataItem();
  item.contentThumbnailUrl = metadata['thumbnailURL'];
  item.contentThumbnailTransform = metadata['thumbnailTransform'];
  item.contentImageTransform = metadata['imageTransform'];
  item.mediaTitle = metadata['title'];
  item.mediaArtist = metadata['artist'];
  return item;
};

ContentMetadataProvider.prototype.__proto__ = NewMetadataProvider.prototype;

/**
 * @override
 */
ContentMetadataProvider.prototype.getImpl = function(requests) {
  var promises = [];
  for (var i = 0; i < requests.length; i++) {
    promises.push(new Promise(function(request, fulfill) {
      this.fetch(request.entry, request.names, fulfill);
    }.bind(this, requests[i])));
  }
  return Promise.all(promises);
};

/**
 * Fetches the metadata.
 * @param {Entry} entry File entry.
 * @param {!Array<string>} names Requested metadata type.
 * @param {function(Object)} callback Callback expects a map from metadata type
 *     to metadata value. This callback is called asynchronously.
 */
ContentMetadataProvider.prototype.fetch = function(entry, names, callback) {
  if (entry.isDirectory) {
    setTimeout(callback.bind(null, {}), 0);
    return;
  }
  var url = entry.toURL();
  if (this.callbacks_[url]) {
    this.callbacks_[url].push(callback);
  } else {
    this.callbacks_[url] = [callback];
    this.dispatcher_.postMessage({verb: 'request', arguments: [url]});
  }
};

/**
 * Dispatch a message from a metadata reader to the appropriate on* method.
 * @param {Object} event The event.
 * @private
 */
ContentMetadataProvider.prototype.onMessage_ = function(event) {
  var data = event.data;
  switch (data.verb) {
    case 'initialized':
      this.onInitialized_(data.arguments[0]);
      break;
    case 'result':
      this.onResult_(data.arguments[0], data.arguments[1]);
      break;
    case 'error':
      this.onError_(
          data.arguments[0],
          data.arguments[1],
          data.arguments[2],
          data.arguments[3]);
      break;
    case 'log':
      this.onLog_(data.arguments[0]);
      break;
    default:
      assertNotReached();
      break;
  }
};

/**
 * Handles the 'initialized' message from the metadata reader Worker.
 * @param {Object} regexp Regexp of supported urls.
 * @private
 */
ContentMetadataProvider.prototype.onInitialized_ = function(regexp) {
  this.urlFilter_ = regexp;

  // Tests can monitor for this state with
  // ExtensionTestMessageListener listener("worker-initialized");
  // ASSERT_TRUE(listener.WaitUntilSatisfied());
  // Automated tests need to wait for this, otherwise we crash in
  // browser_test cleanup because the worker process still has
  // URL requests in-flight.
  util.testSendMessage('worker-initialized');
  this.initialized_ = true;
};

/**
 * Handles the 'result' message from the worker.
 * @param {string} url File url.
 * @param {Object} metadata The metadata.
 * @private
 */
ContentMetadataProvider.prototype.onResult_ = function(url, metadata) {
  var callbacks = this.callbacks_[url];
  delete this.callbacks_[url];
  for (var i = 0; i < callbacks.length; i++) {
    callbacks[i](
        metadata ?
        ContentMetadataProvider.convertContentMetadata(metadata) :
        new MetadataItem());
  }
};

/**
 * Handles the 'error' message from the worker.
 * @param {string} url File entry.
 * @param {string} step Step failed.
 * @param {string} error Error description.
 * @param {Object?} metadata The metadata, if available.
 * @private
 */
ContentMetadataProvider.prototype.onError_ = function(
    url, step, error, metadata) {
  console.error(
      'ContentMetadataProvider failed to obtain metadata: '+
      url + ': ' + step + ': ' + error);
  this.onResult_(url, new MetadataItem());
};

/**
 * Handles the 'log' message from the worker.
 * @param {Array.<*>} arglist Log arguments.
 * @private
 */
ContentMetadataProvider.prototype.onLog_ = function(arglist) {
  console.log.apply(console, ['ContentMetadataProvider log:'].concat(arglist));
};
