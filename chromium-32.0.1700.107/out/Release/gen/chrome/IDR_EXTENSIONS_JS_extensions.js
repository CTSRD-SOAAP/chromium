// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A collection of utility methods for UberPage and its contained
 *     pages.
 */

cr.define('uber', function() {

  /**
   * Fixed position header elements on the page to be shifted by handleScroll.
   * @type {NodeList}
   */
  var headerElements;

  /**
   * This should be called by uber content pages when DOM content has loaded.
   */
  function onContentFrameLoaded() {
    headerElements = document.getElementsByTagName('header');
    document.addEventListener('scroll', handleScroll);

    // Prevent the navigation from being stuck in a disabled state when a
    // content page is reloaded while an overlay is visible (crbug.com/246939).
    invokeMethodOnParent('stopInterceptingEvents');

    // Trigger the scroll handler to tell the navigation if our page started
    // with some scroll (happens when you use tab restore).
    handleScroll();

    window.addEventListener('message', handleWindowMessage);
  }

  /**
   * Handles scroll events on the document. This adjusts the position of all
   * headers and updates the parent frame when the page is scrolled.
   * @private
   */
  function handleScroll() {
    var offset = document.documentElement.scrollLeft * -1;
    for (var i = 0; i < headerElements.length; i++) {
      // As a workaround for http://crbug.com/231830, set the transform to
      // 'none' rather than 0px.
      headerElements[i].style.webkitTransform = offset ?
          'translateX(' + offset + 'px)' : 'none';
    }

    invokeMethodOnParent('adjustToScroll', document.documentElement.scrollLeft);
  };

  /**
   * Handles 'message' events on window.
   * @param {Event} e The message event.
   */
  function handleWindowMessage(e) {
    if (e.data.method === 'frameSelected')
      handleFrameSelected();
    else if (e.data.method === 'mouseWheel')
      handleMouseWheel(e.data.params);
  }

  /**
   * This is called when a user selects this frame via the navigation bar
   * frame (and is triggered via postMessage() from the uber page).
   * @private
   */
  function handleFrameSelected() {
    document.documentElement.scrollLeft = 0;
  }

  /**
   * Called when a user mouse wheels (or trackpad scrolls) over the nav frame.
   * The wheel event is forwarded here and we scroll the body.
   * There's no way to figure out the actual scroll amount for a given delta.
   * It differs for every platform and even initWebKitWheelEvent takes a
   * pixel amount instead of a wheel delta. So we just choose something
   * reasonable and hope no one notices the difference.
   * @param {Object} params A structure that holds wheel deltas in X and Y.
   */
  function handleMouseWheel(params) {
    window.scrollBy(-params.deltaX * 49 / 120, -params.deltaY * 49 / 120);
  }

  /**
   * Invokes a method on the parent window (UberPage). This is a convenience
   * method for API calls into the uber page.
   * @param {string} method The name of the method to invoke.
   * @param {Object=} opt_params Optional property bag of parameters to pass to
   *     the invoked method.
   * @private
   */
  function invokeMethodOnParent(method, opt_params) {
    if (window.location == window.parent.location)
      return;

    invokeMethodOnWindow(window.parent, method, opt_params, 'chrome://chrome');
  }

  /**
   * Invokes a method on the target window.
   * @param {string} method The name of the method to invoke.
   * @param {Object=} opt_params Optional property bag of parameters to pass to
   *     the invoked method.
   * @param {string=} opt_url The origin of the target window.
   * @private
   */
  function invokeMethodOnWindow(targetWindow, method, opt_params, opt_url) {
    var data = {method: method, params: opt_params};
    targetWindow.postMessage(data, opt_url ? opt_url : '*');
  }

  return {
    invokeMethodOnParent: invokeMethodOnParent,
    invokeMethodOnWindow: invokeMethodOnWindow,
    onContentFrameLoaded: onContentFrameLoaded,
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  'use strict';

  /**
   * Creates a new list of extension commands.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {cr.ui.div}
   */
  var ExtensionCommandList = cr.ui.define('div');

  /** @const */ var keyComma = 188;
  /** @const */ var keyDel = 46;
  /** @const */ var keyDown = 40;
  /** @const */ var keyEnd = 35;
  /** @const */ var keyHome = 36;
  /** @const */ var keyIns = 45;
  /** @const */ var keyLeft = 37;
  /** @const */ var keyMediaNextTrack = 176;
  /** @const */ var keyMediaPlayPause = 179;
  /** @const */ var keyMediaPrevTrack = 177;
  /** @const */ var keyMediaStop = 178;
  /** @const */ var keyPageDown = 34;
  /** @const */ var keyPageUp = 33;
  /** @const */ var keyPeriod = 190;
  /** @const */ var keyRight = 39;
  /** @const */ var keyTab = 9;
  /** @const */ var keyUp = 38;

  /**
   * Enum for whether we require modifiers of a keycode.
   * @enum {number}
   */
  var Modifiers = {
    ARE_NOT_ALLOWED: 0,
    ARE_REQUIRED: 1
  };

  /**
   * Returns whether the passed in |keyCode| is a valid extension command
   * char or not. This is restricted to A-Z and 0-9 (ignoring modifiers) at
   * the moment.
   * @param {int} keyCode The keycode to consider.
   * @return {boolean} Returns whether the char is valid.
   */
  function validChar(keyCode) {
    return keyCode == keyComma ||
           keyCode == keyDel ||
           keyCode == keyDown ||
           keyCode == keyEnd ||
           keyCode == keyHome ||
           keyCode == keyIns ||
           keyCode == keyLeft ||
           keyCode == keyMediaNextTrack ||
           keyCode == keyMediaPlayPause ||
           keyCode == keyMediaPrevTrack ||
           keyCode == keyMediaStop ||
           keyCode == keyPageDown ||
           keyCode == keyPageUp ||
           keyCode == keyPeriod ||
           keyCode == keyRight ||
           keyCode == keyTab ||
           keyCode == keyUp ||
           (keyCode >= 'A'.charCodeAt(0) && keyCode <= 'Z'.charCodeAt(0)) ||
           (keyCode >= '0'.charCodeAt(0) && keyCode <= '9'.charCodeAt(0));
  }

  /**
   * Convert a keystroke event to string form, while taking into account
   * (ignoring) invalid extension commands.
   * @param {Event} event The keyboard event to convert.
   * @return {string} The keystroke as a string.
   */
  function keystrokeToString(event) {
    var output = '';
    if (cr.isMac && event.metaKey)
      output = 'Command+';
    if (event.ctrlKey)
      output = 'Ctrl+';
    if (!event.ctrlKey && event.altKey)
      output += 'Alt+';
    if (event.shiftKey)
      output += 'Shift+';

    var keyCode = event.keyCode;
    if (validChar(keyCode)) {
      if ((keyCode >= 'A'.charCodeAt(0) && keyCode <= 'Z'.charCodeAt(0)) ||
          (keyCode >= '0'.charCodeAt(0) && keyCode <= '9'.charCodeAt(0))) {
        output += String.fromCharCode('A'.charCodeAt(0) + keyCode - 65);
      } else {
        switch (keyCode) {
          case keyComma:
            output += 'Comma'; break;
          case keyDel:
            output += 'Delete'; break;
          case keyDown:
            output += 'Down'; break;
          case keyEnd:
            output += 'End'; break;
          case keyHome:
            output += 'Home'; break;
          case keyIns:
            output += 'Insert'; break;
          case keyLeft:
            output += 'Left'; break;
          case keyMediaNextTrack:
            output += 'MediaNextTrack'; break;
          case keyMediaPlayPause:
            output += 'MediaPlayPause'; break;
          case keyMediaPrevTrack:
            output += 'MediaPrevTrack'; break;
          case keyMediaStop:
            output += 'MediaStop'; break;
          case keyPageDown:
            output += 'PageDown'; break;
          case keyPageUp:
            output += 'PageUp'; break;
          case keyPeriod:
            output += 'Period'; break;
          case keyRight:
            output += 'Right'; break;
          case keyTab:
            output += 'Tab'; break;
          case keyUp:
            output += 'Up'; break;
        }
      }
    }

    return output;
  }

  /**
   * Returns whether the passed in |keyCode| require modifiers. Currently only
   * "MediaNextTrack", "MediaPrevTrack", "MediaStop", "MediaPlayPause" are
   * required to be used without any modifier.
   * @param {int} keyCode The keycode to consider.
   * @return {Modifiers} Returns whether the keycode require modifiers.
   */
  function modifiers(keyCode) {
    switch (keyCode) {
      case keyMediaNextTrack:
      case keyMediaPlayPause:
      case keyMediaPrevTrack:
      case keyMediaStop:
        return Modifiers.ARE_NOT_ALLOWED;
      default:
        return Modifiers.ARE_REQUIRED;
    }
  }

  /**
   * Return true if the specified keyboard event has any one of following
   * modifiers: "Ctrl", "Alt", "Cmd" on Mac, and "Shift" when the
   * countShiftAsModifier is true.
   * @param {Event} event The keyboard event to consider.
   * @param {boolean} countShiftAsModifier Whether the 'ShiftKey' should be
   *     counted as modifier.
   */
  function hasModifier(event, countShiftAsModifier) {
    return event.ctrlKey || event.altKey || (cr.isMac && event.metaKey) ||
           (countShiftAsModifier && event.shiftKey);
  }

  ExtensionCommandList.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * While capturing, this records the current (last) keyboard event generated
     * by the user. Will be |null| after capture and during capture when no
     * keyboard event has been generated.
     * @type: {keyboard event}.
     * @private
     */
    currentKeyEvent_: null,

    /**
     * While capturing, this keeps track of the previous selection so we can
     * revert back to if no valid assignment is made during capture.
     * @type: {string}.
     * @private
     */
    oldValue_: '',

    /**
     * While capturing, this keeps track of which element the user asked to
     * change.
     * @type: {HTMLElement}.
     * @private
     */
    capturingElement_: null,

    /** @override */
    decorate: function() {
      this.textContent = '';

      // Iterate over the extension data and add each item to the list.
      this.data_.commands.forEach(this.createNodeForExtension_.bind(this));
    },

    /**
     * Synthesizes and initializes an HTML element for the extension command
     * metadata given in |extension|.
     * @param {Object} extension A dictionary of extension metadata.
     * @private
     */
    createNodeForExtension_: function(extension) {
      var template = $('template-collection-extension-commands').querySelector(
          '.extension-command-list-extension-item-wrapper');
      var node = template.cloneNode(true);

      var title = node.querySelector('.extension-title');
      title.textContent = extension.name;

      this.appendChild(node);

      // Iterate over the commands data within the extension and add each item
      // to the list.
      extension.commands.forEach(this.createNodeForCommand_.bind(this));
    },

    /**
     * Synthesizes and initializes an HTML element for the extension command
     * metadata given in |command|.
     * @param {Object} command A dictionary of extension command metadata.
     * @private
     */
    createNodeForCommand_: function(command) {
      var template = $('template-collection-extension-commands').querySelector(
          '.extension-command-list-command-item-wrapper');
      var node = template.cloneNode(true);
      node.id = this.createElementId_(
          'command', command.extension_id, command.command_name);

      var description = node.querySelector('.command-description');
      description.textContent = command.description;

      var shortcutNode = node.querySelector('.command-shortcut-text');
      shortcutNode.addEventListener('mouseup',
                                    this.startCapture_.bind(this));
      shortcutNode.addEventListener('blur', this.endCapture_.bind(this));
      shortcutNode.addEventListener('keydown',
                                    this.handleKeyDown_.bind(this));
      shortcutNode.addEventListener('keyup', this.handleKeyUp_.bind(this));
      if (!command.active) {
        shortcutNode.textContent =
            loadTimeData.getString('extensionCommandsInactive');

        var commandShortcut = node.querySelector('.command-shortcut');
        commandShortcut.classList.add('inactive-keybinding');
      } else {
        shortcutNode.textContent = command.keybinding;
      }

      var commandClear = node.querySelector('.command-clear');
      commandClear.id = this.createElementId_(
          'clear', command.extension_id, command.command_name);
      commandClear.title = loadTimeData.getString('extensionCommandsDelete');
      commandClear.addEventListener('click', this.handleClear_.bind(this));

      if (command.scope_ui_visible) {
        var select = node.querySelector('.command-scope');
        select.id = this.createElementId_(
            'setCommandScope', command.extension_id, command.command_name);
        select.hidden = false;
        // Add the 'In Chrome' option.
        var option = document.createElement('option');
        option.textContent = loadTimeData.getString('extensionCommandsRegular');
        select.appendChild(option);
        if (command.extension_action) {
          // Extension actions cannot be global, so we might as well disable the
          // combo box, to signify that.
          select.disabled = true;
        } else {
          // Add the 'Global' option.
          option = document.createElement('option');
          option.textContent =
              loadTimeData.getString('extensionCommandsGlobal');
          select.appendChild(option);
          select.selectedIndex = command.global ? 1 : 0;
        }

        select.addEventListener(
            'click', this.handleSetCommandScope_.bind(this));
      }

      this.appendChild(node);
    },

    /**
     * Starts keystroke capture to determine which key to use for a particular
     * extension command.
     * @param {Event} event The keyboard event to consider.
     * @private
     */
    startCapture_: function(event) {
      if (this.capturingElement_)
        return;  // Already capturing.

      chrome.send('setShortcutHandlingSuspended', [true]);

      var shortcutNode = event.target;
      this.oldValue_ = shortcutNode.textContent;
      shortcutNode.textContent =
          loadTimeData.getString('extensionCommandsStartTyping');
      shortcutNode.parentElement.classList.add('capturing');

      var commandClear =
          shortcutNode.parentElement.querySelector('.command-clear');
      commandClear.hidden = true;

      this.capturingElement_ = event.target;
    },

    /**
     * Ends keystroke capture and either restores the old value or (if valid
     * value) sets the new value as active..
     * @param {Event} event The keyboard event to consider.
     * @private
     */
    endCapture_: function(event) {
      if (!this.capturingElement_)
        return;  // Not capturing.

      chrome.send('setShortcutHandlingSuspended', [false]);

      var shortcutNode = this.capturingElement_;
      var commandShortcut = shortcutNode.parentElement;

      commandShortcut.classList.remove('capturing');
      commandShortcut.classList.remove('contains-chars');

      // When the capture ends, the user may have not given a complete and valid
      // input (or even no input at all). Only a valid key event followed by a
      // valid key combination will cause a shortcut selection to be activated.
      // If no valid selection was made, howver, revert back to what the textbox
      // had before to indicate that the shortcut registration was cancelled.
      if (!this.currentKeyEvent_ || !validChar(this.currentKeyEvent_.keyCode))
        shortcutNode.textContent = this.oldValue_;

      var commandClear = commandShortcut.querySelector('.command-clear');
      if (this.oldValue_ == '') {
        commandShortcut.classList.remove('clearable');
        commandClear.hidden = true;
      } else {
        commandShortcut.classList.add('clearable');
        commandClear.hidden = false;
      }

      this.oldValue_ = '';
      this.capturingElement_ = null;
      this.currentKeyEvent_ = null;
    },

    /**
     * The KeyDown handler.
     * @param {Event} event The keyboard event to consider.
     * @private
     */
    handleKeyDown_: function(event) {
      if (!this.capturingElement_)
        this.startCapture_(event);

      this.handleKey_(event);
    },

    /**
     * The KeyUp handler.
     * @param {Event} event The keyboard event to consider.
     * @private
     */
    handleKeyUp_: function(event) {
      // We want to make it easy to change from Ctrl+Shift+ to just Ctrl+ by
      // releasing Shift, but we also don't want it to be easy to lose for
      // example Ctrl+Shift+F to Ctrl+ just because you didn't release Ctrl
      // as fast as the other two keys. Therefore, we process KeyUp until you
      // have a valid combination and then stop processing it (meaning that once
      // you have a valid combination, we won't change it until the next
      // KeyDown message arrives).
      if (!this.currentKeyEvent_ || !validChar(this.currentKeyEvent_.keyCode)) {
        if (!event.ctrlKey && !event.altKey) {
          // If neither Ctrl nor Alt is pressed then it is not a valid shortcut.
          // That means we're back at the starting point so we should restart
          // capture.
          this.endCapture_(event);
          this.startCapture_(event);
        } else {
          this.handleKey_(event);
        }
      }
    },

    /**
     * A general key handler (used for both KeyDown and KeyUp).
     * @param {Event} event The keyboard event to consider.
     * @private
     */
    handleKey_: function(event) {
      // While capturing, we prevent all events from bubbling, to prevent
      // shortcuts lacking the right modifier (F3 for example) from activating
      // and ending capture prematurely.
      event.preventDefault();
      event.stopPropagation();

      if (modifiers(event.keyCode) == Modifiers.ARE_REQUIRED &&
          !hasModifier(event, false)) {
        // Ctrl or Alt (or Cmd on Mac) is a must for most shortcuts.
        return;
      }

      if (modifiers(event.keyCode) == Modifiers.ARE_NOT_ALLOWED &&
          hasModifier(event, true)) {
        return;
      }

      var shortcutNode = this.capturingElement_;
      var keystroke = keystrokeToString(event);
      shortcutNode.textContent = keystroke;
      event.target.classList.add('contains-chars');

      if (validChar(event.keyCode)) {
        var node = event.target;
        while (node && !node.id)
          node = node.parentElement;

        this.oldValue_ = keystroke;  // Forget what the old value was.
        var parsed = this.parseElementId_('command', node.id);
        chrome.send('setExtensionCommandShortcut',
                    [parsed.extensionId, parsed.commandName, keystroke]);
        this.endCapture_(event);
      }

      this.currentKeyEvent_ = event;
    },

    /**
     * A handler for the delete command button.
     * @param {Event} event The mouse event to consider.
     * @private
     */
    handleClear_: function(event) {
      var parsed = this.parseElementId_('clear', event.target.id);
      chrome.send('setExtensionCommandShortcut',
          [parsed.extensionId, parsed.commandName, '']);
    },

    /**
     * A handler for the setting the scope of the command.
     * @param {Event} event The mouse event to consider.
     * @private
     */
    handleSetCommandScope_: function(event) {
      var parsed = this.parseElementId_('setCommandScope', event.target.id);
      var element = document.getElementById(
          'setCommandScope-' + parsed.extensionId + '-' + parsed.commandName);
      chrome.send('setCommandScope',
          [parsed.extensionId, parsed.commandName, element.selectedIndex == 1]);
    },

    /**
     * A utility function to create a unique element id based on a namespace,
     * extension id and a command name.
     * @param {string} namespace   The namespace to prepend the id with.
     * @param {string} extensionId The extension ID to use in the id.
     * @param {string} commandName The command name to append the id with.
     * @private
     */
    createElementId_: function(namespace, extensionId, commandName) {
      return namespace + '-' + extensionId + '-' + commandName;
    },

    /**
     * A utility function to parse a unique element id based on a namespace,
     * extension id and a command name.
     * @param {string} namespace   The namespace to prepend the id with.
     * @param {string} id          The id to parse.
     * @return {object} The parsed id, as an object with two members:
     *                  extensionID and commandName.
     * @private
     */
    parseElementId_: function(namespace, id) {
      var kExtensionIdLength = 32;
      return {
        extensionId: id.substring(namespace.length + 1,
                                  namespace.length + 1 + kExtensionIdLength),
        commandName: id.substring(namespace.length + 1 + kExtensionIdLength + 1)
      };
    },
  };

  return {
    ExtensionCommandList: ExtensionCommandList
  };
});


