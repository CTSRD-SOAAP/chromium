// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


var peerConnectionsListElem = null;
var ssrcInfoManager = null;
var peerConnectionUpdateTable = null;
var statsTable = null;
var dumpCreator = null;
/** A map from peer connection id to the PeerConnectionRecord. */
var peerConnectionDataStore = {};

/** A simple class to store the updates and stats data for a peer connection. */
var PeerConnectionRecord = (function() {
  /** @constructor */
  function PeerConnectionRecord() {
    /** @private */
    this.record_ = {
      constraints: {},
      servers: [],
      stats: {},
      updateLog: [],
      url: '',
    };
  };

  PeerConnectionRecord.prototype = {
    /** @override */
    toJSON: function() {
      return this.record_;
    },

    /**
     * Adds the initilization info of the peer connection.
     * @param {string} url The URL of the web page owning the peer connection.
     * @param {Array} servers STUN servers used by the peer connection.
     * @param {!Object} constraints Media constraints.
     */
    initialize: function(url, servers, constraints) {
      this.record_.url = url;
      this.record_.servers = servers;
      this.record_.constraints = constraints;
    },

    /**
     * @param {string} dataSeriesId The TimelineDataSeries identifier.
     * @return {!TimelineDataSeries}
     */
    getDataSeries: function(dataSeriesId) {
      return this.record_.stats[dataSeriesId];
    },

    /**
     * @param {string} dataSeriesId The TimelineDataSeries identifier.
     * @param {!TimelineDataSeries} dataSeries The TimelineDataSeries to set to.
     */
    setDataSeries: function(dataSeriesId, dataSeries) {
      this.record_.stats[dataSeriesId] = dataSeries;
    },

    /**
     * @param {string} type The type of the update.
     * @param {string} value The value of the update.
     */
    addUpdate: function(type, value) {
      this.record_.updateLog.push({
        time: (new Date()).toLocaleString(),
        type: type,
        value: value,
      });
    },
  };

  return PeerConnectionRecord;
})();

// The maximum number of data points bufferred for each stats. Old data points
// will be shifted out when the buffer is full.
var MAX_STATS_DATA_POINT_BUFFER_SIZE = 1000;

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A TimelineDataSeries collects an ordered series of (time, value) pairs,
 * and converts them to graph points.  It also keeps track of its color and
 * current visibility state.
 * It keeps MAX_STATS_DATA_POINT_BUFFER_SIZE data points at most. Old data
 * points will be dropped when it reaches this size.
 */
var TimelineDataSeries = (function() {
  'use strict';

  /**
   * @constructor
   */
  function TimelineDataSeries() {
    // List of DataPoints in chronological order.
    this.dataPoints_ = [];

    // Default color.  Should always be overridden prior to display.
    this.color_ = 'red';
    // Whether or not the data series should be drawn.
    this.isVisible_ = true;

    this.cacheStartTime_ = null;
    this.cacheStepSize_ = 0;
    this.cacheValues_ = [];
  }

  TimelineDataSeries.prototype = {
    /**
     * @override
     */
    toJSON: function() {
      if (this.dataPoints_.length < 1)
        return {};

      var values = [];
      for (var i = 0; i < this.dataPoints_.length; ++i) {
        values.push(this.dataPoints_[i].value);
      }
      return {
        startTime: this.dataPoints_[0].time,
        endTime: this.dataPoints_[this.dataPoints_.length - 1].time,
        values: JSON.stringify(values),
      };
    },

    /**
     * Adds a DataPoint to |this| with the specified time and value.
     * DataPoints are assumed to be received in chronological order.
     */
    addPoint: function(timeTicks, value) {
      var time = new Date(timeTicks);
      this.dataPoints_.push(new DataPoint(time, value));

      if (this.dataPoints_.length > MAX_STATS_DATA_POINT_BUFFER_SIZE)
        this.dataPoints_.shift();
    },

    isVisible: function() {
      return this.isVisible_;
    },

    show: function(isVisible) {
      this.isVisible_ = isVisible;
    },

    getColor: function() {
      return this.color_;
    },

    setColor: function(color) {
      this.color_ = color;
    },

    /**
     * Returns a list containing the values of the data series at |count|
     * points, starting at |startTime|, and |stepSize| milliseconds apart.
     * Caches values, so showing/hiding individual data series is fast.
     */
    getValues: function(startTime, stepSize, count) {
      // Use cached values, if we can.
      if (this.cacheStartTime_ == startTime &&
          this.cacheStepSize_ == stepSize &&
          this.cacheValues_.length == count) {
        return this.cacheValues_;
      }

      // Do all the work.
      this.cacheValues_ = this.getValuesInternal_(startTime, stepSize, count);
      this.cacheStartTime_ = startTime;
      this.cacheStepSize_ = stepSize;

      return this.cacheValues_;
    },

    /**
     * Returns the cached |values| in the specified time period.
     */
    getValuesInternal_: function(startTime, stepSize, count) {
      var values = [];
      var nextPoint = 0;
      var currentValue = 0;
      var time = startTime;
      for (var i = 0; i < count; ++i) {
        while (nextPoint < this.dataPoints_.length &&
               this.dataPoints_[nextPoint].time < time) {
          currentValue = this.dataPoints_[nextPoint].value;
          ++nextPoint;
        }
        values[i] = currentValue;
        time += stepSize;
      }
      return values;
    }
  };

  /**
   * A single point in a data series.  Each point has a time, in the form of
   * milliseconds since the Unix epoch, and a numeric value.
   * @constructor
   */
  function DataPoint(time, value) {
    this.time = time;
    this.value = value;
  }

  return TimelineDataSeries;
})();

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.



/**
 * Get the ssrc if |report| is an ssrc report.
 *
 * @param {!Object} report The object contains id, type, and stats, where stats
 *     is the object containing timestamp and values, which is an array of
 *     strings, whose even index entry is the name of the stat, and the odd
 *     index entry is the value.
 * @return {?string} The ssrc.
 */
function GetSsrcFromReport(report) {
  if (report.type != 'ssrc') {
    console.warn("Trying to get ssrc from non-ssrc report.");
    return null;
  }

  // If the 'ssrc' name-value pair exists, return the value; otherwise, return
  // the report id.
  // The 'ssrc' name-value pair only exists in an upcoming Libjingle change. Old
  // versions use id to refer to the ssrc.
  //
  // TODO(jiayl): remove the fallback to id once the Libjingle change is rolled
  // to Chrome.
  if (report.stats && report.stats.values) {
    for (var i = 0; i < report.stats.values.length - 1; i += 2) {
      if (report.stats.values[i] == 'ssrc') {
        return report.stats.values[i + 1];
      }
    }
  }
  return report.id;
};

