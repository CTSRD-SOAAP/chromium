// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * @suppress {checkTypes}
 * Browser test for the scenario below:
 * 1. Enter full-screen mode
 * 2. Move the mouse to each edge; verify that the desktop bump-scrolls.
 */

'use strict';

/**
 * @constructor
 * @extends {base.EventSourceImpl}
 */
browserTest.FakeDesktopConnectedView = function() {
  this.pluginPosition = {
    top: 0,
    left: 0
  };
  this.defineEvents(Object.keys(remoting.DesktopConnectedView.Events));
};

base.extend(browserTest.FakeDesktopConnectedView, base.EventSourceImpl);

/**
 * @return {{top: number, left:number}} The top-left corner of the plugin.
 */
browserTest.FakeDesktopConnectedView.prototype.getPluginPositionForTesting =
    function() {
  return this.pluginPosition;
};


/** @constructor */
browserTest.Bump_Scroll = function() {
  // To avoid dependencies on the actual host desktop size, we simulate a
  // desktop larger or smaller than the client window. The exact value is
  // arbitrary, but must be positive.
  /** @type {number} */
  this.kHostDesktopSizeDelta = 10;
};

/**
 * @param {{pin:string}} data
 */
browserTest.Bump_Scroll.prototype.run = function(data) {
  browserTest.expect(typeof data.pin == 'string');

  if (!base.isAppsV2()) {
    browserTest.fail(
        'Bump-scroll requires full-screen, which can only be activated ' +
        'programmatically in apps v2.')
  }

  this.testVerifyScroll().then(function() {
    return browserTest.connectMe2Me();
  }).then(function() {
    return browserTest.enterPIN(data.pin);
  }).then(
    this.noScrollWindowed.bind(this)
  ).then(
    this.activateFullscreen.bind(this)
  ).then(
    this.noScrollSmaller.bind(this)
    // The order of these operations is important. Because the plugin starts
    // scrolled to the top-left, it needs to be scrolled right and down first.
  ).then(
    this.scrollDirection.bind(this, 1.0, 0.5)  // Right edge
  ).then(
    this.scrollDirection.bind(this, 0.5, 1.0)  // Bottom edge
  ).then(
    this.scrollDirection.bind(this, 0.0, 0.5)  // Left edge
  ).then(
    this.scrollDirection.bind(this, 0.5, 0.0)  // Top edge
  ).then(
    function(value) {
      browserTest.disconnect();
      return browserTest.pass(value);
    },
    function(error) {
      browserTest.disconnect();
      return browserTest.fail(error);
    }
  );
};

/**
 * @return {Promise}
 */
browserTest.Bump_Scroll.prototype.noScrollWindowed = function() {
  remoting.desktopConnectedView.setPluginSizeForBumpScrollTesting(
      window.innerWidth + this.kHostDesktopSizeDelta,
      window.innerHeight + this.kHostDesktopSizeDelta);
  this.moveMouseTo(0, 0);
  return this.verifyScroll(undefined, undefined);
};

/**
 * @return {Promise}
 */
browserTest.Bump_Scroll.prototype.noScrollSmaller = function() {
  remoting.desktopConnectedView.setPluginSizeForBumpScrollTesting(
      window.innerWidth - this.kHostDesktopSizeDelta,
      window.innerHeight - this.kHostDesktopSizeDelta);
  this.moveMouseTo(0, 0);
  return this.verifyScroll(undefined, undefined);
};

/**
 * @param {number} widthFraction
 * @param {number} heightFraction
 * @return {Promise}
 */
browserTest.Bump_Scroll.prototype.scrollDirection =
    function(widthFraction, heightFraction) {
  remoting.desktopConnectedView.setPluginSizeForBumpScrollTesting(
      screen.width + this.kHostDesktopSizeDelta,
      screen.height + this.kHostDesktopSizeDelta);
  /** @type {number} */
  var expectedTop = heightFraction == 0.0 ? 0 :
                    heightFraction == 1.0 ? -this.kHostDesktopSizeDelta :
                    undefined;
  /** @type {number} */
  var expectedLeft = widthFraction == 0.0 ? 0 :
                     widthFraction == 1.0 ? -this.kHostDesktopSizeDelta :
                     undefined;
  var result = this.verifyScroll(expectedTop, expectedLeft);
  this.moveMouseTo(widthFraction * screen.width,
                   heightFraction * screen.height);
  return result;
};

/**
 * @return {Promise}
 */
browserTest.Bump_Scroll.prototype.activateFullscreen = function() {
  return new Promise(function(fulfill, reject) {
    remoting.fullscreen.activate(true, function() {
      // The onFullscreen callback is invoked before the window has
      // resized, so defer fulfilling the promise so that innerWidth
      // and innerHeight are correct.
      base.Promise.sleep(1000).then(fulfill);
    });
    base.Promise.sleep(5000).then(function(){
      reject('Timed out waiting for full-screen');
    });
  });
};

/**
 * @param {number} x
 * @param {number} y
 */