cr.define('extensions', function() {
  'use strict';

  // The Extension Commands list object that will be used to show the commands
  // on the page.
  var ExtensionCommandList = options.ExtensionCommandList;

  /**
   * ExtensionCommandsOverlay class
   * Encapsulated handling of the 'Extension Commands' overlay page.
   * @constructor
   */
  function ExtensionCommandsOverlay() {
  }

  cr.addSingletonGetter(ExtensionCommandsOverlay);

  ExtensionCommandsOverlay.prototype = {
    /**
     * Initialize the page.
     */
    initializePage: function() {
      var overlay = $('overlay');
      cr.ui.overlay.setupOverlay(overlay);
      cr.ui.overlay.globalInitialization();
      overlay.addEventListener('cancelOverlay', this.handleDismiss_.bind(this));

      $('extension-commands-dismiss').addEventListener('click',
          this.handleDismiss_.bind(this));

      // This will request the data to show on the page and will get a response
      // back in returnExtensionsData.
      chrome.send('extensionCommandsRequestExtensionsData');
    },

    /**
     * Handles a click on the dismiss button.
     * @param {Event} e The click event.
     */
    handleDismiss_: function(e) {
      extensions.ExtensionSettings.showOverlay(null);
    },
  };

  /**
   * Called by the dom_ui_ to re-populate the page with data representing
   * the current state of extension commands.
   */
  ExtensionCommandsOverlay.returnExtensionsData = function(extensionsData) {
    ExtensionCommandList.prototype.data_ = extensionsData;
    var extensionCommandList = $('extension-command-list');
    ExtensionCommandList.decorate(extensionCommandList);

    // Make sure the config link is visible, since there are commands to show
    // and potentially configure.
    document.querySelector('.extension-commands-config').hidden =
        extensionsData.commands.length == 0;

    $('no-commands').hidden = extensionsData.commands.length > 0;
    var list = $('extension-command-list');
    if (extensionsData.commands.length == 0)
      list.classList.add('empty-extension-commands-list');
    else
      list.classList.remove('empty-extension-commands-list');
  }

  // Export
  return {
    ExtensionCommandsOverlay: ExtensionCommandsOverlay
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  var FocusManager = cr.ui.FocusManager;

  function ExtensionFocusManager() {
    FocusManager.disableMouseFocusOnButtons();
  }

  cr.addSingletonGetter(ExtensionFocusManager);

  ExtensionFocusManager.prototype = {
    __proto__: FocusManager.prototype,

    /** @override */
    getFocusParent: function() {
      var overlay = extensions.ExtensionSettings.getCurrentOverlay();
      return overlay || $('extension-settings');
    },
  };

  return {
    ExtensionFocusManager: ExtensionFocusManager,
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  /**
   * Returns whether or not a given |url| is associated with an extension.
   * @param {string} url The url to examine.
   * @param {string} extensionUrl The url of the extension.
   * @return {boolean} Whether or not the url is associated with the extension.
   */
  function isExtensionUrl(url, extensionUrl) {
    return url.substring(0, extensionUrl.length) == extensionUrl;
  }

  /**
   * Get the url relative to the main extension url. If the url is
   * unassociated with the extension, this will be the full url.
   * @param {string} url The url to make relative.
   * @param {string} extensionUrl The host for which the url is relative.
   * @return {string} The url relative to the host.
   */
  function getRelativeUrl(url, extensionUrl) {
    return isExtensionUrl(url, extensionUrl) ?
        url.substring(extensionUrl.length) : url;
  }

  /**
   * Clone a template within the extension error template collection.
   * @param {string} templateName The class name of the template to clone.
   * @return {HTMLElement} The clone of the template.
   */
  function cloneTemplate(templateName) {
    return $('template-collection-extension-error').
        querySelector('.' + templateName).cloneNode(true);
  }

  /**
   * Creates a new ExtensionError HTMLElement; this is used to show a
   * notification to the user when an error is caused by an extension.
   * @param {Object} error The error the element should represent.
   * @param {string} templateName The name of the template to clone for the
   *     error ('extension-error-[detailed|simple]-wrapper').
   * @constructor
   * @extends {HTMLDivElement}
   */
  function ExtensionError(error, templateName) {
    var div = cloneTemplate(templateName);
    div.__proto__ = ExtensionError.prototype;
    div.error_ = error;
    div.decorate();
    return div;
  }

  ExtensionError.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @override */
    decorate: function() {
      var metadata = cloneTemplate('extension-error-metadata');

      // Add an additional class for the severity level.
      if (this.error_.level == 0)
        metadata.classList.add('extension-error-severity-info');
      else if (this.error_.level == 1)
        metadata.classList.add('extension-error-severity-warning');
      else
        metadata.classList.add('extension-error-severity-fatal');

      var iconNode = document.createElement('img');
      iconNode.className = 'extension-error-icon';
      metadata.insertBefore(iconNode, metadata.firstChild);

      // Add a property for the extension's base url in order to determine if
      // a url belongs to the extension.
      this.extensionUrl_ =
          'chrome-extension://' + this.error_.extensionId + '/';

      metadata.querySelector('.extension-error-message').textContent =
          this.error_.message;

      metadata.appendChild(this.createViewSourceAndInspect_(
          getRelativeUrl(this.error_.source, this.extensionUrl_),
          this.error_.source));

      // The error template may specify a <summary> to put template metadata in.
      // If not, just append it to the top-level element.
      var metadataContainer = this.querySelector('summary') || this;
      metadataContainer.appendChild(metadata);

      var detailsNode = this.querySelector('.extension-error-details');
      if (detailsNode && this.error_.contextUrl)
        detailsNode.appendChild(this.createContextNode_());
      if (detailsNode && this.error_.stackTrace) {
        var stackNode = this.createStackNode_();
        if (stackNode)
          detailsNode.appendChild(this.createStackNode_());
      }
    },

    /**
     * Return a div with text |description|. If it's possible to view the source
     * for |url|, linkify the div to do so. Attach an inspect button if it's
     * possible to open the inspector for |url|.
     * @param {string} description a human-friendly description the location
     *     (e.g., filename, line).
     * @param {string} url The url of the resource to view.
     * @param {?number} line An optional line number of the resource.
     * @param {?number} column An optional column number of the resource.
     * @return {HTMLElement} The created node, either a link or plaintext.
     * @private
     */
    createViewSourceAndInspect_: function(description, url, line, column) {
      var errorLinks = document.createElement('div');
      errorLinks.className = 'extension-error-links';

      if (this.error_.canInspect)
        errorLinks.appendChild(this.createInspectLink_(url, line, column));

      if (this.canViewSource_(url))
        var viewSource = this.createViewSourceLink_(url, line);
      else
        var viewSource = document.createElement('div');
      viewSource.className = 'extension-error-view-source';
      viewSource.textContent = description;
      errorLinks.appendChild(viewSource);
      return errorLinks;
    },

    /**
     * Determine whether we can view the source of a given url.
     * @param {string} url The url of the resource to view.
     * @return {boolean} Whether or not we can view the source for the url.
     * @private
     */
    canViewSource_: function(url) {
      return isExtensionUrl(url, this.extensionUrl_) || url == 'manifest.json';
    },

    /**
     * Determine whether or not we should display the url to the user. We don't
     * want to include any of our own code in stack traces.
     * @param {string} url The url in question.
     * @return {boolean} True if the url should be displayed, and false
     *     otherwise (i.e., if it is an internal script).
     */
    shouldDisplayForUrl_: function(url) {
      var extensionsNamespace = 'extensions::';
      // All our internal scripts are in the 'extensions::' namespace.
      return url.substr(0, extensionsNamespace.length) != extensionsNamespace;
    },

    /**
     * Create a clickable node to view the source for the given url.
     * @param {string} url The url to the resource to view.
     * @param {?number} line An optional line number of the resource (for
     *     source files).
     * @return {HTMLElement} The clickable node to view the source.
     * @private
     */
    createViewSourceLink_: function(url, line) {
      var viewSource = document.createElement('a');
      viewSource.href = 'javascript:void(0)';
      var relativeUrl = getRelativeUrl(url, this.extensionUrl_);
      var requestFileSourceArgs = { 'extensionId': this.error_.extensionId,
                                    'message': this.error_.message,
                                    'pathSuffix': relativeUrl };
      if (relativeUrl == 'manifest.json') {
        requestFileSourceArgs.manifestKey = this.error_.manifestKey;
        requestFileSourceArgs.manifestSpecific = this.error_.manifestSpecific;
      } else {
        // Prefer |line| if available, or default to the line of the last stack
        // frame.
        requestFileSourceArgs.lineNumber =
            line ? line : this.getLastPosition_('lineNumber');
      }

      viewSource.addEventListener('click', function(e) {
        chrome.send('extensionErrorRequestFileSource', [requestFileSourceArgs]);
      });
      viewSource.title = loadTimeData.getString('extensionErrorViewSource');
      return viewSource;
    },

    /**
     * Check the most recent stack frame to get the last position in the code.
     * @param {string} type The position type, i.e. '[line|column]Number'.
     * @return {?number} The last position of the given |type|, or undefined if
     *     there is no stack trace to check.
     * @private
     */
    getLastPosition_: function(type) {
      var stackTrace = this.error_.stackTrace;
      return stackTrace && stackTrace[0] ? stackTrace[0][type] : undefined;
    },

    /**
     * Create an "Inspect" link, in the form of an icon.
     * @param {?string} url The url of the resource to inspect; if absent, the
     *     render view (and no particular resource) is inspected.
     * @param {?number} line An optional line number of the resource.
     * @param {?number} column An optional column number of the resource.
     * @return {HTMLImageElement} The created "Inspect" link for the resource.
     * @private
     */
    createInspectLink_: function(url, line, column) {
      var linkWrapper = document.createElement('a');
      linkWrapper.href = 'javascript:void(0)';
      var inspectIcon = document.createElement('img');
      inspectIcon.className = 'extension-error-inspect';
      inspectIcon.title = loadTimeData.getString('extensionErrorInspect');

      inspectIcon.addEventListener('click', function(e) {
          chrome.send('extensionErrorOpenDevTools',
                      [{'renderProcessId': this.error_.renderProcessId,
                        'renderViewId': this.error_.renderViewId,
                        'url': url,
                        'lineNumber': line ? line :
                            this.getLastPosition_('lineNumber'),
                        'columnNumber': column ? column :
                            this.getLastPosition_('columnNumber')}]);
      }.bind(this));
      linkWrapper.appendChild(inspectIcon);
      return linkWrapper;
    },

    /**
     * Get the context node for this error. This will attempt to link to the
     * context in which the error occurred, and can be either an extension page
     * or an external page.
     * @return {HTMLDivElement} The context node for the error, including the
     *     label and a link to the context.
     * @private
     */
    createContextNode_: function() {
      var node = cloneTemplate('extension-error-context-wrapper');
      var linkNode = node.querySelector('a');
      if (isExtensionUrl(this.error_.contextUrl, this.extensionUrl_)) {
        linkNode.textContent = getRelativeUrl(this.error_.contextUrl,
                                              this.extensionUrl_);
      } else {
        linkNode.textContent = this.error_.contextUrl;
      }

      // Prepend a link to inspect the context page, if possible.
      if (this.error_.canInspect)
        node.insertBefore(this.createInspectLink_(), linkNode);

      linkNode.href = this.error_.contextUrl;
      linkNode.target = '_blank';
      return node;
    },

    /**
     * Get a node for the stack trace for this error. Each stack frame will
     * include a resource url, line number, and function name (possibly
     * anonymous). If possible, these frames will also be linked for viewing the
     * source and inspection.
     * @return {HTMLDetailsElement} The stack trace node for this error, with
     *     all stack frames nested in a details-summary object.
     * @private
     */
    createStackNode_: function() {
      var node = cloneTemplate('extension-error-stack-trace');
      var listNode = node.querySelector('.extension-error-stack-trace-list');
      this.error_.stackTrace.forEach(function(frame) {
        if (!this.shouldDisplayForUrl_(frame.url))
          return;
        var frameNode = document.createElement('div');
        var description = getRelativeUrl(frame.url, this.extensionUrl_) +
                          ':' + frame.lineNumber;
        if (frame.functionName) {
          var functionName = frame.functionName == '(anonymous function)' ?
              loadTimeData.getString('extensionErrorAnonymousFunction') :
              frame.functionName;
          description += ' (' + functionName + ')';
        }
        frameNode.appendChild(this.createViewSourceAndInspect_(
            description, frame.url, frame.lineNumber, frame.columnNumber));
        listNode.appendChild(
            document.createElement('li')).appendChild(frameNode);
      }, this);

      if (listNode.childElementCount == 0)
        return undefined;

      return node;
    },
  };

  /**
   * A variable length list of runtime or manifest errors for a given extension.
   * @param {Array.<Object>} errors The list of extension errors with which
   *     to populate the list.
   * @param {string} title The i18n key for the title of the error list, i.e.
   *     'extensionErrors[Manifest,Runtime]Errors'.
   * @constructor
   * @extends {HTMLDivElement}
   */
  function ExtensionErrorList(errors, title) {
    var div = cloneTemplate('extension-error-list');
    div.__proto__ = ExtensionErrorList.prototype;
    div.errors_ = errors;
    div.title_ = title;
    div.decorate();
    return div;
  }

  ExtensionErrorList.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * @private
     * @const
     * @type {number}
     */
    MAX_ERRORS_TO_SHOW_: 3,

    /** @override */
    decorate: function() {
      this.querySelector('.extension-error-list-title').textContent =
          loadTimeData.getString(this.title_);

      this.contents_ = this.querySelector('.extension-error-list-contents');
      this.errors_.forEach(function(error) {
        this.contents_.appendChild(document.createElement('li')).appendChild(
            new ExtensionError(error,
                               error.contextUrl || error.stackTrace ?
                                   'extension-error-detailed-wrapper' :
                                   'extension-error-simple-wrapper'));
      }, this);

      if (this.contents_.children.length > this.MAX_ERRORS_TO_SHOW_) {
        for (var i = this.MAX_ERRORS_TO_SHOW_;
             i < this.contents_.children.length; ++i) {
          this.contents_.children[i].hidden = true;
        }
        this.initShowMoreButton_();
      }
    },

    /**
     * Initialize the "Show More" button for the error list. If there are more
     * than |MAX_ERRORS_TO_SHOW_| errors in the list.
     * @private
     */
    initShowMoreButton_: function() {
      var button = this.querySelector('.extension-error-list-show-more a');
      button.hidden = false;
      button.isShowingAll = false;
      button.addEventListener('click', function(e) {
        for (var i = this.MAX_ERRORS_TO_SHOW_;
             i < this.contents_.children.length; ++i) {
          this.contents_.children[i].hidden = button.isShowingAll;
        }
        var message = button.isShowingAll ? 'extensionErrorsShowMore' :
                                            'extensionErrorsShowFewer';
        button.textContent = loadTimeData.getString(message);
        button.isShowingAll = !button.isShowingAll;
      }.bind(this));
    }
  };

  return {
    ExtensionErrorList: ExtensionErrorList
  };
});


