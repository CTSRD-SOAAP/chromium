// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Connect set-up state machine for Me2Me and IT2Me
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @param {HTMLElement} clientContainer Container element for the client view.
 * @param {function(remoting.ClientSession):void} onConnected Callback on
 *     success.
 * @param {function(remoting.Error):void} onError Callback on error.
 * @param {function(string, string):boolean} onExtensionMessage The handler for
 *     protocol extension messages. Returns true if a message is recognized;
 *     false otherwise.
 * @param {function(remoting.Error):void} onConnectionFailed Callback for when
 *     the connection fails.
 * @param {Array<string>} requiredCapabilities Connector capabilities
 *     required by this application.
 * @param {string} defaultRemapKeys The default set of key mappings for the
 *     client session to use.
 * @constructor
 * @implements {remoting.SessionConnector}
 */
remoting.SessionConnectorImpl = function(clientContainer, onConnected, onError,
                                         onExtensionMessage,
                                         onConnectionFailed,
                                         requiredCapabilities,
                                         defaultRemapKeys) {
  /**
   * @type {HTMLElement}
   * @private
   */
  this.clientContainer_ = clientContainer;

  /**
   * @type {function(remoting.ClientSession):void}
   * @private
   */
  this.onConnected_ = onConnected;

  /**
   * @type {function(remoting.Error):void}
   * @private
   */
  this.onError_ = onError;

  /**
   * @type {function(string, string):boolean}
   * @private
   */
  this.onExtensionMessage_ = onExtensionMessage;

  /**
   * @type {function(remoting.Error):void}
   * @private
   */
  this.onConnectionFailed_ = onConnectionFailed;

  /**
   * @type {Array<string>}
   * @private
   */
  this.requiredCapabilities_ = requiredCapabilities;

  /**
   * @type {string}
   * @private
   */
  this.defaultRemapKeys_ = defaultRemapKeys;

  /**
   * @type {string}
   * @private
   */
  this.clientJid_ = '';

  /**
   * @type {remoting.DesktopConnectedView.Mode}
   * @private
   */
  this.connectionMode_ = remoting.DesktopConnectedView.Mode.ME2ME;

  /**
   * @type {remoting.SignalStrategy}
   * @private
   */
  this.signalStrategy_ = null;

  /**
   * @type {remoting.SmartReconnector}
   * @private
   */
  this.reconnector_ = null;

  /**
   * @private
   */
  this.bound_ = {
    onStateChange : this.onStateChange_.bind(this)
  };

  // Initialize/declare per-connection state.
  this.reset();
};

/**
 * Reset the per-connection state so that the object can be re-used for a
 * second connection. Note the none of the shared WCS state is reset.
 */
remoting.SessionConnectorImpl.prototype.reset = function() {
  /**
   * For paired connections, the client id of this device, issued by the host.
   *
   * @type {string}
   * @private
   */
  this.clientPairingId_ = '';

  /**
   * For paired connections, the paired secret for this device, issued by the
   * host.
   *
   * @type {string}
   * @private
   */
  this.clientPairedSecret_ = '';

  /**
   * String used to authenticate to the host on connection. For IT2Me, this is
   * the access code; for Me2Me it is the PIN.
   *
   * @type {string}
   * @private
   */
  this.passPhrase_ = '';

  /**
   * @type {remoting.Host}
   * @private
   */
  this.host_ = null;

  /**
   * @type {boolean}
   * @private
   */
  this.logHostOfflineErrors_ = false;

  /**
   * @type {remoting.ClientSession}
   * @private
   */
  this.clientSession_ = null;

  /**
   * @type {XMLHttpRequest}
   * @private
   */
  this.pendingXhr_ = null;

  /**
   * Function to interactively obtain the PIN from the user.
   * @type {function(boolean, function(string):void):void}
   * @private
   */
  this.fetchPin_ = function(onPinFetched) {};

  /**
   * @type {function(string, string, string,
   *                 function(string, string):void): void}
   * @private
   */
  this.fetchThirdPartyToken_ = function(
      tokenUrl, hostPublicKey, scope, onThirdPartyTokenFetched) {};
};

