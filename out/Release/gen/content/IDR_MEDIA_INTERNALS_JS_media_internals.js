// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var media = {};

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A global object that gets used by the C++ interface.
 */
var media = (function() {
  'use strict';

  var manager = null;

  // A number->string mapping that is populated through the backend that
  // describes the phase that the network entity is in.
  var eventPhases = {};

  // A number->string mapping that is populated through the backend that
  // describes the type of event sent from the network.
  var eventTypes = {};

  // A mapping of number->CacheEntry where the number is a unique id for that
  // network request.
  var cacheEntries = {};

  // A mapping of url->CacheEntity where the url is the url of the resource.
  var cacheEntriesByKey = {};

  var requrestURLs = {};

  var media = {
    BAR_WIDTH: 200,
    BAR_HEIGHT: 25
  };

  /**
   * Users of |media| must call initialize prior to calling other methods.
   */
  media.initialize = function(theManager) {
    manager = theManager;
  };

  media.onReceiveEverything = function(everything) {
    for (var key in everything.audio_streams) {
      media.updateAudioStream(everything.audio_streams[key]);
    }
  };

  media.onReceiveConstants = function(constants) {
    for (var key in constants.eventTypes) {
      var value = constants.eventTypes[key];
      eventTypes[value] = key;
    }

    for (var key in constants.eventPhases) {
      var value = constants.eventPhases[key];
      eventPhases[value] = key;
    }
  };

  media.cacheForUrl = function(url) {
    return cacheEntriesByKey[url];
  };

  media.onNetUpdate = function(updates) {
    updates.forEach(function(update) {
      var id = update.source.id;
      if (!cacheEntries[id])
        cacheEntries[id] = new media.CacheEntry;

      switch (eventPhases[update.phase] + '.' + eventTypes[update.type]) {
        case 'PHASE_BEGIN.DISK_CACHE_ENTRY_IMPL':
          var key = update.params.key;

          // Merge this source with anything we already know about this key.
          if (cacheEntriesByKey[key]) {
            cacheEntriesByKey[key].merge(cacheEntries[id]);
            cacheEntries[id] = cacheEntriesByKey[key];
          } else {
            cacheEntriesByKey[key] = cacheEntries[id];
          }
          cacheEntriesByKey[key].key = key;
          break;

        case 'PHASE_BEGIN.SPARSE_READ':
          cacheEntries[id].readBytes(update.params.offset,
                                      update.params.buff_len);
          cacheEntries[id].sparse = true;
          break;

        case 'PHASE_BEGIN.SPARSE_WRITE':
          cacheEntries[id].writeBytes(update.params.offset,
                                       update.params.buff_len);
          cacheEntries[id].sparse = true;
          break;

        case 'PHASE_BEGIN.URL_REQUEST_START_JOB':
          requrestURLs[update.source.id] = update.params.url;
          break;

        case 'PHASE_NONE.HTTP_TRANSACTION_READ_RESPONSE_HEADERS':
          // Record the total size of the file if this was a range request.
          var range = /content-range:\s*bytes\s*\d+-\d+\/(\d+)/i.exec(
              update.params.headers);
          var key = requrestURLs[update.source.id];
          delete requrestURLs[update.source.id];
          if (range && key) {
            if (!cacheEntriesByKey[key]) {
              cacheEntriesByKey[key] = new media.CacheEntry;
              cacheEntriesByKey[key].key = key;
            }
            cacheEntriesByKey[key].size = range[1];
          }
          break;
      }
    });
  };

  media.onRendererTerminated = function(renderId) {
    util.object.forEach(manager.players_, function(playerInfo, id) {
      if (playerInfo.properties['render_id'] == renderId) {
        manager.removePlayer(id);
      }
    });
  };

  // For whatever reason, addAudioStream is also called on
  // the removal of audio streams.
  media.addAudioStream = function(event) {
    switch (event.status) {
      case 'created':
        manager.addAudioStream(event.id);
        manager.updateAudioStream(event.id, { 'playing': event.playing });
        break;
      case 'closed':
        manager.removeAudioStream(event.id);
        break;
    }
  };

  media.updateAudioStream = function(stream) {
    manager.addAudioStream(stream.id);
    manager.updateAudioStream(stream.id, stream);
  };

  media.onItemDeleted = function() {
    // This only gets called when an audio stream is removed, which
    // for whatever reason is also handled by addAudioStream...
    // Because it is already handled, we can safely ignore it.
  };

  media.onPlayerOpen = function(id, timestamp) {
    manager.addPlayer(id, timestamp);
  };

  media.onMediaEvent = function(event) {
    var source = event.renderer + ':' + event.player;

    // Although this gets called on every event, there is nothing we can do
    // because there is no onOpen event.
    media.onPlayerOpen(source);
    manager.updatePlayerInfoNoRecord(
        source, event.ticksMillis, 'render_id', event.renderer);
    manager.updatePlayerInfoNoRecord(
        source, event.ticksMillis, 'player_id', event.player);

    var propertyCount = 0;
    util.object.forEach(event.params, function(value, key) {
      key = key.trim();

      // These keys get spammed *a lot*, so put them on the display
      // but don't log list.
      if (key === 'buffer_start' ||
          key === 'buffer_end' ||
          key === 'buffer_current' ||
          key === 'is_downloading_data') {
        manager.updatePlayerInfoNoRecord(
            source, event.ticksMillis, key, value);
      } else {
        manager.updatePlayerInfo(source, event.ticksMillis, key, value);
      }
      propertyCount += 1;
    });

    if (propertyCount === 0) {
      manager.updatePlayerInfo(
          source, event.ticksMillis, 'EVENT', event.type);
    }
  };

  // |chrome| is not defined during tests.
  if (window.chrome && window.chrome.send) {
    chrome.send('getEverything');
  }
  return media;
}());

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Some utility functions that don't belong anywhere else in the
 * code.
 */