/**
 * SsrcInfoManager stores the ssrc stream info extracted from SDP.
 */
var SsrcInfoManager = (function() {
  'use strict';

  /**
   * @constructor
   */
  function SsrcInfoManager() {
    /**
     * Map from ssrc id to an object containing all the stream properties.
     * @type {!Object.<string, !Object.<string>>}
     * @private
     */
    this.streamInfoContainer_ = {};

    /**
     * The string separating attibutes in an SDP.
     * @type {string}
     * @const
     * @private
     */
    this.ATTRIBUTE_SEPARATOR_ = /[\r,\n]/;

    /**
     * The regex separating fields within an ssrc description.
     * @type {RegExp}
     * @const
     * @private
     */
    this.FIELD_SEPARATOR_REGEX_ = / .*:/;

    /**
     * The prefix string of an ssrc description.
     * @type {string}
     * @const
     * @private
     */
    this.SSRC_ATTRIBUTE_PREFIX_ = 'a=ssrc:';

    /**
     * The className of the ssrc info parent element.
     * @type {string}
     * @const
     */
    this.SSRC_INFO_BLOCK_CLASS = 'ssrc-info-block';
  }

  SsrcInfoManager.prototype = {
    /**
     * Extracts the stream information from |sdp| and saves it.
     * For example:
     *     a=ssrc:1234 msid:abcd
     *     a=ssrc:1234 label:hello
     *
     * @param {string} sdp The SDP string.
     */
    addSsrcStreamInfo: function(sdp) {
      var attributes = sdp.split(this.ATTRIBUTE_SEPARATOR_);
      for (var i = 0; i < attributes.length; ++i) {
        // Check if this is a ssrc attribute.
        if (attributes[i].indexOf(this.SSRC_ATTRIBUTE_PREFIX_) != 0)
          continue;

        var nextFieldIndex = attributes[i].search(this.FIELD_SEPARATOR_REGEX_);

        if (nextFieldIndex == -1)
          continue;

        var ssrc = attributes[i].substring(this.SSRC_ATTRIBUTE_PREFIX_.length,
                                           nextFieldIndex);
        if (!this.streamInfoContainer_[ssrc])
          this.streamInfoContainer_[ssrc] = {};

        // Make |rest| starting at the next field.
        var rest = attributes[i].substring(nextFieldIndex + 1);
        var name, value;
        while (rest.length > 0) {
          nextFieldIndex = rest.search(this.FIELD_SEPARATOR_REGEX_);
          if (nextFieldIndex == -1)
            nextFieldIndex = rest.length;

          // The field name is the string before the colon.
          name = rest.substring(0, rest.indexOf(':'));
          // The field value is from after the colon to the next field.
          value = rest.substring(rest.indexOf(':') + 1, nextFieldIndex);
          this.streamInfoContainer_[ssrc][name] = value;

          // Move |rest| to the start of the next field.
          rest = rest.substring(nextFieldIndex + 1);
        }
      }
    },

    /**
     * @param {string} sdp The ssrc id.
     * @return {!Object.<string>} The object containing the ssrc infomation.
     */
    getStreamInfo: function(ssrc) {
      return this.streamInfoContainer_[ssrc];
    },

    /**
     * Populate the ssrc information into |parentElement|, each field as a
     * DIV element.
     *
     * @param {!Element} parentElement The parent element for the ssrc info.
     * @param {string} ssrc The ssrc id.
     */
    populateSsrcInfo: function(parentElement, ssrc) {
      if (!this.streamInfoContainer_[ssrc])
        return;

      parentElement.className = this.SSRC_INFO_BLOCK_CLASS;

      var fieldElement;
      for (var property in this.streamInfoContainer_[ssrc]) {
        fieldElement = document.createElement('div');
        parentElement.appendChild(fieldElement);
        fieldElement.textContent =
            property + ':' + this.streamInfoContainer_[ssrc][property];
      }
    }
  };

  return SsrcInfoManager;
})();

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//
// This file contains helper methods to draw the stats timeline graphs.
// Each graph represents a series of stats report for a PeerConnection,
// e.g. 1234-0-ssrc-abcd123-bytesSent is the graph for the series of bytesSent
// for ssrc-abcd123 of PeerConnection 0 in process 1234.
// The graphs are drawn as CANVAS, grouped per report type per PeerConnection.
// Each group has an expand/collapse button and is collapsed initially.
//

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A TimelineGraphView displays a timeline graph on a canvas element.
 */
