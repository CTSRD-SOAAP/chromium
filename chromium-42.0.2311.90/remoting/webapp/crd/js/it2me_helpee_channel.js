// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 *
 * It2MeHelpeeChannel relays messages between the Hangouts web page (Hangouts)
 * and the It2Me Native Messaging Host (It2MeHost) for the helpee (the Hangouts
 * participant who is receiving remoting assistance).
 *
 * It runs in the background page. It contains a chrome.runtime.Port object,
 * representing a connection to Hangouts, and a remoting.It2MeHostFacade object,
 * representing a connection to the IT2Me Native Messaging Host.
 *
 *   Hangouts                       It2MeHelpeeChannel                 It2MeHost
 *      |---------runtime.connect()-------->|                              |
 *      |-----------hello message---------->|                              |
 *      |<-----helloResponse message------->|                              |
 *      |----------connect message--------->|                              |
 *      |                                   |-----showConfirmDialog()----->|
 *      |                                   |----------connect()---------->|
 *      |                                   |<-------hostStateChanged------|
 *      |                                   |    (RECEIVED_ACCESS_CODE)    |
 *      |<---connect response (access code)-|                              |
 *      |                                   |                              |
 *
 * Hangouts will send the access code to the web app on the helper side.
 * The helper will then connect to the It2MeHost using the access code.
 *
 *   Hangouts                       It2MeHelpeeChannel                 It2MeHost
 *      |                                   |<-------hostStateChanged------|
 *      |                                   |          (CONNECTED)         |
 *      |<-- hostStateChanged(CONNECTED)----|                              |
 *      |-------disconnect message--------->|                              |
 *      |<--hostStateChanged(DISCONNECTED)--|                              |
 *
 *
 * It also handles host downloads and install status queries:
 *
 *   Hangouts                       It2MeHelpeeChannel
 *      |------isHostInstalled message----->|
 *      |<-isHostInstalled response(false)--|
 *      |                                   |
 *      |--------downloadHost message------>|
 *      |                                   |
 *      |------isHostInstalled message----->|
 *      |<-isHostInstalled response(false)--|
 *      |                                   |
 *      |------isHostInstalled message----->|
 *      |<-isHostInstalled response(true)---|
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @param {chrome.runtime.Port} hangoutPort
 * @param {remoting.It2MeHostFacade} host
 * @param {remoting.HostInstaller} hostInstaller
 * @param {function()} onDisposedCallback Callback to notify the client when
 *    the connection is torn down.
 *
 * @constructor
 * @implements {base.Disposable}
 */
remoting.It2MeHelpeeChannel =
    function(hangoutPort, host, hostInstaller, onDisposedCallback) {
  /**
   * @type {chrome.runtime.Port}
   * @private
   */
  this.hangoutPort_ = hangoutPort;

  /**
   * @type {remoting.It2MeHostFacade}
   * @private
   */
  this.host_ = host;

  /**
   * @type {?remoting.HostInstaller}
   * @private
   */
  this.hostInstaller_ = hostInstaller;

  /**
   * @type {remoting.HostSession.State}
   * @private
   */
  this.hostState_ = remoting.HostSession.State.UNKNOWN;

  /**
   * @type {?function()}
   * @private
   */
  this.onDisposedCallback_ = onDisposedCallback;

  this.onHangoutMessageRef_ = this.onHangoutMessage_.bind(this);
  this.onHangoutDisconnectRef_ = this.onHangoutDisconnect_.bind(this);
};

/** @enum {string} */
remoting.It2MeHelpeeChannel.HangoutMessageTypes = {
  CONNECT: 'connect',
  CONNECT_RESPONSE: 'connectResponse',
  DISCONNECT: 'disconnect',
  DOWNLOAD_HOST: 'downloadHost',
  ERROR: 'error',
  HELLO: 'hello',
  HELLO_RESPONSE: 'helloResponse',
  HOST_STATE_CHANGED: 'hostStateChanged',
  IS_HOST_INSTALLED: 'isHostInstalled',
  IS_HOST_INSTALLED_RESPONSE: 'isHostInstalledResponse'
};