cr.define('options', function() {
  'use strict';

  /**
   * Creates a new list of extensions.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {cr.ui.div}
   */
  var ExtensionsList = cr.ui.define('div');

  /**
   * @type {Object.<string, boolean>} A map from extension id to a boolean
   *     indicating whether the incognito warning is showing. This persists
   *     between calls to decorate.
   */
  var butterBarVisibility = {};

  /**
   * @type {Object.<string, string>} A map from extension id to last reloaded
   *     timestamp. The timestamp is recorded when the user click the 'Reload'
   *     link. It is used to refresh the icon of an unpacked extension.
   *     This persists between calls to decorate.
   */
  var extensionReloadedTimestamp = {};

  ExtensionsList.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @override */
    decorate: function() {
      this.textContent = '';

      this.showExtensionNodes_();
    },

    getIdQueryParam_: function() {
      return parseQueryParams(document.location)['id'];
    },

    /**
     * Creates all extension items from scratch.
     * @private
     */
    showExtensionNodes_: function() {
      // Iterate over the extension data and add each item to the list.
      this.data_.extensions.forEach(this.createNode_, this);

      var idToHighlight = this.getIdQueryParam_();
      if (idToHighlight && $(idToHighlight)) {
        // Scroll offset should be calculated slightly higher than the actual
        // offset of the element being scrolled to, so that it ends up not all
        // the way at the top. That way it is clear that there are more elements
        // above the element being scrolled to.
        var scrollFudge = 1.2;
        document.documentElement.scrollTop = $(idToHighlight).offsetTop -
            scrollFudge * $(idToHighlight).clientHeight;
      }

      if (this.data_.extensions.length == 0)
        this.classList.add('empty-extension-list');
      else
        this.classList.remove('empty-extension-list');
    },

    /**
     * Synthesizes and initializes an HTML element for the extension metadata
     * given in |extension|.
     * @param {Object} extension A dictionary of extension metadata.
     * @private
     */
    createNode_: function(extension) {
      var template = $('template-collection').querySelector(
          '.extension-list-item-wrapper');
      var node = template.cloneNode(true);
      node.id = extension.id;

      if (!extension.enabled || extension.terminated)
        node.classList.add('inactive-extension');

      if (!extension.userModifiable)
        node.classList.add('may-not-disable');

      var idToHighlight = this.getIdQueryParam_();
      if (node.id == idToHighlight)
        node.classList.add('extension-highlight');

      var item = node.querySelector('.extension-list-item');
      // Prevent the image cache of extension icon by using the reloaded
      // timestamp as a query string. The timestamp is recorded when the user
      // clicks the 'Reload' link. http://crbug.com/159302.
      if (extensionReloadedTimestamp[extension.id]) {
        item.style.backgroundImage =
            'url(' + extension.icon + '?' +
            extensionReloadedTimestamp[extension.id] + ')';
      } else {
        item.style.backgroundImage = 'url(' + extension.icon + ')';
      }

      var title = node.querySelector('.extension-title');
      title.textContent = extension.name;

      var version = node.querySelector('.extension-version');
      version.textContent = extension.version;

      var locationText = node.querySelector('.location-text');
      locationText.textContent = extension.locationText;

      var description = node.querySelector('.extension-description span');
      description.textContent = extension.description;

      // The 'Show Browser Action' button.
      if (extension.enable_show_button) {
        var showButton = node.querySelector('.show-button');
        showButton.addEventListener('click', function(e) {
          chrome.send('extensionSettingsShowButton', [extension.id]);
        });
        showButton.hidden = false;
      }

      // The 'allow in incognito' checkbox.
      var incognito = node.querySelector('.incognito-control input');
      incognito.disabled = !extension.incognitoCanBeToggled;
      incognito.checked = extension.enabledIncognito;
      if (!incognito.disabled) {
        incognito.addEventListener('change', function(e) {
          var checked = e.target.checked;
          butterBarVisibility[extension.id] = checked;
          butterBar.hidden = !checked || extension.is_hosted_app;
          chrome.send('extensionSettingsEnableIncognito',
                      [extension.id, String(checked)]);
        });
      }
      var butterBar = node.querySelector('.butter-bar');
      butterBar.hidden = !butterBarVisibility[extension.id];

      // The 'allow file:// access' checkbox.
      if (extension.wantsFileAccess) {
        var fileAccess = node.querySelector('.file-access-control');
        fileAccess.addEventListener('click', function(e) {
          chrome.send('extensionSettingsAllowFileAccess',
                      [extension.id, String(e.target.checked)]);
        });
        fileAccess.querySelector('input').checked = extension.allowFileAccess;
        fileAccess.hidden = false;
      }

      // The 'Options' link.
      if (extension.enabled && extension.optionsUrl) {
        var options = node.querySelector('.options-link');
        options.addEventListener('click', function(e) {
          chrome.send('extensionSettingsOptions', [extension.id]);
          e.preventDefault();
        });
        options.hidden = false;
      }

      // The 'Permissions' link.
      var permissions = node.querySelector('.permissions-link');
      permissions.addEventListener('click', function(e) {
        chrome.send('extensionSettingsPermissions', [extension.id]);
        e.preventDefault();
      });

      // The 'View in Web Store/View Web Site' link.
      if (extension.homepageUrl) {
        var siteLink = node.querySelector('.site-link');
        siteLink.href = extension.homepageUrl;
        siteLink.textContent = loadTimeData.getString(
                extension.homepageProvided ? 'extensionSettingsVisitWebsite' :
                                             'extensionSettingsVisitWebStore');
        siteLink.hidden = false;
      }

      if (extension.allow_reload) {
        // The 'Reload' link.
        var reload = node.querySelector('.reload-link');
        reload.addEventListener('click', function(e) {
          chrome.send('extensionSettingsReload', [extension.id]);
          extensionReloadedTimestamp[extension.id] = Date.now();
        });
        reload.hidden = false;

        if (extension.is_platform_app) {
          // The 'Launch' link.
          var launch = node.querySelector('.launch-link');
          launch.addEventListener('click', function(e) {
            chrome.send('extensionSettingsLaunch', [extension.id]);
          });
          launch.hidden = false;
        }
      }

      if (!extension.terminated) {
        // The 'Enabled' checkbox.
        var enable = node.querySelector('.enable-checkbox');
        enable.hidden = false;
        enable.querySelector('input').disabled = !extension.userModifiable;

        if (extension.userModifiable) {
          enable.addEventListener('click', function(e) {
            // When e.target is the label instead of the checkbox, it doesn't
            // have the checked property and the state of the checkbox is
            // left unchanged.
            var checked = e.target.checked;
            if (checked == undefined)
              checked = !e.currentTarget.querySelector('input').checked;
            chrome.send('extensionSettingsEnable',
                        [extension.id, checked ? 'true' : 'false']);

            // This may seem counter-intuitive (to not set/clear the checkmark)
            // but this page will be updated asynchronously if the extension
            // becomes enabled/disabled. It also might not become enabled or
            // disabled, because the user might e.g. get prompted when enabling
            // and choose not to.
            e.preventDefault();
          });
        }

        enable.querySelector('input').checked = extension.enabled;
      } else {
        var terminatedReload = node.querySelector('.terminated-reload-link');
        terminatedReload.hidden = false;
        terminatedReload.addEventListener('click', function(e) {
          chrome.send('extensionSettingsReload', [extension.id]);
        });
      }

      // 'Remove' button.
      var trashTemplate = $('template-collection').querySelector('.trash');
      var trash = trashTemplate.cloneNode(true);
      trash.title = loadTimeData.getString('extensionUninstall');
      trash.addEventListener('click', function(e) {
        butterBarVisibility[extension.id] = false;
        chrome.send('extensionSettingsUninstall', [extension.id]);
      });
      node.querySelector('.enable-controls').appendChild(trash);

      // Developer mode ////////////////////////////////////////////////////////

      // First we have the id.
      var idLabel = node.querySelector('.extension-id');
      idLabel.textContent = ' ' + extension.id;

      // Then the path, if provided by unpacked extension.
      if (extension.isUnpacked) {
        var loadPath = node.querySelector('.load-path');
        loadPath.hidden = false;
        loadPath.querySelector('span:nth-of-type(2)').textContent =
            ' ' + extension.path;
      }

      // Then the 'managed, cannot uninstall/disable' message.
      if (!extension.userModifiable)
        node.querySelector('.managed-message').hidden = false;

      // Then active views.
      if (extension.views.length > 0) {
        var activeViews = node.querySelector('.active-views');
        activeViews.hidden = false;
        var link = activeViews.querySelector('a');

        extension.views.forEach(function(view, i) {
          var displayName = view.generatedBackgroundPage ?
              loadTimeData.getString('backgroundPage') : view.path;
          var label = displayName +
              (view.incognito ?
                  ' ' + loadTimeData.getString('viewIncognito') : '') +
              (view.renderProcessId == -1 ?
                  ' ' + loadTimeData.getString('viewInactive') : '');
          link.textContent = label;
          link.addEventListener('click', function(e) {
            // TODO(estade): remove conversion to string?
            chrome.send('extensionSettingsInspect', [
              String(extension.id),
              String(view.renderProcessId),
              String(view.renderViewId),
              view.incognito
            ]);
          });

          if (i < extension.views.length - 1) {
            link = link.cloneNode(true);
            activeViews.appendChild(link);
          }
        });
      }

      // The extension warnings (describing runtime issues).
      if (extension.warnings) {
        var panel = node.querySelector('.extension-warnings');
        panel.hidden = false;
        var list = panel.querySelector('ul');
        extension.warnings.forEach(function(warning) {
          list.appendChild(document.createElement('li')).innerText = warning;
        });
      }

      // If the ErrorConsole is enabled, we should have manifest and/or runtime
      // errors. Otherwise, we may have install warnings. We should not have
      // both ErrorConsole errors and install warnings.
      if (extension.manifestErrors) {
        var panel = node.querySelector('.manifest-errors');
        panel.hidden = false;
        panel.appendChild(new extensions.ExtensionErrorList(
            extension.manifestErrors, 'extensionErrorsManifestErrors'));
      }
      if (extension.runtimeErrors) {
        var panel = node.querySelector('.runtime-errors');
        panel.hidden = false;
        panel.appendChild(new extensions.ExtensionErrorList(
            extension.runtimeErrors, 'extensionErrorsRuntimeErrors'));
      }
      if (extension.installWarnings) {
        var panel = node.querySelector('.install-warnings');
        panel.hidden = false;
        var list = panel.querySelector('ul');
        extension.installWarnings.forEach(function(warning) {
          var li = document.createElement('li');
          li.innerText = warning.message;
          list.appendChild(li);
        });
      }

      this.appendChild(node);
      if (location.hash.substr(1) == extension.id) {
        // Scroll beneath the fixed header so that the extension is not
        // obscured.
        var topScroll = node.offsetTop - $('page-header').offsetHeight;
        var pad = parseInt(getComputedStyle(node, null).marginTop, 10);
        if (!isNaN(pad))
          topScroll -= pad / 2;
        document.documentElement.scrollTop = topScroll;
      }
    },
  };

  return {
    ExtensionsList: ExtensionsList
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  /**
   * PackExtensionOverlay class
   * Encapsulated handling of the 'Pack Extension' overlay page.
   * @constructor
   */
  function PackExtensionOverlay() {
  }

  cr.addSingletonGetter(PackExtensionOverlay);

  PackExtensionOverlay.prototype = {
    /**
     * Initialize the page.
     */
    initializePage: function() {
      var overlay = $('overlay');
      cr.ui.overlay.setupOverlay(overlay);
      cr.ui.overlay.globalInitialization();
      overlay.addEventListener('cancelOverlay', this.handleDismiss_.bind(this));

      $('pack-extension-dismiss').addEventListener('click',
          this.handleDismiss_.bind(this));
      $('pack-extension-commit').addEventListener('click',
          this.handleCommit_.bind(this));
      $('browse-extension-dir').addEventListener('click',
          this.handleBrowseExtensionDir_.bind(this));
      $('browse-private-key').addEventListener('click',
          this.handleBrowsePrivateKey_.bind(this));
    },

    /**
     * Handles a click on the dismiss button.
     * @param {Event} e The click event.
     */
    handleDismiss_: function(e) {
      extensions.ExtensionSettings.showOverlay(null);
    },

    /**
     * Handles a click on the pack button.
     * @param {Event} e The click event.
     */
    handleCommit_: function(e) {
      var extensionPath = $('extension-root-dir').value;
      var privateKeyPath = $('extension-private-key').value;
      chrome.send('pack', [extensionPath, privateKeyPath, 0]);
    },

    /**
     * Utility function which asks the C++ to show a platform-specific file
     * select dialog, and fire |callback| with the |filePath| that resulted.
     * |selectType| can be either 'file' or 'folder'. |operation| can be 'load'
     * or 'pem' which are signals to the C++ to do some operation-specific
     * configuration.
     * @private
     */
    showFileDialog_: function(selectType, operation, callback) {
      handleFilePathSelected = function(filePath) {
        callback(filePath);
        handleFilePathSelected = function() {};
      };

      chrome.send('packExtensionSelectFilePath', [selectType, operation]);
    },

    /**
     * Handles the showing of the extension directory browser.
     * @param {Event} e Change event.
     * @private
     */
    handleBrowseExtensionDir_: function(e) {
      this.showFileDialog_('folder', 'load', function(filePath) {
        $('extension-root-dir').value = filePath;
      });
    },

    /**
     * Handles the showing of the extension private key file.
     * @param {Event} e Change event.
     * @private
     */
    handleBrowsePrivateKey_: function(e) {
      this.showFileDialog_('file', 'pem', function(filePath) {
        $('extension-private-key').value = filePath;
      });
    },
  };

  /**
   * Wrap up the pack process by showing the success |message| and closing
   * the overlay.
   * @param {string} message The message to show to the user.
   */
  PackExtensionOverlay.showSuccessMessage = function(message) {
    alertOverlay.setValues(
        loadTimeData.getString('packExtensionOverlay'),
        message,
        loadTimeData.getString('ok'),
        '',
        function() {
          extensions.ExtensionSettings.showOverlay(null);
        },
        null);
    extensions.ExtensionSettings.showOverlay($('alertOverlay'));
  };

  /**
   * Post an alert overlay showing |message|, and upon acknowledgement, close
   * the alert overlay and return to showing the PackExtensionOverlay.
   */
  PackExtensionOverlay.showError = function(message) {
    alertOverlay.setValues(
        loadTimeData.getString('packExtensionErrorTitle'),
        message,
        loadTimeData.getString('ok'),
        '',
        function() {
          extensions.ExtensionSettings.showOverlay($('pack-extension-overlay'));
        },
        null);
    extensions.ExtensionSettings.showOverlay($('alertOverlay'));
  };

  // Export
  return {
    PackExtensionOverlay: PackExtensionOverlay
  };
});

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  /**
   * The ExtensionErrorOverlay will show the contents of a file which pertains
   * to the ExtensionError; this is either the manifest file (for manifest
   * errors) or a source file (for runtime errors). If possible, the portion
   * of the file which caused the error will be highlighted.
   * @constructor
   */
  function ExtensionErrorOverlay() {
  }

  cr.addSingletonGetter(ExtensionErrorOverlay);

  ExtensionErrorOverlay.prototype = {
    /**
     * Initialize the page.
     */
    initializePage: function() {
      var overlay = $('overlay');
      cr.ui.overlay.setupOverlay(overlay);
      cr.ui.overlay.globalInitialization();
      overlay.addEventListener('cancelOverlay', this.handleDismiss_.bind(this));

      $('extension-error-overlay-dismiss').addEventListener(
          'click', this.handleDismiss_.bind(this));
    },

    /**
     * Handles a click on the dismiss button.
     * @param {Event} e The click event.
     * @private
     */
    handleDismiss_: function(e) {
      $('extension-error-overlay-content').innerHTML = '';
      extensions.ExtensionSettings.showOverlay(null);
    },
  };

  /**
   * Called by the ExtensionErrorHandler responding to the request for a file's
   * source. Populate the content area of the overlay and display the overlay.
   * @param {Object} result An object with four strings - the title,
   *     beforeHighlight, afterHighlight, and highlight. The three 'highlight'
   *     strings represent three portions of the file's content to display - the
   *     portion which is most relevant and should be emphasized (highlight),
   *     and the parts both before and after this portion. These may be empty.
   */
  ExtensionErrorOverlay.requestFileSourceResponse = function(result) {
    var content = $('extension-error-overlay-content');
    document.querySelector(
        '#extension-error-overlay .extension-error-overlay-title').
            innerText = result.title;

    var createSpan = function(source, isHighlighted) {
      var span = document.createElement('span');
      span.className = isHighlighted ? 'highlighted-source' : 'normal-source';
      source = source.replace(/ /g, '&nbsp;').replace(/\n|\r/g, '<br>');
      span.innerHTML = source;
      return span;
    };

    if (result.beforeHighlight)
      content.appendChild(createSpan(result.beforeHighlight, false));
    if (result.highlight) {
      var highlightSpan = createSpan(result.highlight, true);
      highlightSpan.title = result.message;
      content.appendChild(highlightSpan);
    }
    if (result.afterHighlight)
      content.appendChild(createSpan(result.afterHighlight, false));

    extensions.ExtensionSettings.showOverlay($('extension-error-overlay'));
  };

  // Export
  return {
    ExtensionErrorOverlay: ExtensionErrorOverlay
  };
});