var util = (function() {
  var util = {};
  util.object = {};
  /**
   * Calls a function for each element in an object/map/hash.
   *
   * @param obj The object to iterate over.
   * @param f The function to call on every value in the object.  F should have
   * the following arguments: f(value, key, object) where value is the value
   * of the property, key is the corresponding key, and obj is the object that
   * was passed in originally.
   * @param optObj The object use as 'this' within f.
   */
  util.object.forEach = function(obj, f, optObj) {
    'use strict';
    var key;
    for (key in obj) {
      if (obj.hasOwnProperty(key)) {
        f.call(optObj, obj[key], key, obj);
      }
    }
  };
  util.millisecondsToString = function(timeMillis) {
    function pad(num) {
      num = num.toString();
      if (num.length < 2) {
        return '0' + num;
      }
      return num;
    }

    var date = new Date(timeMillis);
    return pad(date.getUTCHours()) + ':' + pad(date.getUTCMinutes()) + ':' +
        pad(date.getUTCSeconds()) + ' ' + pad((date.getMilliseconds()) % 1000);
  };

  return util;
}());

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('media', function() {
  'use strict';

  /**
   * This class represents a file cached by net.
   */
  function CacheEntry() {
    this.read_ = new media.DisjointRangeSet;
    this.written_ = new media.DisjointRangeSet;
    this.available_ = new media.DisjointRangeSet;

    // Set to true when we know the entry is sparse.
    this.sparse = false;
    this.key = null;
    this.size = null;

    // The <details> element representing this CacheEntry.
    this.details_ = document.createElement('details');
    this.details_.className = 'cache-entry';
    this.details_.open = false;

    // The <details> summary line. It contains a chart of requested file ranges
    // and the url if we know it.
    var summary = document.createElement('summary');

    this.summaryText_ = document.createTextNode('');
    summary.appendChild(this.summaryText_);

    summary.appendChild(document.createTextNode(' '));

    // Controls to modify this CacheEntry.
    var controls = document.createElement('span');
    controls.className = 'cache-entry-controls';
    summary.appendChild(controls);
    summary.appendChild(document.createElement('br'));

    // A link to clear recorded data from this CacheEntry.
    var clearControl = document.createElement('a');
    clearControl.href = 'javascript:void(0)';
    clearControl.onclick = this.clear.bind(this);
    clearControl.textContent = '(clear entry)';
    controls.appendChild(clearControl);

    this.details_.appendChild(summary);

    // The canvas for drawing cache writes.
    this.writeCanvas = document.createElement('canvas');
    this.writeCanvas.width = media.BAR_WIDTH;
    this.writeCanvas.height = media.BAR_HEIGHT;
    this.details_.appendChild(this.writeCanvas);

    // The canvas for drawing cache reads.
    this.readCanvas = document.createElement('canvas');
    this.readCanvas.width = media.BAR_WIDTH;
    this.readCanvas.height = media.BAR_HEIGHT;
    this.details_.appendChild(this.readCanvas);

    // A tabular representation of the data in the above canvas.
    this.detailTable_ = document.createElement('table');
    this.detailTable_.className = 'cache-table';
    this.details_.appendChild(this.detailTable_);
  }

  CacheEntry.prototype = {
    /**
     * Mark a range of bytes as read from the cache.
     * @param {int} start The first byte read.
     * @param {int} length The number of bytes read.
     */
    readBytes: function(start, length) {
      start = parseInt(start);
      length = parseInt(length);
      this.read_.add(start, start + length);
      this.available_.add(start, start + length);
      this.sparse = true;
    },

    /**
     * Mark a range of bytes as written to the cache.
     * @param {int} start The first byte written.
     * @param {int} length The number of bytes written.
     */
    writeBytes: function(start, length) {
      start = parseInt(start);
      length = parseInt(length);
      this.written_.add(start, start + length);
      this.available_.add(start, start + length);
      this.sparse = true;
    },

    /**
     * Merge this CacheEntry with another, merging recorded ranges and flags.
     * @param {CacheEntry} other The CacheEntry to merge into this one.
     */
    merge: function(other) {
      this.read_.merge(other.read_);
      this.written_.merge(other.written_);
      this.available_.merge(other.available_);
      this.sparse = this.sparse || other.sparse;
      this.key = this.key || other.key;
      this.size = this.size || other.size;
    },

    /**
     * Clear all recorded ranges from this CacheEntry and redraw this.details_.
     */
    clear: function() {
      this.read_ = new media.DisjointRangeSet;
      this.written_ = new media.DisjointRangeSet;
      this.available_ = new media.DisjointRangeSet;
      this.generateDetails();
    },

    /**
     * Helper for drawCacheReadsToCanvas() and drawCacheWritesToCanvas().
     *
     * Accepts the entries to draw, a canvas fill style, and the canvas to
     * draw on.
     */
    drawCacheEntriesToCanvas: function(entries, fillStyle, canvas) {
      // Don't bother drawing anything if we don't know the total size.
      if (!this.size) {
        return;
      }

      var width = canvas.width;
      var height = canvas.height;
      var context = canvas.getContext('2d');
      var fileSize = this.size;

      context.fillStyle = '#aaa';
      context.fillRect(0, 0, width, height);

      function drawRange(start, end) {
        var left = start / fileSize * width;
        var right = end / fileSize * width;
        context.fillRect(left, 0, right - left, height);
      }

      context.fillStyle = fillStyle;
      entries.map(function(start, end) {
        drawRange(start, end);
      });
    },

    /**
     * Draw cache writes to the given canvas.
     *
     * It should consist of a horizontal bar with highlighted sections to
     * represent which parts of a file have been written to the cache.
     *
     * e.g. |xxxxxx----------x|
     */
    drawCacheWritesToCanvas: function(canvas) {
      this.drawCacheEntriesToCanvas(this.written_, '#00a', canvas);
    },

    /**
     * Draw cache reads to the given canvas.
     *
     * It should consist of a horizontal bar with highlighted sections to
     * represent which parts of a file have been read from the cache.
     *
     * e.g. |xxxxxx----------x|
     */
    drawCacheReadsToCanvas: function(canvas) {
      this.drawCacheEntriesToCanvas(this.read_, '#0a0', canvas);
    },

    /**
     * Update this.details_ to contain everything we currently know about
     * this file.
     */
    generateDetails: function() {
      function makeElement(tag, content) {
        var toReturn = document.createElement(tag);
        toReturn.textContent = content;
        return toReturn;
      }

      this.details_.id = this.key;
      this.summaryText_.textContent = this.key || 'Unknown File';

      this.detailTable_.textContent = '';
      var header = document.createElement('thead');
      var footer = document.createElement('tfoot');
      var body = document.createElement('tbody');
      this.detailTable_.appendChild(header);
      this.detailTable_.appendChild(footer);
      this.detailTable_.appendChild(body);

      var headerRow = document.createElement('tr');
      headerRow.appendChild(makeElement('th', 'Read From Cache'));
      headerRow.appendChild(makeElement('th', 'Written To Cache'));
      header.appendChild(headerRow);

      var footerRow = document.createElement('tr');
      var footerCell = document.createElement('td');
      footerCell.textContent = 'Out of ' + (this.size || 'unkown size');
      footerCell.setAttribute('colspan', 2);
      footerRow.appendChild(footerCell);
      footer.appendChild(footerRow);

      var read = this.read_.map(function(start, end) {
        return start + ' - ' + end;
      });
      var written = this.written_.map(function(start, end) {
        return start + ' - ' + end;
      });

      var length = Math.max(read.length, written.length);
      for (var i = 0; i < length; i++) {
        var row = document.createElement('tr');
        row.appendChild(makeElement('td', read[i] || ''));
        row.appendChild(makeElement('td', written[i] || ''));
        body.appendChild(row);
      }

      this.drawCacheWritesToCanvas(this.writeCanvas);
      this.drawCacheReadsToCanvas(this.readCanvas);
    },

    /**
     * Render this CacheEntry as a <li>.
     * @return {HTMLElement} A <li> representing this CacheEntry.
     */
    toListItem: function() {
      this.generateDetails();

      var result = document.createElement('li');
      result.appendChild(this.details_);
      return result;
    }
  };

  return {
    CacheEntry: CacheEntry
  };
});

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('media', function() {

  /**
   * This class represents a collection of non-intersecting ranges. Ranges
   * specified by (start, end) can be added and removed at will. It is used to
   * record which sections of a media file have been cached, e.g. the first and
   * last few kB plus several MB in the middle.
   *
   * Example usage:
   * someRange.add(0, 100);     // Contains 0-100.
   * someRange.add(150, 200);   // Contains 0-100, 150-200.
   * someRange.remove(25, 75);  // Contains 0-24, 76-100, 150-200.
   * someRange.add(25, 149);    // Contains 0-200.
   */
  function DisjointRangeSet() {
    this.ranges_ = {};
  }

  DisjointRangeSet.prototype = {
    /**
     * Deletes all ranges intersecting with (start ... end) and returns the
     * extents of the cleared area.
     * @param {int} start The start of the range to remove.
     * @param {int} end The end of the range to remove.
     * @param {int} sloppiness 0 removes only strictly overlapping ranges, and
     *                         1 removes adjacent ones.
     * @return {Object} The start and end of the newly cleared range.
     */
    clearRange: function(start, end, sloppiness) {
      var ranges = this.ranges_;
      var result = {start: start, end: end};

      for (var rangeStart in this.ranges_) {
        rangeEnd = this.ranges_[rangeStart];
        // A range intersects another if its start lies within the other range
        // or vice versa.
        if ((rangeStart >= start && rangeStart <= (end + sloppiness)) ||
            (start >= rangeStart && start <= (rangeEnd + sloppiness))) {
          delete ranges[rangeStart];
          result.start = Math.min(result.start, rangeStart);
          result.end = Math.max(result.end, rangeEnd);
        }
      }

      return result;
    },

    /**
     * Adds a range to this DisjointRangeSet.
     * Joins adjacent and overlapping ranges together.
     * @param {int} start The beginning of the range to add, inclusive.
     * @param {int} end The end of the range to add, inclusive.
     */
    add: function(start, end) {
      if (end < start)
        return;

      // Remove all touching ranges.
      result = this.clearRange(start, end, 1);
      // Add back a single contiguous range.
      this.ranges_[Math.min(start, result.start)] = Math.max(end, result.end);
    },

    /**
     * Combines a DisjointRangeSet with this one.
     * @param {DisjointRangeSet} ranges A DisjointRangeSet to be squished into
     * this one.
     */
    merge: function(other) {
      var ranges = this;
      other.forEach(function(start, end) { ranges.add(start, end); });
    },

    /**
     * Removes a range from this DisjointRangeSet.
     * Will split existing ranges if necessary.
     * @param {int} start The beginning of the range to remove, inclusive.
     * @param {int} end The end of the range to remove, inclusive.
     */
    remove: function(start, end) {
      if (end < start)
        return;

      // Remove instersecting ranges.
      result = this.clearRange(start, end, 0);

      // Add back non-overlapping ranges.
      if (result.start < start)
        this.ranges_[result.start] = start - 1;
      if (result.end > end)
        this.ranges_[end + 1] = result.end;
    },

    /**
     * Iterates over every contiguous range in this DisjointRangeSet, calling a
     * function for each (start, end).
     * @param {function(int, int)} iterator The function to call on each range.
     */
    forEach: function(iterator) {
      for (var start in this.ranges_)
        iterator(start, this.ranges_[start]);
    },

    /**
     * Maps this DisjointRangeSet to an array by calling a given function on the
     * start and end of each contiguous range, sorted by start.
     * @param {function(int, int)} mapper Maps a range to an array element.
     * @return {Array} An array of each mapper(range).
     */
    map: function(mapper) {
      var starts = [];
      for (var start in this.ranges_)
        starts.push(parseInt(start));
      starts.sort(function(a, b) {
        return a - b;
      });

      var ranges = this.ranges_;
      var results = starts.map(function(s) {
        return mapper(s, ranges[s]);
      });

      return results;
    },

    /**
     * Finds the maximum value present in any of the contained ranges.
     * @return {int} The maximum value contained by this DisjointRangeSet.
     */
    max: function() {
      var max = -Infinity;
      for (var start in this.ranges_)
        max = Math.max(max, this.ranges_[start]);
      return max;
    },
  };

  return {
    DisjointRangeSet: DisjointRangeSet
  };
});

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A class for keeping track of the details of a player.
 */