var TimelineGraphView = (function() {
  'use strict';

  // Default starting scale factor, in terms of milliseconds per pixel.
  var DEFAULT_SCALE = 1000;

  // Maximum number of labels placed vertically along the sides of the graph.
  var MAX_VERTICAL_LABELS = 6;

  // Vertical spacing between labels and between the graph and labels.
  var LABEL_VERTICAL_SPACING = 4;
  // Horizontal spacing between vertically placed labels and the edges of the
  // graph.
  var LABEL_HORIZONTAL_SPACING = 3;
  // Horizintal spacing between two horitonally placed labels along the bottom
  // of the graph.
  var LABEL_LABEL_HORIZONTAL_SPACING = 25;

  // Length of ticks, in pixels, next to y-axis labels.  The x-axis only has
  // one set of labels, so it can use lines instead.
  var Y_AXIS_TICK_LENGTH = 10;

  var GRID_COLOR = '#CCC';
  var TEXT_COLOR = '#000';
  var BACKGROUND_COLOR = '#FFF';

  /**
   * @constructor
   */
  function TimelineGraphView(divId, canvasId) {
    this.scrollbar_ = {position_: 0, range_: 0};

    this.graphDiv_ = $(divId);
    this.canvas_ = $(canvasId);

    // Set the range and scale of the graph.  Times are in milliseconds since
    // the Unix epoch.

    // All measurements we have must be after this time.
    this.startTime_ = 0;
    // The current rightmost position of the graph is always at most this.
    this.endTime_ = 1;

    this.graph_ = null;

    // Initialize the scrollbar.
    this.updateScrollbarRange_(true);
  }

  TimelineGraphView.prototype = {
    // Returns the total length of the graph, in pixels.
    getLength_: function() {
      var timeRange = this.endTime_ - this.startTime_;
      // Math.floor is used to ignore the last partial area, of length less
      // than DEFAULT_SCALE.
      return Math.floor(timeRange / DEFAULT_SCALE);
    },

    /**
     * Returns true if the graph is scrolled all the way to the right.
     */
    graphScrolledToRightEdge_: function() {
      return this.scrollbar_.position_ == this.scrollbar_.range_;
    },

    /**
     * Update the range of the scrollbar.  If |resetPosition| is true, also
     * sets the slider to point at the rightmost position and triggers a
     * repaint.
     */
    updateScrollbarRange_: function(resetPosition) {
      var scrollbarRange = this.getLength_() - this.canvas_.width;
      if (scrollbarRange < 0)
        scrollbarRange = 0;

      // If we've decreased the range to less than the current scroll position,
      // we need to move the scroll position.
      if (this.scrollbar_.position_ > scrollbarRange)
        resetPosition = true;

      this.scrollbar_.range_ = scrollbarRange;
      if (resetPosition) {
        this.scrollbar_.position_ = scrollbarRange;
        this.repaint();
      }
    },

    /**
     * Sets the date range displayed on the graph, switches to the default
     * scale factor, and moves the scrollbar all the way to the right.
     */
    setDateRange: function(startDate, endDate) {
      this.startTime_ = startDate.getTime();
      this.endTime_ = endDate.getTime();

      // Safety check.
      if (this.endTime_ <= this.startTime_)
        this.startTime_ = this.endTime_ - 1;

      this.updateScrollbarRange_(true);
    },

    /**
     * Updates the end time at the right of the graph to be the current time.
     * Specifically, updates the scrollbar's range, and if the scrollbar is
     * all the way to the right, keeps it all the way to the right.  Otherwise,
     * leaves the view as-is and doesn't redraw anything.
     */
    updateEndDate: function() {
      this.endTime_ = (new Date()).getTime();
      this.updateScrollbarRange_(this.graphScrolledToRightEdge_());
    },

    getStartDate: function() {
      return new Date(this.startTime_);
    },

    /**
     * Replaces the current TimelineDataSeries with |dataSeries|.
     */
    setDataSeries: function(dataSeries) {
      // Simply recreates the Graph.
      this.graph_ = new Graph();
      for (var i = 0; i < dataSeries.length; ++i)
        this.graph_.addDataSeries(dataSeries[i]);
      this.repaint();
    },

    /**
    * Adds |dataSeries| to the current graph.
    */
    addDataSeries: function(dataSeries) {
      if (!this.graph_)
        this.graph_ = new Graph();
      this.graph_.addDataSeries(dataSeries);
      this.repaint();
    },

    /**
     * Draws the graph on |canvas_|.
     */
    repaint: function() {
      this.repaintTimerRunning_ = false;

      var width = this.canvas_.width;
      var height = this.canvas_.height;
      var context = this.canvas_.getContext('2d');

      // Clear the canvas.
      context.fillStyle = BACKGROUND_COLOR;
      context.fillRect(0, 0, width, height);

      // Try to get font height in pixels.  Needed for layout.
      var fontHeightString = context.font.match(/([0-9]+)px/)[1];
      var fontHeight = parseInt(fontHeightString);

      // Safety check, to avoid drawing anything too ugly.
      if (fontHeightString.length == 0 || fontHeight <= 0 ||
          fontHeight * 4 > height || width < 50) {
        return;
      }

      // Save current transformation matrix so we can restore it later.
      context.save();

      // The center of an HTML canvas pixel is technically at (0.5, 0.5).  This
      // makes near straight lines look bad, due to anti-aliasing.  This
      // translation reduces the problem a little.
      context.translate(0.5, 0.5);

      // Figure out what time values to display.
      var position = this.scrollbar_.position_;
      // If the entire time range is being displayed, align the right edge of
      // the graph to the end of the time range.
      if (this.scrollbar_.range_ == 0)
        position = this.getLength_() - this.canvas_.width;
      var visibleStartTime = this.startTime_ + position * DEFAULT_SCALE;

      // Make space at the bottom of the graph for the time labels, and then
      // draw the labels.
      var textHeight = height;
      height -= fontHeight + LABEL_VERTICAL_SPACING;
      this.drawTimeLabels(context, width, height, textHeight, visibleStartTime);

      // Draw outline of the main graph area.
      context.strokeStyle = GRID_COLOR;
      context.strokeRect(0, 0, width - 1, height - 1);

      if (this.graph_) {
        // Layout graph and have them draw their tick marks.
        this.graph_.layout(
            width, height, fontHeight, visibleStartTime, DEFAULT_SCALE);
        this.graph_.drawTicks(context);

        // Draw the lines of all graphs, and then draw their labels.
        this.graph_.drawLines(context);
        this.graph_.drawLabels(context);
      }

      // Restore original transformation matrix.
      context.restore();
    },

    /**
     * Draw time labels below the graph.  Takes in start time as an argument
     * since it may not be |startTime_|, when we're displaying the entire
     * time range.
     */
    drawTimeLabels: function(context, width, height, textHeight, startTime) {
      // Draw the labels 1 minute apart.
      var timeStep = 1000 * 60;

      // Find the time for the first label.  This time is a perfect multiple of
      // timeStep because of how UTC times work.
      var time = Math.ceil(startTime / timeStep) * timeStep;

      context.textBaseline = 'bottom';
      context.textAlign = 'center';
      context.fillStyle = TEXT_COLOR;
      context.strokeStyle = GRID_COLOR;

      // Draw labels and vertical grid lines.
      while (true) {
        var x = Math.round((time - startTime) / DEFAULT_SCALE);
        if (x >= width)
          break;
        var text = (new Date(time)).toLocaleTimeString();
        context.fillText(text, x, textHeight);
        context.beginPath();
        context.lineTo(x, 0);
        context.lineTo(x, height);
        context.stroke();
        time += timeStep;
      }
    },

    getDataSeriesCount: function() {
      if (this.graph_)
        return this.graph_.dataSeries_.length;
      return 0;
    },

    hasDataSeries: function(dataSeries) {
      if (this.graph_)
        return this.graph_.hasDataSeries(dataSeries);
      return false;
    },

  };

  /**
   * A Graph is responsible for drawing all the TimelineDataSeries that have
   * the same data type.  Graphs are responsible for scaling the values, laying
   * out labels, and drawing both labels and lines for its data series.
   */
  var Graph = (function() {
    /**
     * @constructor
     */
    function Graph() {
      this.dataSeries_ = [];

      // Cached properties of the graph, set in layout.
      this.width_ = 0;
      this.height_ = 0;
      this.fontHeight_ = 0;
      this.startTime_ = 0;
      this.scale_ = 0;

      // At least the highest value in the displayed range of the graph.
      // Used for scaling and setting labels.  Set in layoutLabels.
      this.max_ = 0;

      // Cached text of equally spaced labels.  Set in layoutLabels.
      this.labels_ = [];
    }

    /**
     * A Label is the label at a particular position along the y-axis.
     * @constructor
     */
    function Label(height, text) {
      this.height = height;
      this.text = text;
    }

    Graph.prototype = {
      addDataSeries: function(dataSeries) {
        this.dataSeries_.push(dataSeries);
      },

      hasDataSeries: function(dataSeries) {
        for (var i = 0; i < this.dataSeries_.length; ++i) {
          if (this.dataSeries_[i] == dataSeries)
            return true;
        }
        return false;
      },

      /**
       * Returns a list of all the values that should be displayed for a given
       * data series, using the current graph layout.
       */
      getValues: function(dataSeries) {
        if (!dataSeries.isVisible())
          return null;
        return dataSeries.getValues(this.startTime_, this.scale_, this.width_);
      },

      /**
       * Updates the graph's layout.  In particular, both the max value and
       * label positions are updated.  Must be called before calling any of the
       * drawing functions.
       */
      layout: function(width, height, fontHeight, startTime, scale) {
        this.width_ = width;
        this.height_ = height;
        this.fontHeight_ = fontHeight;
        this.startTime_ = startTime;
        this.scale_ = scale;

        // Find largest value.
        var max = 0;
        for (var i = 0; i < this.dataSeries_.length; ++i) {
          var values = this.getValues(this.dataSeries_[i]);
          if (!values)
            continue;
          for (var j = 0; j < values.length; ++j) {
            if (values[j] > max)
              max = values[j];
          }
        }

        this.layoutLabels_(max);
      },

      /**
       * Lays out labels and sets |max_|, taking the time units into
       * consideration.  |maxValue| is the actual maximum value, and
       * |max_| will be set to the value of the largest label, which
       * will be at least |maxValue|.
       */
      layoutLabels_: function(maxValue) {
        if (maxValue < 1024) {
          this.layoutLabelsBasic_(maxValue, 0);
          return;
        }

        // Find appropriate units to use.
        var units = ['', 'k', 'M', 'G', 'T', 'P'];
        // Units to use for labels.  0 is '1', 1 is K, etc.
        // We start with 1, and work our way up.
        var unit = 1;
        maxValue /= 1024;
        while (units[unit + 1] && maxValue >= 1024) {
          maxValue /= 1024;
          ++unit;
        }

        // Calculate labels.
        this.layoutLabelsBasic_(maxValue, 1);

        // Append units to labels.
        for (var i = 0; i < this.labels_.length; ++i)
          this.labels_[i] += ' ' + units[unit];

        // Convert |max_| back to unit '1'.
        this.max_ *= Math.pow(1024, unit);
      },

      /**
       * Same as layoutLabels_, but ignores units.  |maxDecimalDigits| is the
       * maximum number of decimal digits allowed.  The minimum allowed
       * difference between two adjacent labels is 10^-|maxDecimalDigits|.
       */
      layoutLabelsBasic_: function(maxValue, maxDecimalDigits) {
        this.labels_ = [];
        // No labels if |maxValue| is 0.
        if (maxValue == 0) {
          this.max_ = maxValue;
          return;
        }

        // The maximum number of equally spaced labels allowed.  |fontHeight_|
        // is doubled because the top two labels are both drawn in the same
        // gap.
        var minLabelSpacing = 2 * this.fontHeight_ + LABEL_VERTICAL_SPACING;

        // The + 1 is for the top label.
        var maxLabels = 1 + this.height_ / minLabelSpacing;
        if (maxLabels < 2) {
          maxLabels = 2;
        } else if (maxLabels > MAX_VERTICAL_LABELS) {
          maxLabels = MAX_VERTICAL_LABELS;
        }

        // Initial try for step size between conecutive labels.
        var stepSize = Math.pow(10, -maxDecimalDigits);
        // Number of digits to the right of the decimal of |stepSize|.
        // Used for formating label strings.
        var stepSizeDecimalDigits = maxDecimalDigits;

        // Pick a reasonable step size.
        while (true) {
          // If we use a step size of |stepSize| between labels, we'll need:
          //
          // Math.ceil(maxValue / stepSize) + 1
          //
          // labels.  The + 1 is because we need labels at both at 0 and at
          // the top of the graph.

          // Check if we can use steps of size |stepSize|.
          if (Math.ceil(maxValue / stepSize) + 1 <= maxLabels)
            break;
          // Check |stepSize| * 2.
          if (Math.ceil(maxValue / (stepSize * 2)) + 1 <= maxLabels) {
            stepSize *= 2;
            break;
          }
          // Check |stepSize| * 5.
          if (Math.ceil(maxValue / (stepSize * 5)) + 1 <= maxLabels) {
            stepSize *= 5;
            break;
          }
          stepSize *= 10;
          if (stepSizeDecimalDigits > 0)
            --stepSizeDecimalDigits;
        }

        // Set the max so it's an exact multiple of the chosen step size.
        this.max_ = Math.ceil(maxValue / stepSize) * stepSize;

        // Create labels.
        for (var label = this.max_; label >= 0; label -= stepSize)
          this.labels_.push(label.toFixed(stepSizeDecimalDigits));
      },

      /**
       * Draws tick marks for each of the labels in |labels_|.
       */
      drawTicks: function(context) {
        var x1;
        var x2;
        x1 = this.width_ - 1;
        x2 = this.width_ - 1 - Y_AXIS_TICK_LENGTH;

        context.fillStyle = GRID_COLOR;
        context.beginPath();
        for (var i = 1; i < this.labels_.length - 1; ++i) {
          // The rounding is needed to avoid ugly 2-pixel wide anti-aliased
          // lines.
          var y = Math.round(this.height_ * i / (this.labels_.length - 1));
          context.moveTo(x1, y);
          context.lineTo(x2, y);
        }
        context.stroke();
      },

      /**
       * Draws a graph line for each of the data series.
       */
      drawLines: function(context) {
        // Factor by which to scale all values to convert them to a number from
        // 0 to height - 1.
        var scale = 0;
        var bottom = this.height_ - 1;
        if (this.max_)
          scale = bottom / this.max_;

        // Draw in reverse order, so earlier data series are drawn on top of
        // subsequent ones.
        for (var i = this.dataSeries_.length - 1; i >= 0; --i) {
          var values = this.getValues(this.dataSeries_[i]);
          if (!values)
            continue;
          context.strokeStyle = this.dataSeries_[i].getColor();
          context.beginPath();
          for (var x = 0; x < values.length; ++x) {
            // The rounding is needed to avoid ugly 2-pixel wide anti-aliased
            // horizontal lines.
            context.lineTo(x, bottom - Math.round(values[x] * scale));
          }
          context.stroke();
        }
      },

      /**
       * Draw labels in |labels_|.
       */
      drawLabels: function(context) {
        if (this.labels_.length == 0)
          return;
        var x = this.width_ - LABEL_HORIZONTAL_SPACING;

        // Set up the context.
        context.fillStyle = TEXT_COLOR;
        context.textAlign = 'right';

        // Draw top label, which is the only one that appears below its tick
        // mark.
        context.textBaseline = 'top';
        context.fillText(this.labels_[0], x, 0);

        // Draw all the other labels.
        context.textBaseline = 'bottom';
        var step = (this.height_ - 1) / (this.labels_.length - 1);
        for (var i = 1; i < this.labels_.length; ++i)
          context.fillText(this.labels_[i], x, step * i);
      }
    };

    return Graph;
  })();

  return TimelineGraphView;
})();