/**
 * Initiate a Me2Me connection.
 *
 * This doesn't report host-offline errors because the connection will
 * be retried and retryConnectMe2Me is responsible for reporting these errors.
 *
 * @param {remoting.Host} host The Me2Me host to which to connect.
 * @param {function(boolean, function(string):void):void} fetchPin Function to
 *     interactively obtain the PIN from the user.
 * @param {function(string, string, string,
 *                  function(string, string): void): void}
 *     fetchThirdPartyToken Function to obtain a token from a third party
 *     authentication server.
 * @param {string} clientPairingId The client id issued by the host when
 *     this device was paired, if it is already paired.
 * @param {string} clientPairedSecret The shared secret issued by the host when
 *     this device was paired, if it is already paired.
 * @return {void} Nothing.
 */
remoting.SessionConnectorImpl.prototype.connectMe2Me =
    function(host, fetchPin, fetchThirdPartyToken,
             clientPairingId, clientPairedSecret) {
  this.connectionMode_ = remoting.DesktopConnectedView.Mode.ME2ME;
  this.logHostOfflineErrors_ = false;
  this.connectMe2MeInternal_(host, fetchPin, fetchThirdPartyToken,
                             clientPairingId, clientPairedSecret);
};

/**
 * Retry connecting to a Me2Me host after a connection failure.
 *
 * This is the same as connectMe2Me except that is will log errors if the
 * host is offline.
 *
 * @param {remoting.Host} host The Me2Me host to refresh.
 * @return {void} Nothing.
 */
remoting.SessionConnectorImpl.prototype.retryConnectMe2Me = function(host) {
  this.connectionMode_ = remoting.DesktopConnectedView.Mode.ME2ME;
  this.logHostOfflineErrors_ = true;
  this.connectMe2MeInternal_(host, this.fetchPin_, this.fetchThirdPartyToken_,
                             this.clientPairingId_, this.clientPairedSecret_);
};

/**
 * Initiate a Me2App connection.
 *
 * @param {remoting.Host} host The Me2Me host to which to connect.
 * @param {function(string, string, string,
 *                  function(string, string): void): void}
 *     fetchThirdPartyToken Function to obtain a token from a third party
 *     authenticaiton server.
 * @return {void} Nothing.
 */
remoting.SessionConnectorImpl.prototype.connectMe2App =
    function(host, fetchThirdPartyToken) {
  this.connectionMode_ = remoting.DesktopConnectedView.Mode.APP_REMOTING;
  this.logHostOfflineErrors_ = true;
  this.connectMe2MeInternal_(host, function() {}, fetchThirdPartyToken, '', '');
};

/**
 * Update the pairing info so that the reconnect function will work correctly.
 *
 * @param {string} clientId The paired client id.
 * @param {string} sharedSecret The shared secret.
 */
remoting.SessionConnectorImpl.prototype.updatePairingInfo =
    function(clientId, sharedSecret) {
  this.clientPairingId_ = clientId;
  this.clientPairedSecret_ = sharedSecret;
};

/**
 * Initiate a Me2Me connection.
 *
 * @param {remoting.Host} host the Host to connect to.
 * @param {function(boolean, function(string):void):void} fetchPin Function to
 *     interactively obtain the PIN from the user.
 * @param {function(string, string, string,
 *                  function(string, string): void): void}
 *     fetchThirdPartyToken Function to obtain a token from a third party
 *     authentication server.
 * @param {string} clientPairingId The client id issued by the host when
 *     this device was paired, if it is already paired.
 * @param {string} clientPairedSecret The shared secret issued by the host when
 *     this device was paired, if it is already paired.
 * @return {void} Nothing.
 * @private
 */
remoting.SessionConnectorImpl.prototype.connectMe2MeInternal_ =
    function(host, fetchPin, fetchThirdPartyToken,
             clientPairingId, clientPairedSecret) {
  // Cancel any existing connect operation.
  this.cancel();

  this.host_ = host;
  this.fetchPin_ = fetchPin;
  this.fetchThirdPartyToken_ = fetchThirdPartyToken;
  this.updatePairingInfo(clientPairingId, clientPairedSecret);

  this.connectSignaling_();
};

