// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Base class for all login WebUI screens.
 */
cr.define('login', function() {
  var Screen = cr.ui.define('div');

  Screen.prototype = {
    __proto__: HTMLDivElement.prototype,

    decorate: function() {
    }
  };

  return {
    Screen: Screen
  };
});

cr.define('login', function() {
  return {
    /**
     * Creates class and object for screen.
     * Methods specified in EXTERNAL_API array of prototype
     * will be available from C++ part.
     * Example:
     *     login.createScreen('ScreenName', 'screen-id', {
     *       foo: function() { console.log('foo'); },
     *       bar: function() { console.log('bar'); }
     *       EXTERNAL_API: ['foo'];
     *     });
     *     login.ScreenName.register();
     *     var screen = $('screen-id');
     *     screen.foo(); // valid
     *     login.ScreenName.foo(); // valid
     *     screen.bar(); // valid
     *     login.ScreenName.bar(); // invalid
     *
     * @param {string} name Name of created class.
     * @param {string} id Id of div representing screen.
     * @param {(function()|Object)} proto Prototype of object or function that
     *     returns prototype.
     */
    createScreen: function(name, id, proto) {
      if (typeof proto == 'function')
        proto = proto();
      cr.define('login', function() {
        var api = proto.EXTERNAL_API || [];
        for (var i = 0; i < api.length; ++i) {
          var methodName = api[i];
          if (typeof proto[methodName] !== 'function')
            throw Error('External method "' + methodName + '" for screen "' +
                name + '" not a function or undefined.');
        }

        var constructor = cr.ui.define(login.Screen);
        constructor.prototype = Object.create(login.Screen.prototype);
        Object.getOwnPropertyNames(proto).forEach(function(propertyName) {
          var descriptor =
              Object.getOwnPropertyDescriptor(proto, propertyName);
          Object.defineProperty(constructor.prototype,
              propertyName, descriptor);
          if (api.indexOf(propertyName) >= 0) {
            constructor[propertyName] = (function(x) {
              return function() {
                var screen = $(id);
                return screen[x].apply(screen, arguments);
              };
            })(propertyName);
          }
        });
        constructor.prototype.name = function() { return id; };

        constructor.register = function() {
          var screen = $(id);
          constructor.decorate(screen);
          Oobe.getInstance().registerScreen(screen);
        };

        var result = {};
        result[name] = constructor;
        return result;
      });
    }
  };
});


// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Bubble implementation.
 */

