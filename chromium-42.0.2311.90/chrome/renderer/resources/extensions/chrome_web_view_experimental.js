// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements experimental API for <webview>.
// See web_view.js for details.
//
// <webview> Chrome Experimental API is only available on canary and dev
// channels of Chrome.

var ChromeWebView = require('chromeWebViewInternal').ChromeWebView;
var ChromeWebViewSchema =
    requireNative('schema_registry').GetSchema('chromeWebViewInternal');
var ContextMenusSchema =
    requireNative('schema_registry').GetSchema('contextMenus');
var CreateEvent = require('webViewEvents').CreateEvent;
var EventBindings = require('event_bindings');
var idGeneratorNatives = requireNative('id_generator');
var MessagingNatives = requireNative('messaging_natives');
var utils = require('utils');
var WebViewImpl = require('webView').WebViewImpl;
var DeclarativeContentSchema =
    requireNative('schema_registry').GetSchema('declarativeContent');

var DeclarativeContentEvent = function(opt_eventName,
                                       opt_argSchemas,
                                       opt_eventOptions,
                                       opt_webViewInstanceId) {
  EventBindings.Event.call(this,
                           opt_eventName,
                           opt_argSchemas,
                           opt_eventOptions,
                           opt_webViewInstanceId);
}

DeclarativeContentEvent.prototype = {
  __proto__: EventBindings.Event.prototype
};

function GetUniqueSubEventName(eventName) {
  return eventName + '/' + idGeneratorNatives.GetNextId();
}

// This is the only "webViewInternal.onClicked" named event for this renderer.
//
// Since we need an event per <webview>, we define events with suffix
// (subEventName) in each of the <webview>. Behind the scenes, this event is
// registered as a ContextMenusEvent, with filter set to the webview's
// |viewInstanceId|. Any time a ContextMenusEvent is dispatched, we re-dispatch
// it to the subEvent's listeners. This way
// <webview>.contextMenus.onClicked behave as a regular chrome Event type.
var ContextMenusEvent = CreateEvent('chromeWebViewInternal.onClicked');

/**
 * This event is exposed as <webview>.contextMenus.onClicked.
 *
 * @constructor
 */
function ContextMenusOnClickedEvent(opt_eventName,
                                    opt_argSchemas,
                                    opt_eventOptions,
                                    opt_webViewInstanceId) {
  var subEventName = GetUniqueSubEventName(opt_eventName);
  EventBindings.Event.call(this,
                           subEventName,
                           opt_argSchemas,
                           opt_eventOptions,
                           opt_webViewInstanceId);

  // TODO(lazyboy): When do we dispose this listener?
  ContextMenusEvent.addListener(function() {
    // Re-dispatch to subEvent's listeners.
    $Function.apply(this.dispatch, this, $Array.slice(arguments));
  }.bind(this), {instanceId: opt_webViewInstanceId || 0});
}

ContextMenusOnClickedEvent.prototype = {
  __proto__: EventBindings.Event.prototype
};

/**
 * An instance of this class is exposed as <webview>.contextMenus.
 * @constructor
 */
function WebViewContextMenusImpl(viewInstanceId) {
  this.viewInstanceId_ = viewInstanceId;
}

WebViewContextMenusImpl.prototype.create = function() {
  var args = $Array.concat([this.viewInstanceId_], $Array.slice(arguments));
  return $Function.apply(ChromeWebView.contextMenusCreate, null, args);
};

WebViewContextMenusImpl.prototype.remove = function() {
  var args = $Array.concat([this.viewInstanceId_], $Array.slice(arguments));
  return $Function.apply(ChromeWebView.contextMenusRemove, null, args);
};

WebViewContextMenusImpl.prototype.removeAll = function() {
  var args = $Array.concat([this.viewInstanceId_], $Array.slice(arguments));
  return $Function.apply(ChromeWebView.contextMenusRemoveAll, null, args);
};

WebViewContextMenusImpl.prototype.update = function() {
  var args = $Array.concat([this.viewInstanceId_], $Array.slice(arguments));
  return $Function.apply(ChromeWebView.contextMenusUpdate, null, args);
};