var PlayerInfo = (function() {
  'use strict';

  /**
   * A class that keeps track of properties on a media player.
   * @param id A unique id that can be used to identify this player.
   */
  function PlayerInfo(id) {
    this.id = id;
    // The current value of the properties for this player.
    this.properties = {};
    // All of the past (and present) values of the properties.
    this.pastValues = {};

    // Every single event in the order in which they were received.
    this.allEvents = [];
    this.lastRendered = 0;

    this.firstTimestamp_ = -1;
  }

  PlayerInfo.prototype = {
    /**
     * Adds or set a property on this player.
     * This is the default logging method as it keeps track of old values.
     * @param timestamp  The time in milliseconds since the Epoch.
     * @param key A String key that describes the property.
     * @param value The value of the property.
     */
    addProperty: function(timestamp, key, value) {
      // The first timestamp that we get will be recorded.
      // Then, all future timestamps are deltas of that.
      if (this.firstTimestamp_ === -1) {
        this.firstTimestamp_ = timestamp;
      }

      if (typeof key !== 'string') {
        throw new Error(typeof key + ' is not a valid key type');
      }

      this.properties[key] = value;

      if (!this.pastValues[key]) {
        this.pastValues[key] = [];
      }

      var recordValue = {
        time: timestamp - this.firstTimestamp_,
        key: key,
        value: value
      };

      this.pastValues[key].push(recordValue);
      this.allEvents.push(recordValue);
    },

    /**
     * Adds or set a property on this player.
     * Does not keep track of old values.  This is better for
     * values that get spammed repeatedly.
     * @param timestamp The time in milliseconds since the Epoch.
     * @param key A String key that describes the property.
     * @param value The value of the property.
     */
    addPropertyNoRecord: function(timestamp, key, value) {
      this.addProperty(timestamp, key, value);
      this.allEvents.pop();
    }
  };

  return PlayerInfo;
}());

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Keeps track of all the existing PlayerInfo and
 * audio stream objects and is the entry-point for messages from the backend.
 *
 * The events captured by Manager (add, remove, update) are relayed
 * to the clientRenderer which it can choose to use to modify the UI.
 */