// Used for observing function of the backend datasource for this page by
// tests.
var webuiResponded = false;

cr.define('extensions', function() {
  var ExtensionsList = options.ExtensionsList;

  // Implements the DragWrapper handler interface.
  var dragWrapperHandler = {
    /** @override */
    shouldAcceptDrag: function(e) {
      // We can't access filenames during the 'dragenter' event, so we have to
      // wait until 'drop' to decide whether to do something with the file or
      // not.
      // See: http://www.w3.org/TR/2011/WD-html5-20110113/dnd.html#concept-dnd-p
      return (e.dataTransfer.types &&
              e.dataTransfer.types.indexOf('Files') > -1);
    },
    /** @override */
    doDragEnter: function() {
      chrome.send('startDrag');
      ExtensionSettings.showOverlay(null);
      ExtensionSettings.showOverlay($('drop-target-overlay'));
    },
    /** @override */
    doDragLeave: function() {
      ExtensionSettings.showOverlay(null);
      chrome.send('stopDrag');
    },
    /** @override */
    doDragOver: function(e) {
      e.preventDefault();
    },
    /** @override */
    doDrop: function(e) {
      ExtensionSettings.showOverlay(null);
      if (e.dataTransfer.files.length != 1)
        return;

      var toSend = null;
      // Files lack a check if they're a directory, but we can find out through
      // its item entry.
      for (var i = 0; i < e.dataTransfer.items.length; ++i) {
        if (e.dataTransfer.items[i].kind == 'file' &&
            e.dataTransfer.items[i].webkitGetAsEntry().isDirectory) {
          toSend = 'installDroppedDirectory';
          break;
        }
      }
      // Only process files that look like extensions. Other files should
      // navigate the browser normally.
      if (!toSend && /\.(crx|user\.js)$/i.test(e.dataTransfer.files[0].name))
        toSend = 'installDroppedFile';

      if (toSend) {
        e.preventDefault();
        chrome.send(toSend);
      }
    }
  };

  /**
   * ExtensionSettings class
   * @class
   */
  function ExtensionSettings() {}

  cr.addSingletonGetter(ExtensionSettings);

  ExtensionSettings.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Perform initial setup.
     */
    initialize: function() {
      uber.onContentFrameLoaded();
      cr.ui.FocusOutlineManager.forDocument(document);
      measureCheckboxStrings();

      // Set the title.
      var title = loadTimeData.getString('extensionSettings');
      uber.invokeMethodOnParent('setTitle', {title: title});

      // This will request the data to show on the page and will get a response
      // back in returnExtensionsData.
      chrome.send('extensionSettingsRequestExtensionsData');

      $('toggle-dev-on').addEventListener('change',
          this.handleToggleDevMode_.bind(this));
      $('dev-controls').addEventListener('webkitTransitionEnd',
          this.handleDevControlsTransitionEnd_.bind(this));
      $('open-apps-dev-tools').addEventListener('click',
          this.handleOpenAppsDevTools_.bind(this));

      // Set up the three dev mode buttons (load unpacked, pack and update).
      $('load-unpacked').addEventListener('click',
          this.handleLoadUnpackedExtension_.bind(this));
      $('pack-extension').addEventListener('click',
          this.handlePackExtension_.bind(this));
      $('update-extensions-now').addEventListener('click',
          this.handleUpdateExtensionNow_.bind(this));

      if (!loadTimeData.getBoolean('offStoreInstallEnabled')) {
        this.dragWrapper_ = new cr.ui.DragWrapper(document.documentElement,
                                                  dragWrapperHandler);
      }

      extensions.PackExtensionOverlay.getInstance().initializePage();

      // Hook up the configure commands link to the overlay.
      var link = document.querySelector('.extension-commands-config');
      link.addEventListener('click',
          this.handleExtensionCommandsConfig_.bind(this));

      // Initialize the Commands overlay.
      extensions.ExtensionCommandsOverlay.getInstance().initializePage();

      extensions.ExtensionErrorOverlay.getInstance().initializePage();

      // Initialize the kiosk overlay.
      if (cr.isChromeOS) {
        var kioskOverlay = extensions.KioskAppsOverlay.getInstance();
        kioskOverlay.initialize();

        $('add-kiosk-app').addEventListener('click', function() {
          ExtensionSettings.showOverlay($('kiosk-apps-page'));
          kioskOverlay.didShowPage();
        });

        extensions.KioskDisableBailoutConfirm.getInstance().initialize();
      }

      cr.ui.overlay.setupOverlay($('drop-target-overlay'));
      cr.ui.overlay.globalInitialization();

      extensions.ExtensionFocusManager.getInstance().initialize();

      var path = document.location.pathname;
      if (path.length > 1) {
        // Skip starting slash and remove trailing slash (if any).
        var overlayName = path.slice(1).replace(/\/$/, '');
        if (overlayName == 'configureCommands')
          this.showExtensionCommandsConfigUi_();
      }

      preventDefaultOnPoundLinkClicks();  // From webui/js/util.js.
    },

    /**
     * Handles the Load Unpacked Extension button.
     * @param {Event} e Change event.
     * @private
     */
    handleLoadUnpackedExtension_: function(e) {
      chrome.send('extensionSettingsLoadUnpackedExtension');
    },

    /**
     * Handles the Pack Extension button.
     * @param {Event} e Change event.
     * @private
     */
    handlePackExtension_: function(e) {
      ExtensionSettings.showOverlay($('pack-extension-overlay'));
      chrome.send('metricsHandler:recordAction', ['Options_PackExtension']);
    },

    /**
     * Shows the Extension Commands configuration UI.
     * @param {Event} e Change event.
     * @private
     */
    showExtensionCommandsConfigUi_: function(e) {
      ExtensionSettings.showOverlay($('extension-commands-overlay'));
      chrome.send('metricsHandler:recordAction',
                  ['Options_ExtensionCommands']);
    },

    /**
     * Handles the Configure (Extension) Commands link.
     * @param {Event} e Change event.
     * @private
     */
    handleExtensionCommandsConfig_: function(e) {
      this.showExtensionCommandsConfigUi_();
    },

    /**
     * Handles the Update Extension Now button.
     * @param {Event} e Change event.
     * @private
     */
    handleUpdateExtensionNow_: function(e) {
      chrome.send('extensionSettingsAutoupdate');
    },

    /**
     * Handles the Toggle Dev Mode button.
     * @param {Event} e Change event.
     * @private
     */
    handleToggleDevMode_: function(e) {
      if ($('toggle-dev-on').checked) {
        $('dev-controls').hidden = false;
        window.setTimeout(function() {
          $('extension-settings').classList.add('dev-mode');
        }, 0);
      } else {
        $('extension-settings').classList.remove('dev-mode');
      }

      chrome.send('extensionSettingsToggleDeveloperMode');
    },

    /**
     * Called when a transition has ended for #dev-controls.
     * @param {Event} e webkitTransitionEnd event.
     * @private
     */
    handleDevControlsTransitionEnd_: function(e) {
      if (e.propertyName == 'height' &&
          !$('extension-settings').classList.contains('dev-mode')) {
        $('dev-controls').hidden = true;
      }
    },

    /**
     * Called when the user clicked on the button to launch Apps Developer
     * Tools.
     * @param {!Event} e A click event.
     * @private
     */
    handleOpenAppsDevTools_: function(e) {
      chrome.send('extensionSettingsLaunch',
                  ['lphgohfeebnhcpiohjndkgbhhkoapkjc']);
    },
  };

  /**
   * Called by the dom_ui_ to re-populate the page with data representing
   * the current state of installed extensions.
   */
  ExtensionSettings.returnExtensionsData = function(extensionsData) {
    // We can get called many times in short order, thus we need to
    // be careful to remove the 'finished loading' timeout.
    if (this.loadingTimeout_)
      window.clearTimeout(this.loadingTimeout_);
    document.documentElement.classList.add('loading');
    this.loadingTimeout_ = window.setTimeout(function() {
      document.documentElement.classList.remove('loading');
    }, 0);

    webuiResponded = true;

    if (extensionsData.extensions.length > 0) {
      // Enforce order specified in the data or (if equal) then sort by
      // extension name (case-insensitive) followed by their ID (in the case
      // where extensions have the same name).
      extensionsData.extensions.sort(function(a, b) {
        function compare(x, y) {
          return x < y ? -1 : (x > y ? 1 : 0);
        }
        return compare(a.order, b.order) ||
               compare(a.name.toLowerCase(), b.name.toLowerCase()) ||
               compare(a.id, b.id);
      });
    }

    var pageDiv = $('extension-settings');
    var marginTop = 0;
    if (extensionsData.profileIsManaged) {
      pageDiv.classList.add('profile-is-managed');
    } else {
      pageDiv.classList.remove('profile-is-managed');
    }
    if (extensionsData.profileIsManaged) {
      pageDiv.classList.add('showing-banner');
      $('toggle-dev-on').disabled = true;
      marginTop += 45;
    } else {
      pageDiv.classList.remove('showing-banner');
      $('toggle-dev-on').disabled = false;
    }

    pageDiv.style.marginTop = marginTop + 'px';

    if (extensionsData.developerMode) {
      pageDiv.classList.add('dev-mode');
      $('toggle-dev-on').checked = true;
      $('dev-controls').hidden = false;
    } else {
      pageDiv.classList.remove('dev-mode');
      $('toggle-dev-on').checked = false;
    }

    if (extensionsData.appsDevToolsEnabled)
      pageDiv.classList.add('apps-dev-tools-mode');

    $('load-unpacked').disabled = extensionsData.loadUnpackedDisabled;

    ExtensionsList.prototype.data_ = extensionsData;
    var extensionList = $('extension-settings-list');
    ExtensionsList.decorate(extensionList);
  }

  // Indicate that warning |message| has occured for pack of |crx_path| and
  // |pem_path| files.  Ask if user wants override the warning.  Send
  // |overrideFlags| to repeated 'pack' call to accomplish the override.
  ExtensionSettings.askToOverrideWarning =
      function(message, crx_path, pem_path, overrideFlags) {
    var closeAlert = function() {
      ExtensionSettings.showOverlay(null);
    };

    alertOverlay.setValues(
        loadTimeData.getString('packExtensionWarningTitle'),
        message,
        loadTimeData.getString('packExtensionProceedAnyway'),
        loadTimeData.getString('cancel'),
        function() {
          chrome.send('pack', [crx_path, pem_path, overrideFlags]);
          closeAlert();
        },
        closeAlert);
    ExtensionSettings.showOverlay($('alertOverlay'));
  }

  /**
   * Returns the current overlay or null if one does not exist.
   * @return {Element} The overlay element.
   */
  ExtensionSettings.getCurrentOverlay = function() {
    return document.querySelector('#overlay .page.showing');
  }

  /**
   * Sets the given overlay to show. This hides whatever overlay is currently
   * showing, if any.
   * @param {HTMLElement} node The overlay page to show. If falsey, all overlays
   *     are hidden.
   */
  ExtensionSettings.showOverlay = function(node) {
    var pageDiv = $('extension-settings');
    if (node) {
      pageDiv.style.width = window.getComputedStyle(pageDiv).width;
      document.body.classList.add('no-scroll');
    } else {
      document.body.classList.remove('no-scroll');
      pageDiv.style.width = '';
    }

    var currentlyShowingOverlay = ExtensionSettings.getCurrentOverlay();
    if (currentlyShowingOverlay)
      currentlyShowingOverlay.classList.remove('showing');

    if (node)
      node.classList.add('showing');

    var pages = document.querySelectorAll('.page');
    for (var i = 0; i < pages.length; i++) {
      pages[i].setAttribute('aria-hidden', node ? 'true' : 'false');
    }

    overlay.hidden = !node;
    uber.invokeMethodOnParent(node ? 'beginInterceptingEvents' :
                                     'stopInterceptingEvents');
  }

  /**
   * Utility function to find the width of various UI strings and synchronize
   * the width of relevant spans. This is crucial for making sure the
   * Enable/Enabled checkboxes align, as well as the Developer Mode checkbox.
   */
  function measureCheckboxStrings() {
    var trashWidth = 30;
    var measuringDiv = $('font-measuring-div');
    measuringDiv.textContent =
        loadTimeData.getString('extensionSettingsEnabled');
    var pxWidth = measuringDiv.clientWidth + trashWidth;
    measuringDiv.textContent =
        loadTimeData.getString('extensionSettingsEnable');
    pxWidth = Math.max(measuringDiv.clientWidth + trashWidth, pxWidth);
    measuringDiv.textContent =
        loadTimeData.getString('extensionSettingsDeveloperMode');
    pxWidth = Math.max(measuringDiv.clientWidth, pxWidth);

    var style = document.createElement('style');
    style.type = 'text/css';
    style.textContent =
        '.enable-checkbox-text {' +
        '  min-width: ' + (pxWidth - trashWidth) + 'px;' +
        '}' +
        '#dev-toggle span {' +
        '  min-width: ' + pxWidth + 'px;' +
        '}';
    document.querySelector('head').appendChild(style);
  }

  // Export
  return {
    ExtensionSettings: ExtensionSettings
  };
});

window.addEventListener('load', function(e) {
  extensions.ExtensionSettings.getInstance().initialize();
});