/**
 * Initiate an IT2Me connection.
 *
 * @param {string} accessCode The access code as entered by the user.
 * @return {void} Nothing.
 */
remoting.SessionConnectorImpl.prototype.connectIT2Me = function(accessCode) {
  var kSupportIdLen = 7;
  var kHostSecretLen = 5;
  var kAccessCodeLen = kSupportIdLen + kHostSecretLen;

  // Cancel any existing connect operation.
  this.cancel();

  var normalizedAccessCode = this.normalizeAccessCode_(accessCode);
  if (normalizedAccessCode.length != kAccessCodeLen) {
    this.onError_(remoting.Error.INVALID_ACCESS_CODE);
    return;
  }

  var hostId = normalizedAccessCode.substring(0, kSupportIdLen);
  this.passPhrase_ = normalizedAccessCode;
  this.connectionMode_ = remoting.DesktopConnectedView.Mode.IT2ME;
  remoting.identity.callWithToken(
      this.connectIT2MeWithToken_.bind(this, hostId), this.onError_);
};

/**
 * Reconnect a closed connection.
 *
 * @return {void} Nothing.
 */
remoting.SessionConnectorImpl.prototype.reconnect = function() {
  if (this.connectionMode_ == remoting.DesktopConnectedView.Mode.IT2ME) {
    console.error('reconnect not supported for IT2Me.');
    return;
  }
  this.logHostOfflineErrors_ = false;
  this.connectMe2MeInternal_(this.host_, this.fetchPin_,
                             this.fetchThirdPartyToken_, this.clientPairingId_,
                             this.clientPairedSecret_);
};

/**
 * Cancel a connection-in-progress.
 */
remoting.SessionConnectorImpl.prototype.cancel = function() {
  if (this.clientSession_) {
    this.clientSession_.removePlugin();
    this.clientSession_ = null;
  }
  if (this.pendingXhr_) {
    this.pendingXhr_.abort();
    this.pendingXhr_ = null;
  }
  this.reset();
};

/**
 * Get the connection mode (Me2Me or IT2Me)
 *
 * @return {remoting.DesktopConnectedView.Mode}
 */
remoting.SessionConnectorImpl.prototype.getConnectionMode = function() {
  return this.connectionMode_;
};

/**
 * Get host ID.
 *
 * @return {string}
 */
remoting.SessionConnectorImpl.prototype.getHostId = function() {
  return this.host_.hostId;
};

/**
 * @private
 */
remoting.SessionConnectorImpl.prototype.connectSignaling_ = function() {
  base.dispose(this.signalStrategy_);
  this.signalStrategy_ = null;

  /** @type {remoting.SessionConnectorImpl} */
  var that = this;

  /** @param {string} token */
  function connectSignalingWithToken(token) {
    remoting.identity.getUserInfo(
        connectSignalingWithTokenAndUserInfo.bind(null, token), that.onError_);
  }

  /**
   * Success callback for when the email and fullName have been retrieved
   * for this user.
   * Note that the full name will be null unless the webapp has requested
   * and been granted the userinfo.profile permission.
   *
   * @param {string} token
   * @param {string} email
   * @param {string} fullName
   */
  function connectSignalingWithTokenAndUserInfo(token, email, fullName) {
    that.signalStrategy_.connect(
        remoting.settings.XMPP_SERVER_FOR_CLIENT, email, token);
  }

  this.signalStrategy_ = remoting.SignalStrategy.create();
  this.signalStrategy_.setStateChangedCallback(
      this.onSignalingState_.bind(this));

  remoting.identity.callWithToken(connectSignalingWithToken, this.onError_);
};

/**
 * @private
 * @param {remoting.SignalStrategy.State} state
 */