var Manager = (function() {
  'use strict';

  function Manager(clientRenderer) {
    this.players_ = {};
    this.audioStreams_ = {};
    this.clientRenderer_ = clientRenderer;
  }

  Manager.prototype = {
    /**
     * Adds an audio-stream to the dictionary of audio-streams to manage.
     * @param id The unique-id of the audio-stream.
     */
    addAudioStream: function(id) {
      this.audioStreams_[id] = this.audioStreams_[id] || {};
      this.clientRenderer_.audioStreamAdded(this.audioStreams_,
                                            this.audioStreams_[id]);
    },

    /**
     * Sets properties of an audiostream.
     * @param id The unique-id of the audio-stream.
     * @param properties A dictionary of properties to be added to the
     * audio-stream.
     */
    updateAudioStream: function(id, properties) {
      for (var key in properties) {
        this.audioStreams_[id][key] = properties[key];
      }
      this.clientRenderer_.audioStreamAdded(
          this.audioStreams_, this.audioStreams_[id]);
    },

    /**
     * Removes an audio-stream from the manager.
     * @param id The unique-id of the audio-stream.
     */
    removeAudioStream: function(id) {
      this.clientRenderer_.audioStreamRemoved(
          this.audioStreams_, this.audioStreams_[id]);
      delete this.audioStreams_[id];
    },


    /**
     * Adds a player to the list of players to manage.
     */
    addPlayer: function(id) {
      if (this.players_[id]) {
        return;
      }
      // Make the PlayerProperty and add it to the mapping
      this.players_[id] = new PlayerInfo(id);
      this.clientRenderer_.playerAdded(this.players_, this.players_[id]);
    },

    /**
     * Attempts to remove a player from the UI.
     * @param id The ID of the player to remove.
     */
    removePlayer: function(id) {
      delete this.players_[id];
      this.clientRenderer_.playerRemoved(this.players_, this.players_[id]);
    },

    updatePlayerInfoNoRecord: function(id, timestamp, key, value) {
      if (!this.players_[id]) {
        console.error('[updatePlayerInfo] Id ' + id + ' does not exist');
        return;
      }

      this.players_[id].addPropertyNoRecord(timestamp, key, value);
      this.clientRenderer_.playerUpdated(this.players_,
                                         this.players_[id],
                                         key,
                                         value);
    },

    /**
     *
     * @param id The unique ID that identifies the player to be updated.
     * @param timestamp The timestamp of when the change occured.  This
     * timestamp is *not* normalized.
     * @param key The name of the property to be added/changed.
     * @param value The value of the property.
     */
    updatePlayerInfo: function(id, timestamp, key, value) {
      if (!this.players_[id]) {
        console.error('[updatePlayerInfo] Id ' + id + ' does not exist');
        return;
      }

      this.players_[id].addProperty(timestamp, key, value);
      this.clientRenderer_.playerUpdated(this.players_,
                                         this.players_[id],
                                         key,
                                         value);
    }
  };

  return Manager;
}());

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var ClientRenderer = (function() {
  var ClientRenderer = function() {
    this.playerListElement = document.getElementById('player-list');
    this.audioStreamListElement = document.getElementById('audio-stream-list');
    this.propertiesTable = document.getElementById('property-table');
    this.logTable = document.getElementById('log');
    this.graphElement = document.getElementById('graphs');

    this.selectedPlayer = null;
    this.selectedStream = null;

    this.selectedPlayerLogIndex = 0;

    this.filterFunction = function() { return true; };
    this.filterText = document.getElementById('filter-text');
    this.filterText.onkeyup = this.onTextChange_.bind(this);

    this.bufferCanvas = document.createElement('canvas');
    this.bufferCanvas.width = media.BAR_WIDTH;
    this.bufferCanvas.height = media.BAR_HEIGHT;

    this.clipboardTextarea = document.getElementById('clipboard-textarea');
    this.clipboardButton = document.getElementById('copy-button');
    this.clipboardButton.onclick = this.copyToClipboard_.bind(this);
  };

  function removeChildren(element) {
    while (element.hasChildNodes()) {
      element.removeChild(element.lastChild);
    }
  };

  function createButton(text, select_cb) {
    var button = document.createElement('button');

    button.appendChild(document.createTextNode(text));
    button.onclick = function() {
      select_cb();
    };

    return button;
  };

  ClientRenderer.prototype = {
    audioStreamAdded: function(audioStreams, audioStreamAdded) {
      this.redrawAudioStreamList_(audioStreams);
    },

    audioStreamUpdated: function(audioStreams, stream, key, value) {
      if (stream === this.selectedStream) {
        this.drawProperties_(stream);
      }
    },

    audioStreamRemoved: function(audioStreams, audioStreamRemoved) {
      this.redrawAudioStreamList_(audioStreams);
    },

    /**
     * Called when a player is added to the collection.
     * @param players The entire map of id -> player.
     * @param player_added The player that is added.
     */
    playerAdded: function(players, playerAdded) {
      this.redrawPlayerList_(players);
    },

    /**
     * Called when a playre is removed from the collection.
     * @param players The entire map of id -> player.
     * @param player_added The player that was removed.
     */
    playerRemoved: function(players, playerRemoved) {
      this.redrawPlayerList_(players);
    },

    /**
     * Called when a property on a player is changed.
     * @param players The entire map of id -> player.
     * @param player The player that had its property changed.
     * @param key The name of the property that was changed.
     * @param value The new value of the property.
     */
    playerUpdated: function(players, player, key, value) {
      if (player === this.selectedPlayer) {
        this.drawProperties_(player.properties);
        this.drawLog_();
        this.drawGraphs_();
      }
      if (key === 'name' || key === 'url') {
        this.redrawPlayerList_(players);
      }
    },

    redrawAudioStreamList_: function(streams) {
      removeChildren(this.audioStreamListElement);

      for (id in streams) {
        var li = document.createElement('li');
        li.appendChild(createButton(
            id, this.selectAudioStream_.bind(this, streams[id])));
        this.audioStreamListElement.appendChild(li);
      }
    },

    selectAudioStream_: function(audioStream) {
      this.selectedStream = audioStream;
      this.selectedPlayer = null;
      this.drawProperties_(audioStream);
      removeChildren(this.logTable.querySelector('tbody'));
      removeChildren(this.graphElement);
    },

    redrawPlayerList_: function(players) {
      removeChildren(this.playerListElement);

      for (id in players) {
        var li = document.createElement('li');
        var player = players[id];
        var usableName = player.properties.name ||
            player.properties.url ||
            'player ' + player.id;

        li.appendChild(createButton(
            usableName, this.selectPlayer_.bind(this, player)));
        this.playerListElement.appendChild(li);
      }
    },

    selectPlayer_: function(player) {
      this.selectedPlayer = player;
      this.selectedPlayerLogIndex = 0;
      this.selectedStream = null;
      this.drawProperties_(player.properties);

      removeChildren(this.logTable.querySelector('tbody'));
      removeChildren(this.graphElement);
      this.drawLog_();
      this.drawGraphs_();
    },

    drawProperties_: function(propertyMap) {
      removeChildren(this.propertiesTable);

      for (key in propertyMap) {
        var value = propertyMap[key];

        var row = this.propertiesTable.insertRow(-1);
        var keyCell = row.insertCell(-1);
        var valueCell = row.insertCell(-1);

        keyCell.appendChild(document.createTextNode(key));
        valueCell.appendChild(document.createTextNode(value));
      }
    },

    appendEventToLog_: function(event) {
      if (this.filterFunction(event.key)) {
        var row = this.logTable.querySelector('tbody').insertRow(-1);

        row.insertCell(-1).appendChild(document.createTextNode(
            util.millisecondsToString(event.time)));
        row.insertCell(-1).appendChild(document.createTextNode(event.key));
        row.insertCell(-1).appendChild(document.createTextNode(event.value));
      }
    },

    drawLog_: function() {
      var toDraw = this.selectedPlayer.allEvents.slice(
          this.selectedPlayerLogIndex);
      toDraw.forEach(this.appendEventToLog_.bind(this));
      this.selectedPlayerLogIndex = this.selectedPlayer.allEvents.length;
    },

    drawGraphs_: function() {
      function addToGraphs(name, graph, graphElement) {
        var li = document.createElement('li');
        li.appendChild(graph);
        li.appendChild(document.createTextNode(name));
        graphElement.appendChild(li);
      }

      var url = this.selectedPlayer.properties.url;
      if (!url) {
        return;
      }

      var cache = media.cacheForUrl(url);

      var player = this.selectedPlayer;
      var props = player.properties;

      var cacheExists = false;
      var bufferExists = false;

      if (props['buffer_start'] !== undefined &&
          props['buffer_current'] !== undefined &&
          props['buffer_end'] !== undefined &&
          props['total_bytes'] !== undefined) {
        this.drawBufferGraph_(props['buffer_start'],
                              props['buffer_current'],
                              props['buffer_end'],
                              props['total_bytes']);
        bufferExists = true;
      }

      if (cache) {
        if (player.properties['total_bytes']) {
          cache.size = Number(player.properties['total_bytes']);
        }
        cache.generateDetails();
        cacheExists = true;

      }

      if (!this.graphElement.hasChildNodes()) {
        if (bufferExists) {
          addToGraphs('buffer', this.bufferCanvas, this.graphElement);
        }
        if (cacheExists) {
          addToGraphs('cache read', cache.readCanvas, this.graphElement);
          addToGraphs('cache write', cache.writeCanvas, this.graphElement);
        }
      }
    },

    drawBufferGraph_: function(start, current, end, size) {
      var ctx = this.bufferCanvas.getContext('2d');
      var width = this.bufferCanvas.width;
      var height = this.bufferCanvas.height;
      ctx.fillStyle = '#aaa';
      ctx.fillRect(0, 0, width, height);

      var scale_factor = width / size;
      var left = start * scale_factor;
      var middle = current * scale_factor;
      var right = end * scale_factor;

      ctx.fillStyle = '#a0a';
      ctx.fillRect(left, 0, middle - left, height);
      ctx.fillStyle = '#aa0';
      ctx.fillRect(middle, 0, right - middle, height);
    },

    copyToClipboard_: function() {
      var properties = this.selectedStream ||
          this.selectedPlayer.properties || false;
      if (!properties) {
        return;
      }
      var stringBuffer = [];

      for (var key in properties) {
        var value = properties[key];
        stringBuffer.push(key.toString());
        stringBuffer.push(': ');
        stringBuffer.push(value.toString());
        stringBuffer.push('\n');
      }

      this.clipboardTextarea.value = stringBuffer.join('');
      this.clipboardTextarea.classList.remove('hidden');
      this.clipboardTextarea.focus();
      this.clipboardTextarea.select();

      // The act of copying anything from the textarea gets canceled
      // if the element in question gets the class 'hidden' (which contains the
      // css property display:none) before the event is finished. For this, it
      // is necessary put the property setting on the event loop to be executed
      // after the copy has taken place.
      this.clipboardTextarea.oncopy = function(event) {
        setTimeout(function(element) {
          event.target.classList.add('hidden');
        }, 0);
      };
    },

    onTextChange_: function(event) {
      var text = this.filterText.value.toLowerCase();
      var parts = text.split(',').map(function(part) {
        return part.trim();
      }).filter(function(part) {
        return part.trim().length > 0;
      });

      this.filterFunction = function(text) {
        text = text.toLowerCase();
        return parts.length === 0 || parts.some(function(part) {
          return text.indexOf(part) != -1;
        });
      };

      if (this.selectedPlayer) {
        removeChildren(this.logTable.querySelector('tbody'));
        this.selectedPlayerLogIndex = 0;
        this.drawLog_();
      }
    },
  };

  return ClientRenderer;
})();


media.initialize(new Manager(new ClientRenderer()));