var STATS_GRAPH_CONTAINER_HEADING_CLASS = 'stats-graph-container-heading';

// Specifies which stats should be drawn on the 'bweCompound' graph and how.
var bweCompoundGraphConfig = {
  googAvailableSendBandwidth: {color: 'red'},
  googTargetEncBitrateCorrected: {color: 'purple'},
  googActualEncBitrate: {color: 'orange'},
  googRetransmitBitrate: {color: 'blue'},
  googTransmitBitrate: {color: 'green'},
};

// Converts the last entry of |srcDataSeries| from the total amount to the
// amount per second.
var totalToPerSecond = function(srcDataSeries) {
  var length = srcDataSeries.dataPoints_.length;
  if (length >= 2) {
    var lastDataPoint = srcDataSeries.dataPoints_[length - 1];
    var secondLastDataPoint = srcDataSeries.dataPoints_[length - 2];
    return (lastDataPoint.value - secondLastDataPoint.value) * 1000 /
           (lastDataPoint.time - secondLastDataPoint.time);
  }

  return 0;
};

// Converts the value of total bytes to bits per second.
var totalBytesToBitsPerSecond = function(srcDataSeries) {
  return totalToPerSecond(srcDataSeries) * 8;
};

// Specifies which stats should be converted before drawn and how.
// |convertedName| is the name of the converted value, |convertFunction|
// is the function used to calculate the new converted value based on the
// original dataSeries.
var dataConversionConfig = {
  packetsSent: {
    convertedName: 'packetsSentPerSecond',
    convertFunction: totalToPerSecond,
  },
  bytesSent: {
    convertedName: 'bitsSentPerSecond',
    convertFunction: totalBytesToBitsPerSecond,
  },
  packetsReceived: {
    convertedName: 'packetsReceivedPerSecond',
    convertFunction: totalToPerSecond,
  },
  bytesReceived: {
    convertedName: 'bitsReceivedPerSecond',
    convertFunction: totalBytesToBitsPerSecond,
  },
  // This is due to a bug of wrong units reported for googTargetEncBitrate.
  // TODO (jiayl): remove this when the unit bug is fixed.
  googTargetEncBitrate: {
    convertedName: 'googTargetEncBitrateCorrected',
    convertFunction: function (srcDataSeries) {
      var length = srcDataSeries.dataPoints_.length;
      var lastDataPoint = srcDataSeries.dataPoints_[length - 1];
      if (lastDataPoint.value < 5000)
        return lastDataPoint.value * 1000;
      return lastDataPoint.value;
    }
  }
};