// TODO(xiyuan): Move this into shared.
cr.define('cr.ui', function() {
  /**
   * Creates a bubble div.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var Bubble = cr.ui.define('div');

  /**
   * Bubble attachment side.
   * @enum {string}
   */
  Bubble.Attachment = {
    RIGHT: 'bubble-right',
    LEFT: 'bubble-left',
    TOP: 'bubble-top',
    BOTTOM: 'bubble-bottom'
  };

  Bubble.prototype = {
    __proto__: HTMLDivElement.prototype,

    // Anchor element for this bubble.
    anchor_: undefined,

    // If defined, sets focus to this element once bubble is closed. Focus is
    // set to this element only if there's no any other focused element.
    elementToFocusOnHide_: undefined,

    // Whether to hide bubble when key is pressed.
    hideOnKeyPress_: true,

    /** @override */
    decorate: function() {
      this.docKeyDownHandler_ = this.handleDocKeyDown_.bind(this);
      this.selfClickHandler_ = this.handleSelfClick_.bind(this);
      this.ownerDocument.addEventListener('click',
                                          this.handleDocClick_.bind(this));
      this.ownerDocument.addEventListener('keydown',
                                          this.docKeyDownHandler_);
      window.addEventListener('blur', this.handleWindowBlur_.bind(this));
      this.addEventListener('webkitTransitionEnd',
                            this.handleTransitionEnd_.bind(this));
      // Guard timer for 200ms + epsilon.
      ensureTransitionEndEvent(this, 250);
    },

    /**
     * Element that should be focused on hide.
     * @type {HTMLElement}
     */
    set elementToFocusOnHide(value) {
      this.elementToFocusOnHide_ = value;
    },

    /**
     * Whether to hide bubble when key is pressed.
     * @type {boolean}
     */
    set hideOnKeyPress(value) {
      this.hideOnKeyPress_ = value;
    },

    /**
     * Whether to hide bubble when clicked inside bubble element.
     * Default is true.
     * @type {boolean}
     */
    set hideOnSelfClick(value) {
      if (value)
        this.removeEventListener('click', this.selfClickHandler_);
      else
        this.addEventListener('click', this.selfClickHandler_);
    },

    /**
     * Handler for click event which prevents bubble auto hide.
     * @private
     */
    handleSelfClick_: function(e) {
      // Allow clicking on [x] button.
      if (e.target && e.target.classList.contains('close-button'))
        return;

      e.stopPropagation();
    },

    /**
     * Sets the attachment of the bubble.
     * @param {!Attachment} attachment Bubble attachment.
     */
    setAttachment_: function(attachment) {
      for (var k in Bubble.Attachment) {
        var v = Bubble.Attachment[k];
        this.classList.toggle(v, v == attachment);
      }
    },

    /**
     * Shows the bubble for given anchor element.
     * @param {!Object} pos Bubble position (left, top, right, bottom in px).
     * @param {!Attachment} attachment Bubble attachment (on which side of the
     *     specified position it should be displayed).
     * @param {HTMLElement} opt_content Content to show in bubble.
     *     If not specified, bubble element content is shown.
     * @private
     */
    showContentAt_: function(pos, attachment, opt_content) {
      this.style.top = this.style.left = this.style.right = this.style.bottom =
          'auto';
      for (var k in pos) {
        if (typeof pos[k] == 'number')
          this.style[k] = pos[k] + 'px';
      }
      if (opt_content !== undefined) {
        this.innerHTML = '';
        this.appendChild(opt_content);
      }
      this.setAttachment_(attachment);
      this.hidden = false;
      this.classList.remove('faded');
    },

    /**
     * Shows the bubble for given anchor element. Bubble content is not cleared.
     * @param {!HTMLElement} el Anchor element of the bubble.
     * @param {!Attachment} attachment Bubble attachment (on which side of the
     *     element it should be displayed).
     * @param {number=} opt_offset Offset of the bubble.
     * @param {number=} opt_padding Optional padding of the bubble.
     */
    showForElement: function(el, attachment, opt_offset, opt_padding) {
      this.showContentForElement(
          el, attachment, undefined, opt_offset, opt_padding);
    },

    /**
     * Shows the bubble for given anchor element.
     * @param {!HTMLElement} el Anchor element of the bubble.
     * @param {!Attachment} attachment Bubble attachment (on which side of the
     *     element it should be displayed).
     * @param {HTMLElement} opt_content Content to show in bubble.
     *     If not specified, bubble element content is shown.
     * @param {number=} opt_offset Offset of the bubble attachment point from
     *     left (for vertical attachment) or top (for horizontal attachment)
     *     side of the element. If not specified, the bubble is positioned to
     *     be aligned with the left/top side of the element but not farther than
     *     half of its width/height.
     * @param {number=} opt_padding Optional padding of the bubble.
     */
    showContentForElement: function(el, attachment, opt_content,
                                    opt_offset, opt_padding) {
      /** @const */ var ARROW_OFFSET = 25;
      /** @const */ var DEFAULT_PADDING = 18;

      if (opt_padding == undefined)
        opt_padding = DEFAULT_PADDING;

      var origin = cr.ui.login.DisplayManager.getPosition(el);
      var offset = opt_offset == undefined ?
          [Math.min(ARROW_OFFSET, el.offsetWidth / 2),
           Math.min(ARROW_OFFSET, el.offsetHeight / 2)] :
          [opt_offset, opt_offset];

      var pos = {};
      if (isRTL()) {
        switch (attachment) {
          case Bubble.Attachment.TOP:
            pos.right = origin.right + offset[0] - ARROW_OFFSET;
            pos.bottom = origin.bottom + el.offsetHeight + opt_padding;
            break;
          case Bubble.Attachment.RIGHT:
            pos.top = origin.top + offset[1] - ARROW_OFFSET;
            pos.right = origin.right + el.offsetWidth + opt_padding;
            break;
          case Bubble.Attachment.BOTTOM:
            pos.right = origin.right + offset[0] - ARROW_OFFSET;
            pos.top = origin.top + el.offsetHeight + opt_padding;
            break;
          case Bubble.Attachment.LEFT:
            pos.top = origin.top + offset[1] - ARROW_OFFSET;
            pos.left = origin.left + el.offsetWidth + opt_padding;
            break;
        }
      } else {
        switch (attachment) {
          case Bubble.Attachment.TOP:
            pos.left = origin.left + offset[0] - ARROW_OFFSET;
            pos.bottom = origin.bottom + el.offsetHeight + opt_padding;
            break;
          case Bubble.Attachment.RIGHT:
            pos.top = origin.top + offset[1] - ARROW_OFFSET;
            pos.left = origin.left + el.offsetWidth + opt_padding;
            break;
          case Bubble.Attachment.BOTTOM:
            pos.left = origin.left + offset[0] - ARROW_OFFSET;
            pos.top = origin.top + el.offsetHeight + opt_padding;
            break;
          case Bubble.Attachment.LEFT:
            pos.top = origin.top + offset[1] - ARROW_OFFSET;
            pos.right = origin.right + el.offsetWidth + opt_padding;
            break;
        }
      }

      this.anchor_ = el;
      this.showContentAt_(pos, attachment, opt_content);
    },

    /**
     * Shows the bubble for given anchor element.
     * @param {!HTMLElement} el Anchor element of the bubble.
     * @param {string} text Text content to show in bubble.
     * @param {!Attachment} attachment Bubble attachment (on which side of the
     *     element it should be displayed).
     * @param {number=} opt_offset Offset of the bubble attachment point from
     *     left (for vertical attachment) or top (for horizontal attachment)
     *     side of the element. If not specified, the bubble is positioned to
     *     be aligned with the left/top side of the element but not farther than
     *     half of its weight/height.
     * @param {number=} opt_padding Optional padding of the bubble.
     */
    showTextForElement: function(el, text, attachment,
                                 opt_offset, opt_padding) {
      var span = this.ownerDocument.createElement('span');
      span.textContent = text;
      this.showContentForElement(el, attachment, span, opt_offset, opt_padding);
    },

    /**
     * Hides the bubble.
     */
    hide: function() {
      if (!this.classList.contains('faded'))
        this.classList.add('faded');
    },

    /**
     * Hides the bubble anchored to the given element (if any).
     * @param {!Object} el Anchor element.
     */
    hideForElement: function(el) {
      if (!this.hidden && this.anchor_ == el)
        this.hide();
    },

    /**
     * Handler for faded transition end.
     * @private
     */
    handleTransitionEnd_: function(e) {
      if (this.classList.contains('faded')) {
        this.hidden = true;
        if (this.elementToFocusOnHide_ &&
            document.activeElement == document.body) {
          // Restore focus to default element only if there's no other
          // element that is focused.
          this.elementToFocusOnHide_.focus();
        }
      }
    },

    /**
     * Handler of document click event.
     * @private
     */
    handleDocClick_: function(e) {
      // Ignore clicks on anchor element.
      if (e.target == this.anchor_)
        return;

      if (!this.hidden)
        this.hide();
    },

    /**
     * Handle of document keydown event.
     * @private
     */
    handleDocKeyDown_: function(e) {
      if (this.hideOnKeyPress_ && !this.hidden) {
        this.hide();
        return;
      }

      if (e.keyCode == 27 && !this.hidden) {
        if (this.elementToFocusOnHide_)
          this.elementToFocusOnHide_.focus();
        this.hide();
      }
    },

    /**
     * Handler of window blur event.
     * @private
     */
    handleWindowBlur_: function(e) {
      if (!this.hidden)
        this.hide();
    }
  };

  return {
    Bubble: Bubble
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Display manager for WebUI OOBE and login.
 */

// TODO(xiyuan): Find a better to share those constants.
/** @const */ var SCREEN_OOBE_NETWORK = 'connect';
/** @const */ var SCREEN_OOBE_EULA = 'eula';
/** @const */ var SCREEN_OOBE_UPDATE = 'update';
/** @const */ var SCREEN_OOBE_ENROLLMENT = 'oauth-enrollment';
/** @const */ var SCREEN_OOBE_KIOSK_ENABLE = 'kiosk-enable';
/** @const */ var SCREEN_GAIA_SIGNIN = 'gaia-signin';
/** @const */ var SCREEN_ACCOUNT_PICKER = 'account-picker';
/** @const */ var SCREEN_ERROR_MESSAGE = 'error-message';
/** @const */ var SCREEN_USER_IMAGE_PICKER = 'user-image';
/** @const */ var SCREEN_TPM_ERROR = 'tpm-error-message';
/** @const */ var SCREEN_PASSWORD_CHANGED = 'password-changed';
/** @const */ var SCREEN_CREATE_MANAGED_USER_FLOW =
    'managed-user-creation';
/** @const */ var SCREEN_APP_LAUNCH_SPLASH = 'app-launch-splash';
/** @const */ var SCREEN_CONFIRM_PASSWORD = 'confirm-password';
/** @const */ var SCREEN_MESSAGE_BOX = 'message-box';

/* Accelerator identifiers. Must be kept in sync with webui_login_view.cc. */
/** @const */ var ACCELERATOR_CANCEL = 'cancel';
/** @const */ var ACCELERATOR_ENROLLMENT = 'enrollment';
/** @const */ var ACCELERATOR_KIOSK_ENABLE = 'kiosk_enable';
/** @const */ var ACCELERATOR_VERSION = 'version';
/** @const */ var ACCELERATOR_RESET = 'reset';
/** @const */ var ACCELERATOR_FOCUS_PREV = 'focus_prev';
/** @const */ var ACCELERATOR_FOCUS_NEXT = 'focus_next';
/** @const */ var ACCELERATOR_DEVICE_REQUISITION = 'device_requisition';
/** @const */ var ACCELERATOR_DEVICE_REQUISITION_REMORA =
    'device_requisition_remora';
/** @const */ var ACCELERATOR_APP_LAUNCH_BAILOUT = 'app_launch_bailout';

/* Help topic identifiers. */
/** @const */ var HELP_TOPIC_ENTERPRISE_REPORTING = 2535613;

/* Signin UI state constants. Used to control header bar UI. */
/** @const */ var SIGNIN_UI_STATE = {
  HIDDEN: 0,
  GAIA_SIGNIN: 1,
  ACCOUNT_PICKER: 2,
  WRONG_HWID_WARNING: 3,
  MANAGED_USER_CREATION_FLOW: 4,
};

/* Possible UI states of the error screen. */
/** @const */ var ERROR_SCREEN_UI_STATE = {
  UNKNOWN: 'ui-state-unknown',
  UPDATE: 'ui-state-update',
  SIGNIN: 'ui-state-signin',
  MANAGED_USER_CREATION_FLOW: 'ui-state-locally-managed',
  KIOSK_MODE: 'ui-state-kiosk-mode',
  LOCAL_STATE_ERROR: 'ui-state-local-state-error'
};

/* Possible types of UI. */
/** @const */ var DISPLAY_TYPE = {
  UNKNOWN: 'unknown',
  OOBE: 'oobe',
  LOGIN: 'login',
  LOCK: 'lock',
  USER_ADDING: 'user-adding',
  APP_LAUNCH_SPLASH: 'app-launch-splash'
};

cr.define('cr.ui.login', function() {
  var Bubble = cr.ui.Bubble;

  /**
   * Maximum time in milliseconds to wait for step transition to finish.
   * The value is used as the duration for ensureTransitionEndEvent below.
   * It needs to be inline with the step screen transition duration time
   * defined in css file. The current value in css is 200ms. To avoid emulated
   * webkitTransitionEnd fired before real one, 250ms is used.
   * @const
   */
  var MAX_SCREEN_TRANSITION_DURATION = 250;

  /**
   * Groups of screens (screen IDs) that should have the same dimensions.
   * @type Array.<Array.<string>>
   * @const
   */
  var SCREEN_GROUPS = [[SCREEN_OOBE_NETWORK,
                        SCREEN_OOBE_EULA,
                        SCREEN_OOBE_UPDATE]
                      ];

  /**
   * Constructor a display manager that manages initialization of screens,
   * transitions, error messages display.
   *
   * @constructor
   */
  function DisplayManager() {
  }

  DisplayManager.prototype = {
    /**
     * Registered screens.
     */
    screens_: [],

    /**
     * Current OOBE step, index in the screens array.
     * @type {number}
     */
    currentStep_: 0,

    /**
     * Whether version label can be toggled by ACCELERATOR_VERSION.
     * @type {boolean}
     */
    allowToggleVersion_: false,

    /**
     * Whether keyboard navigation flow is enforced.
     * @type {boolean}
     */
    forceKeyboardFlow_: false,

    /**
     * Type of UI.
     * @type {string}
     */
    displayType_: DISPLAY_TYPE.UNKNOWN,

    get displayType() {
      return this.displayType_;
    },

    set displayType(displayType) {
      this.displayType_ = displayType;
    },

    /**
     * Gets current screen element.
     * @type {HTMLElement}
     */
    get currentScreen() {
      return $(this.screens_[this.currentStep_]);
    },

    /**
     * Hides/shows header (Shutdown/Add User/Cancel buttons).
     * @param {boolean} hidden Whether header is hidden.
     */
    set headerHidden(hidden) {
      $('login-header-bar').hidden = hidden;
    },

    /**
     * Toggles background of main body between transparency and solid.
     * @param {boolean} solid Whether to show a solid background.
     */
    set solidBackground(solid) {
      if (solid)
        document.body.classList.add('solid');
      else
        document.body.classList.remove('solid');
    },

    /**
     * Forces keyboard based OOBE navigation.
     * @param {boolean} value True if keyboard navigation flow is forced.
     */
    set forceKeyboardFlow(value) {
      this.forceKeyboardFlow_ = value;
      if (value) {
        keyboard.initializeKeyboardFlow();
        cr.ui.Dropdown.enableKeyboardFlow();
      }
    },

    /**
     * Shows/hides version labels.
     * @param {boolean} show Whether labels should be visible by default. If
     *     false, visibility can be toggled by ACCELERATOR_VERSION.
     */
    showVersion: function(show) {
      $('version-labels').hidden = !show;
      this.allowToggleVersion_ = !show;
    },

    /**
     * Handle accelerators.
     * @param {string} name Accelerator name.
     */
    handleAccelerator: function(name) {
      if (name == ACCELERATOR_CANCEL) {
        if (this.currentScreen.cancel) {
          this.currentScreen.cancel();
        }
      } else if (name == ACCELERATOR_ENROLLMENT) {
        var currentStepId = this.screens_[this.currentStep_];
        if (currentStepId == SCREEN_GAIA_SIGNIN ||
            currentStepId == SCREEN_ACCOUNT_PICKER) {
          chrome.send('toggleEnrollmentScreen');
        } else if (currentStepId == SCREEN_OOBE_NETWORK ||
                   currentStepId == SCREEN_OOBE_EULA) {
          // In this case update check will be skipped and OOBE will
          // proceed straight to enrollment screen when EULA is accepted.
          chrome.send('skipUpdateEnrollAfterEula');
        } else if (currentStepId == SCREEN_OOBE_ENROLLMENT) {
          // This accelerator is also used to manually cancel auto-enrollment.
          if (this.currentScreen.cancelAutoEnrollment)
            this.currentScreen.cancelAutoEnrollment();
        }
      } else if (name == ACCELERATOR_KIOSK_ENABLE) {
        var currentStepId = this.screens_[this.currentStep_];
        if (currentStepId == SCREEN_GAIA_SIGNIN ||
            currentStepId == SCREEN_ACCOUNT_PICKER) {
          chrome.send('toggleKioskEnableScreen');
        }
      } else if (name == ACCELERATOR_VERSION) {
        if (this.allowToggleVersion_)
          $('version-labels').hidden = !$('version-labels').hidden;
      } else if (name == ACCELERATOR_RESET) {
        var currentStepId = this.screens_[this.currentStep_];
        if (currentStepId == SCREEN_GAIA_SIGNIN ||
            currentStepId == SCREEN_ACCOUNT_PICKER) {
          chrome.send('toggleResetScreen');
        }
      } else if (name == ACCELERATOR_DEVICE_REQUISITION) {
        if (this.isOobeUI())
          this.showDeviceRequisitionPrompt_();
      } else if (name == ACCELERATOR_DEVICE_REQUISITION_REMORA) {
        if (this.isOobeUI())
          this.showDeviceRequisitionRemoraPrompt_();
      } else if (name == ACCELERATOR_APP_LAUNCH_BAILOUT) {
        var currentStepId = this.screens_[this.currentStep_];
        if (currentStepId == SCREEN_APP_LAUNCH_SPLASH)
          chrome.send('cancelAppLaunch');
      }

      if (!this.forceKeyboardFlow_)
        return;

      // Handle special accelerators for keyboard enhanced navigation flow.
      if (name == ACCELERATOR_FOCUS_PREV)
        keyboard.raiseKeyFocusPrevious(document.activeElement);
      else if (name == ACCELERATOR_FOCUS_NEXT)
        keyboard.raiseKeyFocusNext(document.activeElement);
    },

    /**
     * Appends buttons to the button strip.
     * @param {Array.<HTMLElement>} buttons Array with the buttons to append.
     * @param {string} screenId Id of the screen that buttons belong to.
     */
    appendButtons_: function(buttons, screenId) {
      if (buttons) {
        var buttonStrip = $(screenId + '-controls');
        if (buttonStrip) {
          for (var i = 0; i < buttons.length; ++i)
            buttonStrip.appendChild(buttons[i]);
        }
      }
    },

    /**
     * Disables or enables control buttons on the specified screen.
     * @param {HTMLElement} screen Screen which controls should be affected.
     * @param {boolean} disabled Whether to disable controls.
     */
    disableButtons_: function(screen, disabled) {
      var buttons = document.querySelectorAll(
          '#' + screen.id + '-controls button:not(.preserve-disabled-state)');
      for (var i = 0; i < buttons.length; ++i) {
        buttons[i].disabled = disabled;
      }
    },

    /**
     * Updates a step's css classes to reflect left, current, or right position.
     * @param {number} stepIndex step index.
     * @param {string} state one of 'left', 'current', 'right'.
     */
    updateStep_: function(stepIndex, state) {
      var stepId = this.screens_[stepIndex];
      var step = $(stepId);
      var header = $('header-' + stepId);
      var states = ['left', 'right', 'current'];
      for (var i = 0; i < states.length; ++i) {
        if (states[i] != state) {
          step.classList.remove(states[i]);
          header.classList.remove(states[i]);
        }
      }
      step.classList.add(state);
      header.classList.add(state);
    },

    /**
     * Switches to the next OOBE step.
     * @param {number} nextStepIndex Index of the next step.
     */
    toggleStep_: function(nextStepIndex, screenData) {
      var currentStepId = this.screens_[this.currentStep_];
      var nextStepId = this.screens_[nextStepIndex];
      var oldStep = $(currentStepId);
      var newStep = $(nextStepId);
      var newHeader = $('header-' + nextStepId);

      // Disable controls before starting animation.
      this.disableButtons_(oldStep, true);

      if (oldStep.onBeforeHide)
        oldStep.onBeforeHide();

      if (newStep.onBeforeShow)
        newStep.onBeforeShow(screenData);

      newStep.classList.remove('hidden');

      if (newStep.onAfterShow)
        newStep.onAfterShow(screenData);

      this.disableButtons_(newStep, false);

      // Default control to be focused (if specified).
      var defaultControl = newStep.defaultControl;

      if (this.isOobeUI()) {
        // Start gliding animation for OOBE steps.
        if (nextStepIndex > this.currentStep_) {
          for (var i = this.currentStep_; i < nextStepIndex; ++i)
            this.updateStep_(i, 'left');
          this.updateStep_(nextStepIndex, 'current');
        } else if (nextStepIndex < this.currentStep_) {
          for (var i = this.currentStep_; i > nextStepIndex; --i)
            this.updateStep_(i, 'right');
          this.updateStep_(nextStepIndex, 'current');
        }
      } else {
        // Start fading animation for login display.
        oldStep.classList.add('faded');
        newStep.classList.remove('faded');
      }

      // Adjust inner container height based on new step's height.
      this.updateScreenSize(newStep);

      var innerContainer = $('inner-container');
      if (this.currentStep_ != nextStepIndex &&
          !oldStep.classList.contains('hidden')) {
        if (oldStep.classList.contains('animated')) {
          innerContainer.classList.add('animation');
          oldStep.addEventListener('webkitTransitionEnd', function f(e) {
            oldStep.removeEventListener('webkitTransitionEnd', f);
            if (oldStep.classList.contains('faded') ||
                oldStep.classList.contains('left') ||
                oldStep.classList.contains('right')) {
              innerContainer.classList.remove('animation');
              oldStep.classList.add('hidden');
            }
            // Refresh defaultControl. It could have changed.
            var defaultControl = newStep.defaultControl;
            if (defaultControl)
              defaultControl.focus();
          });
          ensureTransitionEndEvent(oldStep, MAX_SCREEN_TRANSITION_DURATION);
        } else {
          oldStep.classList.add('hidden');
          if (defaultControl)
            defaultControl.focus();
        }
      } else {
        // First screen on OOBE launch.
        if (this.isOobeUI() && innerContainer.classList.contains('down')) {
          innerContainer.classList.remove('down');
          innerContainer.addEventListener(
              'webkitTransitionEnd', function f(e) {
                innerContainer.removeEventListener('webkitTransitionEnd', f);
                $('progress-dots').classList.remove('down');
                chrome.send('loginVisible', ['oobe']);
                // Refresh defaultControl. It could have changed.
                var defaultControl = newStep.defaultControl;
                if (defaultControl)
                  defaultControl.focus();
              });
          ensureTransitionEndEvent(innerContainer,
                                   MAX_SCREEN_TRANSITION_DURATION);
        } else {
          if (defaultControl)
            defaultControl.focus();
          chrome.send('loginVisible', ['oobe']);
        }
      }
      this.currentStep_ = nextStepIndex;
      $('oobe').className = nextStepId;

      $('step-logo').hidden = newStep.classList.contains('no-logo');

      chrome.send('updateCurrentScreen', [this.currentScreen.id]);
    },

    /**
     * Make sure that screen is initialized and decorated.
     * @param {Object} screen Screen params dict, e.g. {id: screenId, data: {}}.
     */
    preloadScreen: function(screen) {
      var screenEl = $(screen.id);
      if (screenEl.deferredDecorate !== undefined) {
        screenEl.deferredDecorate();
        delete screenEl.deferredDecorate;
      }
    },

    /**
     * Show screen of given screen id.
     * @param {Object} screen Screen params dict, e.g. {id: screenId, data: {}}.
     */
    showScreen: function(screen) {
      var screenId = screen.id;

      // Make sure the screen is decorated.
      this.preloadScreen(screen);

      if (screen.data !== undefined && screen.data.disableAddUser)
        DisplayManager.updateAddUserButtonStatus(true);


      // Show sign-in screen instead of account picker if pod row is empty.
      if (screenId == SCREEN_ACCOUNT_PICKER && $('pod-row').pods.length == 0) {
        // Manually hide 'add-user' header bar, because of the case when
        // 'Cancel' button is used on the offline login page.
        $('add-user-header-bar-item').hidden = true;
        Oobe.showSigninUI(true);
        return;
      }

      var data = screen.data;
      var index = this.getScreenIndex_(screenId);
      if (index >= 0)
        this.toggleStep_(index, data);
    },

    /**
     * Gets index of given screen id in screens_.
     * @param {string} screenId Id of the screen to look up.
     * @private
     */
    getScreenIndex_: function(screenId) {
      for (var i = 0; i < this.screens_.length; ++i) {
        if (this.screens_[i] == screenId)
          return i;
      }
      return -1;
    },

    /**
     * Register an oobe screen.
     * @param {Element} el Decorated screen element.
     */
    registerScreen: function(el) {
      var screenId = el.id;
      this.screens_.push(screenId);

      var header = document.createElement('span');
      header.id = 'header-' + screenId;
      header.textContent = el.header ? el.header : '';
      header.className = 'header-section';
      $('header-sections').appendChild(header);

      var dot = document.createElement('div');
      dot.id = screenId + '-dot';
      dot.className = 'progdot';
      var progressDots = $('progress-dots');
      if (progressDots)
        progressDots.appendChild(dot);

      this.appendButtons_(el.buttons, screenId);
    },

    /**
     * Updates inner container size based on the size of the current screen and
     * other screens in the same group.
     * Should be executed on screen change / screen size change.
     * @param {!HTMLElement} screen Screen that is being shown.
     */
    updateScreenSize: function(screen) {
      // Have to reset any previously predefined screen size first
      // so that screen contents would define it instead (offsetHeight/width).
      // http://crbug.com/146539
      $('inner-container').style.height = '';
      $('inner-container').style.width = '';
      screen.style.width = '';
      screen.style.height = '';

     $('outer-container').classList.toggle(
        'fullscreen', screen.classList.contains('fullscreen'));

      var height = screen.offsetHeight;
      var width = screen.offsetWidth;
      for (var i = 0, screenGroup; screenGroup = SCREEN_GROUPS[i]; i++) {
        if (screenGroup.indexOf(screen.id) != -1) {
          // Set screen dimensions to maximum dimensions within this group.
          for (var j = 0, screen2; screen2 = $(screenGroup[j]); j++) {
            height = Math.max(height, screen2.offsetHeight);
            width = Math.max(width, screen2.offsetWidth);
          }
          break;
        }
      }
      $('inner-container').style.height = height + 'px';
      $('inner-container').style.width = width + 'px';
      // This requires |screen| to have 'box-sizing: border-box'.
      screen.style.width = width + 'px';
      screen.style.height = height + 'px';
    },

    /**
     * Updates localized content of the screens like headers, buttons and links.
     * Should be executed on language change.
     */
    updateLocalizedContent_: function() {
      for (var i = 0, screenId; screenId = this.screens_[i]; ++i) {
        var screen = $(screenId);
        var buttonStrip = $(screenId + '-controls');
        if (buttonStrip)
          buttonStrip.innerHTML = '';
        // TODO(nkostylev): Update screen headers for new OOBE design.
        this.appendButtons_(screen.buttons, screenId);
        if (screen.updateLocalizedContent)
          screen.updateLocalizedContent();
      }

      var currentScreenId = this.screens_[this.currentStep_];
      var currentScreen = $(currentScreenId);
      this.updateScreenSize(currentScreen);

      // Trigger network drop-down to reload its state
      // so that strings are reloaded.
      // Will be reloaded if drowdown is actually shown.
      cr.ui.DropDown.refresh();
    },

    /**
     * Prepares screens to use in login display.
     */
    prepareForLoginDisplay_: function() {
      for (var i = 0, screenId; screenId = this.screens_[i]; ++i) {
        var screen = $(screenId);
        screen.classList.add('faded');
        screen.classList.remove('right');
        screen.classList.remove('left');
      }
    },

    /**
     * Shows the device requisition prompt.
     */
    showDeviceRequisitionPrompt_: function() {
      if (!this.deviceRequisitionDialog_) {
        this.deviceRequisitionDialog_ =
            new cr.ui.dialogs.PromptDialog(document.body);
        this.deviceRequisitionDialog_.setOkLabel(
            loadTimeData.getString('deviceRequisitionPromptOk'));
        this.deviceRequisitionDialog_.setCancelLabel(
            loadTimeData.getString('deviceRequisitionPromptCancel'));
      }
      this.deviceRequisitionDialog_.show(
          loadTimeData.getString('deviceRequisitionPromptText'),
          this.deviceRequisition_,
          this.onConfirmDeviceRequisitionPrompt_.bind(this));
    },

    /**
     * Confirmation handle for the device requisition prompt.
     * @param {string} value The value entered by the user.
     */
    onConfirmDeviceRequisitionPrompt_: function(value) {
      this.deviceRequisition_ = value;
      chrome.send('setDeviceRequisition', [value]);
    },

    /*
     * Updates the device requisition string shown in the requisition prompt.
     * @param {string} requisition The device requisition.
     */
    updateDeviceRequisition: function(requisition) {
      this.deviceRequisition_ = requisition;
    },

    /**
     * Shows the special remora device requisition prompt.
     * @private
     */
    showDeviceRequisitionRemoraPrompt_: function() {
      if (!this.deviceRequisitionRemoraDialog_) {
        this.deviceRequisitionRemoraDialog_ =
            new cr.ui.dialogs.ConfirmDialog(document.body);
        this.deviceRequisitionRemoraDialog_.setOkLabel(
            loadTimeData.getString('deviceRequisitionRemoraPromptOk'));
        this.deviceRequisitionRemoraDialog_.setCancelLabel(
            loadTimeData.getString('deviceRequisitionRemoraPromptCancel'));
      }
      this.deviceRequisitionRemoraDialog_.showWithTitle(
          loadTimeData.getString('deviceRequisitionRemoraPromptTitle'),
          loadTimeData.getString('deviceRequisitionRemoraPromptText'),
          function() {
            chrome.send('setDeviceRequisition', ['remora']);
          });
    },

    /**
     * Returns true if Oobe UI is shown.
     */
    isOobeUI: function() {
      return document.body.classList.contains('oobe-display');
    },

    /**
     * Returns true if sign in UI should trigger wallpaper load on boot.
     */
    shouldLoadWallpaperOnBoot: function() {
      return loadTimeData.getString('bootIntoWallpaper') == 'on';
    },
  };

  /**
   * Initializes display manager.
   */
  DisplayManager.initialize = function() {
    // Extracting display type from URL.
    var path = window.location.pathname.substr(1);
    var displayType = DISPLAY_TYPE.UNKNOWN;
    Object.getOwnPropertyNames(DISPLAY_TYPE).forEach(function(type) {
      if (DISPLAY_TYPE[type] == path) {
        displayType = path;
      }
    });
    if (displayType == DISPLAY_TYPE.UNKNOWN) {
      console.error("Unknown display type '" + path + "'. Setting default.");
      displayType = DISPLAY_TYPE.LOGIN;
    }
    Oobe.getInstance().displayType = displayType;
    document.documentElement.setAttribute('screen', displayType);

    var link = $('enterprise-info-hint-link');
    link.addEventListener(
        'click', DisplayManager.handleEnterpriseHintLinkClick);
  },

  /**
   * Returns offset (top, left) of the element.
   * @param {!Element} element HTML element.
   * @return {!Object} The offset (top, left).
   */
  DisplayManager.getOffset = function(element) {
    var x = 0;
    var y = 0;
    while (element && !isNaN(element.offsetLeft) && !isNaN(element.offsetTop)) {
      x += element.offsetLeft - element.scrollLeft;
      y += element.offsetTop - element.scrollTop;
      element = element.offsetParent;
    }
    return { top: y, left: x };
  };

  /**
   * Returns position (top, left, right, bottom) of the element.
   * @param {!Element} element HTML element.
   * @return {!Object} Element position (top, left, right, bottom).
   */
  DisplayManager.getPosition = function(element) {
    var offset = DisplayManager.getOffset(element);
    return { top: offset.top,
             right: window.innerWidth - element.offsetWidth - offset.left,
             bottom: window.innerHeight - element.offsetHeight - offset.top,
             left: offset.left };
  };

  /**
   * Disables signin UI.
   */
  DisplayManager.disableSigninUI = function() {
    $('login-header-bar').disabled = true;
    $('pod-row').disabled = true;
  };

  /**
   * Shows signin UI.
   * @param {string} opt_email An optional email for signin UI.
   */
  DisplayManager.showSigninUI = function(opt_email) {
    var currentScreenId = Oobe.getInstance().currentScreen.id;
    if (currentScreenId == SCREEN_GAIA_SIGNIN)
      $('login-header-bar').signinUIState = SIGNIN_UI_STATE.GAIA_SIGNIN;
    else if (currentScreenId == SCREEN_ACCOUNT_PICKER)
      $('login-header-bar').signinUIState = SIGNIN_UI_STATE.ACCOUNT_PICKER;
    chrome.send('showAddUser', [opt_email]);
  };

  /**
   * Resets sign-in input fields.
   * @param {boolean} forceOnline Whether online sign-in should be forced.
   *     If |forceOnline| is false previously used sign-in type will be used.
   */
  DisplayManager.resetSigninUI = function(forceOnline) {
    var currentScreenId = Oobe.getInstance().currentScreen.id;

    $(SCREEN_GAIA_SIGNIN).reset(
        currentScreenId == SCREEN_GAIA_SIGNIN, forceOnline);
    $('login-header-bar').disabled = false;
    $('pod-row').reset(currentScreenId == SCREEN_ACCOUNT_PICKER);
  };

  /**
   * Shows sign-in error bubble.
   * @param {number} loginAttempts Number of login attemps tried.
   * @param {string} message Error message to show.
   * @param {string} link Text to use for help link.
   * @param {number} helpId Help topic Id associated with help link.
   */
  DisplayManager.showSignInError = function(loginAttempts, message, link,
                                            helpId) {
    var error = document.createElement('div');

    var messageDiv = document.createElement('div');
    messageDiv.className = 'error-message-bubble';
    messageDiv.textContent = message;
    error.appendChild(messageDiv);

    if (link) {
      messageDiv.classList.add('error-message-bubble-padding');

      var helpLink = document.createElement('a');
      helpLink.href = '#';
      helpLink.textContent = link;
      helpLink.addEventListener('click', function(e) {
        chrome.send('launchHelpApp', [helpId]);
        e.preventDefault();
      });
      error.appendChild(helpLink);
    }

    var currentScreen = Oobe.getInstance().currentScreen;
    if (currentScreen && typeof currentScreen.showErrorBubble === 'function')
      currentScreen.showErrorBubble(loginAttempts, error);
  };

  /**
   * Shows password changed screen that offers migration.
   * @param {boolean} showError Whether to show the incorrect password error.
   */
  DisplayManager.showPasswordChangedScreen = function(showError) {
    login.PasswordChangedScreen.show(showError);
  };

  /**
   * Shows dialog to create managed user.
   */
  DisplayManager.showManagedUserCreationScreen = function() {
    login.ManagedUserCreationScreen.show();
  };

  /**
   * Shows TPM error screen.
   */
  DisplayManager.showTpmError = function() {
    login.TPMErrorMessageScreen.show();
  };

  /**
   * Clears error bubble.
   */
  DisplayManager.clearErrors = function() {
    $('bubble').hide();
  };

  /**
   * Sets text content for a div with |labelId|.
   * @param {string} labelId Id of the label div.
   * @param {string} labelText Text for the label.
   */
  DisplayManager.setLabelText = function(labelId, labelText) {
    $(labelId).textContent = labelText;
  };

  /**
   * Shows help topic about enrolled devices.
   * @param {MouseEvent} Event object.
   */
  DisplayManager.handleEnterpriseHintLinkClick = function(e) {
    chrome.send('launchHelpApp', [HELP_TOPIC_ENTERPRISE_REPORTING]);
    e.preventDefault();
  }

  /**
   * Sets the text content of the enterprise info message.
   * @param {string} messageText The message text.
   */
  DisplayManager.setEnterpriseInfo = function(messageText) {
    $('enterprise-info-message').textContent = messageText;
    if (messageText) {
      $('enterprise-info').hidden = false;
    }
  };

  /**
   * Disable Add users button if said.
   * @param {boolean} disable true to disable
   */
  DisplayManager.updateAddUserButtonStatus = function(disable) {
    $('add-user-button').disabled = disable;
    $('add-user-button').classList[
        disable ? 'add' : 'remove']('button-restricted');
    $('add-user-button').title = disable ?
        loadTimeData.getString('disabledAddUserTooltip') : '';
  }

  /**
   * Clears password field in user-pod.
   */
  DisplayManager.clearUserPodPassword = function() {
    $('pod-row').clearFocusedPod();
  };

  /**
   * Restores input focus to currently selected pod.
   */
  DisplayManager.refocusCurrentPod = function() {
    $('pod-row').refocusCurrentPod();
  };

  // Export
  return {
    DisplayManager: DisplayManager
  };
});

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Desktop User Chooser UI control bar implementation.
 */

cr.define('login', function() {
  /**
   * Creates a header bar element.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var HeaderBar = cr.ui.define('div');

  HeaderBar.prototype = {
    __proto__: HTMLDivElement.prototype,

    // Whether guest button should be shown when header bar is in normal mode.
    showGuest_: true,

    // Current UI state of the sign-in screen.
    signinUIState_: SIGNIN_UI_STATE.ACCOUNT_PICKER,

    // Whether to show kiosk apps menu.
    hasApps_: false,

    /** @override */
    decorate: function() {
      $('add-user-button').addEventListener('click',
          this.handleAddUserClick_);
      $('cancel-add-user-button').addEventListener('click',
          this.handleCancelAddUserClick_);
      $('guest-user-header-bar-item').addEventListener('click',
          this.handleGuestClick_);
      $('guest-user-button').addEventListener('click',
          this.handleGuestClick_);
      this.updateUI_();
    },

    /**
     * Tab index value for all button elements.
     * @type {number}
     */
    set buttonsTabIndex(tabIndex) {
      var buttons = this.getElementsByTagName('button');
      for (var i = 0, button; button = buttons[i]; ++i) {
        button.tabIndex = tabIndex;
      }
    },

    /**
     * Disables the header bar and all of its elements.
     * @type {boolean}
     */
    set disabled(value) {
      var buttons = this.getElementsByTagName('button');
      for (var i = 0, button; button = buttons[i]; ++i)
        if (!button.classList.contains('button-restricted'))
          button.disabled = value;
    },

    /**
     * Add user button click handler.
     * @private
     */
    handleAddUserClick_: function(e) {
      chrome.send('addUser');
      // Prevent further propagation of click event. Otherwise, the click event
      // handler of document object will set wallpaper to user's wallpaper when
      // there is only one existing user. See http://crbug.com/166477
      e.stopPropagation();
    },

    /**
     * Cancel add user button click handler.
     * @private
     */
    handleCancelAddUserClick_: function(e) {
      // Let screen handle cancel itself if that is capable of doing so.
      if (Oobe.getInstance().currentScreen &&
          Oobe.getInstance().currentScreen.cancel) {
        Oobe.getInstance().currentScreen.cancel();
        return;
      }

      Oobe.showScreen({id: SCREEN_ACCOUNT_PICKER});
      Oobe.resetSigninUI(true);
    },

    /**
     * Guest button click handler.
     * @private
     */
    handleGuestClick_: function(e) {
      chrome.send('launchGuest');
      e.stopPropagation();
    },

    /**
     * If true then "Browse as Guest" button is shown.
     * @type {boolean}
     */
    set showGuestButton(value) {
      this.showGuest_ = value;
      this.updateUI_();
    },

    /**
     * Update current header bar UI.
     * @type {number} state Current state of the sign-in screen
     *                      (see SIGNIN_UI_STATE).
     */
    set signinUIState(state) {
      this.signinUIState_ = state;
      this.updateUI_();
    },

    /**
     * Whether the Cancel button is enabled during Gaia sign-in.
     * @type {boolean}
     */
    set allowCancel(value) {
      this.allowCancel_ = value;
      this.updateUI_();
    },

    /**
     * Updates visibility state of action buttons.
     * @private
     */
    updateUI_: function() {
      $('add-user-button').hidden = false;
      $('cancel-add-user-button').hidden = !this.allowCancel_;
      $('guest-user-header-bar-item').hidden = false;
      $('add-user-header-bar-item').hidden = false;
    },

    /**
     * Animates Header bar to hide from the screen.
     * @param {function()} callback will be called once animation is finished.
     */
    animateOut: function(callback) {
      var launcher = this;
      launcher.addEventListener(
          'webkitTransitionEnd', function f(e) {
            launcher.removeEventListener('webkitTransitionEnd', f);
            callback();
          });
      this.classList.remove('login-header-bar-animate-slow');
      this.classList.add('login-header-bar-animate-fast');
      this.classList.add('login-header-bar-hidden');
    },

    /**
     * Animates Header bar to slowly appear on the screen.
     */
    animateIn: function() {
      this.classList.remove('login-header-bar-animate-fast');
      this.classList.add('login-header-bar-animate-slow');
      this.classList.remove('login-header-bar-hidden');
    },
  };

  /**
   * Convenience wrapper of animateOut.
   */
  HeaderBar.animateOut = function(callback) {
    $('login-header-bar').animateOut(callback);
  };

  /**
   * Convenience wrapper of animateIn.
   */
  HeaderBar.animateIn = function() {
    $('login-header-bar').animateIn();
  }

  return {
    HeaderBar: HeaderBar
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Account picker screen implementation.
 */

login.createScreen('AccountPickerScreen', 'account-picker', function() {
  /**
   * Maximum number of offline login failures before online login.
   * @type {number}
   * @const
   */
  var MAX_LOGIN_ATTEMPTS_IN_POD = 3;
  /**
   * Whether to preselect the first pod automatically on login screen.
   * @type {boolean}
   * @const
   */
  var PRESELECT_FIRST_POD = true;

  return {
    EXTERNAL_API: [
      'loadUsers',
      'updateUserImage',
      'updateUserGaiaNeeded',
      'setCapsLockState',
      'forceLockedUserPodFocus'
    ],

    /** @override */
    decorate: function() {
      login.PodRow.decorate($('pod-row'));
    },

    // Whether this screen is shown for the first time.
    firstShown_: true,

    /**
     * When the account picker is being used to lock the screen, pressing the
     * exit accelerator key will sign out the active user as it would when
     * they are signed in.
     */
    exit: function() {
      // Check and disable the sign out button so that we can never have two
      // sign out requests generated in a row.
      if ($('pod-row').lockedPod && !$('sign-out-user-button').disabled) {
        $('sign-out-user-button').disabled = true;
        chrome.send('signOutUser');
      }
    },

    /**
     * Event handler that is invoked just after the frame is shown.
     * @param {string} data Screen init payload.
     */
    onAfterShow: function(data) {
      $('pod-row').handleAfterShow();
    },

    /**
     * Event handler that is invoked just before the frame is shown.
     * @param {string} data Screen init payload.
     */
    onBeforeShow: function(data) {
      chrome.send('loginUIStateChanged', ['account-picker', true]);
      $('login-header-bar').signinUIState = SIGNIN_UI_STATE.ACCOUNT_PICKER;
      chrome.send('hideCaptivePortal');
      var podRow = $('pod-row');
      podRow.handleBeforeShow();

      // If this is showing for the lock screen display the sign out button,
      // hide the add user button and activate the locked user's pod.
      var lockedPod = podRow.lockedPod;
      $('add-user-header-bar-item').hidden = !!lockedPod;
      var signOutUserItem = $('sign-out-user-item');
      if (signOutUserItem)
        signOutUserItem.hidden = !lockedPod;
      // In case of the preselected pod onShow will be called once pod
      // receives focus.
      if (!podRow.preselectedPod)
        this.onShow();
    },

    /**
     * Event handler invoked when the page is shown and ready.
     */
    onShow: function() {
      if (!this.firstShown_) return;
      this.firstShown_ = false;
      // TODO(nkostylev): Enable animation back when session start jank
      // is reduced. See http://crosbug.com/11116 http://crosbug.com/18307
      // $('pod-row').startInitAnimation();

      // Ensure that login is actually visible.
      window.webkitRequestAnimationFrame(function() {
        chrome.send('accountPickerReady');
        chrome.send('loginVisible', ['account-picker']);
      });
    },

    /**
     * Event handler that is invoked just before the frame is hidden.
     */
    onBeforeHide: function() {
      chrome.send('loginUIStateChanged', ['account-picker', false]);
      $('login-header-bar').signinUIState = SIGNIN_UI_STATE.HIDDEN;
      $('pod-row').handleHide();
    },

    /**
     * Shows sign-in error bubble.
     * @param {number} loginAttempts Number of login attemps tried.
     * @param {HTMLElement} content Content to show in bubble.
     */
    showErrorBubble: function(loginAttempts, error) {
      var activatedPod = $('pod-row').activatedPod;
      if (!activatedPod) {
        $('bubble').showContentForElement($('pod-row'),
                                          cr.ui.Bubble.Attachment.RIGHT,
                                          error);
        return;
      }
      // Show web authentication if this is not a locally managed user.
      if (loginAttempts > MAX_LOGIN_ATTEMPTS_IN_POD &&
          !activatedPod.user.locallyManagedUser) {
        activatedPod.showSigninUI();
      } else {
        // We want bubble's arrow to point to the first letter of input.
        /** @const */ var BUBBLE_OFFSET = 7;
        /** @const */ var BUBBLE_PADDING = 4;
        $('bubble').showContentForElement(activatedPod.mainInput,
                                          cr.ui.Bubble.Attachment.BOTTOM,
                                          error,
                                          BUBBLE_OFFSET, BUBBLE_PADDING);
      }
    },

    /**
     * Loads givens users in pod row.
     * @param {array} users Array of user.
     * @param {boolean} animated Whether to use init animation.
     * @param {boolean} showGuest Whether to show guest session button.
     */
    loadUsers: function(users, animated, showGuest) {
      $('pod-row').loadPods(users, animated);
      $('login-header-bar').showGuestButton = showGuest;

      // The desktop User Manager can send the index of a pod that should be
      // initially focused.
      var hash = window.location.hash;
      if (hash) {
        var podIndex = hash.substr(1);
        if (podIndex)
          $('pod-row').focusPodByIndex(podIndex, false);
      }
    },

    /**
     * Updates current image of a user.
     * @param {string} username User for which to update the image.
     */
    updateUserImage: function(username) {
      $('pod-row').updateUserImage(username);
    },

    /**
     * Updates user to use gaia login.
     * @param {string} username User for which to state the state.
     */
    updateUserGaiaNeeded: function(username) {
      $('pod-row').resetUserOAuthTokenStatus(username);
    },

    /**
     * Updates Caps Lock state (for Caps Lock hint in password input field).
     * @param {boolean} enabled Whether Caps Lock is on.
     */
    setCapsLockState: function(enabled) {
      $('pod-row').classList.toggle('capslock-on', enabled);
    },

    /**
     * Enforces focus on user pod of locked user.
     */
    forceLockedUserPodFocus: function() {
      var row = $('pod-row');
      if (row.lockedPod)
        row.focusPod(row.lockedPod, true);
    }
  };
});


// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview User pod row implementation.
 */

cr.define('login', function() {
  /**
   * Number of displayed columns depending on user pod count.
   * @type {Array.<number>}
   * @const
   */
  var COLUMNS = [0, 1, 2, 3, 4, 5, 4, 4, 4, 5, 5, 6, 6, 5, 5, 6, 6, 6, 6];

  /**
   * Whether to preselect the first pod automatically on login screen.
   * @type {boolean}
   * @const
   */
  var PRESELECT_FIRST_POD = true;

  /**
   * Wallpaper load delay in milliseconds.
   * @type {number}
   * @const
   */
  var WALLPAPER_LOAD_DELAY_MS = 500;

  /**
   * Wallpaper load delay in milliseconds. TODO(nkostylev): Tune this constant.
   * @type {number}
   * @const
   */
  var WALLPAPER_BOOT_LOAD_DELAY_MS = 100;

  /**
   * Maximum time for which the pod row remains hidden until all user images
   * have been loaded.
   * @type {number}
   * @const
   */
  var POD_ROW_IMAGES_LOAD_TIMEOUT_MS = 3000;

  /**
   * Public session help topic identifier.
   * @type {number}
   * @const
   */
  var HELP_TOPIC_PUBLIC_SESSION = 3041033;

  /**
   * Oauth token status. These must match UserManager::OAuthTokenStatus.
   * @enum {number}
   * @const
   */
  var OAuthTokenStatus = {
    UNKNOWN: 0,
    INVALID_OLD: 1,
    VALID_OLD: 2,
    INVALID_NEW: 3,
    VALID_NEW: 4
  };

  /**
   * Tab order for user pods. Update these when adding new controls.
   * @enum {number}
   * @const
   */
  var UserPodTabOrder = {
    POD_INPUT: 1,     // Password input fields (and whole pods themselves).
    HEADER_BAR: 2,    // Buttons on the header bar (Shutdown, Add User).
    ACTION_BOX: 3,    // Action box buttons.
    PAD_MENU_ITEM: 4  // User pad menu items (Remove this user).
  };

  // Focus and tab order are organized as follows:
  //
  // (1) all user pods have tab index 1 so they are traversed first;
  // (2) when a user pod is activated, its tab index is set to -1 and its
  //     main input field gets focus and tab index 1;
  // (3) buttons on the header bar have tab index 2 so they follow user pods;
  // (4) Action box buttons have tab index 3 and follow header bar buttons;
  // (5) lastly, focus jumps to the Status Area and back to user pods.
  //
  // 'Focus' event is handled by a capture handler for the whole document
  // and in some cases 'mousedown' event handlers are used instead of 'click'
  // handlers where it's necessary to prevent 'focus' event from being fired.

  /**
   * Helper function to remove a class from given element.
   * @param {!HTMLElement} el Element whose class list to change.
   * @param {string} cl Class to remove.
   */
  function removeClass(el, cl) {
    el.classList.remove(cl);
  }

  /**
   * Creates a user pod.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var UserPod = cr.ui.define(function() {
    var node = $('user-pod-template').cloneNode(true);
    node.removeAttribute('id');
    return node;
  });

  /**
   * Stops event propagation from the any user pod child element.
   * @param {Event} e Event to handle.
   */
  function stopEventPropagation(e) {
    // Prevent default so that we don't trigger a 'focus' event.
    e.preventDefault();
    e.stopPropagation();
  }

  /**
   * Unique salt added to user image URLs to prevent caching. Dictionary with
   * user names as keys.
   * @type {Object}
   */
  UserPod.userImageSalt_ = {};

  UserPod.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @override */
    decorate: function() {
      this.tabIndex = UserPodTabOrder.POD_INPUT;
      this.actionBoxAreaElement.tabIndex = UserPodTabOrder.ACTION_BOX;

      // Mousedown has to be used instead of click to be able to prevent 'focus'
      // event later.
      this.addEventListener('mousedown',
          this.handleMouseDown_.bind(this));

      this.signinButtonElement.addEventListener('click',
          this.activate.bind(this));

      this.actionBoxAreaElement.addEventListener('mousedown',
                                                 stopEventPropagation);
      this.actionBoxAreaElement.addEventListener('click',
          this.handleActionAreaButtonClick_.bind(this));
      this.actionBoxAreaElement.addEventListener('keydown',
          this.handleActionAreaButtonKeyDown_.bind(this));

      this.actionBoxMenuRemoveElement.addEventListener('click',
          this.handleRemoveCommandClick_.bind(this));
      this.actionBoxMenuRemoveElement.addEventListener('keydown',
          this.handleRemoveCommandKeyDown_.bind(this));
      this.actionBoxMenuRemoveElement.addEventListener('blur',
          this.handleRemoveCommandBlur_.bind(this));

      if (this.actionBoxRemoveUserWarningButtonElement) {
        this.actionBoxRemoveUserWarningButtonElement.addEventListener(
            'click',
            this.handleRemoveUserConfirmationClick_.bind(this));
      }
    },

    /**
     * Initializes the pod after its properties set and added to a pod row.
     */
    initialize: function() {
      this.passwordElement.addEventListener('keydown',
          this.parentNode.handleKeyDown.bind(this.parentNode));
      this.passwordElement.addEventListener('keypress',
          this.handlePasswordKeyPress_.bind(this));

      this.imageElement.addEventListener('load',
          this.parentNode.handlePodImageLoad.bind(this.parentNode, this));
    },

    /**
     * Resets tab order for pod elements to its initial state.
     */
    resetTabOrder: function() {
      this.tabIndex = UserPodTabOrder.POD_INPUT;
      this.mainInput.tabIndex = -1;
    },

    /**
     * Handles keypress event (i.e. any textual input) on password input.
     * @param {Event} e Keypress Event object.
     * @private
     */
    handlePasswordKeyPress_: function(e) {
      // When tabbing from the system tray a tab key press is received. Suppress
      // this so as not to type a tab character into the password field.
      if (e.keyCode == 9) {
        e.preventDefault();
        return;
      }
    },

    /**
     * Gets signed in indicator element.
     * @type {!HTMLDivElement}
     */
    get signedInIndicatorElement() {
      return this.querySelector('.signed-in-indicator');
    },

    /**
     * Gets image element.
     * @type {!HTMLImageElement}
     */
    get imageElement() {
      return this.querySelector('.user-image');
    },

    /**
     * Gets name element.
     * @type {!HTMLDivElement}
     */
    get nameElement() {
      return this.querySelector('.name');
    },

    /**
     * Gets password field.
     * @type {!HTMLInputElement}
     */
    get passwordElement() {
      return this.querySelector('.password');
    },

    /**
     * Gets Caps Lock hint image.
     * @type {!HTMLImageElement}
     */
    get capslockHintElement() {
      return this.querySelector('.capslock-hint');
    },

    /**
     * Gets user signin button.
     * @type {!HTMLInputElement}
     */
    get signinButtonElement() {
      return this.querySelector('.signin-button');
    },

    /**
     * Gets action box area.
     * @type {!HTMLInputElement}
     */
    get actionBoxAreaElement() {
      return this.querySelector('.action-box-area');
    },

    /**
     * Gets user type icon area.
     * @type {!HTMLInputElement}
     */
    get userTypeIconAreaElement() {
      return this.querySelector('.user-type-icon-area');
    },

    /**
     * Gets action box menu.
     * @type {!HTMLInputElement}
     */
    get actionBoxMenuElement() {
      return this.querySelector('.action-box-menu');
    },

    /**
     * Gets action box menu title.
     * @type {!HTMLInputElement}
     */
    get actionBoxMenuTitleElement() {
      return this.querySelector('.action-box-menu-title');
    },

    /**
     * Gets action box menu title, user name item.
     * @type {!HTMLInputElement}
     */
    get actionBoxMenuTitleNameElement() {
      return this.querySelector('.action-box-menu-title-name');
    },

    /**
     * Gets action box menu title, user email item.
     * @type {!HTMLInputElement}
     */
    get actionBoxMenuTitleEmailElement() {
      return this.querySelector('.action-box-menu-title-email');
    },

    /**
     * Gets action box menu, remove user command item.
     * @type {!HTMLInputElement}
     */
    get actionBoxMenuCommandElement() {
      return this.querySelector('.action-box-menu-remove-command');
    },

    /**
     * Gets action box menu, remove user command item div.
     * @type {!HTMLInputElement}
     */
    get actionBoxMenuRemoveElement() {
      return this.querySelector('.action-box-menu-remove');
    },

    /**
     * Gets action box menu, remove user command item div.
     * @type {!HTMLInputElement}
     */
    get actionBoxRemoveUserWarningElement() {
      return this.querySelector('.action-box-remove-user-warning');
    },

    /**
     * Gets action box menu, remove user command item div.
     * @type {!HTMLInputElement}
     */
    get actionBoxRemoveUserWarningButtonElement() {
      return this.querySelector(
          '.remove-warning-button');
    },

    /**
     * Gets the locked user indicator box.
     * @type {!HTMLInputElement}
     */
    get lockedIndicatorElement() {
      return this.querySelector('.locked-indicator');
    },

    /**
     * Updates the user pod element.
     */
    update: function() {
      this.imageElement.src = 'chrome://userimage/' + this.user.username +
          '?id=' + UserPod.userImageSalt_[this.user.username];

      this.nameElement.textContent = this.user_.displayName;
      this.signedInIndicatorElement.hidden = !this.user_.signedIn;

      var needSignin = this.needSignin;
      this.passwordElement.hidden = needSignin;
      this.signinButtonElement.hidden = !needSignin;

      this.updateActionBoxArea();
    },

    updateActionBoxArea: function() {
      this.actionBoxAreaElement.hidden = this.user_.publicAccount;
      this.actionBoxMenuRemoveElement.hidden = !this.user_.canRemove;

      this.actionBoxAreaElement.setAttribute(
          'aria-label', loadTimeData.getStringF(
              'podMenuButtonAccessibleName', this.user_.emailAddress));
      this.actionBoxMenuRemoveElement.setAttribute(
          'aria-label', loadTimeData.getString(
               'podMenuRemoveItemAccessibleName'));
      this.actionBoxMenuTitleNameElement.textContent = this.user_.isOwner ?
          loadTimeData.getStringF('ownerUserPattern', this.user_.displayName) :
          this.user_.displayName;
      this.actionBoxMenuTitleEmailElement.textContent = this.user_.emailAddress;
      this.actionBoxMenuTitleEmailElement.hidden =
          this.user_.locallyManagedUser;

      this.actionBoxMenuCommandElement.textContent =
          loadTimeData.getString('removeUser');
      this.passwordElement.setAttribute('aria-label', loadTimeData.getStringF(
          'passwordFieldAccessibleName', this.user_.emailAddress));
      this.userTypeIconAreaElement.hidden = !this.user_.locallyManagedUser;
    },

    /**
     * The user that this pod represents.
     * @type {!Object}
     */
    user_: undefined,
    get user() {
      return this.user_;
    },
    set user(userDict) {
      this.user_ = userDict;
      this.update();
    },

    /**
     * Whether signin is required for this user.
     */
    get needSignin() {
      // Signin is performed if the user has an invalid oauth token and is
      // not currently signed in (i.e. not the lock screen).
      return this.user.oauthTokenStatus != OAuthTokenStatus.VALID_OLD &&
          this.user.oauthTokenStatus != OAuthTokenStatus.VALID_NEW &&
          !this.user.signedIn;
    },

    /**
     * Gets main input element.
     * @type {(HTMLButtonElement|HTMLInputElement)}
     */
    get mainInput() {
      if (!this.signinButtonElement.hidden)
        return this.signinButtonElement;
      else
        return this.passwordElement;
    },

    /**
     * Whether action box button is in active state.
     * @type {boolean}
     */
    get isActionBoxMenuActive() {
      return this.actionBoxAreaElement.classList.contains('active');
    },
    set isActionBoxMenuActive(active) {
      if (active == this.isActionBoxMenuActive)
        return;

      if (active) {
        this.actionBoxMenuRemoveElement.hidden = !this.user_.canRemove;
        if (this.actionBoxRemoveUserWarningElement)
          this.actionBoxRemoveUserWarningElement.hidden = true;

        // Clear focus first if another pod is focused.
        if (!this.parentNode.isFocused(this)) {
          this.parentNode.focusPod(undefined, true);
          this.actionBoxAreaElement.focus();
        }
        this.actionBoxAreaElement.classList.add('active');
      } else {
        this.actionBoxAreaElement.classList.remove('active');
      }
    },

    /**
     * Whether action box button is in hovered state.
     * @type {boolean}
     */
    get isActionBoxMenuHovered() {
      return this.actionBoxAreaElement.classList.contains('hovered');
    },
    set isActionBoxMenuHovered(hovered) {
      if (hovered == this.isActionBoxMenuHovered)
        return;

      if (hovered) {
        this.actionBoxAreaElement.classList.add('hovered');
        this.classList.add('hovered');
      } else {
        this.actionBoxAreaElement.classList.remove('hovered');
        this.classList.remove('hovered');
      }
    },

    /**
     * Updates the image element of the user.
     */
    updateUserImage: function() {
      UserPod.userImageSalt_[this.user.username] = new Date().getTime();
      this.update();
    },

    /**
     * Focuses on input element.
     */
    focusInput: function() {
      var needSignin = this.needSignin;
      this.signinButtonElement.hidden = !needSignin;
      this.passwordElement.hidden = needSignin;

      // Move tabIndex from the whole pod to the main input.
      this.tabIndex = -1;
      this.mainInput.tabIndex = UserPodTabOrder.POD_INPUT;
      this.mainInput.focus();
    },

    /**
     * Activates the pod.
     * @return {boolean} True if activated successfully.
     */
    activate: function() {
      if (!this.signinButtonElement.hidden) {
        this.showSigninUI();
      } else if (!this.passwordElement.value) {
        return false;
      } else {
        Oobe.disableSigninUI();
        chrome.send('authenticateUser',
                    [this.user.username, this.passwordElement.value]);
      }

      return true;
    },

    showSupervisedUserSigninWarning: function() {
      // Locally managed user token has been invalidated.
      // Make sure that pod is focused i.e. "Sign in" button is seen.
      this.parentNode.focusPod(this);

      var error = document.createElement('div');
      var messageDiv = document.createElement('div');
      messageDiv.className = 'error-message-bubble';
      messageDiv.textContent =
          loadTimeData.getString('supervisedUserExpiredTokenWarning');
      error.appendChild(messageDiv);

      $('bubble').showContentForElement(
          this.signinButtonElement,
          cr.ui.Bubble.Attachment.TOP,
          error,
          this.signinButtonElement.offsetWidth / 2,
          4);
    },

    /**
     * Shows signin UI for this user.
     */
    showSigninUI: function() {
      if (this.user.locallyManagedUser) {
        this.showSupervisedUserSigninWarning();
      } else {
        this.parentNode.showSigninUI(this.user.emailAddress);
      }
    },

    /**
     * Resets the input field and updates the tab order of pod controls.
     * @param {boolean} takeFocus If true, input field takes focus.
     */
    reset: function(takeFocus) {
      this.passwordElement.value = '';
      if (takeFocus)
        this.focusInput();  // This will set a custom tab order.
      else
        this.resetTabOrder();
    },

    /**
     * Handles a click event on action area button.
     * @param {Event} e Click event.
     */
    handleActionAreaButtonClick_: function(e) {
      if (this.parentNode.disabled)
        return;
      this.isActionBoxMenuActive = !this.isActionBoxMenuActive;
    },

    /**
     * Handles a keydown event on action area button.
     * @param {Event} e KeyDown event.
     */
    handleActionAreaButtonKeyDown_: function(e) {
      if (this.disabled)
        return;
      switch (e.keyIdentifier) {
        case 'Enter':
        case 'U+0020':  // Space
          if (this.parentNode.focusedPod_ && !this.isActionBoxMenuActive)
            this.isActionBoxMenuActive = true;
          e.stopPropagation();
          break;
        case 'Up':
        case 'Down':
          if (this.isActionBoxMenuActive) {
            this.actionBoxMenuRemoveElement.tabIndex =
                UserPodTabOrder.PAD_MENU_ITEM;
            this.actionBoxMenuRemoveElement.focus();
          }
          e.stopPropagation();
          break;
        case 'U+001B':  // Esc
          this.isActionBoxMenuActive = false;
          e.stopPropagation();
          break;
        case 'U+0009':  // Tab
          this.parentNode.focusPod();
        default:
          this.isActionBoxMenuActive = false;
          break;
      }
    },

    /**
     * Handles a click event on remove user command.
     * @param {Event} e Click event.
     */
    handleRemoveCommandClick_: function(e) {
      if (this.user.locallyManagedUser || this.user.isDesktopUser) {
        this.showRemoveWarning_();
        return;
      }
      if (this.isActionBoxMenuActive)
        chrome.send('removeUser', [this.user.username]);
    },

    /**
     * Shows remove warning for managed users.
     */
    showRemoveWarning_: function() {
      this.actionBoxMenuRemoveElement.hidden = true;
      this.actionBoxRemoveUserWarningElement.hidden = false;
    },

    /**
     * Handles a click event on remove user confirmation button.
     * @param {Event} e Click event.
     */
    handleRemoveUserConfirmationClick_: function(e) {
      if (this.isActionBoxMenuActive)
        chrome.send('removeUser', [this.user.username]);
    },

    /**
     * Handles a keydown event on remove command.
     * @param {Event} e KeyDown event.
     */
    handleRemoveCommandKeyDown_: function(e) {
      if (this.disabled)
        return;
      switch (e.keyIdentifier) {
        case 'Enter':
          chrome.send('removeUser', [this.user.username]);
          e.stopPropagation();
          break;
        case 'Up':
        case 'Down':
          e.stopPropagation();
          break;
        case 'U+001B':  // Esc
          this.actionBoxAreaElement.focus();
          this.isActionBoxMenuActive = false;
          e.stopPropagation();
          break;
        default:
          this.actionBoxAreaElement.focus();
          this.isActionBoxMenuActive = false;
          break;
      }
    },

    /**
     * Handles a blur event on remove command.
     * @param {Event} e Blur event.
     */
    handleRemoveCommandBlur_: function(e) {
      if (this.disabled)
        return;
      this.actionBoxMenuRemoveElement.tabIndex = -1;
    },

    /**
     * Handles mousedown event on a user pod.
     * @param {Event} e Mousedown event.
     */
    handleMouseDown_: function(e) {
      if (this.parentNode.disabled)
        return;

      if (!this.signinButtonElement.hidden && !this.isActionBoxMenuActive) {
        this.showSigninUI();
        // Prevent default so that we don't trigger 'focus' event.
        e.preventDefault();
      }
    }
  };

  /**
   * Creates a public account user pod.
   * @constructor
   * @extends {UserPod}
   */
  var PublicAccountUserPod = cr.ui.define(function() {
    var node = UserPod();

    var extras = $('public-account-user-pod-extras-template').children;
    for (var i = 0; i < extras.length; ++i) {
      var el = extras[i].cloneNode(true);
      node.appendChild(el);
    }

    return node;
  });

  PublicAccountUserPod.prototype = {
    __proto__: UserPod.prototype,

    /**
     * "Enter" button in expanded side pane.
     * @type {!HTMLButtonElement}
     */
    get enterButtonElement() {
      return this.querySelector('.enter-button');
    },

    /**
     * Boolean flag of whether the pod is showing the side pane. The flag
     * controls whether 'expanded' class is added to the pod's class list and
     * resets tab order because main input element changes when the 'expanded'
     * state changes.
     * @type {boolean}
     */
    get expanded() {
      return this.classList.contains('expanded');
    },
    set expanded(expanded) {
      if (this.expanded == expanded)
        return;

      this.resetTabOrder();
      this.classList.toggle('expanded', expanded);

      var self = this;
      this.classList.add('animating');
      this.addEventListener('webkitTransitionEnd', function f(e) {
        self.removeEventListener('webkitTransitionEnd', f);
        self.classList.remove('animating');

        // Accessibility focus indicator does not move with the focused
        // element. Sends a 'focus' event on the currently focused element
        // so that accessibility focus indicator updates its location.
        if (document.activeElement)
          document.activeElement.dispatchEvent(new Event('focus'));
      });
    },

    /** @override */
    get needSignin() {
      return false;
    },

    /** @override */
    get mainInput() {
      if (this.expanded)
        return this.enterButtonElement;
      else
        return this.nameElement;
    },

    /** @override */
    decorate: function() {
      UserPod.prototype.decorate.call(this);

      this.classList.remove('need-password');
      this.classList.add('public-account');

      this.nameElement.addEventListener('keydown', (function(e) {
        if (e.keyIdentifier == 'Enter') {
          this.parentNode.activatedPod = this;
          // Stop this keydown event from bubbling up to PodRow handler.
          e.stopPropagation();
          // Prevent default so that we don't trigger a 'click' event on the
          // newly focused "Enter" button.
          e.preventDefault();
        }
      }).bind(this));

      var learnMore = this.querySelector('.learn-more');
      learnMore.addEventListener('mousedown', stopEventPropagation);
      learnMore.addEventListener('click', this.handleLearnMoreEvent);
      learnMore.addEventListener('keydown', this.handleLearnMoreEvent);

      learnMore = this.querySelector('.side-pane-learn-more');
      learnMore.addEventListener('click', this.handleLearnMoreEvent);
      learnMore.addEventListener('keydown', this.handleLearnMoreEvent);

      this.enterButtonElement.addEventListener('click', (function(e) {
        this.enterButtonElement.disabled = true;
        chrome.send('launchPublicAccount', [this.user.username]);
      }).bind(this));
    },

    /**
     * Updates the user pod element.
     */
    update: function() {
      UserPod.prototype.update.call(this);
      this.querySelector('.side-pane-name').textContent =
          this.user_.displayName;
      this.querySelector('.info').textContent =
          loadTimeData.getStringF('publicAccountInfoFormat',
                                  this.user_.enterpriseDomain);
    },

    /** @override */
    focusInput: function() {
      // Move tabIndex from the whole pod to the main input.
      this.tabIndex = -1;
      this.mainInput.tabIndex = UserPodTabOrder.POD_INPUT;
      this.mainInput.focus();
    },

    /** @override */
    reset: function(takeFocus) {
      if (!takeFocus)
        this.expanded = false;
      this.enterButtonElement.disabled = false;
      UserPod.prototype.reset.call(this, takeFocus);
    },

    /** @override */
    activate: function() {
      this.expanded = true;
      this.focusInput();
      return true;
    },

    /** @override */
    handleMouseDown_: function(e) {
      if (this.parentNode.disabled)
        return;

      this.parentNode.focusPod(this);
      this.parentNode.activatedPod = this;
      // Prevent default so that we don't trigger 'focus' event.
      e.preventDefault();
    },

    /**
     * Handle mouse and keyboard events for the learn more button.
     * Triggering the button causes information about public sessions to be
     * shown.
     * @param {Event} event Mouse or keyboard event.
     */
    handleLearnMoreEvent: function(event) {
      switch (event.type) {
        // Show informaton on left click. Let any other clicks propagate.
        case 'click':
          if (event.button != 0)
            return;
          break;
        // Show informaton when <Return> or <Space> is pressed. Let any other
        // key presses propagate.
        case 'keydown':
          switch (event.keyCode) {
            case 13:  // Return.
            case 32:  // Space.
              break;
            default:
              return;
          }
          break;
      }
      chrome.send('launchHelpApp', [HELP_TOPIC_PUBLIC_SESSION]);
      stopEventPropagation(event);
    },
  };

  /**
   * Creates a user pod to be used only in desktop chrome.
   * @constructor
   * @extends {UserPod}
   */
  var DesktopUserPod = cr.ui.define(function() {
    // Don't just instantiate a UserPod(), as this will call decorate() on the
    // parent object, and add duplicate event listeners.
    var node = $('user-pod-template').cloneNode(true);
    node.removeAttribute('id');
    return node;
  });

  DesktopUserPod.prototype = {
    __proto__: UserPod.prototype,

    /** @override */
    get mainInput() {
      if (!this.passwordElement.hidden)
        return this.passwordElement;
      else
        return this.nameElement;
    },

    /** @override */
    decorate: function() {
      UserPod.prototype.decorate.call(this);
    },

    /** @override */
    update: function() {
      // TODO(noms): Use the actual profile avatar for local profiles once the
      // new, non-pixellated avatars are available.
      this.imageElement.src = this.user.emailAddress == '' ?
          'chrome://theme/IDR_USER_MANAGER_DEFAULT_AVATAR' :
          this.user.userImage;
      this.nameElement.textContent = this.user.displayName;

      var isLockedUser = this.user.needsSignin;
      this.signinButtonElement.hidden = true;
      this.lockedIndicatorElement.hidden = !isLockedUser;
      this.passwordElement.hidden = !isLockedUser;
      this.nameElement.hidden = isLockedUser;

      UserPod.prototype.updateActionBoxArea.call(this);
    },

    /** @override */
    focusInput: function() {
      // For focused pods, display the name unless the pod is locked.
      var isLockedUser = this.user.needsSignin;
      this.signinButtonElement.hidden = true;
      this.lockedIndicatorElement.hidden = !isLockedUser;
      this.passwordElement.hidden = !isLockedUser;
      this.nameElement.hidden = isLockedUser;

      // Move tabIndex from the whole pod to the main input.
      this.tabIndex = -1;
      this.mainInput.tabIndex = UserPodTabOrder.POD_INPUT;
      this.mainInput.focus();
    },

    /** @override */
    reset: function(takeFocus) {
      // Always display the user's name for unfocused pods.
      if (!takeFocus)
        this.nameElement.hidden = false;
      UserPod.prototype.reset.call(this, takeFocus);
    },

    /** @override */
    activate: function() {
      Oobe.launchUser(this.user.emailAddress, this.user.displayName);
      return true;
    },

    /** @override */
    handleMouseDown_: function(e) {
      if (this.parentNode.disabled)
        return;

      Oobe.clearErrors();
      this.parentNode.lastFocusedPod_ = this;

      // If this is an unlocked pod, then open a browser window. Otherwise
      // just activate the pod and show the password field.
      if (!this.user.needsSignin && !this.isActionBoxMenuActive)
        this.activate();
    },

    /** @override */
    handleRemoveUserConfirmationClick_: function(e) {
      chrome.send('removeUser', [this.user.profilePath]);
    },
  };

  /**
   * Creates a new pod row element.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var PodRow = cr.ui.define('podrow');

  PodRow.prototype = {
    __proto__: HTMLDivElement.prototype,

    // Whether this user pod row is shown for the first time.
    firstShown_: true,

    // Whether the initial wallpaper load after boot has been requested. Used
    // only if |Oobe.getInstance().shouldLoadWallpaperOnBoot()| is true.
    bootWallpaperLoaded_: false,

    // True if inside focusPod().
    insideFocusPod_: false,

    // True if user pod has been activated with keyboard.
    // In case of activation with keyboard we delay wallpaper change.
    keyboardActivated_: false,

    // Focused pod.
    focusedPod_: undefined,

    // Activated pod, i.e. the pod of current login attempt.
    activatedPod_: undefined,

    // Pod that was most recently focused, if any.
    lastFocusedPod_: undefined,

    // When moving through users quickly at login screen, set a timeout to
    // prevent loading intermediate wallpapers.
    loadWallpaperTimeout_: null,

    // Pods whose initial images haven't been loaded yet.
    podsWithPendingImages_: [],

    /** @override */
    decorate: function() {
      this.style.left = 0;

      // Event listeners that are installed for the time period during which
      // the element is visible.
      this.listeners_ = {
        focus: [this.handleFocus_.bind(this), true /* useCapture */],
        click: [this.handleClick_.bind(this), true],
        mousemove: [this.handleMouseMove_.bind(this), false],
        keydown: [this.handleKeyDown.bind(this), false]
      };
    },

    /**
     * Returns all the pods in this pod row.
     * @type {NodeList}
     */
    get pods() {
      return this.children;
    },

    /**
     * Return true if user pod row has only single user pod in it.
     * @type {boolean}
     */
    get isSinglePod() {
      return this.children.length == 1;
    },

    /**
     * Returns pod with the given username (null if there is no such pod).
     * @param {string} username Username to be matched.
     * @return {Object} Pod with the given username. null if pod hasn't been
     *                  found.
     */
    getPodWithUsername_: function(username) {
      for (var i = 0, pod; pod = this.pods[i]; ++i) {
        if (pod.user.username == username)
          return pod;
      }
      return null;
    },

    /**
     * True if the the pod row is disabled (handles no user interaction).
     * @type {boolean}
     */
    disabled_: false,
    get disabled() {
      return this.disabled_;
    },
    set disabled(value) {
      this.disabled_ = value;
      var controls = this.querySelectorAll('button,input');
      for (var i = 0, control; control = controls[i]; ++i) {
        control.disabled = value;
      }
    },

    /**
     * Creates a user pod from given email.
     * @param {string} email User's email.
     */
    createUserPod: function(user) {
      var userPod;
      if (user.isDesktopUser)
        userPod = new DesktopUserPod({user: user});
      else if (user.publicAccount)
        userPod = new PublicAccountUserPod({user: user});
      else
        userPod = new UserPod({user: user});

      userPod.hidden = false;
      return userPod;
    },

    /**
     * Add an existing user pod to this pod row.
     * @param {!Object} user User info dictionary.
     * @param {boolean} animated Whether to use init animation.
     */
    addUserPod: function(user, animated) {
      var userPod = this.createUserPod(user);
      if (animated) {
        userPod.classList.add('init');
        userPod.nameElement.classList.add('init');
      }

      this.appendChild(userPod);
      userPod.initialize();
    },

    /**
     * Returns index of given pod or -1 if not found.
     * @param {UserPod} pod Pod to look up.
     * @private
     */
    indexOf_: function(pod) {
      for (var i = 0; i < this.pods.length; ++i) {
        if (pod == this.pods[i])
          return i;
      }
      return -1;
    },

    /**
     * Start first time show animation.
     */
    startInitAnimation: function() {
      // Schedule init animation.
      for (var i = 0, pod; pod = this.pods[i]; ++i) {
        window.setTimeout(removeClass, 500 + i * 70, pod, 'init');
        window.setTimeout(removeClass, 700 + i * 70, pod.nameElement, 'init');
      }
    },

    /**
     * Start login success animation.
     */
    startAuthenticatedAnimation: function() {
      var activated = this.indexOf_(this.activatedPod_);
      if (activated == -1)
        return;

      for (var i = 0, pod; pod = this.pods[i]; ++i) {
        if (i < activated)
          pod.classList.add('left');
        else if (i > activated)
          pod.classList.add('right');
        else
          pod.classList.add('zoom');
      }
    },

    /**
     * Populates pod row with given existing users and start init animation.
     * @param {array} users Array of existing user emails.
     * @param {boolean} animated Whether to use init animation.
     */
    loadPods: function(users, animated) {
      // Clear existing pods.
      this.innerHTML = '';
      this.focusedPod_ = undefined;
      this.activatedPod_ = undefined;
      this.lastFocusedPod_ = undefined;

      // Populate the pod row.
      for (var i = 0; i < users.length; ++i) {
        this.addUserPod(users[i], animated);
      }
      for (var i = 0, pod; pod = this.pods[i]; ++i) {
        this.podsWithPendingImages_.push(pod);
      }
      // Make sure we eventually show the pod row, even if some image is stuck.
      setTimeout(function() {
        $('pod-row').classList.remove('images-loading');
      }, POD_ROW_IMAGES_LOAD_TIMEOUT_MS);

      var columns = users.length < COLUMNS.length ?
          COLUMNS[users.length] : COLUMNS[COLUMNS.length - 1];
      var rows = Math.floor((users.length - 1) / columns) + 1;

      // Cancel any pending resize operation.
      this.removeEventListener('mouseout', this.deferredResizeListener_);

      // If this pod row is used in the desktop user manager, we need to
      // force a resize, as it may be a background window which won't get a
      // mouseout event for a while; the pods would be displayed incorrectly
      // until then.
      if (this.preselectedPod && this.preselectedPod.user.isDesktopUser)
        this.resize_(columns, rows);

      if (!this.columns || !this.rows) {
        // Set initial dimensions.
        this.resize_(columns, rows);
      } else if (columns != this.columns || rows != this.rows) {
        // Defer the resize until mouse cursor leaves the pod row.
        this.deferredResizeListener_ = function(e) {
          if (!findAncestorByClass(e.toElement, 'podrow')) {
            this.resize_(columns, rows);
          }
        }.bind(this);
        this.addEventListener('mouseout', this.deferredResizeListener_);
      }

      this.focusPod(this.preselectedPod);
    },

    /**
     * Resizes the pod row and cancel any pending resize operations.
     * @param {number} columns Number of columns.
     * @param {number} rows Number of rows.
     * @private
     */
    resize_: function(columns, rows) {
      this.removeEventListener('mouseout', this.deferredResizeListener_);
      this.columns = columns;
      this.rows = rows;
      if (this.parentNode == Oobe.getInstance().currentScreen) {
        Oobe.getInstance().updateScreenSize(this.parentNode);
      }
    },

    /**
     * Number of columns.
     * @type {?number}
     */
    set columns(columns) {
      // Cannot use 'columns' here.
      this.setAttribute('ncolumns', columns);
    },
    get columns() {
      return this.getAttribute('ncolumns');
    },

    /**
     * Number of rows.
     * @type {?number}
     */
    set rows(rows) {
      // Cannot use 'rows' here.
      this.setAttribute('nrows', rows);
    },
    get rows() {
      return this.getAttribute('nrows');
    },

    /**
     * Whether the pod is currently focused.
     * @param {UserPod} pod Pod to check for focus.
     * @return {boolean} Pod focus status.
     */
    isFocused: function(pod) {
      return this.focusedPod_ == pod;
    },

    /**
     * Focuses a given user pod or clear focus when given null.
     * @param {UserPod=} podToFocus User pod to focus (undefined clears focus).
     * @param {boolean=} opt_force If true, forces focus update even when
     *                             podToFocus is already focused.
     */
    focusPod: function(podToFocus, opt_force) {
      if (this.isFocused(podToFocus) && !opt_force) {
        this.keyboardActivated_ = false;
        return;
      }

      // Make sure there's only one focusPod operation happening at a time.
      if (this.insideFocusPod_) {
        this.keyboardActivated_ = false;
        return;
      }
      this.insideFocusPod_ = true;

      clearTimeout(this.loadWallpaperTimeout_);
      for (var i = 0, pod; pod = this.pods[i]; ++i) {
        if (!this.isSinglePod) {
          pod.isActionBoxMenuActive = false;
        }
        if (pod != podToFocus) {
          pod.isActionBoxMenuHovered = false;
          pod.classList.remove('focused');
          pod.classList.remove('faded');
          pod.reset(false);
        }
      }

      // Clear any error messages for previous pod.
      if (!this.isFocused(podToFocus))
        Oobe.clearErrors();

      var hadFocus = !!this.focusedPod_;
      this.focusedPod_ = podToFocus;
      if (podToFocus) {
        podToFocus.classList.remove('faded');
        podToFocus.classList.add('focused');
        podToFocus.reset(true);  // Reset and give focus.
        chrome.send('focusPod', [podToFocus.user.emailAddress]);
        if (hadFocus && this.keyboardActivated_) {
          // Delay wallpaper loading to let user tab through pods without lag.
          this.loadWallpaperTimeout_ = window.setTimeout(
              this.loadWallpaper_.bind(this), WALLPAPER_LOAD_DELAY_MS);
        } else if (!this.firstShown_) {
          // Load wallpaper immediately if there no pod was focused
          // previously, and it is not a boot into user pod list case.
          this.loadWallpaper_();
        }
        this.firstShown_ = false;
        this.lastFocusedPod_ = podToFocus;
      }
      this.insideFocusPod_ = false;
      this.keyboardActivated_ = false;
    },

    /**
     * Loads wallpaper for the active user pod, if any.
     * @private
     */
    loadWallpaper_: function() {
      if (this.focusedPod_)
        chrome.send('loadWallpaper', [this.focusedPod_.user.username]);
    },

    /**
     * Focuses a given user pod by index or clear focus when given null.
     * @param {int=} podToFocus index of User pod to focus.
     * @param {boolean=} opt_force If true, forces focus update even when
     *                             podToFocus is already focused.
     */
    focusPodByIndex: function(podToFocus, opt_force) {
      if (podToFocus < this.pods.length)
        this.focusPod(this.pods[podToFocus], opt_force);
    },

    /**
     * Resets wallpaper to the last active user's wallpaper, if any.
     */
    loadLastWallpaper: function() {
      if (this.lastFocusedPod_)
        chrome.send('loadWallpaper', [this.lastFocusedPod_.user.username]);
    },

    /**
     * Returns the currently activated pod.
     * @type {UserPod}
     */
    get activatedPod() {
      return this.activatedPod_;
    },
    set activatedPod(pod) {
      if (pod && pod.activate())
        this.activatedPod_ = pod;
    },

    /**
     * The pod of the signed-in user, if any; null otherwise.
     * @type {?UserPod}
     */
    get lockedPod() {
      for (var i = 0, pod; pod = this.pods[i]; ++i) {
        if (pod.user.signedIn)
          return pod;
      }
      return null;
    },

    /**
     * The pod that is preselected on user pod row show.
     * @type {?UserPod}
     */
    get preselectedPod() {
      var lockedPod = this.lockedPod;
      var preselectedPod = PRESELECT_FIRST_POD ?
          lockedPod || this.pods[0] : lockedPod;
      return preselectedPod;
    },

    /**
     * Resets input UI.
     * @param {boolean} takeFocus True to take focus.
     */
    reset: function(takeFocus) {
      this.disabled = false;
      if (this.activatedPod_)
        this.activatedPod_.reset(takeFocus);
    },

    /**
     * Restores input focus to current selected pod, if there is any.
     */
    refocusCurrentPod: function() {
      if (this.focusedPod_) {
        this.focusedPod_.focusInput();
      }
    },

    /**
     * Clears focused pod password field.
     */
    clearFocusedPod: function() {
      if (!this.disabled && this.focusedPod_)
        this.focusedPod_.reset(true);
    },

    /**
     * Shows signin UI.
     * @param {string} email Email for signin UI.
     */
    showSigninUI: function(email) {
      // Clear any error messages that might still be around.
      Oobe.clearErrors();
      this.disabled = true;
      this.lastFocusedPod_ = this.getPodWithUsername_(email);
      Oobe.showSigninUI(email);
    },

    /**
     * Updates current image of a user.
     * @param {string} username User for which to update the image.
     */
    updateUserImage: function(username) {
      var pod = this.getPodWithUsername_(username);
      if (pod)
        pod.updateUserImage();
    },

    /**
     * Resets OAuth token status (invalidates it).
     * @param {string} username User for which to reset the status.
     */
    resetUserOAuthTokenStatus: function(username) {
      var pod = this.getPodWithUsername_(username);
      if (pod) {
        pod.user.oauthTokenStatus = OAuthTokenStatus.INVALID_OLD;
        pod.update();
      } else {
        console.log('Failed to update Gaia state for: ' + username);
      }
    },

    /**
     * Handler of click event.
     * @param {Event} e Click Event object.
     * @private
     */
    handleClick_: function(e) {
      if (this.disabled)
        return;

      // Clear all menus if the click is outside pod menu and its
      // button area.
      if (!findAncestorByClass(e.target, 'action-box-menu') &&
          !findAncestorByClass(e.target, 'action-box-area')) {
        for (var i = 0, pod; pod = this.pods[i]; ++i)
          pod.isActionBoxMenuActive = false;
      }

      // Clears focus if not clicked on a pod and if there's more than one pod.
      var pod = findAncestorByClass(e.target, 'pod');
      if ((!pod || pod.parentNode != this) && !this.isSinglePod) {
        this.focusPod();
      }

      if (pod)
        pod.isActionBoxMenuHovered = true;

      // Return focus back to single pod.
      if (this.isSinglePod) {
        this.focusPod(this.focusedPod_, true /* force */);
        if (!pod)
          this.focusedPod_.isActionBoxMenuHovered = false;
      }

      // Also stop event propagation.
      if (pod && e.target == pod.imageElement)
        e.stopPropagation();
    },

    /**
     * Handler of mouse move event.
     * @param {Event} e Click Event object.
     * @private
     */
    handleMouseMove_: function(e) {
      if (this.disabled)
        return;
      if (e.webkitMovementX == 0 && e.webkitMovementY == 0)
        return;

      // Defocus (thus hide) action box, if it is focused on a user pod
      // and the pointer is not hovering over it.
      var pod = findAncestorByClass(e.target, 'pod');
      if (document.activeElement &&
          document.activeElement.parentNode != pod &&
          document.activeElement.classList.contains('action-box-area')) {
        document.activeElement.parentNode.focus();
      }

      if (pod)
        pod.isActionBoxMenuHovered = true;

      // Hide action boxes on other user pods.
      for (var i = 0, p; p = this.pods[i]; ++i)
        if (p != pod && !p.isActionBoxMenuActive)
          p.isActionBoxMenuHovered = false;
    },

    /**
     * Handles focus event.
     * @param {Event} e Focus Event object.
     * @private
     */
    handleFocus_: function(e) {
      if (this.disabled)
        return;
      if (e.target.parentNode == this) {
        // Focus on a pod
        if (e.target.classList.contains('focused'))
          e.target.focusInput();
        else
          this.focusPod(e.target);
        return;
      }

      var pod = findAncestorByClass(e.target, 'pod');
      if (pod && pod.parentNode == this) {
        // Focus on a control of a pod but not on the action area button.
        if (!pod.classList.contains('focused') &&
            !e.target.classList.contains('action-box-button')) {
          this.focusPod(pod);
          e.target.focus();
        }
        return;
      }

      // Clears pod focus when we reach here. It means new focus is neither
      // on a pod nor on a button/input for a pod.
      // Do not "defocus" user pod when it is a single pod.
      // That means that 'focused' class will not be removed and
      // input field/button will always be visible.
      if (!this.isSinglePod)
        this.focusPod();
    },

    /**
     * Handler of keydown event.
     * @param {Event} e KeyDown Event object.
     */
    handleKeyDown: function(e) {
      if (this.disabled)
        return;
      var editing = e.target.tagName == 'INPUT' && e.target.value;
      switch (e.keyIdentifier) {
        case 'Left':
          if (!editing) {
            this.keyboardActivated_ = true;
            if (this.focusedPod_ && this.focusedPod_.previousElementSibling)
              this.focusPod(this.focusedPod_.previousElementSibling);
            else
              this.focusPod(this.lastElementChild);

            e.stopPropagation();
          }
          break;
        case 'Right':
          if (!editing) {
            this.keyboardActivated_ = true;
            if (this.focusedPod_ && this.focusedPod_.nextElementSibling)
              this.focusPod(this.focusedPod_.nextElementSibling);
            else
              this.focusPod(this.firstElementChild);

            e.stopPropagation();
          }
          break;
        case 'Enter':
          if (this.focusedPod_) {
            this.activatedPod = this.focusedPod_;
            e.stopPropagation();
          }
          break;
        case 'U+001B':  // Esc
          if (!this.isSinglePod)
            this.focusPod();
          break;
      }
    },

    /**
     * Called right after the pod row is shown.
     */
    handleAfterShow: function() {
      // Force input focus for user pod on show and once transition ends.
      if (this.focusedPod_) {
        var focusedPod = this.focusedPod_;
        var screen = this.parentNode;
        var self = this;
        focusedPod.addEventListener('webkitTransitionEnd', function f(e) {
          if (e.target == focusedPod) {
            focusedPod.removeEventListener('webkitTransitionEnd', f);
            focusedPod.reset(true);
            // Notify screen that it is ready.
            screen.onShow();
            // Boot transition: load wallpaper.
            if (!self.bootWallpaperLoaded_ &&
                Oobe.getInstance().shouldLoadWallpaperOnBoot()) {
              self.loadWallpaperTimeout_ = window.setTimeout(
                  self.loadWallpaper_.bind(self), WALLPAPER_BOOT_LOAD_DELAY_MS);
              self.bootWallpaperLoaded_ = true;
            }
          }
        });
        // Guard timer for 1 second -- it would conver all possible animations.
        ensureTransitionEndEvent(focusedPod, 1000);
      }
    },

    /**
     * Called right before the pod row is shown.
     */
    handleBeforeShow: function() {
      for (var event in this.listeners_) {
        this.ownerDocument.addEventListener(
            event, this.listeners_[event][0], this.listeners_[event][1]);
      }
      $('login-header-bar').buttonsTabIndex = UserPodTabOrder.HEADER_BAR;
    },

    /**
     * Called when the element is hidden.
     */
    handleHide: function() {
      for (var event in this.listeners_) {
        this.ownerDocument.removeEventListener(
            event, this.listeners_[event][0], this.listeners_[event][1]);
      }
      $('login-header-bar').buttonsTabIndex = 0;
    },

    /**
     * Called when a pod's user image finishes loading.
     */
    handlePodImageLoad: function(pod) {
      var index = this.podsWithPendingImages_.indexOf(pod);
      if (index == -1) {
        return;
      }

      this.podsWithPendingImages_.splice(index, 1);
      if (this.podsWithPendingImages_.length == 0) {
        this.classList.remove('images-loading');
      }
    }
  };

  return {
    PodRow: PodRow
  };
});

// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Deferred resource loader for OOBE/Login screens.
 */

cr.define('cr.ui.login.ResourceLoader', function() {
  'use strict';

  // Deferred assets.
  var ASSETS = {};

  /**
   * Register assets for deferred loading.  When the bundle is loaded
   * assets will be added to the current page's DOM: <link> and <script>
   * tags pointing to the CSS and JavaScript will be added to the
   * <head>, and HTML will be appended to a specified element.
   *
   * @param {Object} desc Descriptor for the asset bundle
   * @param {string} desc.id Unique identifier for the asset bundle.
   * @param {Array=} desc.js URLs containing JavaScript sources.
   * @param {Array=} desc.css URLs containing CSS rules.
   * @param {Array.<Object>=} desc.html Descriptors for HTML fragments,
   * each of which has a 'url' property and a 'targetID' property that
   * specifies the node under which the HTML should be appended.
   *
   * Example:
   *   ResourceLoader.registerAssets({
   *     id: 'bundle123',
   *     js: ['//foo.com/src.js', '//bar.com/lib.js'],
   *     css: ['//foo.com/style.css'],
   *     html: [{ url: '//foo.com/tmpls.html' targetID: 'tmpls'}]
   *   });
   *
   * Note: to avoid cross-site requests, all HTML assets must be served
   * from the same host as the rendered page.  For example, if the
   * rendered page is served as chrome://oobe, then all the HTML assets
   * must be served as chrome://oobe/path/to/something.html.
   */
  function registerAssets(desc) {
    var html = desc.html || [];
    var css = desc.css || [];
    var js = desc.js || [];
    ASSETS[desc.id] = {
      html: html, css: css, js: js,
      loaded: false,
      count: html.length + css.length + js.length
    };
  }

  /**
   * Determines whether an asset bundle is defined for a specified id.
   * @param {string} id The possible identifier.
   */
  function hasDeferredAssets(id) {
    return id in ASSETS;
  }

  /**
   * Determines whether an asset bundle has already been loaded.
   * @param {string} id The identifier of the asset bundle.
   */
  function alreadyLoadedAssets(id) {
    return hasDeferredAssets(id) && ASSETS[id].loaded;
  }

  /**
   * Load a stylesheet into the current document.
   * @param {string} id Identifier of the stylesheet's asset bundle.
   * @param {string} url The URL resolving to a stylesheet.
   */
  function loadCSS(id, url) {
    var link = document.createElement('link');
    link.setAttribute('rel', 'stylesheet');
    link.setAttribute('href', url);
    link.onload = resourceLoaded.bind(null, id);
    document.head.appendChild(link);
  }

  /**
   * Load a script into the current document.
   * @param {string} id Identifier of the script's asset bundle.
   * @param {string} url The URL resolving to a script.
   */
  function loadJS(id, url) {
    var script = document.createElement('script');
    script.src = url;
    script.onload = resourceLoaded.bind(null, id);
    document.head.appendChild(script);
  }

  /**
   * Move DOM nodes from one parent element to another.
   * @param {HTMLElement} from Element whose children should be moved.
   * @param {HTMLElement} to Element to which nodes should be appended.
   */
  function moveNodes(from, to) {
    Array.prototype.forEach.call(from.children, to.appendChild, to);
  }

  /**
   * Tests whether an XMLHttpRequest has successfully finished loading.
   * @param {string} url The requested URL.
   * @param {XMLHttpRequest} xhr The XHR object.
   */
  function isSuccessful(url, xhr) {
    var fileURL = /^file:\/\//;
    return xhr.readyState == 4 &&
        (xhr.status == 200 || fileURL.test(url) && xhr.status == 0);
  }

  /*
   * Load a chunk of HTML into the current document.
   * @param {string} id Identifier of the page's asset bundle.
   * @param {Object} html Descriptor of the HTML to fetch.
   * @param {string} html.url The URL resolving to some HTML.
   * @param {string} html.targetID The element ID to which the retrieved
   * HTML nodes should be appended.
   */
  function loadHTML(id, html) {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', html.url);
    xhr.onreadystatechange = function() {
      if (isSuccessful(html.url, xhr)) {
        moveNodes(this.responseXML.body, $(html.targetID));
        resourceLoaded(id);
      }
    };
    xhr.responseType = 'document';
    xhr.send();
  }

  /**
   * Record that a resource has been loaded for an asset bundle.  When
   * all the resources have been loaded the callback that was specified
   * in the loadAssets call is invoked.
   * @param {string} id Identifier of the asset bundle.
   */
  function resourceLoaded(id) {
    var assets = ASSETS[id];
    assets.count--;
    if (assets.count == 0)
      finishedLoading(id);
  }

  /**
   * Finishes loading an asset bundle.
   * @param {string} id Identifier of the asset bundle.
   */
  function finishedLoading(id) {
    var assets = ASSETS[id];
    console.log('Finished loading asset bundle', id);
    assets.loaded = true;
    window.setTimeout(function() {
      assets.callback();
      chrome.send('screenAssetsLoaded', [id]);
    }, 0);
  }

  /**
   * Load an asset bundle, invoking the callback when finished.
   * @param {string} id Identifier for the asset bundle to load.
   * @param {function()=} callback Function to invoke when done loading.
   */
  function loadAssets(id, callback) {
    var assets = ASSETS[id];
    assets.callback = callback || function() {};
    console.log('Loading asset bundle', id);
    if (alreadyLoadedAssets(id))
      console.warn('asset bundle', id, 'already loaded!');
    if (assets.count == 0) {
      finishedLoading(id);
    } else {
      assets.css.forEach(loadCSS.bind(null, id));
      assets.js.forEach(loadJS.bind(null, id));
      assets.html.forEach(loadHTML.bind(null, id));
    }
  }

  return {
    alreadyLoadedAssets: alreadyLoadedAssets,
    hasDeferredAssets: hasDeferredAssets,
    loadAssets: loadAssets,
    registerAssets: registerAssets
  };
});


cr.define('cr.ui', function() {
  var DisplayManager = cr.ui.login.DisplayManager;

  /**
  * Constructs an Out of box controller. It manages initialization of screens,
  * transitions, error messages display.
  * @extends {DisplayManager}
  * @constructor
  */
  function Oobe() {
  }

  cr.addSingletonGetter(Oobe);

  Oobe.prototype = {
    __proto__: DisplayManager.prototype,
  };

  /**
   * Shows the given screen.
   * @param {Object} screen Screen params dict, e.g. {id: screenId, data: data}
   */
  Oobe.showUserManagerScreen = function() {
    Oobe.getInstance().showScreen({id: 'account-picker',
                                   data: {disableAddUser: false}});
    // The ChromeOS account-picker will hide the AddUser button if a user is
    // logged in and the screen is "locked", so we must re-enabled it
    $('add-user-header-bar-item').hidden = false;
  };

  /**
   * Shows signin UI.
   * @param {string} opt_email An optional email for signin UI.
   */
  Oobe.launchUser = function(email, displayName) {
    chrome.send('launchUser', [email, displayName]);
  };

  /**
   * Disables signin UI.
   */
  Oobe.disableSigninUI = function() {
    DisplayManager.disableSigninUI();
  };

  /**
   * Shows signin UI.
   * @param {string} opt_email An optional email for signin UI.
   */
  Oobe.showSigninUI = function(opt_email) {
    DisplayManager.showSigninUI(opt_email);
  };

  /**
   * Clears error bubble as well as optional menus that could be open.
   */
  Oobe.clearErrors = function() {
    DisplayManager.clearErrors();
  };

  /**
   * Clears password field in user-pod.
   */
  Oobe.clearUserPodPassword = function() {
    DisplayManager.clearUserPodPassword();
  };

  /**
   * Restores input focus to currently selected pod.
   */
  Oobe.refocusCurrentPod = function() {
    DisplayManager.refocusCurrentPod();
  };

  // Export
  return {
    Oobe: Oobe
  };
});

cr.define('UserManager', function() {
  'use strict';

  function initialize() {
    login.AccountPickerScreen.register();
    cr.ui.Bubble.decorate($('bubble'));
    login.HeaderBar.decorate($('login-header-bar'));
    chrome.send('userManagerInitialize');
  }

  // Return an object with all of the exports.
  return {
    initialize: initialize
  };
});

var Oobe = cr.ui.Oobe;

// Allow selection events on components with editable text (password field)
// bug (http://code.google.com/p/chromium/issues/detail?id=125863)
disableTextSelectAndDrag(function(e) {
  var src = e.target;
  return src instanceof HTMLTextAreaElement ||
         src instanceof HTMLInputElement &&
         /text|password|search/.test(src.type);
});

document.addEventListener('DOMContentLoaded', UserManager.initialize);