browserTest.Bump_Scroll.prototype.moveMouseTo = function(x, y) {
  var e = {
    bubbles: true,
    cancelable: false,
    view: window,
    detail: 0,
    screenX: x,
    screenY: y,
    clientX: x,
    clientY: y,
    ctrlKey: false,
    altKey: false,
    shiftKey: false,
    metaKey: false,
    button: 0,
    relatedTarget: undefined
  };
  var event = document.createEvent('MouseEvents');
  event.initMouseEvent('mousemove',
                       e.bubbles, e.cancelable, e.view, e.detail,
                       e.screenX, e.screenY, e.clientX, e.clientY,
                       e.ctrlKey, e.altKey, e.shiftKey, e.metaKey,
                       e.button, document.documentElement);
  document.documentElement.dispatchEvent(event);
};

/**
 * verifyScroll() is complicated enough to warrant a test.
 * @return {Promise}
 */
browserTest.Bump_Scroll.prototype.testVerifyScroll = function() {
  var STARTED = remoting.DesktopConnectedView.Events.bumpScrollStarted;
  var STOPPED = remoting.DesktopConnectedView.Events.bumpScrollStopped;
  var fakeSession = new browserTest.FakeDesktopConnectedView;
  var that = this;

  // No events raised (e.g. windowed mode).
  var result = this.verifyScroll(undefined, undefined, fakeSession)

  .then(function() {
    // Start and end events raised, but no scrolling (e.g. full-screen mode
    // with host desktop <= window size).
    fakeSession = new browserTest.FakeDesktopConnectedView;
    var result = that.verifyScroll(undefined, undefined, fakeSession);
    fakeSession.raiseEvent(STARTED, {});
    fakeSession.raiseEvent(STOPPED, {});
    return result;

  }).then(function() {
    // Start and end events raised, with incorrect scrolling.
    fakeSession = new browserTest.FakeDesktopConnectedView;
    var result = base.Promise.negate(
        that.verifyScroll(2, 2, fakeSession));
    fakeSession.raiseEvent(STARTED, {});
    fakeSession.pluginPosition.top = 1;
    fakeSession.pluginPosition.left = 1;
    fakeSession.raiseEvent(STOPPED, {});
    return result;

  }).then(function() {
    // Start event raised, but not end event.
    fakeSession = new browserTest.FakeDesktopConnectedView;
    var result = base.Promise.negate(
        that.verifyScroll(2, 2, fakeSession));
    fakeSession.raiseEvent(STARTED, {});
    fakeSession.pluginPosition.top = 2;
    fakeSession.pluginPosition.left = 2;
    return result;

  }).then(function() {
    // Start and end events raised, with correct scrolling.
    fakeSession = new browserTest.FakeDesktopConnectedView;
    var result = that.verifyScroll(2, 2, fakeSession);
    fakeSession.raiseEvent(STARTED, {});
    fakeSession.pluginPosition.top = 2;
    fakeSession.pluginPosition.left = 2;
    fakeSession.raiseEvent(STOPPED, {});
    return result;
  });

  return result;
};

/**
 * Verify that a bump scroll operation takes place and that the top-left corner
 * of the plugin is as expected when it completes.
 * @param {number|undefined} expectedTop The expected vertical position of the
 *    plugin, or undefined if it is not expected to change.
 * @param {number|undefined} expectedLeft The expected horizontal position of
 *    the plugin, or undefined if it is not expected to change.
 * @param {browserTest.FakeDesktopConnectedView=} opt_desktopConnectedView
 *     DesktopConnectedView fake, for testing.
 * @return {Promise}
 */
browserTest.Bump_Scroll.prototype.verifyScroll =
    function (expectedTop, expectedLeft, opt_desktopConnectedView) {
  /** @type {browserTest.FakeDesktopConnectedView} */
  var desktopConnectedView = opt_desktopConnectedView ||
      remoting.desktopConnectedView;
  base.debug.assert(desktopConnectedView != null);
  var STARTED = remoting.DesktopConnectedView.Events.bumpScrollStarted;
  var STOPPED = remoting.DesktopConnectedView.Events.bumpScrollStopped;

  var initialPosition = desktopConnectedView.getPluginPositionForTesting();
  var initialTop = initialPosition.top;
  var initialLeft = initialPosition.left;

  /** @return {Promise} */
  var verifyPluginPosition = function() {
    var position = desktopConnectedView.getPluginPositionForTesting();
    if (expectedLeft === undefined) {
      expectedLeft = initialLeft;
    }
    if (expectedTop === undefined) {
      expectedTop = initialTop;
    }
    if (position.top != expectedTop || position.left != expectedLeft) {
      return Promise.reject(
          new Error('No or incorrect scroll detected: (' +
                    position.left + ',' + position.top + ' instead of ' +
                    expectedLeft + ',' + expectedTop + ')'));
    } else {
      return Promise.resolve();
    }
  };

  var started = browserTest.expectEvent(desktopConnectedView, STARTED, 1000);
  var stopped = browserTest.expectEvent(desktopConnectedView, STOPPED, 5000);
  return started.then(function() {
    return stopped.then(function() {
      return verifyPluginPosition();
    });
  }, function() {
    // If no started event is raised, the test might still pass if it asserted
    // no scrolling.
    if (expectedTop == undefined && expectedLeft == undefined) {
      return Promise.resolve();
    } else {
      return Promise.reject(
          new Error('Scroll expected but no start event fired.'));
    }
  });
};