// The object contains the stats names that should not be added to the graph,
// even if they are numbers.
var statsNameBlackList = {
  'ssrc': true,
  'googTrackId': true,
  'googComponent': true,
  'googLocalAddress': true,
  'googRemoteAddress': true,
};

var graphViews = {};

// Returns number parsed from |value|, or NaN if the stats name is black-listed.
function getNumberFromValue(name, value) {
  if (statsNameBlackList[name])
    return NaN;
  return parseFloat(value);
}

// Adds the stats report |report| to the timeline graph for the given
// |peerConnectionElement|.
function drawSingleReport(peerConnectionElement, report) {
  var reportType = report.type;
  var reportId = report.id;
  var stats = report.stats;
  if (!stats || !stats.values)
    return;

  for (var i = 0; i < stats.values.length - 1; i = i + 2) {
    var rawLabel = stats.values[i];
    var rawDataSeriesId = reportId + '-' + rawLabel;
    var rawValue = getNumberFromValue(rawLabel, stats.values[i + 1]);
    if (isNaN(rawValue)) {
      // We do not draw non-numerical values, but still want to record it in the
      // data series.
      addDataSeriesPoint(peerConnectionElement,
                         rawDataSeriesId, stats.timestamp,
                         rawLabel, stats.values[i + 1]);
      continue;
    }

    var finalDataSeriesId = rawDataSeriesId;
    var finalLabel = rawLabel;
    var finalValue = rawValue;
    // We need to convert the value if dataConversionConfig[rawLabel] exists.
    if (dataConversionConfig[rawLabel]) {
      // Updates the original dataSeries before the conversion.
      addDataSeriesPoint(peerConnectionElement,
                         rawDataSeriesId, stats.timestamp,
                         rawLabel, rawValue);

      // Convert to another value to draw on graph, using the original
      // dataSeries as input.
      finalValue = dataConversionConfig[rawLabel].convertFunction(
          peerConnectionDataStore[peerConnectionElement.id].getDataSeries(
              rawDataSeriesId));
      finalLabel = dataConversionConfig[rawLabel].convertedName;
      finalDataSeriesId = reportId + '-' + finalLabel;
    }

    // Updates the final dataSeries to draw.
    addDataSeriesPoint(peerConnectionElement,
                       finalDataSeriesId,
                       stats.timestamp,
                       finalLabel,
                       finalValue);

    // Updates the graph.
    var graphType = bweCompoundGraphConfig[finalLabel] ?
                    'bweCompound' : finalLabel;
    var graphViewId =
        peerConnectionElement.id + '-' + reportId + '-' + graphType;

    if (!graphViews[graphViewId]) {
      graphViews[graphViewId] = createStatsGraphView(peerConnectionElement,
                                                     report,
                                                     graphType);
      var date = new Date(stats.timestamp);
      graphViews[graphViewId].setDateRange(date, date);
    }
    // Adds the new dataSeries to the graphView. We have to do it here to cover
    // both the simple and compound graph cases.
    var dataSeries =
        peerConnectionDataStore[peerConnectionElement.id].getDataSeries(
            finalDataSeriesId);
    if (!graphViews[graphViewId].hasDataSeries(dataSeries))
      graphViews[graphViewId].addDataSeries(dataSeries);
    graphViews[graphViewId].updateEndDate();
  }
}