var WebViewContextMenus = utils.expose(
    'WebViewContextMenus', WebViewContextMenusImpl,
    { functions: ['create', 'remove', 'removeAll', 'update'] });

/** @private */
WebViewImpl.prototype.maybeHandleContextMenu = function(e, webViewEvent) {
  var requestId = e.requestId;
  // Construct the event.menu object.
  var actionTaken = false;
  var validateCall = function() {
    var ERROR_MSG_CONTEXT_MENU_ACTION_ALREADY_TAKEN = '<webview>: ' +
        'An action has already been taken for this "contextmenu" event.';

    if (actionTaken) {
      throw new Error(ERROR_MSG_CONTEXT_MENU_ACTION_ALREADY_TAKEN);
    }
    actionTaken = true;
  };
  var menu = {
    show: function(items) {
      validateCall();
      // TODO(lazyboy): WebViewShowContextFunction doesn't do anything useful
      // with |items|, implement.
      ChromeWebView.showContextMenu(this.guest.getId(), requestId, items);
    }.bind(this)
  };
  webViewEvent.menu = menu;
  var element = this.element;
  var defaultPrevented = !element.dispatchEvent(webViewEvent);
  if (actionTaken) {
    return;
  }
  if (!defaultPrevented) {
    actionTaken = true;
    // The default action is equivalent to just showing the context menu as is.
    ChromeWebView.showContextMenu(this.guest.getId(), requestId, undefined);

    // TODO(lazyboy): Figure out a way to show warning message only when
    // listeners are registered for this event.
  } //  else we will ignore showing the context menu completely.
};

/** @private */
WebViewImpl.prototype.setupExperimentalContextMenus = function() {
  var createContextMenus = function() {
    return function() {
      if (this.contextMenus_) {
        return this.contextMenus_;
      }

      this.contextMenus_ = new WebViewContextMenus(this.viewInstanceId);

      // Define 'onClicked' event property on |this.contextMenus_|.
      var getOnClickedEvent = function() {
        return function() {
          if (!this.contextMenusOnClickedEvent_) {
            var eventName = 'chromeWebViewInternal.onClicked';
            // TODO(lazyboy): Find event by name instead of events[0].
            var eventSchema = ChromeWebViewSchema.events[0];
            var eventOptions = {supportsListeners: true};
            var onClickedEvent = new ContextMenusOnClickedEvent(
                eventName, eventSchema, eventOptions, this.viewInstanceId);
            this.contextMenusOnClickedEvent_ = onClickedEvent;
            return onClickedEvent;
          }
          return this.contextMenusOnClickedEvent_;
        }.bind(this);
      }.bind(this);
      Object.defineProperty(
          this.contextMenus_,
          'onClicked',
          {get: getOnClickedEvent(), enumerable: true});

      return this.contextMenus_;
    }.bind(this);
  }.bind(this);

  // Expose <webview>.contextMenus object.
  Object.defineProperty(
      this.element,
      'contextMenus',
      {
        get: createContextMenus(),
        enumerable: true
      });
};

WebViewImpl.prototype.maybeSetupExperimentalChromeWebViewEvents = function(
    request) {
  var createDeclarativeContentEvent = function(declarativeContentEvent) {
    return function() {
      if (!this[declarativeContentEvent.name]) {
        this[declarativeContentEvent.name] =
            new DeclarativeContentEvent(
                'webViewInternal.declarativeContent.' +
                declarativeContentEvent.name,
                declarativeContentEvent.parameters,
                declarativeContentEvent.options,
                this.viewInstanceId);
      }
      return this[declarativeContentEvent.name];
    }.bind(this);
  }.bind(this);

  for (var i = 0; i < DeclarativeContentSchema.events.length; ++i) {
    var eventSchema = DeclarativeContentSchema.events[i];
    var declarativeContentEvent = createDeclarativeContentEvent(eventSchema);
    Object.defineProperty(
        request,
        eventSchema.name,
        {
          get: declarativeContentEvent,
          enumerable: true
        }
    );
  }
  return request;
};