/** @enum {string} */
remoting.It2MeHelpeeChannel.Features = {
  REMOTE_ASSISTANCE: 'remoteAssistance'
};

remoting.It2MeHelpeeChannel.prototype.init = function() {
  this.hangoutPort_.onMessage.addListener(this.onHangoutMessageRef_);
  this.hangoutPort_.onDisconnect.addListener(this.onHangoutDisconnectRef_);
};

remoting.It2MeHelpeeChannel.prototype.dispose = function() {
  if (this.host_ !== null) {
    this.host_.unhookCallbacks();
    this.host_.disconnect();
    this.host_ = null;
  }

  if (this.hangoutPort_ !== null) {
    this.hangoutPort_.onMessage.removeListener(this.onHangoutMessageRef_);
    this.hangoutPort_.onDisconnect.removeListener(this.onHangoutDisconnectRef_);
    this.hostState_ = remoting.HostSession.State.DISCONNECTED;

    try {
      var MessageTypes = remoting.It2MeHelpeeChannel.HangoutMessageTypes;
      this.hangoutPort_.postMessage({
        method: MessageTypes.HOST_STATE_CHANGED,
        state: this.hostState_
      });
    } catch (e) {
      // |postMessage| throws if |this.hangoutPort_| is disconnected
      // It is safe to ignore the exception.
    }
    this.hangoutPort_.disconnect();
    this.hangoutPort_ = null;
  }

  if (this.onDisposedCallback_ !== null) {
    this.onDisposedCallback_();
    this.onDisposedCallback_ = null;
  }
};

/**
 * Message Handler for incoming runtime messages from Hangouts.
 *
 * @param {{method:string, data:Object<string,*>}} message
 * @private
 */
remoting.It2MeHelpeeChannel.prototype.onHangoutMessage_ = function(message) {
  try {
    var MessageTypes = remoting.It2MeHelpeeChannel.HangoutMessageTypes;
    switch (message.method) {
      case MessageTypes.HELLO:
        this.hangoutPort_.postMessage({
          method: MessageTypes.HELLO_RESPONSE,
          supportedFeatures: base.values(remoting.It2MeHelpeeChannel.Features)
        });
        return true;
      case MessageTypes.IS_HOST_INSTALLED:
        this.handleIsHostInstalled_(message);
        return true;
      case MessageTypes.DOWNLOAD_HOST:
        this.handleDownloadHost_(message);
        return true;
      case MessageTypes.CONNECT:
        this.handleConnect_(message);
        return true;
      case MessageTypes.DISCONNECT:
        this.dispose();
        return true;
    }
    throw new Error('Unsupported message method=' + message.method);
  } catch(/** @type {Error} */ error) {
    this.sendErrorResponse_(message, error.message);
  }
  return false;
};

/**
 * Queries the |hostInstaller| for the installation status.
 *
 * @param {{method:string, data:Object<string,*>}} message
 * @private
 */
remoting.It2MeHelpeeChannel.prototype.handleIsHostInstalled_ =
    function(message) {
  /** @type {remoting.It2MeHelpeeChannel} */
  var that = this;

  /** @param {boolean} installed */
  function sendResponse(installed) {
    var MessageTypes = remoting.It2MeHelpeeChannel.HangoutMessageTypes;
    that.hangoutPort_.postMessage({
      method: MessageTypes.IS_HOST_INSTALLED_RESPONSE,
      result: installed
    });
  }

  remoting.HostInstaller.isInstalled().then(
    sendResponse,
    /** @type {function(*):void} */(this.sendErrorResponse_.bind(this, message))
  );
};

/**
 * @param {{method:string, data:Object<string,*>}} message
 * @private
 */
remoting.It2MeHelpeeChannel.prototype.handleDownloadHost_ = function(message) {
  try {
    this.hostInstaller_.download();
  } catch (/** @type {*} */ e) {
    var error = /** @type {Error} */ (e);
    this.sendErrorResponse_(message, error.message);
  }
};

/**
 * Disconnect the session if the |hangoutPort| gets disconnected.
 * @private
 */
remoting.It2MeHelpeeChannel.prototype.onHangoutDisconnect_ = function() {
  this.dispose();
};