// Makes sure the TimelineDataSeries with id |dataSeriesId| is created,
// and adds the new data point to it.
function addDataSeriesPoint(
    peerConnectionElement, dataSeriesId, time, label, value) {
  var dataSeries =
    peerConnectionDataStore[peerConnectionElement.id].getDataSeries(
        dataSeriesId);
  if (!dataSeries) {
    dataSeries = new TimelineDataSeries();
    peerConnectionDataStore[peerConnectionElement.id].setDataSeries(
        dataSeriesId, dataSeries);
    if (bweCompoundGraphConfig[label]) {
      dataSeries.setColor(bweCompoundGraphConfig[label].color);
    }
  }
  dataSeries.addPoint(time, value);
}

// Ensures a div container to hold all stats graphs for one track is created as
// a child of |peerConnectionElement|.
function ensureStatsGraphTopContainer(peerConnectionElement, report) {
  var containerId = peerConnectionElement.id + '-' +
      report.type + '-' + report.id + '-graph-container';
  var container = $(containerId);
  if (!container) {
    container = document.createElement('details');
    container.id = containerId;
    container.className = 'stats-graph-container';

    peerConnectionElement.appendChild(container);
    container.innerHTML ='<summary><span></span></summary>';
    container.firstChild.firstChild.className =
        STATS_GRAPH_CONTAINER_HEADING_CLASS;
    container.firstChild.firstChild.textContent =
        'Stats graphs for ' + report.id;

    if (report.type == 'ssrc') {
      var ssrcInfoElement = document.createElement('div');
      container.firstChild.appendChild(ssrcInfoElement);
      ssrcInfoManager.populateSsrcInfo(ssrcInfoElement,
                                       GetSsrcFromReport(report));
    }
  }
  return container;
}

// Creates the container elements holding a timeline graph
// and the TimelineGraphView object.
function createStatsGraphView(
    peerConnectionElement, report, statsName) {
  var topContainer = ensureStatsGraphTopContainer(peerConnectionElement,
                                                  report);

  var graphViewId =
      peerConnectionElement.id + '-' + report.id + '-' + statsName;
  var divId = graphViewId + '-div';
  var canvasId = graphViewId + '-canvas';
  var container = document.createElement("div");
  container.className = 'stats-graph-sub-container';

  topContainer.appendChild(container);
  container.innerHTML = '<div>' + statsName + '</div>' +
      '<div id=' + divId + '><canvas id=' + canvasId + '></canvas></div>';
  if (statsName == 'bweCompound') {
      container.insertBefore(
          createBweCompoundLegend(peerConnectionElement, report.id),
          $(divId));
  }
  return new TimelineGraphView(divId, canvasId);
}

// Creates the legend section for the bweCompound graph.
// Returns the legend element.
function createBweCompoundLegend(peerConnectionElement, reportId) {
  var legend = document.createElement('div');
  for (var prop in bweCompoundGraphConfig) {
    var div = document.createElement('div');
    legend.appendChild(div);
    div.innerHTML = '<input type=checkbox checked></input>' + prop;
    div.style.color = bweCompoundGraphConfig[prop].color;
    div.dataSeriesId = reportId + '-' + prop;
    div.graphViewId =
        peerConnectionElement.id + '-' + reportId + '-bweCompound';
    div.firstChild.addEventListener('click', function(event) {
        var target =
            peerConnectionDataStore[peerConnectionElement.id].getDataSeries(
                event.target.parentNode.dataSeriesId);
        target.show(event.target.checked);
        graphViews[event.target.parentNode.graphViewId].repaint();
    });
  }
  return legend;
}

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * Maintains the stats table.
 * @param {SsrcInfoManager} ssrcInfoManager The source of the ssrc info.
 */
var StatsTable = (function(ssrcInfoManager) {
  'use strict';

  /**
   * @param {SsrcInfoManager} ssrcInfoManager The source of the ssrc info.
   * @constructor
   */
  function StatsTable(ssrcInfoManager) {
    /**
     * @type {SsrcInfoManager}
     * @private
     */
    this.ssrcInfoManager_ = ssrcInfoManager;
  }

  StatsTable.prototype = {
    /**
     * Adds |report| to the stats table of |peerConnectionElement|.
     *
     * @param {!Element} peerConnectionElement The root element.
     * @param {!Object} report The object containing stats, which is the object
     *     containing timestamp and values, which is an array of strings, whose
     *     even index entry is the name of the stat, and the odd index entry is
     *     the value.
     */
    addStatsReport: function(peerConnectionElement, report) {
      var statsTable = this.ensureStatsTable_(peerConnectionElement, report);

      if (report.stats) {
        this.addStatsToTable_(statsTable,
                              report.stats.timestamp, report.stats.values);
      }
    },

    /**
     * Ensure the DIV container for the stats tables is created as a child of
     * |peerConnectionElement|.
     *
     * @param {!Element} peerConnectionElement The root element.
     * @return {!Element} The stats table container.
     * @private
     */
    ensureStatsTableContainer_: function(peerConnectionElement) {
      var containerId = peerConnectionElement.id + '-table-container';
      var container = $(containerId);
      if (!container) {
        container = document.createElement('div');
        container.id = containerId;
        container.className = 'stats-table-container';
        peerConnectionElement.appendChild(container);
      }
      return container;
    },

    /**
     * Ensure the stats table for track specified by |report| of PeerConnection
     * |peerConnectionElement| is created.
     *
     * @param {!Element} peerConnectionElement The root element.
     * @param {!Object} report The object containing stats, which is the object
     *     containing timestamp and values, which is an array of strings, whose
     *     even index entry is the name of the stat, and the odd index entry is
     *     the value.
     * @return {!Element} The stats table element.
     * @private
     */
     ensureStatsTable_: function(peerConnectionElement, report) {
      var tableId = peerConnectionElement.id + '-table-' + report.id;
      var table = $(tableId);
      if (!table) {
        var container = this.ensureStatsTableContainer_(peerConnectionElement);
        table = document.createElement('table');
        container.appendChild(table);
        table.id = tableId;
        table.border = 1;

        table.innerHTML = '<tr><th colspan=2></th></tr>';
        table.rows[0].cells[0].textContent = 'Statistics ' + report.id;
        if (report.type == 'ssrc') {
            table.insertRow(1);
            table.rows[1].innerHTML = '<td colspan=2></td>';
            this.ssrcInfoManager_.populateSsrcInfo(
                table.rows[1].cells[0], GetSsrcFromReport(report));
        }
      }
      return table;
    },

    /**
     * Update |statsTable| with |time| and |statsData|.
     *
     * @param {!Element} statsTable Which table to update.
     * @param {number} time The number of miliseconds since epoch.
     * @param {Array.<string>} statsData An array of stats name and value pairs.
     * @private
     */
    addStatsToTable_: function(statsTable, time, statsData) {
      var date = Date(time);
      this.updateStatsTableRow_(statsTable, 'timestamp', date.toLocaleString());
      for (var i = 0; i < statsData.length - 1; i = i + 2) {
        this.updateStatsTableRow_(statsTable, statsData[i], statsData[i + 1]);
      }
    },

    /**
     * Update the value column of the stats row of |rowName| to |value|.
     * A new row is created is this is the first report of this stats.
     *
     * @param {!Element} statsTable Which table to update.
     * @param {string} rowName The name of the row to update.
     * @param {string} value The new value to set.
     * @private
     */
    updateStatsTableRow_: function(statsTable, rowName, value) {
      var trId = statsTable.id + '-' + rowName;
      var trElement = $(trId);
      if (!trElement) {
        trElement = document.createElement('tr');
        trElement.id = trId;
        statsTable.firstChild.appendChild(trElement);
        trElement.innerHTML = '<td>' + rowName + '</td><td></td>';
      }
      trElement.cells[1].textContent = value;
    }
  };

  return StatsTable;
})();

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * The data of a peer connection update.
 * @param {number} pid The id of the renderer.
 * @param {number} lid The id of the peer conneciton inside a renderer.
 * @param {string} type The type of the update.
 * @param {string} value The details of the update.
 * @constructor
 */
