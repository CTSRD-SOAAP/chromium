// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Functions related to the 'client screen' for Chromoting.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * Initiate an IT2Me connection.
 */
remoting.connectIT2Me = function() {
  var connector = remoting.app.getSessionConnector();
  var accessCode = document.getElementById('access-code-entry').value;
  remoting.setMode(remoting.AppMode.CLIENT_CONNECTING);
  connector.connectIT2Me(accessCode);
};

/**
 * Entry-point for Me2Me connections, handling showing of the host-upgrade nag
 * dialog if necessary.
 *
 * @param {string} hostId The unique id of the host.
 * @return {void} Nothing.
 */
remoting.connectMe2Me = function(hostId) {
  var host = remoting.hostList.getHostForId(hostId);
  if (!host) {
    remoting.app.onError(remoting.Error.HOST_IS_OFFLINE);
    return;
  }
  var webappVersion = chrome.runtime.getManifest().version;
  if (remoting.Host.needsUpdate(host, webappVersion)) {
    var needsUpdateMessage =
        document.getElementById('host-needs-update-message');
    l10n.localizeElementFromTag(needsUpdateMessage,
                                /*i18n-content*/'HOST_NEEDS_UPDATE_TITLE',
                                host.hostName);
    /** @type {Element} */
    var connect = document.getElementById('host-needs-update-connect-button');
    /** @type {Element} */
    var cancel = document.getElementById('host-needs-update-cancel-button');
    /** @param {Event} event */
    var onClick = function(event) {
      connect.removeEventListener('click', onClick, false);
      cancel.removeEventListener('click', onClick, false);
      if (event.target == connect) {
        remoting.connectMe2MeHostVersionAcknowledged_(host);
      } else {
        remoting.setMode(remoting.AppMode.HOME);
      }
    }
    connect.addEventListener('click', onClick, false);
    cancel.addEventListener('click', onClick, false);
    remoting.setMode(remoting.AppMode.CLIENT_HOST_NEEDS_UPGRADE);
  } else {
    remoting.connectMe2MeHostVersionAcknowledged_(host);
  }
};

/**
 * Shows PIN entry screen localized to include the host name, and registers
 * a host-specific one-shot event handler for the form submission.
 *
 * @param {remoting.Host} host The Me2Me host to which to connect.
 * @return {void} Nothing.
 */
remoting.connectMe2MeHostVersionAcknowledged_ = function(host) {
  /** @type {remoting.SessionConnector} */
  var connector = remoting.app.getSessionConnector();
  remoting.setMode(remoting.AppMode.CLIENT_CONNECTING);

  /**
   * @param {string} tokenUrl Token-issue URL received from the host.
   * @param {string} hostPublicKey Host public key (DER and Base64 encoded).
   * @param {string} scope OAuth scope to request the token for.
   * @param {function(string, string):void} onThirdPartyTokenFetched Callback.
   */
  var fetchThirdPartyToken = function(
      tokenUrl, hostPublicKey, scope, onThirdPartyTokenFetched) {
    var thirdPartyTokenFetcher = new remoting.ThirdPartyTokenFetcher(
        tokenUrl, hostPublicKey, scope, host.tokenUrlPatterns,
        onThirdPartyTokenFetched);
    thirdPartyTokenFetcher.fetchToken();
  };

  /**
   * @param {boolean} supportsPairing
   * @param {function(string):void} onPinFetched
   */
  var requestPin = function(supportsPairing, onPinFetched) {
    /** @type {Element} */
    var pinForm = document.getElementById('pin-form');
    /** @type {Element} */
    var pinCancel = document.getElementById('cancel-pin-entry-button');
    /** @type {Element} */
    var rememberPin = document.getElementById('remember-pin');
    /** @type {Element} */
    var rememberPinCheckbox = document.getElementById('remember-pin-checkbox');
    /**
     * Event handler for both the 'submit' and 'cancel' actions. Using
     * a single handler for both greatly simplifies the task of making
     * them one-shot. If separate handlers were used, each would have
     * to unregister both itself and the other.
     *
     * @param {Event} event The click or submit event.
     */
    var onSubmitOrCancel = function(event) {
      pinForm.removeEventListener('submit', onSubmitOrCancel, false);
      pinCancel.removeEventListener('click', onSubmitOrCancel, false);
      var pinField = document.getElementById('pin-entry');
      var pin = pinField.value;
      pinField.value = '';
      if (event.target == pinForm) {
        event.preventDefault();

        // Set the focus away from the password field. This has to be done
        // before the password field gets hidden, to work around a Blink
        // clipboard-handling bug - http://crbug.com/281523.
        document.getElementById('pin-connect-button').focus();

        remoting.setMode(remoting.AppMode.CLIENT_CONNECTING);
        onPinFetched(pin);
        if (rememberPinCheckbox.checked) {
          /** @type {boolean} */
          remoting.pairingRequested = true;
        }
      } else {
        remoting.setMode(remoting.AppMode.HOME);
      }
    };
    pinForm.addEventListener('submit', onSubmitOrCancel, false);
    pinCancel.addEventListener('click', onSubmitOrCancel, false);
    rememberPin.hidden = !supportsPairing;
    rememberPinCheckbox.checked = false;
    var message = document.getElementById('pin-message');
    l10n.localizeElement(message, host.hostName);
    remoting.setMode(remoting.AppMode.CLIENT_PIN_PROMPT);
  };

  /** @param {Object} settings */
  var connectMe2MeHostSettingsRetrieved = function(settings) {
    /** @type {string} */
    var clientId = '';
    /** @type {string} */
    var sharedSecret = '';
    var pairingInfo = /** @type {Object} */ (settings['pairingInfo']);
    if (pairingInfo) {
      clientId = /** @type {string} */ (pairingInfo['clientId']);
      sharedSecret = /** @type {string} */ (pairingInfo['sharedSecret']);
    }
    connector.connectMe2Me(host, requestPin, fetchThirdPartyToken,
                                    clientId, sharedSecret);
  }

  remoting.HostSettings.load(host.hostId, connectMe2MeHostSettingsRetrieved);
};
