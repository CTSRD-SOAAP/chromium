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