var PeerConnectionUpdateEntry = function(pid, lid, type, value) {
  /**
   * @type {number}
   */
  this.pid = pid;

  /**
   * @type {number}
   */
  this.lid = lid;

  /**
   * @type {string}
   */
  this.type = type;

  /**
   * @type {string}
   */
  this.value = value;
};


/**
 * Maintains the peer connection update log table.
 */
var PeerConnectionUpdateTable = (function() {
  'use strict';

  /**
   * @constructor
   */
  function PeerConnectionUpdateTable() {
    /**
     * @type {string}
     * @const
     * @private
     */
    this.UPDATE_LOG_ID_SUFFIX_ = '-update-log';

    /**
     * @type {string}
     * @const
     * @private
     */
    this.UPDATE_LOG_CONTAINER_CLASS_ = 'update-log-container';

    /**
     * @type {string}
     * @const
     * @private
     */
    this.UPDATE_LOG_TABLE_CLASS = 'update-log-table';
  }

  PeerConnectionUpdateTable.prototype = {
    /**
     * Adds the update to the update table as a new row. The type of the update
     * is set to the summary of the cell; clicking the cell will reveal or hide
     * the details as the content of a TextArea element.
     *
     * @param {!Element} peerConnectionElement The root element.
     * @param {!PeerConnectionUpdateEntry} update The update to add.
     */
    addPeerConnectionUpdate: function(peerConnectionElement, update) {
      var tableElement = this.ensureUpdateContainer_(peerConnectionElement);

      var row = document.createElement('tr');
      tableElement.firstChild.appendChild(row);

      row.innerHTML = '<td>' + (new Date()).toLocaleString() + '</td>';

      if (update.value.length == 0) {
        row.innerHTML += '<td>' + update.type + '</td>';
        return;
      }

      row.innerHTML += '<td><details><summary>' + update.type +
          '</summary></details></td>';

      var valueContainer = document.createElement('pre');
      var details = row.cells[1].childNodes[0];
      details.appendChild(valueContainer);
      valueContainer.textContent = update.value;
    },

    /**
     * Makes sure the update log table of the peer connection is created.
     *
     * @param {!Element} peerConnectionElement The root element.
     * @return {!Element} The log table element.
     * @private
     */
    ensureUpdateContainer_: function(peerConnectionElement) {
      var tableId = peerConnectionElement.id + this.UPDATE_LOG_ID_SUFFIX_;
      var tableElement = $(tableId);
      if (!tableElement) {
        var tableContainer = document.createElement('div');
        tableContainer.className = this.UPDATE_LOG_CONTAINER_CLASS_;
        peerConnectionElement.appendChild(tableContainer);

        tableElement = document.createElement('table');
        tableElement.className = this.UPDATE_LOG_TABLE_CLASS;
        tableElement.id = tableId;
        tableElement.border = 1;
        tableContainer.appendChild(tableElement);
        tableElement.innerHTML = '<tr><th>Time</th>' +
            '<th class="update-log-header-event">Event</th></tr>';
      }
      return tableElement;
    }
  };

  return PeerConnectionUpdateTable;
})();

// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * Provides the UI to start and stop RTP recording, forwards the start/stop
 * commands to Chrome, and updates the UI based on dump updates. Also provides
 * creating a file containing all PeerConnection updates and stats.
 */
var DumpCreator = (function() {
  /**
   * @param {Element} containerElement The parent element of the dump creation
   *     UI.
   * @constructor
   */
  function DumpCreator(containerElement) {
    /**
     * True if the RTP packets are being recorded.
     * @type {bool}
     * @private
     */
    this.recording_ = false;

    /**
     * @type {!Object.<string>}
     * @private
     * @const
     */
    this.StatusStrings_ = {
      NOT_STARTED: 'not started.',
      RECORDING: 'recording...',
    },

    /**
     * The status of dump creation.
     * @type {string}
     * @private
     */
    this.status_ = this.StatusStrings_.NOT_STARTED;

    /**
     * The root element of the dump creation UI.
     * @type {Element}
     * @private
     */
    this.root_ = document.createElement('details');

    this.root_.className = 'peer-connection-dump-root';
    containerElement.appendChild(this.root_);
    var summary = document.createElement('summary');
    this.root_.appendChild(summary);
    summary.textContent = 'Create Dump';
    var content = document.createElement('pre');
    this.root_.appendChild(content);

    content.innerHTML = '<button disabled></button> Status: <span></span>' +
        '<div><form><button>' +
        'Download the PeerConnection updates and stats data' +
        '</button></form></div>';
    content.getElementsByTagName('button')[0].addEventListener(
        'click', this.onRtpToggled_.bind(this));
    content.getElementsByTagName('button')[1].addEventListener(
        'click', this.onDownloadData_.bind(this));

    this.updateDisplay_();
  }

  DumpCreator.prototype = {
    /**
     * Downloads the PeerConnection updates and stats data as a file.
     *
     * @private
     */
    onDownloadData_: function() {
      var textBlob =
          new Blob([JSON.stringify(peerConnectionDataStore, null, ' ')],
                                   {type: 'octet/stream'});
      var URL = window.webkitURL.createObjectURL(textBlob);
      this.root_.getElementsByTagName('form')[0].action = URL;
      // The default action of the button will submit the form.
    },

    /**
     * Handles the event of toggling the rtp recording state.
     *
     * @private
     */
    onRtpToggled_: function() {
      if (this.recording_) {
        this.recording_ = false;
        this.status_ = this.StatusStrings_.NOT_STARTED;
        chrome.send('stopRtpRecording');
      } else {
        this.recording_ = true;
        this.status_ = this.StatusStrings_.RECORDING;
        chrome.send('startRtpRecording');
      }
      this.updateDisplay_();
    },

    /**
     * Updates the UI based on the recording status.
     *
     * @private
     */
    updateDisplay_: function() {
      if (this.recording_) {
        this.root_.getElementsByTagName('button')[0].textContent =
            'Stop Recording RTP Packets';
      } else {
        this.root_.getElementsByTagName('button')[0].textContent =
            'Start Recording RTP Packets';
      }

      this.root_.getElementsByTagName('span')[0].textContent = this.status_;
    },

    /**
     * Set the status to the content of the update.
     * @param {!Object} update
     */
    onUpdate: function(update) {
      if (this.recording_) {
        this.status_ = JSON.stringify(update);
        this.updateDisplay_();
      }
    },
  };
  return DumpCreator;
})();