/**
 * Connects to the It2Me Native messaging Host and retrieves the access code.
 *
 * @param {{method:string, data:Object<string,*>}} message
 * @private
 */
remoting.It2MeHelpeeChannel.prototype.handleConnect_ =
    function(message) {
  var bounds =
      /** @type {Bounds} */ (getObjectAttr(message, 'hangoutBounds', null));

  if (this.hostState_ !== remoting.HostSession.State.UNKNOWN) {
    throw new Error('An existing connection is in progress.');
  }

  var that = this;
  this.showConfirmDialog_(bounds)
      .then(this.initializeHost_.bind(this))
      .then(this.fetchOAuthToken_.bind(this))
      .then(this.fetchEmail_.bind(this))
      /** @param {{email:string, token:string}|Promise} result */
      .then(function(result) {
        that.connectToHost_(result.email, result.token);
      /** @param {*} reason */
      }).catch(function(reason) {
        that.sendErrorResponse_(message, /** @type {Error} */ (reason));
        that.dispose();
      });
};

/**
 * Prompts the user before starting the It2Me Native Messaging Host.  This
 * ensures that even if Hangouts is compromised, an attacker cannot start the
 * host without explicit user confirmation.
 *
 * @param {Bounds} bounds Bounds of the hangout window
 * @return {Promise} A promise that will resolve if the user accepts remote
 *   assistance or reject otherwise.
 * @private
 */
remoting.It2MeHelpeeChannel.prototype.showConfirmDialog_ = function(bounds) {
  if (base.isAppsV2()) {
    return this.showConfirmDialogV2_(bounds);
  } else {
    return this.showConfirmDialogV1_();
  }
};

/**
 * @return {Promise}  A promise that will resolve if the user accepts remote
 *   assistance or reject otherwise.
 * @private
 */
remoting.It2MeHelpeeChannel.prototype.showConfirmDialogV1_ = function() {
  var messageHeader = l10n.getTranslationOrError(
      /*i18n-content*/'HANGOUTS_CONFIRM_DIALOG_MESSAGE_1');
  var message1 = l10n.getTranslationOrError(
      /*i18n-content*/'HANGOUTS_CONFIRM_DIALOG_MESSAGE_2');
  var message2 = l10n.getTranslationOrError(
      /*i18n-content*/'HANGOUTS_CONFIRM_DIALOG_MESSAGE_3');
  var message = base.escapeHTML(messageHeader) + '\n' +
                '- ' + base.escapeHTML(message1) + '\n' +
                '- ' + base.escapeHTML(message2) + '\n';

  if(window.confirm(message)) {
    return Promise.resolve();
  } else {
    return Promise.reject(new Error(remoting.Error.CANCELLED));
  }
};

/**
 * @param {Bounds} bounds the bounds of the Hangouts Window.  If set, the
 *   confirm dialog will be centered within |bounds|.
 * @return {Promise} A promise that will resolve if the user accepts remote
 *   assistance or reject otherwise.
 * @private
 */
remoting.It2MeHelpeeChannel.prototype.showConfirmDialogV2_ = function(bounds) {
  var getToken =
      base.Promise.as(chrome.identity.getAuthToken, [{interactive: false}]);

  return getToken.then(
    /** @param {string} token */
    function(token) {
      return remoting.HangoutConsentDialog.getInstance().show(Boolean(token),
                                                              bounds);
    });
};

/**
 * @return {Promise} A promise that resolves when the host is initialized.
 * @private
 */
remoting.It2MeHelpeeChannel.prototype.initializeHost_ = function() {
  /** @type {remoting.It2MeHostFacade} */
  var host = this.host_;

  /**
   * @param {function(*=):void} resolve
   * @param {function(*=):void} reject
   */
  return new Promise(function(resolve, reject) {
    if (host.initialized()) {
      resolve(true);
    } else {
      host.initialize(/** @type {function(*=):void} */ (resolve),
                      /** @type {function(*=):void} */ (reject));
    }
  });
};

/**
 * @return {Promise<string>} Promise that resolves with the OAuth token as the
 * value.
 */
