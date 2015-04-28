// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Interface abstracting the Application functionality.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @param {Array<string>} app_capabilities Array of application capabilities.
 * @constructor
 */
remoting.Application = function(app_capabilities) {
  /**
   * @type {remoting.Application.Delegate}
   * @private
   */
  this.delegate_ = null;

  /**
   * @type {Array<string>}
   * @private
   */
  this.app_capabilities_ = [
    remoting.ClientSession.Capability.SEND_INITIAL_RESOLUTION,
    remoting.ClientSession.Capability.RATE_LIMIT_RESIZE_REQUESTS,
    remoting.ClientSession.Capability.VIDEO_RECORDER
  ];
  // Append the app-specific capabilities.
  this.app_capabilities_.push.apply(this.app_capabilities_, app_capabilities);

  /**
   * @type {remoting.SessionConnector}
   * @private
   */
  this.session_connector_ = null;
};

/**
 * @param {remoting.Application.Delegate} appDelegate The delegate that
 *    contains the app-specific functionality.
 */
remoting.Application.prototype.setDelegate = function(appDelegate) {
  this.delegate_ = appDelegate;
};

/**
 * @return {string} Application product name to be used in UI.
 */
remoting.Application.prototype.getApplicationName = function() {
  return this.delegate_.getApplicationName();
};

/**
 * @return {Array<string>} A list of |ClientSession.Capability|s required
 *     by this application.
 */
remoting.Application.prototype.getRequiredCapabilities_ = function() {
  return this.app_capabilities_;
};

/**
 * @param {remoting.ClientSession.Capability} capability
 * @return {boolean}
 */
remoting.Application.prototype.hasCapability = function(capability) {
  var capabilities = remoting.app.getRequiredCapabilities_();
  return capabilities.indexOf(capability) != -1;
};

/**
 * Initialize the application and register all event handlers. After this
 * is called, the app is running and waiting for user events.
 *
 * @return {void} Nothing.
 */
remoting.Application.prototype.start = function() {
  // Create global objects.
  remoting.ClientPlugin.factory = new remoting.DefaultClientPluginFactory();
  remoting.SessionConnector.factory =
      new remoting.DefaultSessionConnectorFactory();

  // TODO(garykac): This should be owned properly rather than living in the
  // global 'remoting' namespace.
  remoting.settings = new remoting.Settings();

  this.delegate_.init(this.getSessionConnector());
};

/**
 * Called when a new session has been connected.
 *
 * @param {remoting.ClientSession} clientSession
 * @return {void} Nothing.
 */
remoting.Application.prototype.onConnected = function(clientSession) {
  // TODO(garykac): Make clientSession a member var of Application.
  remoting.clientSession = clientSession;
  remoting.clientSession.addEventListener('stateChanged', onClientStateChange_);

  remoting.clipboard.startSession();
  updateStatistics_();
  remoting.hangoutSessionEvents.raiseEvent(
      remoting.hangoutSessionEvents.sessionStateChanged,
      remoting.ClientSession.State.CONNECTED
  );

  this.delegate_.handleConnected(clientSession);
};

/**
 * Called when the current session has been disconnected.
 *
 * @return {void} Nothing.
 */
remoting.Application.prototype.onDisconnected = function() {
  this.delegate_.handleDisconnected();
};

/**
 * Called when the current session's connection has failed.
 *
 * @param {remoting.Error} error
 * @return {void} Nothing.
 */
remoting.Application.prototype.onConnectionFailed = function(error) {
  this.delegate_.handleConnectionFailed(this.session_connector_, error);
};

/**
 * Called when the current session has reached the point where the host has
 * started streaming video frames to the client.
 *
 * @return {void} Nothing.
 */
remoting.Application.prototype.onVideoStreamingStarted = function() {
  this.delegate_.handleVideoStreamingStarted();
};

/**
 * Called when an extension message needs to be handled.
 *
 * @param {string} type The type of the extension message.
 * @param {string} data The payload of the extension message.
 * @return {boolean} Return true if the extension message was recognized.
 */
remoting.Application.prototype.onExtensionMessage = function(type, data) {
  var message = /** @type {Object} */ (base.jsonParseSafe(data));
  if (typeof message != 'object') {
    return false;
  }

  // Give the delegate a chance to handle this extension message first.
  if (this.delegate_.handleExtensionMessage(type, message)) {
    return true;
  }

  if (remoting.clientSession) {
    return remoting.clientSession.handleExtensionMessage(type, message);
  }
  return false;
};

/**
 * Called when an error needs to be displayed to the user.
 *
 * @param {remoting.Error} errorTag The error to be localized and displayed.
 * @return {void} Nothing.
 */
remoting.Application.prototype.onError = function(errorTag) {
  this.delegate_.handleError(errorTag);
};

/**
 * @return {remoting.SessionConnector} A session connector, creating a new one
 *     if necessary.
 */
remoting.Application.prototype.getSessionConnector = function() {
  // TODO(garykac): Check if this can be initialized in the ctor.
  if (!this.session_connector_) {
    this.session_connector_ = remoting.SessionConnector.factory.createConnector(
        document.getElementById('client-container'),
        this.onConnected.bind(this),
        this.onError.bind(this),
        this.onExtensionMessage.bind(this),
        this.onConnectionFailed.bind(this),
        this.getRequiredCapabilities_(),
        this.delegate_.getDefaultRemapKeys());
  }
  return this.session_connector_;
};


/**
 * @interface
 */
remoting.Application.Delegate = function() {};

/**
 * Initialize the application and register all event handlers. After this
 * is called, the app is running and waiting for user events.
 *
 * @param {remoting.SessionConnector} connector
 * @return {void} Nothing.
 */
remoting.Application.Delegate.prototype.init = function(connector) {};

/**
 * @return {string} Application product name to be used in UI.
 */
remoting.Application.Delegate.prototype.getApplicationName = function() {};

/**
 * @return {string} The default remap keys for the current platform.
 */
remoting.Application.Delegate.prototype.getDefaultRemapKeys = function() {};

/**
 * Called when a new session has been connected.
 *
 * @param {remoting.ClientSession} clientSession
 * @return {void} Nothing.
 */
remoting.Application.Delegate.prototype.handleConnected = function(
    clientSession) {};

/**
 * Called when the current session has been disconnected.
 *
 * @return {void} Nothing.
 */
remoting.Application.Delegate.prototype.handleDisconnected = function() {};

/**
 * Called when the current session's connection has failed.
 *
 * @param {remoting.SessionConnector} connector
 * @param {remoting.Error} error
 * @return {void} Nothing.
 */
remoting.Application.Delegate.prototype.handleConnectionFailed = function(
    connector, error) {};

/**
 * Called when the current session has reached the point where the host has
 * started streaming video frames to the client.
 *
 * @return {void} Nothing.
 */
remoting.Application.Delegate.prototype.handleVideoStreamingStarted = function(
    ) {};

/**
 * Called when an extension message needs to be handled.
 *
 * @param {string} type The type of the extension message.
 * @param {Object} message The parsed extension message data.
 * @return {boolean} Return true if the extension message was recognized.
 */
remoting.Application.Delegate.prototype.handleExtensionMessage = function(
    type, message) {};

/**
 * Called when an error needs to be displayed to the user.
 *
 * @param {remoting.Error} errorTag The error to be localized and displayed.
 * @return {void} Nothing.
 */
remoting.Application.Delegate.prototype.handleError = function(errorTag) {};


/** @type {remoting.Application} */
remoting.app = null;