function initialize() {
  peerConnectionsListElem = $('peer-connections-list');
  dumpCreator = new DumpCreator(peerConnectionsListElem);
  ssrcInfoManager = new SsrcInfoManager();
  peerConnectionUpdateTable = new PeerConnectionUpdateTable();
  statsTable = new StatsTable(ssrcInfoManager);

  chrome.send('getAllUpdates');

  // Requests stats from all peer connections every second.
  window.setInterval(function() {
    if (peerConnectionsListElem.getElementsByTagName('li').length > 0)
      chrome.send('getAllStats');
  }, 1000);
}
document.addEventListener('DOMContentLoaded', initialize);


/**
 * A helper function for getting a peer connection element id.
 *
 * @param {!Object.<string, number>} data The object containing the pid and lid
 *     of the peer connection.
 * @return {string} The peer connection element id.
 */
function getPeerConnectionId(data) {
  return data.pid + '-' + data.lid;
}


/**
 * Extracts ssrc info from a setLocal/setRemoteDescription update.
 *
 * @param {!PeerConnectionUpdateEntry} data The peer connection update data.
 */
function extractSsrcInfo(data) {
  if (data.type == 'setLocalDescription' ||
      data.type == 'setRemoteDescription') {
    ssrcInfoManager.addSsrcStreamInfo(data.value);
  }
}


/**
 * Helper for adding a peer connection update.
 *
 * @param {Element} peerConnectionElement
 * @param {!PeerConnectionUpdateEntry} update The peer connection update data.
 */
function addPeerConnectionUpdate(peerConnectionElement, update) {
  peerConnectionUpdateTable.addPeerConnectionUpdate(peerConnectionElement,
                                                    update);
  extractSsrcInfo(update);
  peerConnectionDataStore[peerConnectionElement.id].addUpdate(
      update.type, update.value);
}


/** Browser message handlers. */


/**
 * Removes all information about a peer connection.
 *
 * @param {!Object.<string, number>} data The object containing the pid and lid
 *     of a peer connection.
 */
function removePeerConnection(data) {
  var element = $(getPeerConnectionId(data));
  if (element) {
    delete peerConnectionDataStore[element.id];
    peerConnectionsListElem.removeChild(element);
  }
}


/**
 * Adds a peer connection.
 *
 * @param {!Object} data The object containing the pid, lid, url, servers, and
 *     constraints of a peer connection.
 */
function addPeerConnection(data) {
  var id = getPeerConnectionId(data);

  if (!peerConnectionDataStore[id]) {
    peerConnectionDataStore[id] = new PeerConnectionRecord();
  }
  peerConnectionDataStore[id].initialize(
      data.url, data.servers, data.constraints);

  var peerConnectionElement = $(id);
  if (!peerConnectionElement) {
    peerConnectionElement = document.createElement('li');
    peerConnectionsListElem.appendChild(peerConnectionElement);
    peerConnectionElement.id = id;
  }
  peerConnectionElement.innerHTML =
      '<h3>PeerConnection ' + peerConnectionElement.id + '</h3>' +
      '<div>' + data.url + ' ' + data.servers + ' ' + data.constraints +
      '</div>';

  // Clicking the heading can expand or collapse the peer connection item.
  peerConnectionElement.firstChild.title = 'Click to collapse or expand';
  peerConnectionElement.firstChild.addEventListener('click', function(e) {
    if (e.target.parentElement.className == '')
      e.target.parentElement.className = 'peer-connection-hidden';
    else
      e.target.parentElement.className = '';
  });

  return peerConnectionElement;
}


/**
 * Adds a peer connection update.
 *
 * @param {!PeerConnectionUpdateEntry} data The peer connection update data.
 */
function updatePeerConnection(data) {
  var peerConnectionElement = $(getPeerConnectionId(data));
  addPeerConnectionUpdate(peerConnectionElement, data);
}


/**
 * Adds the information of all peer connections created so far.
 *
 * @param {Array.<!Object>} data An array of the information of all peer
 *     connections. Each array item contains pid, lid, url, servers,
 *     constraints, and an array of updates as the log.
 */
function updateAllPeerConnections(data) {
  for (var i = 0; i < data.length; ++i) {
    var peerConnection = addPeerConnection(data[i]);

    var log = data[i].log;
    if (!log)
      continue;
    for (var j = 0; j < log.length; ++j) {
      addPeerConnectionUpdate(peerConnection, log[j]);
    }
  }
}


/**
 * Handles the report of stats.
 *
 * @param {!Object} data The object containing pid, lid, and reports, where
 *     reports is an array of stats reports. Each report contains id, type,
 *     and stats, where stats is the object containing timestamp and values,
 *     which is an array of strings, whose even index entry is the name of the
 *     stat, and the odd index entry is the value.
 */
function addStats(data) {
  var peerConnectionElement = $(getPeerConnectionId(data));
  if (!peerConnectionElement)
    return;

  for (var i = 0; i < data.reports.length; ++i) {
    var report = data.reports[i];
    statsTable.addStatsReport(peerConnectionElement, report);
    drawSingleReport(peerConnectionElement, report);
  }
}


/**
 * Delegates to dumpCreator to update the recording status.
 * @param {!Object.<string>} update Key-value pairs describing the status of the
 *     RTP recording.
 */
function updateDumpStatus(update) {
  dumpCreator.onUpdate(update);
}