remoting.It2MeHelpeeChannel.prototype.fetchOAuthToken_ = function() {
  if (base.isAppsV2()) {
    /**
     * @param {function(*=):void} resolve
     * @param {function(*=):void} reject
     */
    return new Promise(function(resolve, reject){
      remoting.identity.callWithToken(resolve, reject);
    });
  } else {
    /**
     * @param {function(*=):void} resolve
     * @param {function(*=):void} reject
     */
    return new Promise(function(resolve, reject) {
      /** @param {remoting.Error} error */
      var onError = function(error) {
        if (error === remoting.Error.NOT_AUTHENTICATED) {
          remoting.oauth2.doAuthRedirect(function() {
            remoting.identity.callWithToken(resolve, reject);
          });
          return;
        }
        reject(new Error(remoting.Error.NOT_AUTHENTICATED));
      };
      remoting.identity.callWithToken(resolve, onError);
    });
  }
};

/**
 * @param {string|Promise} token
 * @return {Promise} Promise that resolves with the access token and the email
 *   of the user.
 */
remoting.It2MeHelpeeChannel.prototype.fetchEmail_ = function(token) {
  /**
   * @param {function(*=):void} resolve
   * @param {function(*=):void} reject
   */
  return new Promise(function(resolve, reject){
    /** @param {string} email */
    function onEmail (email) {
      resolve({ email: email, token: token });
    }
    remoting.identity.getEmail(onEmail, reject);
  });
};

/**
 * Connects to the It2Me Native Messaging Host and retrieves the access code
 * in the |onHostStateChanged_| callback.
 *
 * @param {string} email
 * @param {string} accessToken
 * @private
 */
remoting.It2MeHelpeeChannel.prototype.connectToHost_ =
    function(email, accessToken) {
  base.debug.assert(this.host_.initialized());
  this.host_.connect(
    email,
    'oauth2:' + accessToken,
    this.onHostStateChanged_.bind(this),
    base.doNothing, // Ignore |onNatPolicyChanged|.
    console.log.bind(console), // Forward logDebugInfo to console.log.
    remoting.settings.XMPP_SERVER_FOR_IT2ME_HOST,
    remoting.settings.XMPP_SERVER_USE_TLS,
    remoting.settings.DIRECTORY_BOT_JID,
    this.onHostConnectError_);
};

/**
 * @param {remoting.Error} error
 * @private
 */
remoting.It2MeHelpeeChannel.prototype.onHostConnectError_ = function(error) {
  this.sendErrorResponse_(null, error);
};

/**
 * @param {remoting.HostSession.State} state
 * @private
 */
remoting.It2MeHelpeeChannel.prototype.onHostStateChanged_ = function(state) {
  this.hostState_ = state;
  var MessageTypes = remoting.It2MeHelpeeChannel.HangoutMessageTypes;
  var HostState = remoting.HostSession.State;

  switch (state) {
    case HostState.RECEIVED_ACCESS_CODE:
      var accessCode = this.host_.getAccessCode();
      this.hangoutPort_.postMessage({
        method: MessageTypes.CONNECT_RESPONSE,
        accessCode: accessCode
      });
      break;
    case HostState.CONNECTED:
    case HostState.DISCONNECTED:
      this.hangoutPort_.postMessage({
        method: MessageTypes.HOST_STATE_CHANGED,
        state: state
      });
      break;
    case HostState.ERROR:
      this.sendErrorResponse_(null, remoting.Error.UNEXPECTED);
      break;
    case HostState.INVALID_DOMAIN_ERROR:
      this.sendErrorResponse_(null, remoting.Error.INVALID_HOST_DOMAIN);
      break;
    default:
      // It is safe to ignore other state changes.
  }
};

/**
 * @param {?{method:string, data:Object<string,*>}} incomingMessage
 * @param {string|Error} error
 * @private
 */
remoting.It2MeHelpeeChannel.prototype.sendErrorResponse_ =
    function(incomingMessage, error) {
  if (error instanceof Error) {
    error = error.message;
  }

  console.error('Error responding to message method:' +
                (incomingMessage ? incomingMessage.method : 'null') +
                ' error:' + error);
  this.hangoutPort_.postMessage({
    method: remoting.It2MeHelpeeChannel.HangoutMessageTypes.ERROR,
    message: error,
    request: incomingMessage
  });
};