remoting.SessionConnectorImpl.prototype.onSignalingState_ = function(state) {
  switch (state) {
    case remoting.SignalStrategy.State.CONNECTED:
      // Proceed only if the connection hasn't been canceled.
      if (this.host_.jabberId) {
        this.createSession_();
      }
      break;

    case remoting.SignalStrategy.State.FAILED:
      this.onError_(this.signalStrategy_.getError());
      break;
  }
};

/**
 * Continue an IT2Me connection once an access token has been obtained.
 *
 * @param {string} hostId
 * @param {string} token An OAuth2 access token.
 * @return {void} Nothing.
 * @private
 */
remoting.SessionConnectorImpl.prototype.connectIT2MeWithToken_ =
    function(hostId, token) {
  // Resolve the host id to get the host JID.
  this.pendingXhr_ = remoting.xhr.get(
      remoting.settings.DIRECTORY_API_BASE_URL + '/support-hosts/' +
          encodeURIComponent(hostId),
      this.onIT2MeHostInfo_.bind(this, hostId),
      '',
      { 'Authorization': 'OAuth ' + token });
};

/**
 * Continue an IT2Me connection once the host JID has been looked up.
 *
 * @param {string} hostId
 * @param {XMLHttpRequest} xhr The server response to the support-hosts query.
 * @return {void} Nothing.
 * @private
 */
remoting.SessionConnectorImpl.prototype.onIT2MeHostInfo_ =
    function(hostId, xhr) {
  this.pendingXhr_ = null;
  if (xhr.status == 200) {
    var host = /** @type {{data: {jabberId: string, publicKey: string}}} */
        (base.jsonParseSafe(xhr.responseText));
    if (host && host.data && host.data.jabberId && host.data.publicKey) {
      this.host_ = new remoting.Host();
      this.host_.hostId = hostId;
      this.host_.jabberId = host.data.jabberId;
      this.host_.publicKey = host.data.publicKey;
      this.host_.hostName = host.data.jabberId.split('/')[0];
      this.connectSignaling_();
      return;
    } else {
      console.error('Invalid "support-hosts" response from server.');
    }
  } else {
    this.onError_(this.translateSupportHostsError_(xhr.status));
  }
};

/**
 * Creates ClientSession object.
 */
remoting.SessionConnectorImpl.prototype.createSession_ = function() {
  // In some circumstances, the WCS <iframe> can get reloaded, which results
  // in a new clientJid and a new callback. In this case, remove the old
  // client plugin before instantiating a new one.
  if (this.clientSession_) {
    this.clientSession_.removePlugin();
    this.clientSession_ = null;
  }

  var authenticationMethods =
     'third_party,spake2_pair,spake2_hmac,spake2_plain';
  this.clientSession_ = new remoting.ClientSession(
      this.host_, this.signalStrategy_, this.clientContainer_, this.passPhrase_,
      this.fetchPin_, this.fetchThirdPartyToken_, authenticationMethods,
      this.connectionMode_, this.clientPairingId_, this.clientPairedSecret_,
      this.defaultRemapKeys_);
  this.clientSession_.logHostOfflineErrors(this.logHostOfflineErrors_);
  this.clientSession_.addEventListener(
      remoting.ClientSession.Events.stateChanged,
      this.bound_.onStateChange);
  this.clientSession_.createPluginAndConnect(this.onExtensionMessage_,
                                             this.requiredCapabilities_);
};

/**
 * Handle a change in the state of the client session prior to successful
 * connection (after connection, this class no longer handles state change
 * events). Errors that occur while connecting either trigger a reconnect
 * or notify the onError handler.
 *
 * @param {remoting.ClientSession.StateEvent=} event
 * @return {void} Nothing.
 * @private
 */
remoting.SessionConnectorImpl.prototype.onStateChange_ = function(event) {
  switch (event.current) {
    case remoting.ClientSession.State.CONNECTED:
      // When the connection succeeds, deregister for state-change callbacks
      // and pass the session to the onConnected callback. It is expected that
      // it will register a new state-change callback to handle disconnect
      // or error conditions.
      this.clientSession_.removeEventListener(
          remoting.ClientSession.Events.stateChanged,
          this.bound_.onStateChange);

      base.dispose(this.reconnector_);
      if (this.connectionMode_ != remoting.DesktopConnectedView.Mode.IT2ME) {
        this.reconnector_ =
            new remoting.SmartReconnector(this, this.clientSession_);
      }
      this.onConnected_(this.clientSession_);
      break;

    case remoting.ClientSession.State.CREATED:
      console.log('Created plugin');
      break;

    case remoting.ClientSession.State.CONNECTING:
      console.log('Connecting as ' + remoting.identity.getCachedEmail());
      break;

    case remoting.ClientSession.State.INITIALIZING:
      console.log('Initializing connection');
      break;

    case remoting.ClientSession.State.CLOSED:
      // This class deregisters for state-change callbacks when the CONNECTED
      // state is reached, so it only sees the CLOSED state in exceptional
      // circumstances. For example, a CONNECTING -> CLOSED transition happens
      // if the host closes the connection without an error message instead of
      // accepting it. Since there's no way of knowing exactly what went wrong,
      // we rely on server-side logs in this case and report a generic error
      // message.
      this.onError_(remoting.Error.UNEXPECTED);
      break;

    case remoting.ClientSession.State.FAILED:
      var error = this.clientSession_.getError();
      console.error('Client plugin reported connection failed: ' + error);
      if (error == null) {
        error = remoting.Error.UNEXPECTED;
      }
      this.onConnectionFailed_(error);
      break;

    default:
      console.error('Unexpected client plugin state: ' + event.current);
      // This should only happen if the web-app and client plugin get out of
      // sync, and even then the version check should ensure compatibility.
      this.onError_(remoting.Error.MISSING_PLUGIN);
  }
};

/**
 * @param {number} error An HTTP error code returned by the support-hosts
 *     endpoint.
 * @return {remoting.Error} The equivalent remoting.Error code.
 * @private
 */
remoting.SessionConnectorImpl.prototype.translateSupportHostsError_ =
    function(error) {
  switch (error) {
    case 0: return remoting.Error.NETWORK_FAILURE;
    case 404: return remoting.Error.INVALID_ACCESS_CODE;
    case 502: // No break
    case 503: return remoting.Error.SERVICE_UNAVAILABLE;
    default: return remoting.Error.UNEXPECTED;
  }
};

/**
 * Normalize the access code entered by the user.
 *
 * @param {string} accessCode The access code, as entered by the user.
 * @return {string} The normalized form of the code (whitespace removed).
 * @private
 */
remoting.SessionConnectorImpl.prototype.normalizeAccessCode_ =
    function(accessCode) {
  // Trim whitespace.
  return accessCode.replace(/\s/g, '');
};


/**
 * @constructor
 * @implements {remoting.SessionConnectorFactory}
 */
remoting.DefaultSessionConnectorFactory = function() {
};

/**
 * @param {HTMLElement} clientContainer Container element for the client view.
 * @param {function(remoting.ClientSession):void} onConnected Callback on
 *     success.
 * @param {function(remoting.Error):void} onError Callback on error.
 * @param {function(string, string):boolean} onExtensionMessage The handler for
 *     protocol extension messages. Returns true if a message is recognized;
 *     false otherwise.
 * @param {function(remoting.Error):void} onConnectionFailed Callback for when
 *     the connection fails.
 * @param {Array<string>} requiredCapabilities Connector capabilities
 *     required by this application.
 * @param {string} defaultRemapKeys The default set of key mappings to use
 *     in the client session.
 * @return {remoting.SessionConnector}
 */
remoting.DefaultSessionConnectorFactory.prototype.createConnector =
    function(clientContainer, onConnected, onError, onExtensionMessage,
             onConnectionFailed, requiredCapabilities, defaultRemapKeys) {
  return new remoting.SessionConnectorImpl(clientContainer, onConnected,
                                           onError, onExtensionMessage,
                                           onConnectionFailed,
                                           requiredCapabilities,
                                           defaultRemapKeys);
};
