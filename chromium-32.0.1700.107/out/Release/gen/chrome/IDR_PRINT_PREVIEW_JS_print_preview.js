// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(rltoscano): Move data/* into print_preview.data namespace

var localStrings = new LocalStrings(templateData);

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Class that represents a UI component.
   * @constructor
   * @extends {cr.EventTarget}
   */
  function Component() {
    cr.EventTarget.call(this);

    /**
     * Component's HTML element.
     * @type {Element}
     * @private
     */
    this.element_ = null;

    this.isInDocument_ = false;

    /**
     * Component's event tracker.
     * @type {EventTracker}
     * @private
     */
     this.tracker_ = new EventTracker();

    /**
     * Child components of the component.
     * @type {Array.<print_preview.Component>}
     * @private
     */
    this.children_ = [];
  };

  Component.prototype = {
    __proto__: cr.EventTarget.prototype,

    /** Gets the component's element. */
    getElement: function() {
      return this.element_;
    },

    /** @return {EventTracker} Component's event tracker. */
    get tracker() {
      return this.tracker_;
    },

    /**
     * @return {boolean} Whether the element of the component is already in the
     *     HTML document.
     */
    get isInDocument() {
      return this.isInDocument_;
    },

    /**
     * Creates the root element of the component. Sub-classes should override
     * this method.
     */
    createDom: function() {
      this.element_ = cr.doc.createElement('div');
    },

    /**
     * Called when the component's element is known to be in the document.
     * Anything using document.getElementById etc. should be done at this stage.
     * Sub-classes should extend this method and attach listeners.
     */
    enterDocument: function() {
      this.isInDocument_ = true;
      this.children_.forEach(function(child) {
        if (!child.isInDocument && child.getElement()) {
          child.enterDocument();
        }
      });
    },

    /** Removes all event listeners. */
    exitDocument: function() {
      this.children_.forEach(function(child) {
        if (child.isInDocument) {
          child.exitDocument();
        }
      });
      this.tracker_.removeAll();
      this.isInDocument_ = false;
    },

    /**
     * Renders this UI component and appends the element to the given parent
     * element.
     * @param {!Element} parentElement Element to render the component's
     *     element into.
     */
    render: function(parentElement) {
      assert(!this.isInDocument, 'Component is already in the document');
      if (!this.element_) {
        this.createDom();
      }
      parentElement.appendChild(this.element_);
      this.enterDocument();
    },

    /**
     * Decorates an existing DOM element. Sub-classes should override the
     * override the decorateInternal method.
     * @param {Element} element Element to decorate.
     */
    decorate: function(element) {
      assert(!this.isInDocument, 'Component is already in the document');
      this.setElementInternal(element);
      this.decorateInternal();
      this.enterDocument();
    },

    /**
     * @param {print_preview.Component} child Component to add as a child of
     *     this component.
     */
    addChild: function(child) {
      this.children_.push(child);
    },

    /**
     * @param {!print_preview.Component} child Component to remove from this
     *     component's children.
     */
    removeChild: function(child) {
      var childIdx = this.children_.indexOf(child);
      if (childIdx != -1) {
        this.children_.splice(childIdx, 1);
      }
      if (child.isInDocument) {
        child.exitDocument();
        if (child.getElement()) {
          child.getElement().parentNode.removeChild(child.getElement());
        }
      }
    },

    /** Removes all of the component's children. */
    removeChildren: function() {
      while (this.children_.length > 0) {
        this.removeChild(this.children_[0]);
      }
    },

    /**
     * @param {string} query Selector query to select an element starting from
     *     the component's root element using a depth first search for the first
     *     element that matches the query.
     * @return {HTMLElement} Element selected by the given query.
     */
    getChildElement: function(query) {
      return this.element_.querySelector(query);
    },

    /**
     * Sets the component's element.
     * @param {Element} element HTML element to set as the component's element.
     * @protected
     */
    setElementInternal: function(element) {
      this.element_ = element;
    },

    /**
     * Decorates the given element for use as the element of the component.
     * @protected
     */
    decorateInternal: function() { /*abstract*/ },

    /**
     * Clones a template HTML DOM tree.
     * @param {string} templateId Template element ID.
     * @param {boolean=} opt_keepHidden Whether to leave the cloned template
     *     hidden after cloning.
     * @return {Element} Cloned element with its 'id' attribute stripped.
     * @protected
     */
    cloneTemplateInternal: function(templateId, opt_keepHidden) {
      var templateEl = $(templateId);
      assert(templateEl != null,
             'Could not find element with ID: ' + templateId);
      var el = templateEl.cloneNode(true);
      el.id = '';
      if (!opt_keepHidden) {
        setIsVisible(el, true);
      }
      return el;
    }
  };

  return {
    Component: Component
  };
});


cr.define('print_preview', function() {
  'use strict';

  /**
   * Container class for Chromium's print preview.
   * @constructor
   * @extends {print_preview.Component}
   */
  function PrintPreview() {
    print_preview.Component.call(this);

    /**
     * Used to communicate with Chromium's print system.
     * @type {!print_preview.NativeLayer}
     * @private
     */
    this.nativeLayer_ = new print_preview.NativeLayer();

    /**
     * Event target that contains information about the logged in user.
     * @type {!print_preview.UserInfo}
     * @private
     */
    this.userInfo_ = new print_preview.UserInfo();

    /**
     * Metrics object used to report usage statistics.
     * @type {!print_preview.Metrics}
     * @private
     */
    this.metrics_ = new print_preview.Metrics();

    /**
     * Application state.
     * @type {!print_preview.AppState}
     * @private
     */
    this.appState_ = new print_preview.AppState();

    /**
     * Data model that holds information about the document to print.
     * @type {!print_preview.DocumentInfo}
     * @private
     */
    this.documentInfo_ = new print_preview.DocumentInfo();

    /**
     * Data store which holds print destinations.
     * @type {!print_preview.DestinationStore}
     * @private
     */
    this.destinationStore_ = new print_preview.DestinationStore(
        this.nativeLayer_, this.appState_);

    /**
     * Storage of the print ticket used to create the print job.
     * @type {!print_preview.PrintTicketStore}
     * @private
     */
    this.printTicketStore_ = new print_preview.PrintTicketStore(
        this.destinationStore_, this.appState_, this.documentInfo_);

    /**
     * Holds the print and cancel buttons and renders some document statistics.
     * @type {!print_preview.PrintHeader}
     * @private
     */
    this.printHeader_ = new print_preview.PrintHeader(
        this.printTicketStore_, this.destinationStore_);
    this.addChild(this.printHeader_);

    /**
     * Component used to search for print destinations.
     * @type {!print_preview.DestinationSearch}
     * @private
     */
    this.destinationSearch_ = new print_preview.DestinationSearch(
        this.destinationStore_, this.userInfo_, this.metrics_);
    this.addChild(this.destinationSearch_);

    /**
     * Component that renders the print destination.
     * @type {!print_preview.DestinationSettings}
     * @private
     */
    this.destinationSettings_ = new print_preview.DestinationSettings(
        this.destinationStore_);
    this.addChild(this.destinationSettings_);

    /**
     * Component that renders UI for entering in page range.
     * @type {!print_preview.PageSettings}
     * @private
     */
    this.pageSettings_ = new print_preview.PageSettings(
        this.printTicketStore_.pageRange);
    this.addChild(this.pageSettings_);

    /**
     * Component that renders the copies settings.
     * @type {!print_preview.CopiesSettings}
     * @private
     */
    this.copiesSettings_ = new print_preview.CopiesSettings(
        this.printTicketStore_.copies, this.printTicketStore_.collate);
    this.addChild(this.copiesSettings_);

    /**
     * Component that renders the layout settings.
     * @type {!print_preview.LayoutSettings}
     * @private
     */
    this.layoutSettings_ =
        new print_preview.LayoutSettings(this.printTicketStore_.landscape);
    this.addChild(this.layoutSettings_);

    /**
     * Component that renders the color options.
     * @type {!print_preview.ColorSettings}
     * @private
     */
    this.colorSettings_ =
        new print_preview.ColorSettings(this.printTicketStore_.color);
    this.addChild(this.colorSettings_);

    /**
     * Component that renders a select box for choosing margin settings.
     * @type {!print_preview.MarginSettings}
     * @private
     */
    this.marginSettings_ =
        new print_preview.MarginSettings(this.printTicketStore_.marginsType);
    this.addChild(this.marginSettings_);

    /**
     * Component that renders miscellaneous print options.
     * @type {!print_preview.OtherOptionsSettings}
     * @private
     */
    this.otherOptionsSettings_ = new print_preview.OtherOptionsSettings(
        this.printTicketStore_.duplex,
        this.printTicketStore_.fitToPage,
        this.printTicketStore_.cssBackground,
        this.printTicketStore_.selectionOnly,
        this.printTicketStore_.headerFooter);
    this.addChild(this.otherOptionsSettings_);

    /**
     * Area of the UI that holds the print preview.
     * @type {!print_preview.PreviewArea}
     * @private
     */
    this.previewArea_ = new print_preview.PreviewArea(this.destinationStore_,
                                                      this.printTicketStore_,
                                                      this.nativeLayer_,
                                                      this.documentInfo_);
    this.addChild(this.previewArea_);

    /**
     * Interface to the Google Cloud Print API. Null if Google Cloud Print
     * integration is disabled.
     * @type {cloudprint.CloudPrintInterface}
     * @private
     */
    this.cloudPrintInterface_ = null;

    /**
     * Whether in kiosk mode where print preview can print automatically without
     * user intervention. See http://crbug.com/31395. Print will start when
     * both the print ticket has been initialized, and an initial printer has
     * been selected.
     * @type {boolean}
     * @private
     */
    this.isInKioskAutoPrintMode_ = false;

    /**
     * State of the print preview UI.
     * @type {print_preview.PrintPreview.UiState_}
     * @private
     */
    this.uiState_ = PrintPreview.UiState_.INITIALIZING;

    /**
     * Whether document preview generation is in progress.
     * @type {boolean}
     * @private
     */
    this.isPreviewGenerationInProgress_ = true;
  };

  /**
   * States of the print preview.
   * @enum {string}
   * @private
   */
  PrintPreview.UiState_ = {
    INITIALIZING: 'initializing',
    READY: 'ready',
    OPENING_PDF_PREVIEW: 'opening-pdf-preview',
    OPENING_NATIVE_PRINT_DIALOG: 'opening-native-print-dialog',
    PRINTING: 'printing',
    FILE_SELECTION: 'file-selection',
    CLOSING: 'closing',
    ERROR: 'error'
  };

  PrintPreview.prototype = {
    __proto__: print_preview.Component.prototype,

    /** Sets up the page and print preview by getting the printer list. */
    initialize: function() {
      this.decorate($('print-preview'));
      i18nTemplate.process(document, templateData);
      if (!this.previewArea_.hasCompatiblePlugin) {
        this.setIsEnabled_(false);
      }
      this.nativeLayer_.startGetInitialSettings();
      this.destinationStore_.startLoadLocalDestinations();
      cr.ui.FocusOutlineManager.forDocument(document);
    },

    /** @override */
    enterDocument: function() {
      // Native layer events.
      this.tracker.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET,
          this.onInitialSettingsSet_.bind(this));
      this.tracker.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.CLOUD_PRINT_ENABLE,
          this.onCloudPrintEnable_.bind(this));
      this.tracker.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.PRINT_TO_CLOUD,
          this.onPrintToCloud_.bind(this));
      this.tracker.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.FILE_SELECTION_CANCEL,
          this.onFileSelectionCancel_.bind(this));
      this.tracker.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.FILE_SELECTION_COMPLETE,
          this.onFileSelectionComplete_.bind(this));
      this.tracker.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.SETTINGS_INVALID,
          this.onSettingsInvalid_.bind(this));
      this.tracker.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.DISABLE_SCALING,
          this.onDisableScaling_.bind(this));

      this.tracker.add(
          $('system-dialog-link'),
          'click',
          this.openSystemPrintDialog_.bind(this));
      this.tracker.add(
          $('cloud-print-dialog-link'),
          'click',
          this.onCloudPrintDialogLinkClick_.bind(this));
      this.tracker.add(
          $('open-pdf-in-preview-link'),
          'click',
          this.onOpenPdfInPreviewLinkClick_.bind(this));

      this.tracker.add(
          this.previewArea_,
          print_preview.PreviewArea.EventType.PREVIEW_GENERATION_IN_PROGRESS,
          this.onPreviewGenerationInProgress_.bind(this));
      this.tracker.add(
          this.previewArea_,
          print_preview.PreviewArea.EventType.PREVIEW_GENERATION_DONE,
          this.onPreviewGenerationDone_.bind(this));
      this.tracker.add(
          this.previewArea_,
          print_preview.PreviewArea.EventType.PREVIEW_GENERATION_FAIL,
          this.onPreviewGenerationFail_.bind(this));
      this.tracker.add(
          this.previewArea_,
          print_preview.PreviewArea.EventType.OPEN_SYSTEM_DIALOG_CLICK,
          this.openSystemPrintDialog_.bind(this));

      this.tracker.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.
              SELECTED_DESTINATION_CAPABILITIES_READY,
          this.printIfReady_.bind(this));
      this.tracker.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.DESTINATION_SELECT,
          this.onDestinationSelect_.bind(this));
      this.tracker.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.DESTINATION_SEARCH_DONE,
          this.onDestinationSearchDone_.bind(this));

      this.tracker.add(
          this.printHeader_,
          print_preview.PrintHeader.EventType.PRINT_BUTTON_CLICK,
          this.onPrintButtonClick_.bind(this));
      this.tracker.add(
          this.printHeader_,
          print_preview.PrintHeader.EventType.CANCEL_BUTTON_CLICK,
          this.onCancelButtonClick_.bind(this));

      this.tracker.add(window, 'keydown', this.onKeyDown_.bind(this));

      this.tracker.add(
          this.destinationSettings_,
          print_preview.DestinationSettings.EventType.CHANGE_BUTTON_ACTIVATE,
          this.onDestinationChangeButtonActivate_.bind(this));

      this.tracker.add(
          this.destinationSearch_,
          print_preview.DestinationSearch.EventType.MANAGE_CLOUD_DESTINATIONS,
          this.onManageCloudDestinationsActivated_.bind(this));
      this.tracker.add(
          this.destinationSearch_,
          print_preview.DestinationSearch.EventType.MANAGE_LOCAL_DESTINATIONS,
          this.onManageLocalDestinationsActivated_.bind(this));
      this.tracker.add(
          this.destinationSearch_,
          print_preview.DestinationSearch.EventType.SIGN_IN,
          this.onCloudPrintSignInActivated_.bind(this));

      // TODO(rltoscano): Move no-destinations-promo into its own component
      // instead being part of PrintPreview.
      this.tracker.add(
          this.getChildElement('#no-destinations-promo .close-button'),
          'click',
          this.onNoDestinationsPromoClose_.bind(this));
      this.tracker.add(
          this.getChildElement('#no-destinations-promo .not-now-button'),
          'click',
          this.onNoDestinationsPromoClose_.bind(this));
      this.tracker.add(
          this.getChildElement('#no-destinations-promo .add-printer-button'),
          'click',
          this.onNoDestinationsPromoClick_.bind(this));
    },

    /** @override */
    decorateInternal: function() {
      this.printHeader_.decorate($('print-header'));
      this.destinationSearch_.decorate($('destination-search'));
      this.destinationSettings_.decorate($('destination-settings'));
      this.pageSettings_.decorate($('page-settings'));
      this.copiesSettings_.decorate($('copies-settings'));
      this.layoutSettings_.decorate($('layout-settings'));
      this.colorSettings_.decorate($('color-settings'));
      this.marginSettings_.decorate($('margin-settings'));
      this.otherOptionsSettings_.decorate($('other-options-settings'));
      this.previewArea_.decorate($('preview-area'));

      setIsVisible($('open-pdf-in-preview-link'), cr.isMac);
    },

    /**
     * Sets whether the controls in the print preview are enabled.
     * @param {boolean} isEnabled Whether the controls in the print preview are
     *     enabled.
     * @private
     */
    setIsEnabled_: function(isEnabled) {
      $('system-dialog-link').disabled = !isEnabled;
      $('cloud-print-dialog-link').disabled = !isEnabled;
      $('open-pdf-in-preview-link').disabled = !isEnabled;
      this.printHeader_.isEnabled = isEnabled;
      this.destinationSettings_.isEnabled = isEnabled;
      this.pageSettings_.isEnabled = isEnabled;
      this.copiesSettings_.isEnabled = isEnabled;
      this.layoutSettings_.isEnabled = isEnabled;
      this.colorSettings_.isEnabled = isEnabled;
      this.marginSettings_.isEnabled = isEnabled;
      this.otherOptionsSettings_.isEnabled = isEnabled;
    },

    /**
     * Prints the document or launches a pdf preview on the local system.
     * @param {boolean} isPdfPreview Whether to launch the pdf preview.
     * @private
     */
    printDocumentOrOpenPdfPreview_: function(isPdfPreview) {
      assert(this.uiState_ == PrintPreview.UiState_.READY,
             'Print document request received when not in ready state: ' +
                 this.uiState_);
      if (isPdfPreview) {
        this.uiState_ = PrintPreview.UiState_.OPENING_PDF_PREVIEW;
      } else if (this.destinationStore_.selectedDestination.id ==
          print_preview.Destination.GooglePromotedId.SAVE_AS_PDF) {
        this.uiState_ = PrintPreview.UiState_.FILE_SELECTION;
      } else {
        this.uiState_ = PrintPreview.UiState_.PRINTING;
      }
      this.setIsEnabled_(false);
      if (this.printIfReady_() &&
          ((this.destinationStore_.selectedDestination.isLocal &&
            this.destinationStore_.selectedDestination.id !=
                print_preview.Destination.GooglePromotedId.SAVE_AS_PDF) ||
           this.uiState_ == PrintPreview.UiState_.OPENING_PDF_PREVIEW)) {
        // Hide the dialog for now. The actual print command will be issued when
        // the preview generation is done.
        this.nativeLayer_.startHideDialog();
      }
    },

    /**
     * Attempts to print if needed and if ready.
     * @return {boolean} Whether a print request was issued.
     * @private
     */
    printIfReady_: function() {
      if ((this.uiState_ == PrintPreview.UiState_.PRINTING ||
              this.uiState_ == PrintPreview.UiState_.OPENING_PDF_PREVIEW ||
              this.uiState_ == PrintPreview.UiState_.FILE_SELECTION ||
              this.isInKioskAutoPrintMode_) &&
          !this.isPreviewGenerationInProgress_ &&
          this.destinationStore_.selectedDestination &&
          this.destinationStore_.selectedDestination.capabilities) {
        assert(this.printTicketStore_.isTicketValid(),
               'Trying to print with invalid ticket');
        this.nativeLayer_.startPrint(
            this.destinationStore_.selectedDestination,
            this.printTicketStore_,
            this.cloudPrintInterface_,
            this.documentInfo_,
            this.uiState_ == PrintPreview.UiState_.OPENING_PDF_PREVIEW);
        return true;
      } else {
        return false;
      }
    },

    /**
     * Closes the print preview.
     * @private
     */
    close_: function() {
      this.exitDocument();
      this.uiState_ = PrintPreview.UiState_.CLOSING;
      this.nativeLayer_.startCloseDialog();
    },

    /**
     * Opens the native system print dialog after disabling all controls.
     * @private
     */
    openSystemPrintDialog_: function() {
      setIsVisible($('system-dialog-throbber'), true);
      this.setIsEnabled_(false);
      this.uiState_ = PrintPreview.UiState_.OPENING_NATIVE_PRINT_DIALOG;
      this.nativeLayer_.startShowSystemDialog();
    },

    /**
     * Called when the native layer has initial settings to set. Sets the
     * initial settings of the print preview and begins fetching print
     * destinations.
     * @param {Event} event Contains the initial print preview settings
     *     persisted through the session.
     * @private
     */
    onInitialSettingsSet_: function(event) {
      assert(this.uiState_ == PrintPreview.UiState_.INITIALIZING,
             'Updating initial settings when not in initializing state: ' +
                 this.uiState_);
      this.uiState_ = PrintPreview.UiState_.READY;

      var settings = event.initialSettings;
      this.isInKioskAutoPrintMode_ = settings.isInKioskAutoPrintMode;

      // The following components must be initialized in this order.
      this.appState_.init(settings.serializedAppStateStr);
      this.documentInfo_.init(
          settings.isDocumentModifiable,
          settings.documentTitle,
          settings.documentHasSelection);
      this.printTicketStore_.init(
          settings.thousandsDelimeter,
          settings.decimalDelimeter,
          settings.unitType,
          settings.selectionOnly);
      this.destinationStore_.init(settings.systemDefaultDestinationId);
      this.appState_.setInitialized();

      setIsVisible($('system-dialog-link'),
                   !settings.hidePrintWithSystemDialogLink);
    },

    /**
     * Calls when the native layer enables Google Cloud Print integration.
     * Fetches the user's cloud printers.
     * @param {Event} event Contains the base URL of the Google Cloud Print
     *     service.
     * @private
     */
    onCloudPrintEnable_: function(event) {
      this.cloudPrintInterface_ =
          new cloudprint.CloudPrintInterface(event.baseCloudPrintUrl,
                                             this.nativeLayer_);
      this.tracker.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterface.EventType.SUBMIT_DONE,
          this.onCloudPrintSubmitDone_.bind(this));
      this.tracker.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterface.EventType.SEARCH_FAILED,
          this.onCloudPrintError_.bind(this));
      this.tracker.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterface.EventType.SUBMIT_FAILED,
          this.onCloudPrintError_.bind(this));
      this.tracker.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterface.EventType.PRINTER_FAILED,
          this.onCloudPrintError_.bind(this));
      this.tracker.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterface.EventType.
              UPDATE_PRINTER_TOS_ACCEPTANCE_FAILED,
          this.onCloudPrintError_.bind(this));

      this.userInfo_.setCloudPrintInterface(this.cloudPrintInterface_);
      this.destinationStore_.setCloudPrintInterface(this.cloudPrintInterface_);
      this.destinationStore_.startLoadCloudDestinations(true);
      if (this.destinationSearch_.getIsVisible()) {
        this.destinationStore_.startLoadCloudDestinations(false);
      }
    },

    /**
     * Called from the native layer when ready to print to Google Cloud Print.
     * @param {Event} event Contains the body to send in the HTTP request.
     * @private
     */
    onPrintToCloud_: function(event) {
      assert(this.uiState_ == PrintPreview.UiState_.PRINTING,
             'Document ready to be sent to the cloud when not in printing ' +
                 'state: ' + this.uiState_);
      assert(this.cloudPrintInterface_ != null,
             'Google Cloud Print is not enabled');
      this.cloudPrintInterface_.submit(
          this.destinationStore_.selectedDestination,
          this.printTicketStore_,
          this.documentInfo_,
          event.data);
    },

    /**
     * Called from the native layer when the user cancels the save-to-pdf file
     * selection dialog.
     * @private
     */
    onFileSelectionCancel_: function() {
      assert(this.uiState_ == PrintPreview.UiState_.FILE_SELECTION,
             'File selection cancelled when not in file-selection state: ' +
                 this.uiState_);
      this.setIsEnabled_(true);
      this.uiState_ = PrintPreview.UiState_.READY;
    },

    /**
     * Called from the native layer when save-to-pdf file selection is complete.
     * @private
     */
    onFileSelectionComplete_: function() {
      assert(this.uiState_ == PrintPreview.UiState_.FILE_SELECTION,
             'File selection completed when not in file-selection state: ' +
                 this.uiState_);
      this.previewArea_.showCustomMessage(
          localStrings.getString('printingToPDFInProgress'));
      this.uiState_ = PrintPreview.UiState_.PRINTING;
    },

    /**
     * Called after successfully submitting a job to Google Cloud Print.
     * @param {!Event} event Contains the ID of the submitted print job.
     * @private
     */
    onCloudPrintSubmitDone_: function(event) {
      assert(this.uiState_ == PrintPreview.UiState_.PRINTING,
             'Submited job to Google Cloud Print but not in printing state ' +
                 this.uiState_);
      if (this.destinationStore_.selectedDestination.id ==
              print_preview.Destination.GooglePromotedId.FEDEX) {
        this.nativeLayer_.startForceOpenNewTab(
            'https://www.google.com/cloudprint/fedexcode.html?jobid=' +
            event.jobId);
      }
      this.close_();
    },

    /**
     * Called when there was an error communicating with Google Cloud print.
     * Displays an error message in the print header.
     * @param {!Event} event Contains the error message.
     * @private
     */
    onCloudPrintError_: function(event) {
      if (event.status == 403) {
        this.destinationSearch_.showCloudPrintPromo();
      } else if (event.status == 0) {
        return; // Ignore, the system does not have internet connectivity.
      } else {
        this.printHeader_.setErrorMessage(event.message);
      }
      if (event.status == 200) {
        console.error('Google Cloud Print Error: (' + event.errorCode + ') ' +
                      event.message);
      } else {
        console.error('Google Cloud Print Error: HTTP status ' + event.status);
      }
    },

    /**
     * Called when the preview area's preview generation is in progress.
     * @private
     */
    onPreviewGenerationInProgress_: function() {
      this.isPreviewGenerationInProgress_ = true;
    },

    /**
     * Called when the preview area's preview generation is complete.
     * @private
     */
    onPreviewGenerationDone_: function() {
      this.isPreviewGenerationInProgress_ = false;
      this.printHeader_.isPrintButtonEnabled = true;
      this.printIfReady_();
    },

    /**
     * Called when the preview area's preview failed to load.
     * @private
     */
    onPreviewGenerationFail_: function() {
      this.isPreviewGenerationInProgress_ = false;
      this.printHeader_.isPrintButtonEnabled = false;
      if (this.uiState_ == PrintPreview.UiState_.PRINTING) {
        this.nativeLayer_.startCancelPendingPrint();
      }
    },

    /**
     * Called when the 'Open pdf in preview' link is clicked. Launches the pdf
     * preview app.
     * @private
     */
    onOpenPdfInPreviewLinkClick_: function() {
      assert(this.uiState_ == PrintPreview.UiState_.READY,
             'Trying to open pdf in preview when not in ready state: ' +
                 this.uiState_);
      setIsVisible($('open-preview-app-throbber'), true);
      this.previewArea_.showCustomMessage(
          localStrings.getString('openingPDFInPreview'));
      this.printDocumentOrOpenPdfPreview_(true /*isPdfPreview*/);
    },

    /**
     * Called when the print header's print button is clicked. Prints the
     * document.
     * @private
     */
    onPrintButtonClick_: function() {
      assert(this.uiState_ == PrintPreview.UiState_.READY,
             'Trying to print when not in ready state: ' + this.uiState_);
      this.printDocumentOrOpenPdfPreview_(false /*isPdfPreview*/);
    },

    /**
     * Called when the print header's cancel button is clicked. Closes the
     * print dialog.
     * @private
     */
    onCancelButtonClick_: function() {
      this.close_();
    },

    /**
     * Consume escape key presses and ctrl + shift + p. Delegate everything else
     * to the preview area.
     * @param {KeyboardEvent} e The keyboard event.
     * @private
     */
    onKeyDown_: function(e) {
      // Escape key closes the dialog.
      if (e.keyCode == 27 && !e.shiftKey && !e.ctrlKey && !e.altKey &&
          !e.metaKey) {
        if (this.destinationSearch_.getIsVisible()) {
          this.destinationSearch_.setIsVisible(false);
          this.metrics_.incrementDestinationSearchBucket(
              print_preview.Metrics.DestinationSearchBucket.CANCELED);
        } else {
          // 
          // // Dummy comment to absorb previous line's comment symbol.
          this.close_();
          //
        }
        e.preventDefault();
        return;
      }

      // Ctrl + Shift + p / Mac equivalent.
      if (e.keyCode == 80) {
        if ((cr.isMac && e.metaKey && e.altKey && !e.shiftKey && !e.ctrlKey) ||
            (!cr.isMac && e.shiftKey && e.ctrlKey && !e.altKey && !e.metaKey)) {
          this.openSystemPrintDialog_();
          e.preventDefault();
          return;
        }
      }

      if (e.keyCode == 13 /*enter*/ &&
          !this.destinationSearch_.getIsVisible() &&
          this.printTicketStore_.isTicketValid()) {
        assert(this.uiState_ == PrintPreview.UiState_.READY,
          'Trying to print when not in ready state: ' + this.uiState_);
        this.printDocumentOrOpenPdfPreview_(false /*isPdfPreview*/);
        e.preventDefault();
        return;
      }

      // Pass certain directional keyboard events to the PDF viewer.
      this.previewArea_.handleDirectionalKeyEvent(e);
    },

    /**
     * Called when native layer receives invalid settings for a print request.
     * @private
     */
    onSettingsInvalid_: function() {
      this.uiState_ = PrintPreview.UiState_.ERROR;
      console.error('Invalid settings error reported from native layer');
      this.previewArea_.showCustomMessage(
          localStrings.getString('invalidPrinterSettings'));
    },

    /**
     * Called when the destination settings' change button is activated.
     * Displays the destination search component.
     * @private
     */
    onDestinationChangeButtonActivate_: function() {
      this.destinationSearch_.setIsVisible(true);
      this.destinationStore_.startLoadCloudDestinations(false);
      this.metrics_.incrementDestinationSearchBucket(
          print_preview.Metrics.DestinationSearchBucket.SHOWN);
    },

    /**
     * Called when the destination search dispatches manage cloud destinations
     * event. Calls corresponding native layer method.
     * @private
     */
    onManageCloudDestinationsActivated_: function() {
      this.nativeLayer_.startManageCloudDestinations();
    },

    /**
     * Called when the destination search dispatches manage local destinations
     * event. Calls corresponding native layer method.
     * @private
     */
    onManageLocalDestinationsActivated_: function() {
      this.nativeLayer_.startManageLocalDestinations();
    },

    /**
     * Called when the user wants to sign in to Google Cloud Print. Calls the
     * corresponding native layer event.
     * @private
     */
    onCloudPrintSignInActivated_: function() {
      this.nativeLayer_.startCloudPrintSignIn();
    },

    /**
     * Called when the native layer dispatches a DISABLE_SCALING event. Resets
     * fit-to-page selection and updates document info.
     * @private
     */
    onDisableScaling_: function() {
      this.printTicketStore_.fitToPage.updateValue(null);
      this.documentInfo_.updateIsScalingDisabled(true);
    },

    /**
     * Called when the open-cloud-print-dialog link is clicked. Opens the Google
     * Cloud Print web dialog.
     * @private
     */
    onCloudPrintDialogLinkClick_: function() {
      assert(this.uiState_ == PrintPreview.UiState_.READY,
             'Opening Google Cloud Print dialog when not in ready state: ' +
                 this.uiState_);
      setIsVisible($('cloud-print-dialog-throbber'), true);
      this.setIsEnabled_(false);
      this.uiState_ = PrintPreview.UiState_.OPENING_NATIVE_PRINT_DIALOG;
      this.nativeLayer_.startShowCloudPrintDialog(
          this.printTicketStore_.pageRange.getPageNumberSet().size);
    },

    /**
     * Called when a print destination is selected. Shows/hides the "Print with
     * Cloud Print" link in the navbar.
     * @private
     */
    onDestinationSelect_: function() {
      var selectedDest = this.destinationStore_.selectedDestination;
      setIsVisible($('cloud-print-dialog-link'),
                   !cr.isChromeOS && !selectedDest.isLocal);
    },

    /**
     * Called when the destination store loads a group of destinations. Shows
     * a promo on Chrome OS if the user has no print destinations promoting
     * Google Cloud Print.
     * @private
     */
    onDestinationSearchDone_: function() {
      var isPromoVisible = cr.isChromeOS &&
          this.cloudPrintInterface_ &&
          this.userInfo_.getUserEmail() &&
          !this.appState_.isGcpPromoDismissed &&
          !this.destinationStore_.isLocalDestinationsSearchInProgress &&
          !this.destinationStore_.isCloudDestinationsSearchInProgress &&
          this.destinationStore_.hasOnlyDefaultCloudDestinations();
      setIsVisible(this.getChildElement('#no-destinations-promo'),
                   isPromoVisible);
      if (isPromoVisible) {
        this.metrics_.incrementGcpPromoBucket(
            print_preview.Metrics.GcpPromoBucket.SHOWN);
      }
    },

    /**
     * Called when the close button on the no-destinations-promotion is clicked.
     * Hides the promotion.
     * @private
     */
    onNoDestinationsPromoClose_: function() {
      this.metrics_.incrementGcpPromoBucket(
          print_preview.Metrics.GcpPromoBucket.DISMISSED);
      setIsVisible(this.getChildElement('#no-destinations-promo'), false);
      this.appState_.persistIsGcpPromoDismissed(true);
    },

    /**
     * Called when the no-destinations promotion link is clicked. Opens the
     * Google Cloud Print management page and closes the print preview.
     * @private
     */
    onNoDestinationsPromoClick_: function() {
      this.metrics_.incrementGcpPromoBucket(
          print_preview.Metrics.GcpPromoBucket.CLICKED);
      this.appState_.persistIsGcpPromoDismissed(true);
      window.open(this.cloudPrintInterface_.baseUrl + '?user=' +
                  this.userInfo_.getUserEmail() + '#printers');
      this.close_();
    }
  };

  // Export
  return {
    PrintPreview: PrintPreview
  };
});

// Pull in all other scripts in a single shot.
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * An immutable ordered set of page numbers.
   * @param {!Array.<number>} pageNumberList A list of page numbers to include
   *     in the set.
   * @constructor
   */
  function PageNumberSet(pageNumberList) {
    /**
     * Internal data store for the page number set.
     * @type {!Array.<number>}
     * @private
     */
    this.pageNumberSet_ = pageListToPageSet(pageNumberList);
  };

  PageNumberSet.prototype = {
    /** @return {number} The number of page numbers in the set. */
    get size() {
      return this.pageNumberSet_.length;
    },

    /**
     * @param {number} index 0-based index of the page number to get.
     * @return {number} Page number at the given index.
     */
    getPageNumberAt: function(index) {
      return this.pageNumberSet_[index];
    },

    /**
     * @param {number} 1-based page number to check for.
     * @return {boolean} Whether the given page number is in the page range.
     */
    hasPageNumber: function(pageNumber) {
      return arrayContains(this.pageNumberSet_, pageNumber);
    },

    /**
     * @param {number} 1-based number of the page to get index of.
     * @return {number} 0-based index of the given page number with respect to
     *     all of the pages in the page range.
     */
    getPageNumberIndex: function(pageNumber) {
      return this.pageNumberSet_.indexOf(pageNumber);
    },

    /** @return {!Array.<number>} Array representation of the set. */
    asArray: function() {
      return this.pageNumberSet_.slice(0);
    },
  };

  // Export
  return {
    PageNumberSet: PageNumberSet
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Print destination data object that holds data for both local and cloud
   * destinations.
   * @param {string} id ID of the destination.
   * @param {!print_preview.Destination.Type} type Type of the destination.
   * @param {!print_preview.Destination.Origin} origin Origin of the
   *     destination.
   * @param {string} displayName Display name of the destination.
   * @param {boolean} isRecent Whether the destination has been used recently.
   * @param {!print_preview.Destination.ConnectionStatus} connectionStatus
   *     Connection status of the print destination.
   * @param {{tags: Array.<string>,
   *          isOwned: ?boolean,
   *          lastAccessTime: ?number,
   *          isTosAccepted: ?boolean}=} opt_params Optional parameters for the
   *     destination.
   * @constructor
   */
  function Destination(id, type, origin, displayName, isRecent,
                       connectionStatus, opt_params) {
    /**
     * ID of the destination.
     * @type {string}
     * @private
     */
    this.id_ = id;

    /**
     * Type of the destination.
     * @type {!print_preview.Destination.Type}
     * @private
     */
    this.type_ = type;

    /**
     * Origin of the destination.
     * @type {!print_preview.Destination.Origin}
     * @private
     */
    this.origin_ = origin;

    /**
     * Display name of the destination.
     * @type {string}
     * @private
     */
    this.displayName_ = displayName;

    /**
     * Whether the destination has been used recently.
     * @type {boolean}
     * @private
     */
    this.isRecent_ = isRecent;

    /**
     * Tags associated with the destination.
     * @type {!Array.<string>}
     * @private
     */
    this.tags_ = (opt_params && opt_params.tags) || [];

    /**
     * Print capabilities of the destination.
     * @type {print_preview.Cdd}
     * @private
     */
    this.capabilities_ = null;

    /**
     * Whether the destination is owned by the user.
     * @type {boolean}
     * @private
     */
    this.isOwned_ = (opt_params && opt_params.isOwned) || false;

    /**
     * Cache of destination location fetched from tags.
     * @type {?string}
     * @private
     */
    this.location_ = null;

    /**
     * Connection status of the destination.
     * @type {!print_preview.Destination.ConnectionStatus}
     * @private
     */
    this.connectionStatus_ = connectionStatus;

    /**
     * Number of milliseconds since the epoch when the printer was last
     * accessed.
     * @type {number}
     * @private
     */
    this.lastAccessTime_ = (opt_params && opt_params.lastAccessTime) ||
                           Date.now();

    /**
     * Whether the user has accepted the terms-of-service for the print
     * destination. Only applies to the FedEx Office cloud-based printer.
     * {@code} null if terms-of-service does not apply to the print destination.
     * @type {?boolean}
     * @private
     */
    this.isTosAccepted_ = (opt_params && opt_params.isTosAccepted) || false;
  };

  /**
   * Prefix of the location destination tag.
   * @type {string}
   * @const
   */
  Destination.LOCATION_TAG_PREFIX = '__cp__printer-location=';

  /**
   * Enumeration of Google-promoted destination IDs.
   * @enum {string}
   */
  Destination.GooglePromotedId = {
    DOCS: '__google__docs',
    FEDEX: '__google__fedex',
    SAVE_AS_PDF: 'Save as PDF'
  };

  /**
   * Enumeration of the types of destinations.
   * @enum {string}
   */
  Destination.Type = {
    GOOGLE: 'google',
    LOCAL: 'local',
    MOBILE: 'mobile'
  };

  /**
   * Enumeration of the origin types for cloud destinations.
   * @enum {string}
   */
  Destination.Origin = {
    LOCAL: 'local',
    COOKIES: 'cookies',
    PROFILE: 'profile',
    DEVICE: 'device'
  };

  /**
   * Enumeration of the connection statuses of printer destinations.
   * @enum {string}
   */
  Destination.ConnectionStatus = {
    DORMANT: 'DORMANT',
    OFFLINE: 'OFFLINE',
    ONLINE: 'ONLINE',
    UNKNOWN: 'UNKNOWN'
  };

  /**
   * Enumeration of relative icon URLs for various types of destinations.
   * @enum {string}
   * @private
   */
  Destination.IconUrl_ = {
    CLOUD: 'images/printer.png',
    CLOUD_SHARED: 'images/printer_shared.png',
    LOCAL: 'images/printer.png',
    MOBILE: 'images/mobile.png',
    MOBILE_SHARED: 'images/mobile_shared.png',
    THIRD_PARTY: 'images/third_party.png',
    PDF: 'images/pdf.png',
    DOCS: 'images/google_doc.png',
    FEDEX: 'images/third_party_fedex.png'
  };

  Destination.prototype = {
    /** @return {string} ID of the destination. */
    get id() {
      return this.id_;
    },

    /** @return {!print_preview.Destination.Type} Type of the destination. */
    get type() {
      return this.type_;
    },

    /**
     * @return {!print_preview.Destination.Origin} Origin of the destination.
     */
    get origin() {
      return this.origin_;
    },

    /** @return {string} Display name of the destination. */
    get displayName() {
      return this.displayName_;
    },

    /** @return {boolean} Whether the destination has been used recently. */
    get isRecent() {
      return this.isRecent_;
    },

    /**
     * @param {boolean} isRecent Whether the destination has been used recently.
     */
    set isRecent(isRecent) {
      this.isRecent_ = isRecent;
    },

    /**
     * @return {boolean} Whether the user owns the destination. Only applies to
     *     cloud-based destinations.
     */
    get isOwned() {
      return this.isOwned_;
    },

    /** @return {boolean} Whether the destination is local or cloud-based. */
    get isLocal() {
      return this.origin_ == Destination.Origin.LOCAL;
    },

    /**
     * @return {string} The location of the destination, or an empty string if
     *     the location is unknown.
     */
    get location() {
      if (this.location_ == null) {
        for (var tag, i = 0; tag = this.tags_[i]; i++) {
          if (tag.indexOf(Destination.LOCATION_TAG_PREFIX) == 0) {
            this.location_ = tag.substring(
                Destination.LOCATION_TAG_PREFIX.length) || '';
            break;
          }
        }
      }
      return this.location_;
    },

    /** @return {!Array.<string>} Tags associated with the destination. */
    get tags() {
      return this.tags_.slice(0);
    },

    /** @return {print_preview.Cdd} Print capabilities of the destination. */
    get capabilities() {
      return this.capabilities_;
    },

    /**
     * @param {!print_preview.Cdd} capabilities Print capabilities of the
     *     destination.
     */
    set capabilities(capabilities) {
      this.capabilities_ = capabilities;
    },

    /**
     * @return {!print_preview.Destination.ConnectionStatus} Connection status
     *     of the print destination.
     */
    get connectionStatus() {
      return this.connectionStatus_;
    },

    /**
     * @param {!print_preview.Destination.ConnectionStatus} status Connection
     *     status of the print destination.
     */
    set connectionStatus(status) {
      this.connectionStatus_ = status;
    },

    /**
     * @return {number} Number of milliseconds since the epoch when the printer
     *     was last accessed.
     */
    get lastAccessTime() {
      return this.lastAccessTime_;
    },

    /** @return {string} Relative URL of the destination's icon. */
    get iconUrl() {
      if (this.id_ == Destination.GooglePromotedId.DOCS) {
        return Destination.IconUrl_.DOCS;
      } else if (this.id_ == Destination.GooglePromotedId.FEDEX) {
        return Destination.IconUrl_.FEDEX;
      } else if (this.id_ == Destination.GooglePromotedId.SAVE_AS_PDF) {
        return Destination.IconUrl_.PDF;
      } else if (this.isLocal) {
        return Destination.IconUrl_.LOCAL;
      } else if (this.type_ == Destination.Type.MOBILE && this.isOwned_) {
        return Destination.IconUrl_.MOBILE;
      } else if (this.type_ == Destination.Type.MOBILE) {
        return Destination.IconUrl_.MOBILE_SHARED;
      } else if (this.isOwned_) {
        return Destination.IconUrl_.CLOUD;
      } else {
        return Destination.IconUrl_.CLOUD_SHARED;
      }
    },

    /**
     * @return {?boolean} Whether the user has accepted the terms-of-service of
     *     the print destination or {@code null} if a terms-of-service does not
     *     apply.
     */
    get isTosAccepted() {
      return this.isTosAccepted_;
    },

    /**
     * @param {?boolean} Whether the user has accepted the terms-of-service of
     *     the print destination or {@code null} if a terms-of-service does not
     *     apply.
     */
    set isTosAccepted(isTosAccepted) {
      this.isTosAccepted_ = isTosAccepted;
    },

    /**
     * Matches a query against the destination.
     * @param {string} query Query to match against the destination.
     * @return {boolean} {@code true} if the query matches this destination,
     *     {@code false} otherwise.
     */
    matches: function(query) {
      return this.displayName_.toLowerCase().indexOf(
          query.toLowerCase().trim()) != -1;
    }
  };

  /**
   * The CDD (Cloud Device Description) describes the capabilities of a print
   * destination.
   *
   * @typedef {{
   *   version: string,
   *   printer: {
   *     vendor_capability: !Array.<{Object}>,
   *     collate: {default: boolean=}=,
   *     color: {
   *       option: !Array.<{
   *         type: string=,
   *         vendor_id: string=,
   *         custom_display_name: string=,
   *         is_default: boolean=
   *       }>
   *     }=,
   *     copies: {default: number=, max: number=}=,
   *     duplex: {option: !Array.<{type: string=, is_default: boolean=}>}=,
   *     page_orientation: {
   *       option: !Array.<{type: string=, is_default: boolean=}>
   *     }=
   *   }
   * }}
   */
  var Cdd = Object;

  // Export
  return {
    Destination: Destination,
    Cdd: Cdd
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /** Namespace that contains a method to parse local print destinations. */
  function LocalDestinationParser() {};

  /**
   * Parses a local print destination.
   * @param {!Object} destinationInfo Information describing a local print
   *     destination.
   * @return {!print_preview.Destination} Parsed local print destination.
   */
  LocalDestinationParser.parse = function(destinationInfo) {
    return new print_preview.Destination(
        destinationInfo.deviceName,
        print_preview.Destination.Type.LOCAL,
        print_preview.Destination.Origin.LOCAL,
        destinationInfo.printerName,
        false /*isRecent*/,
        print_preview.Destination.ConnectionStatus.ONLINE);
  };

  /** Namespace that contains a method to parse local print capabilities. */
  function LocalCapabilitiesParser() {};

  /**
   * Parses local print capabilities.
   * @param {!Object} settingsInfo Object that describes local print
   *     capabilities.
   * @return {!print_preview.Cdd} Parsed local print capabilities.
   */
  LocalCapabilitiesParser.parse = function(settingsInfo) {
    var cdd = {
      version: '1.0',
      printer: {
        collate: {default: true}
      }
    };

    if (!settingsInfo['disableColorOption']) {
      cdd.printer.color = {
        option: [
          {
            type: 'STANDARD_COLOR',
            is_default: !!settingsInfo['setColorAsDefault']
          },
          {
            type: 'STANDARD_MONOCHROME',
            is_default: !settingsInfo['setColorAsDefault']
          }
        ]
      }
    }

    if (!settingsInfo['disableCopiesOption']) {
      cdd.printer.copies = {default: 1};
    }

    if (settingsInfo['printerDefaultDuplexValue'] !=
        print_preview.NativeLayer.DuplexMode.UNKNOWN_DUPLEX_MODE) {
      cdd.printer.duplex = {
        option: [
          {type: 'NO_DUPLEX', is_default: !settingsInfo['setDuplexAsDefault']},
          {type: 'LONG_EDGE', is_default: !!settingsInfo['setDuplexAsDefault']}
        ]
      };
    }

    if (!settingsInfo['disableLandscapeOption']) {
      cdd.printer.page_orientation = {
        option: [
          {type: 'PORTRAIT', is_default: true},
          {type: 'LANDSCAPE'}
        ]
      };
    }

    return cdd;
  };

  // Export
  return {
    LocalCapabilitiesParser: LocalCapabilitiesParser,
    LocalDestinationParser: LocalDestinationParser
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cloudprint', function() {
  'use strict';

  /** Namespace which contains a method to parse cloud destinations directly. */
  function CloudDestinationParser() {};

  /**
   * Enumeration of cloud destination field names.
   * @enum {string}
   * @private
   */
  CloudDestinationParser.Field_ = {
    LAST_ACCESS: 'accessTime',
    CAPABILITIES: 'capabilities',
    CONNECTION_STATUS: 'connectionStatus',
    DISPLAY_NAME: 'displayName',
    ID: 'id',
    IS_TOS_ACCEPTED: 'isTosAccepted',
    TAGS: 'tags',
    TYPE: 'type'
  };

  /**
   * Special tag that denotes whether the destination has been recently used.
   * @type {string}
   * @const
   * @private
   */
  CloudDestinationParser.RECENT_TAG_ = '^recent';

  /**
   * Special tag that denotes whether the destination is owned by the user.
   * @type {string}
   * @const
   * @private
   */
  CloudDestinationParser.OWNED_TAG_ = '^own';

  /**
   * Enumeration of cloud destination types that are supported by print preview.
   * @enum {string}
   * @private
   */
  CloudDestinationParser.CloudType_ = {
    ANDROID: 'ANDROID_CHROME_SNAPSHOT',
    DOCS: 'DOCS',
    IOS: 'IOS_CHROME_SNAPSHOT'
  };

  /**
   * Parses a destination from JSON from a Google Cloud Print search or printer
   * response.
   * @param {!Object} json Object that represents a Google Cloud Print search or
   *     printer response.
   * @param {!print_preview.Destination.Origin} origin The origin of the
   *     response.
   * @return {!print_preview.Destination} Parsed destination.
   */
  CloudDestinationParser.parse = function(json, origin) {
    if (!json.hasOwnProperty(CloudDestinationParser.Field_.ID) ||
        !json.hasOwnProperty(CloudDestinationParser.Field_.TYPE) ||
        !json.hasOwnProperty(CloudDestinationParser.Field_.DISPLAY_NAME)) {
      throw Error('Cloud destination does not have an ID or a display name');
    }
    var id = json[CloudDestinationParser.Field_.ID];
    var tags = json[CloudDestinationParser.Field_.TAGS] || [];
    var connectionStatus =
        json[CloudDestinationParser.Field_.CONNECTION_STATUS] ||
        print_preview.Destination.ConnectionStatus.UNKNOWN;
    var optionalParams = {
      tags: tags,
      isOwned: arrayContains(tags, CloudDestinationParser.OWNED_TAG_),
      lastAccessTime: parseInt(
          json[CloudDestinationParser.Field_.LAST_ACCESS], 10) || Date.now(),
      isTosAccepted: (id == print_preview.Destination.GooglePromotedId.FEDEX) ?
          json[CloudDestinationParser.Field_.IS_TOS_ACCEPTED] : null
    };
    var cloudDest = new print_preview.Destination(
        id,
        CloudDestinationParser.parseType_(
            json[CloudDestinationParser.Field_.TYPE]),
        origin,
        json[CloudDestinationParser.Field_.DISPLAY_NAME],
        arrayContains(tags, CloudDestinationParser.RECENT_TAG_) /*isRecent*/,
        connectionStatus,
        optionalParams);
    if (json.hasOwnProperty(CloudDestinationParser.Field_.CAPABILITIES)) {
      cloudDest.capabilities = /*@type {!print_preview.Cdd}*/ (
          json[CloudDestinationParser.Field_.CAPABILITIES]);
    }
    return cloudDest;
  };

  /**
   * Parses the destination type.
   * @param {string} typeStr Destination type given by the Google Cloud Print
   *     server.
   * @return {!print_preview.Destination.Type} Destination type.
   * @private
   */
  CloudDestinationParser.parseType_ = function(typeStr) {
    if (typeStr == CloudDestinationParser.CloudType_.ANDROID ||
        typeStr == CloudDestinationParser.CloudType_.IOS) {
      return print_preview.Destination.Type.MOBILE;
    } else if (typeStr == CloudDestinationParser.CloudType_.DOCS) {
      return print_preview.Destination.Type.GOOGLE_PROMOTED;
    } else {
      return print_preview.Destination.Type.GOOGLE;
    }
  };

  // Export
  return {
    CloudDestinationParser: CloudDestinationParser
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * A data store that stores destinations and dispatches events when the data
   * store changes.
   * @param {!print_preview.NativeLayer} nativeLayer Used to fetch local print
   *     destinations.
   * @param {!print_preview.AppState} appState Application state.
   * @constructor
   * @extends {cr.EventTarget}
   */
  function DestinationStore(nativeLayer, appState) {
    cr.EventTarget.call(this);

    /**
     * Used to fetch local print destinations.
     * @type {!print_preview.NativeLayer}
     * @private
     */
    this.nativeLayer_ = nativeLayer;

    /**
     * Used to load and persist the selected destination.
     * @type {!print_preview.AppState}
     * @private
     */
    this.appState_ = appState;

    /**
     * Internal backing store for the data store.
     * @type {!Array.<!print_preview.Destination>}
     * @private
     */
    this.destinations_ = [];

    /**
     * Cache used for constant lookup of destinations by origin and id.
     * @type {object.<string, !print_preview.Destination>}
     * @private
     */
    this.destinationMap_ = {};

    /**
     * Currently selected destination.
     * @type {print_preview.Destination}
     * @private
     */
    this.selectedDestination_ = null;

    /**
     * Initial destination ID used to auto-select the first inserted destination
     * that matches. If {@code null}, the first destination inserted into the
     * store will be selected.
     * @type {?string}
     * @private
     */
    this.initialDestinationId_ = null;

    /**
     * Initial origin used to auto-select destination.
     * @type {print_preview.Destination.Origin}
     * @private
     */
    this.initialDestinationOrigin_ = print_preview.Destination.Origin.LOCAL;

    /**
     * Whether the destination store will auto select the destination that
     * matches the initial destination.
     * @type {boolean}
     * @private
     */
    this.isInAutoSelectMode_ = false;

    /**
     * Event tracker used to track event listeners of the destination store.
     * @type {!EventTracker}
     * @private
     */
    this.tracker_ = new EventTracker();

    /**
     * Used to fetch cloud-based print destinations.
     * @type {print_preview.CloudPrintInterface}
     * @private
     */
    this.cloudPrintInterface_ = null;

    /**
     * Whether the destination store has already loaded or is loading all cloud
     * destinations.
     * @type {boolean}
     * @private
     */
    this.hasLoadedAllCloudDestinations_ = false;

    /**
     * ID of a timeout after the initial destination ID is set. If no inserted
     * destination matches the initial destination ID after the specified
     * timeout, the first destination in the store will be automatically
     * selected.
     * @type {?number}
     * @private
     */
    this.autoSelectTimeout_ = null;

    /**
     * Whether a search for local destinations is in progress.
     * @type {boolean}
     * @private
     */
    this.isLocalDestinationSearchInProgress_ = false;

    this.addEventListeners_();
    this.reset_();
  };

  /**
   * Event types dispatched by the data store.
   * @enum {string}
   */
  DestinationStore.EventType = {
    DESTINATION_SEARCH_DONE:
        'print_preview.DestinationStore.DESTINATION_SEARCH_DONE',
    DESTINATION_SEARCH_STARTED:
        'print_preview.DestinationStore.DESTINATION_SEARCH_STARTED',
    DESTINATION_SELECT: 'print_preview.DestinationStore.DESTINATION_SELECT',
    DESTINATIONS_INSERTED:
        'print_preview.DestinationStore.DESTINATIONS_INSERTED',
    SELECTED_DESTINATION_CAPABILITIES_READY:
        'print_preview.DestinationStore.SELECTED_DESTINATION_CAPABILITIES_READY'
  };

  /**
   * Delay in milliseconds before the destination store ignores the initial
   * destination ID and just selects any printer (since the initial destination
   * was not found).
   * @type {number}
   * @const
   * @private
   */
  DestinationStore.AUTO_SELECT_TIMEOUT_ = 15000;

  /**
   * Creates a local PDF print destination.
   * @return {!print_preview.Destination} Created print destination.
   * @private
   */
  DestinationStore.createLocalPdfPrintDestination_ = function() {
    var dest = new print_preview.Destination(
        print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
        print_preview.Destination.Type.LOCAL,
        print_preview.Destination.Origin.LOCAL,
        localStrings.getString('printToPDF'),
        false /*isRecent*/,
        print_preview.Destination.ConnectionStatus.ONLINE);
    dest.capabilities = {
      version: '1.0',
      printer: {
        page_orientation: {
          option: [
            {type: 'AUTO', is_default: true},
            {type: 'PORTRAIT'},
            {type: 'LANDSCAPE'}
          ]
        },
        color: { option: [{type: 'STANDARD_COLOR', is_default: true}] }
      }
    };
    return dest;
  };

  DestinationStore.prototype = {
    __proto__: cr.EventTarget.prototype,

    /**
     * @return {!Array.<!print_preview.Destination>} List of destinations in
     *     the store.
     */
    get destinations() {
      return this.destinations_.slice(0);
    },

    /**
     * @return {print_preview.Destination} The currently selected destination or
     *     {@code null} if none is selected.
     */
    get selectedDestination() {
      return this.selectedDestination_;
    },

    /**
     * @return {boolean} Whether a search for local destinations is in progress.
     */
    get isLocalDestinationSearchInProgress() {
      return this.isLocalDestinationSearchInProgress_;
    },

    /**
     * @return {boolean} Whether a search for cloud destinations is in progress.
     */
    get isCloudDestinationSearchInProgress() {
      return this.cloudPrintInterface_ &&
             this.cloudPrintInterface_.isCloudDestinationSearchInProgress;
    },

    /**
     * Initializes the destination store. Sets the initially selected
     * destination. If any inserted destinations match this ID, that destination
     * will be automatically selected. This method must be called after the
     * print_preview.AppState has been initialized.
     * @param {?string} systemDefaultDestinationId ID of the system default
     *     destination.
     * @private
     */
    init: function(systemDefaultDestinationId) {
      if (this.appState_.selectedDestinationId &&
          this.appState_.selectedDestinationOrigin) {
        this.initialDestinationId_ = this.appState_.selectedDestinationId;
        this.initialDestinationOrigin_ =
            this.appState_.selectedDestinationOrigin;
      } else {
        this.initialDestinationId_ = systemDefaultDestinationId;
        this.initialDestinationOrigin_ = print_preview.Destination.Origin.LOCAL;
      }
      this.isInAutoSelectMode_ = true;
      if (this.initialDestinationId_ == null ||
          this.initialDestinationOrigin_ == null) {
        assert(this.destinations_.length > 0,
               'No destinations available to select');
        this.selectDestination(this.destinations_[0]);
      } else {
        var key = this.getDestinationKey_(this.initialDestinationOrigin_,
                                          this.initialDestinationId_);
        var candidate = this.destinationMap_[key];
        if (candidate != null) {
          this.selectDestination(candidate);
        } else if (!cr.isChromeOS &&
                   this.initialDestinationOrigin_ ==
                   print_preview.Destination.Origin.LOCAL) {
          this.nativeLayer_.startGetLocalDestinationCapabilities(
              this.initialDestinationId_);
        }
      }
    },

    /**
     * Sets the destination store's Google Cloud Print interface.
     * @param {!print_preview.CloudPrintInterface} cloudPrintInterface Interface
     *     to set.
     */
    setCloudPrintInterface: function(cloudPrintInterface) {
      this.cloudPrintInterface_ = cloudPrintInterface;
      this.tracker_.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterface.EventType.SEARCH_DONE,
          this.onCloudPrintSearchDone_.bind(this));
      this.tracker_.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterface.EventType.SEARCH_FAILED,
          this.onCloudPrintSearchFailed_.bind(this));
      this.tracker_.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterface.EventType.PRINTER_DONE,
          this.onCloudPrintPrinterDone_.bind(this));
      this.tracker_.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterface.EventType.PRINTER_FAILED,
          this.onCloudPrintPrinterFailed_.bind(this));
      // Fetch initial destination if its a cloud destination.
      var origin = this.initialDestinationOrigin_;
      if (this.isInAutoSelectMode_ &&
          origin != print_preview.Destination.Origin.LOCAL) {
        this.cloudPrintInterface_.printer(this.initialDestinationId_, origin);
      }
    },

    /**
     * @return {boolean} Whether only default cloud destinations have been
     *     loaded.
     */
    hasOnlyDefaultCloudDestinations: function() {
      return this.destinations_.every(function(dest) {
        return dest.isLocal ||
            dest.id == print_preview.Destination.GooglePromotedId.DOCS ||
            dest.id == print_preview.Destination.GooglePromotedId.FEDEX;
      });
    },

    /** @param {!print_preview.Destination} Destination to select. */
    selectDestination: function(destination) {
      this.selectedDestination_ = destination;
      this.selectedDestination_.isRecent = true;
      this.isInAutoSelectMode_ = false;
      if (this.autoSelectTimeout_ != null) {
        clearTimeout(this.autoSelectTimeout_);
        this.autoSelectTimeout_ = null;
      }
      if (destination.id == print_preview.Destination.GooglePromotedId.FEDEX &&
          !destination.isTosAccepted) {
        assert(this.cloudPrintInterface_ != null,
               'Selected FedEx Office destination, but Google Cloud Print is ' +
               'not enabled');
        destination.isTosAccepted = true;
        this.cloudPrintInterface_.updatePrinterTosAcceptance(destination.id,
                                                             destination.origin,
                                                             true);
      }
      this.appState_.persistSelectedDestination(this.selectedDestination_);
      cr.dispatchSimpleEvent(
          this, DestinationStore.EventType.DESTINATION_SELECT);
      if (destination.capabilities == null) {
         if (destination.isLocal) {
          this.nativeLayer_.startGetLocalDestinationCapabilities(
              destination.id);
        } else {
          assert(this.cloudPrintInterface_ != null,
                 'Selected destination is a cloud destination, but Google ' +
                 'Cloud Print is not enabled');
          this.cloudPrintInterface_.printer(destination.id,
                                            destination.origin);
        }
      } else {
        cr.dispatchSimpleEvent(
            this,
            DestinationStore.EventType.SELECTED_DESTINATION_CAPABILITIES_READY);
      }
    },

    /**
     * Inserts a print destination to the data store and dispatches a
     * DESTINATIONS_INSERTED event. If the destination matches the initial
     * destination ID, then the destination will be automatically selected.
     * @param {!print_preview.Destination} destination Print destination to
     *     insert.
     */
    insertDestination: function(destination) {
      if (this.insertDestination_(destination)) {
        cr.dispatchSimpleEvent(
            this, DestinationStore.EventType.DESTINATIONS_INSERTED);
        if (this.isInAutoSelectMode_ &&
            this.matchInitialDestination_(destination.id, destination.origin)) {
          this.selectDestination(destination);
        }
      }
    },

    /**
     * Inserts multiple print destinations to the data store and dispatches one
     * DESTINATIONS_INSERTED event. If any of the destinations match the initial
     * destination ID, then that destination will be automatically selected.
     * @param {!Array.<print_preview.Destination>} destinations Print
     *     destinations to insert.
     */
    insertDestinations: function(destinations) {
      var insertedDestination = false;
      var destinationToAutoSelect = null;
      destinations.forEach(function(dest) {
        if (this.insertDestination_(dest)) {
          insertedDestination = true;
          if (this.isInAutoSelectMode_ &&
              destinationToAutoSelect == null &&
              this.matchInitialDestination_(dest.id, dest.origin)) {
            destinationToAutoSelect = dest;
          }
        }
      }, this);
      if (insertedDestination) {
        cr.dispatchSimpleEvent(
            this, DestinationStore.EventType.DESTINATIONS_INSERTED);
      }
      if (destinationToAutoSelect != null) {
        this.selectDestination(destinationToAutoSelect);
      }
    },

    /**
     * Updates an existing print destination with capabilities information. If
     * the destination doesn't already exist, it will be added.
     * @param {!print_preview.Destination} destination Destination to update.
     * @return {!print_preview.Destination} The existing destination that was
     *     updated or {@code null} if it was the new destination.
     */
    updateDestination: function(destination) {
      var key = this.getDestinationKey_(destination.origin, destination.id);
      var existingDestination = this.destinationMap_[key];
      if (existingDestination != null) {
        existingDestination.capabilities = destination.capabilities;
        return existingDestination;
      } else {
        this.insertDestination(destination);
        return null;
      }
    },

    /** Initiates loading of local print destinations. */
    startLoadLocalDestinations: function() {
      this.nativeLayer_.startGetLocalDestinations();
      this.isLocalDestinationSearchInProgress_ = true;
      cr.dispatchSimpleEvent(
          this, DestinationStore.EventType.DESTINATION_SEARCH_STARTED);
    },

    /**
     * Initiates loading of cloud destinations.
     * @param {boolean} recentOnly Whether the load recet destinations only.
     */
    startLoadCloudDestinations: function(recentOnly) {
      if (this.cloudPrintInterface_ != null &&
          !this.hasLoadedAllCloudDestinations_ &&
          (!recentOnly || !this.isCloudDestinationSearchInProgress)) {
        this.cloudPrintInterface_.search(recentOnly);
        this.hasLoadedAllCloudDestinations_ = !recentOnly;
        cr.dispatchSimpleEvent(
            this, DestinationStore.EventType.DESTINATION_SEARCH_STARTED);
      }
    },

    /**
     * Inserts a destination into the store without dispatching any events.
     * @return {boolean} Whether the inserted destination was not already in the
     *     store.
     * @private
     */
    insertDestination_: function(destination) {
      var key = this.getDestinationKey_(destination.origin, destination.id);
      var existingDestination = this.destinationMap_[key];
      if (existingDestination == null) {
        this.destinations_.push(destination);
        this.destinationMap_[key] = destination;
        return true;
      } else if (existingDestination.connectionStatus ==
                     print_preview.Destination.ConnectionStatus.UNKNOWN &&
                 destination.connectionStatus !=
                     print_preview.Destination.ConnectionStatus.UNKNOWN) {
        existingDestination.connectionStatus = destination.connectionStatus;
        return true;
      } else {
        return false;
      }
    },

    /**
     * Binds handlers to events.
     * @private
     */
    addEventListeners_: function() {
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET,
          this.onLocalDestinationsSet_.bind(this));
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.CAPABILITIES_SET,
          this.onLocalDestinationCapabilitiesSet_.bind(this));
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.GET_CAPABILITIES_FAIL,
          this.onGetCapabilitiesFail_.bind(this));
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.DESTINATIONS_RELOAD,
          this.onDestinationsReload_.bind(this));
    },

    /**
     * Resets the state of the destination store to its initial state.
     * @private
     */
    reset_: function() {
      this.destinations_ = [];
      this.destinationMap_ = {};
      this.selectedDestination_ = null;
      this.hasLoadedAllCloudDestinations_ = false;
      this.insertDestination(
          DestinationStore.createLocalPdfPrintDestination_());
      this.autoSelectTimeout_ = setTimeout(
          this.onAutoSelectTimeoutExpired_.bind(this),
          DestinationStore.AUTO_SELECT_TIMEOUT_);
    },

    /**
     * Called when the local destinations have been got from the native layer.
     * @param {Event} Contains the local destinations.
     * @private
     */
    onLocalDestinationsSet_: function(event) {
      var localDestinations = event.destinationInfos.map(function(destInfo) {
        return print_preview.LocalDestinationParser.parse(destInfo);
      });
      this.insertDestinations(localDestinations);
      this.isLocalDestinationSearchInProgress_ = false;
      cr.dispatchSimpleEvent(
          this, DestinationStore.EventType.DESTINATION_SEARCH_DONE);
    },

    /**
     * Called when the native layer retrieves the capabilities for the selected
     * local destination. Updates the destination with new capabilities if the
     * destination already exists, otherwise it creates a new destination and
     * then updates its capabilities.
     * @param {Event} event Contains the capabilities of the local print
     *     destination.
     * @private
     */
    onLocalDestinationCapabilitiesSet_: function(event) {
      var destinationId = event.settingsInfo['printerId'];
      var key =
          this.getDestinationKey_(print_preview.Destination.Origin.LOCAL,
                                  destinationId);
      var destination = this.destinationMap_[key];
      var capabilities = print_preview.LocalCapabilitiesParser.parse(
            event.settingsInfo);
      if (destination) {
        // In case there were multiple capabilities request for this local
        // destination, just ignore the later ones.
        if (destination.capabilities != null) {
          return;
        }
        destination.capabilities = capabilities;
      } else {
        // TODO(rltoscano): This makes the assumption that the "deviceName" is
        // the same as "printerName". We should include the "printerName" in the
        // response. See http://crbug.com/132831.
        destination = print_preview.LocalDestinationParser.parse(
            {deviceName: destinationId, printerName: destinationId});
        destination.capabilities = capabilities;
        this.insertDestination(destination);
      }
      if (this.selectedDestination_ &&
          this.selectedDestination_.id == destinationId) {
        cr.dispatchSimpleEvent(this,
                               DestinationStore.EventType.
                                   SELECTED_DESTINATION_CAPABILITIES_READY);
      }
    },

    /**
     * Called when a request to get a local destination's print capabilities
     * fails. If the destination is the initial destination, auto-select another
     * destination instead.
     * @param {Event} event Contains the destination ID that failed.
     * @private
     */
    onGetCapabilitiesFail_: function(event) {
      console.error('Failed to get print capabilities for printer ' +
                    event.destinationId);
      if (this.isInAutoSelectMode_ &&
          this.matchInitialDestinationStrict_(event.destinationId,
                                              event.destinationOrigin)) {
        assert(this.destinations_.length > 0,
               'No destinations were loaded when failed to get initial ' +
               'destination');
        this.selectDestination(this.destinations_[0]);
      }
    },

    /**
     * Called when the /search call completes. Adds the fetched destinations to
     * the destination store.
     * @param {Event} event Contains the fetched destinations.
     * @private
     */
    onCloudPrintSearchDone_: function(event) {
      this.insertDestinations(event.printers);
      cr.dispatchSimpleEvent(
          this, DestinationStore.EventType.DESTINATION_SEARCH_DONE);
    },

    /**
     * Called when the /search call fails. Updates outstanding request count and
     * dispatches CLOUD_DESTINATIONS_LOADED event.
     * @private
     */
    onCloudPrintSearchFailed_: function() {
      cr.dispatchSimpleEvent(
          this, DestinationStore.EventType.DESTINATION_SEARCH_DONE);
    },

    /**
     * Called when /printer call completes. Updates the specified destination's
     * print capabilities.
     * @param {Event} event Contains detailed information about the
     *     destination.
     * @private
     */
    onCloudPrintPrinterDone_: function(event) {
      var dest = this.updateDestination(event.printer);
      if (this.selectedDestination_ == dest) {
        cr.dispatchSimpleEvent(
            this,
            DestinationStore.EventType.SELECTED_DESTINATION_CAPABILITIES_READY);
      }
    },

    /**
     * Called when the Google Cloud Print interface fails to lookup a
     * destination. Selects another destination if the failed destination was
     * the initial destination.
     * @param {object} event Contains the ID of the destination that was failed
     *     to be looked up.
     * @private
     */
    onCloudPrintPrinterFailed_: function(event) {
      if (this.isInAutoSelectMode_ &&
          this.matchInitialDestinationStrict_(event.destinationId,
                                              event.destinationOrigin)) {
        console.error('Could not find initial printer: ' + event.destinationId);
        assert(this.destinations_.length > 0,
               'No destinations were loaded when failed to get initial ' +
               'destination');
        this.selectDestination(this.destinations_[0]);
      }
    },

    /**
     * Called from native layer after the user was requested to sign in, and did
     * so successfully.
     * @private
     */
    onDestinationsReload_: function() {
      this.reset_();
      this.isInAutoSelectMode_ = true;
      this.startLoadLocalDestinations();
      this.startLoadCloudDestinations(true);
      this.startLoadCloudDestinations(false);
    },

    /**
     * Called when no destination was auto-selected after some timeout. Selects
     * the first destination in store.
     * @private
     */
    onAutoSelectTimeoutExpired_: function() {
      this.autoSelectTimeout_ = null;
      assert(this.destinations_.length > 0,
             'No destinations were loaded before auto-select timeout expired');
      this.selectDestination(this.destinations_[0]);
    },

    // TODO(vitalybuka): Remove three next functions replacing Destination.id
    //    and Destination.origin by complex ID.
    /**
     * Returns key to be used with {@code destinationMap_}.
     * @param {!print_preview.Destination.Origin} origin Destination origin.
     * @return {!string} id Destination id.
     * @private
     */
    getDestinationKey_: function(origin, id) {
      return origin + '/' + id;
    },

    /**
     * @param {?string} id Id of the destination.
     * @param {?string} origin Oring of the destination.
     * @return {boolean} Whether a initial destination matches provided.
     * @private
     */
    matchInitialDestination_: function(id, origin) {
      return this.initialDestinationId_ == null ||
             this.initialDestinationOrigin_ == null ||
             this.matchInitialDestinationStrict_(id, origin);
    },

    /**
     * @param {?string} id Id of the destination.
     * @param {?string} origin Oring of the destination.
     * @return {boolean} Whether destination is the same as initial.
     * @private
     */
    matchInitialDestinationStrict_: function(id, origin) {
      return id == this.initialDestinationId_ &&
             origin == this.initialDestinationOrigin_;
    }
  };

  // Export
  return {
    DestinationStore: DestinationStore
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Creates a Margins object that holds four margin values in points.
   * @param {number} top The top margin in pts.
   * @param {number} right The right margin in pts.
   * @param {number} bottom The bottom margin in pts.
   * @param {number} left The left margin in pts.
   * @constructor
   */
  function Margins(top, right, bottom, left) {
    /**
     * Backing store for the margin values in points.
     * @type {!Object.<
     *     !print_preview.ticket_items.CustomMargins.Orientation, number>}
     * @private
     */
    this.value_ = {};
    this.value_[print_preview.ticket_items.CustomMargins.Orientation.TOP] = top;
    this.value_[print_preview.ticket_items.CustomMargins.Orientation.RIGHT] =
        right;
    this.value_[print_preview.ticket_items.CustomMargins.Orientation.BOTTOM] =
        bottom;
    this.value_[print_preview.ticket_items.CustomMargins.Orientation.LEFT] =
        left;
  };

  /**
   * Parses a margins object from the given serialized state.
   * @param {Object} state Serialized representation of the margins created by
   *     the {@code serialize} method.
   * @return {!print_preview.Margins} New margins instance.
   */
  Margins.parse = function(state) {
    return new print_preview.Margins(
        state[print_preview.ticket_items.CustomMargins.Orientation.TOP] || 0,
        state[print_preview.ticket_items.CustomMargins.Orientation.RIGHT] || 0,
        state[print_preview.ticket_items.CustomMargins.Orientation.BOTTOM] || 0,
        state[print_preview.ticket_items.CustomMargins.Orientation.LEFT] || 0);
  };

  Margins.prototype = {
    /**
     * @param {!print_preview.ticket_items.CustomMargins.Orientation}
     *     orientation Specifies the margin value to get.
     * @return {number} Value of the margin of the given orientation.
     */
    get: function(orientation) {
      return this.value_[orientation];
    },

    /**
     * @param {!print_preview.ticket_items.CustomMargins.Orientation}
     *     orientation Specifies the margin to set.
     * @param {number} value Updated value of the margin in points to modify.
     * @return {!print_preview.Margins} A new copy of |this| with the
     *     modification made to the specified margin.
     */
    set: function(orientation, value) {
      var newValue = this.clone_();
      newValue[orientation] = value;
      return new Margins(
          newValue[print_preview.ticket_items.CustomMargins.Orientation.TOP],
          newValue[print_preview.ticket_items.CustomMargins.Orientation.RIGHT],
          newValue[print_preview.ticket_items.CustomMargins.Orientation.BOTTOM],
          newValue[print_preview.ticket_items.CustomMargins.Orientation.LEFT]);
    },

    /**
     * @param {print_preview.Margins} other The other margins object to compare
     *     against.
     * @return {boolean} Whether this margins object is equal to another.
     */
    equals: function(other) {
      if (other == null) {
        return false;
      }
      for (var orientation in this.value_) {
        if (this.value_[orientation] != other.value_[orientation]) {
          return false;
        }
      }
      return true;
    },

    /** @return {Object} A serialized representation of the margins. */
    serialize: function() {
      return this.clone_();
    },

    /**
     * @return {Object} Cloned state of the margins.
     * @private
     */
    clone_: function() {
      var clone = {};
      for (var o in this.value_) {
        clone[o] = this.value_[o];
      }
      return clone;
    }
  };

  // Export
  return {
    Margins: Margins
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Data model which contains information related to the document to print.
   * @constructor
   * @extends {cr.EventTarget}
   */
  function DocumentInfo() {
    cr.EventTarget.call(this);

    /**
     * Whether the document is styled by CSS media styles.
     * @type {boolean}
     * @private
     */
    this.hasCssMediaStyles_ = false;

    /**
     * Whether the document has selected content.
     * @type {boolean}
     * @private
     */
    this.hasSelection_ = false;

    /**
     * Whether the document to print is modifiable (i.e. can be reflowed).
     * @type {boolean}
     * @private
     */
    this.isModifiable_ = true;

    /**
     * Whether scaling of the document is prohibited.
     * @type {boolean}
     * @private
     */
    this.isScalingDisabled_ = false;

    /**
     * Margins of the document in points.
     * @type {print_preview.Margins}
     * @private
     */
    this.margins_ = null;

    /**
     * Number of pages in the document to print.
     * @type {number}
     * @private
     */
    this.pageCount_ = 0;

    // Create the document info with some initial settings. Actual
    // page-related information won't be set until preview generation occurs,
    // so we'll use some defaults until then. This way, the print ticket store
    // will be valid even if no preview can be generated.
    var initialPageSize = new print_preview.Size(612, 792); // 8.5"x11"

    /**
     * Size of the pages of the document in points.
     * @type {!print_preview.Size}
     * @private
     */
    this.pageSize_ = initialPageSize;

    /**
     * Printable area of the document in points.
     * @type {!print_preview.PrintableArea}
     * @private
     */
    this.printableArea_ = new print_preview.PrintableArea(
        new print_preview.Coordinate2d(0, 0), initialPageSize);

    /**
     * Title of document.
     * @type {string}
     * @private
     */
    this.title_ = '';

    /**
     * Whether this data model has been initialized.
     * @type {boolean}
     * @private
     */
    this.isInitialized_ = false;
  };

  /**
   * Event types dispatched by this data model.
   * @enum {string}
   */
  DocumentInfo.EventType = {
    CHANGE: 'print_preview.DocumentInfo.CHANGE'
  };

  DocumentInfo.prototype = {
    __proto__: cr.EventTarget.prototype,

    /** @return {boolean} Whether the document is styled by CSS media styles. */
    get hasCssMediaStyles() {
      return this.hasCssMediaStyles_;
    },

    /** @return {boolean} Whether the document has selected content. */
    get hasSelection() {
      return this.hasSelection_;
    },

    /**
     * @return {boolean} Whether the document to print is modifiable (i.e. can
     *     be reflowed).
     */
    get isModifiable() {
      return this.isModifiable_;
    },

    /** @return {boolean} Whether scaling of the document is prohibited. */
    get isScalingDisabled() {
      return this.isScalingDisabled_;
    },

    /** @return {print_preview.Margins} Margins of the document in points. */
    get margins() {
      return this.margins_;
    },

    /** @return {number} Number of pages in the document to print. */
    get pageCount() {
      return this.pageCount_;
    },

    /**
     * @return {!print_preview.Size} Size of the pages of the document in
     *     points.
     */
    get pageSize() {
      return this.pageSize_;
    },

    /**
     * @return {!print_preview.PrintableArea} Printable area of the document in
     *     points.
     */
    get printableArea() {
      return this.printableArea_;
    },

    /** @return {string} Title of document. */
    get title() {
      return this.title_;
    },

    /**
     * Initializes the state of the data model and dispatches a CHANGE event.
     * @param {boolean} isModifiable Whether the document is modifiable.
     * @param {string} title Title of the document.
     * @param {boolean} hasSelection Whether the document has user-selected
     *     content.
     */
    init: function(isModifiable, title, hasSelection) {
      this.isModifiable_ = isModifiable;
      this.title_ = title;
      this.hasSelection_ = hasSelection;
      this.isInitialized_ = true;
      cr.dispatchSimpleEvent(this, DocumentInfo.EventType.CHANGE);
    },

    /**
     * Updates whether scaling is disabled for the document and dispatches a
     * CHANGE event.
     * @param {boolean} isScalingDisabled Whether scaling of the document is
     *     prohibited.
     */
    updateIsScalingDisabled: function(isScalingDisabled) {
      if (this.isInitialized_ && this.isScalingDisabled_ != isScalingDisabled) {
        this.isScalingDisabled_ = isScalingDisabled;
        cr.dispatchSimpleEvent(this, DocumentInfo.EventType.CHANGE);
      }
    },

    /**
     * Updates the total number of pages in the document and dispatches a CHANGE
     * event.
     * @param {number} pageCount Number of pages in the document.
     */
    updatePageCount: function(pageCount) {
      if (this.isInitialized_ && this.pageCount_ != pageCount) {
        this.pageCount_ = pageCount;
        cr.dispatchSimpleEvent(this, DocumentInfo.EventType.CHANGE);
      }
    },

    /**
     * Updates information about each page and dispatches a CHANGE event.
     * @param {!print_preview.PrintableArea} printableArea Printable area of the
     *     document in points.
     * @param {!print_preview.Size} pageSize Size of the pages of the document
     *     in points.
     * @param {boolean} hasCssMediaStyles Whether the document is styled by CSS
     *     media styles.
     * @param {print_preview.Margins} margins Margins of the document in points.
     */
    updatePageInfo: function(
        printableArea, pageSize, hasCssMediaStyles, margins) {
      if (this.isInitialized_ &&
          (!this.printableArea_.equals(printableArea) ||
           !this.pageSize_.equals(pageSize) ||
           this.hasCssMediaStyles_ != hasCssMediaStyles ||
           this.margins_ == null || !this.margins_.equals(margins))) {
        this.printableArea_ = printableArea;
        this.pageSize_ = pageSize;
        this.hasCssMediaStyles_ = hasCssMediaStyles;
        this.margins_ = margins;
        cr.dispatchSimpleEvent(this, DocumentInfo.EventType.CHANGE);
      }
    }
  };

  // Export
  return {
    DocumentInfo: DocumentInfo
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Object describing the printable area of a page in the document.
   * @param {!print_preview.Coordinate2d} origin Top left corner of the
   *     printable area of the document.
   * @param {!print_preview.Size} size Size of the printable area of the
   *     document.
   * @constructor
   */
  function PrintableArea(origin, size) {
    /**
     * Top left corner of the printable area of the document.
     * @type {!print_preview.Coordinate2d}
     * @private
     */
    this.origin_ = origin;

    /**
     * Size of the printable area of the document.
     * @type {!print_preview.Size}
     * @private
     */
    this.size_ = size;
  };

  PrintableArea.prototype = {
    /**
     * @return {!print_preview.Coordinate2d} Top left corner of the printable
     *     area of the document.
     */
    get origin() {
      return this.origin_;
    },

    /**
     * @return {!print_preview.Size} Size of the printable area of the document.
     */
    get size() {
      return this.size_;
    },

    /**
     * @param {print_preview.PrintableArea} other Other printable area to check
     *     for equality.
     * @return {boolean} Whether another printable area is equal to this one.
     */
    equals: function(other) {
      return other != null &&
          this.origin_.equals(other.origin_) &&
          this.size_.equals(other.size_);
    }
  };

  // Export
  return {
    PrintableArea: PrintableArea
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Measurement system of the print preview. Used to parse and serialize point
   * measurements into the system's local units (e.g. millimeters, inches).
   * @param {string} thousandsDelimeter Delimeter between thousands digits.
   * @param {string} decimalDelimeter Delimeter between integers and decimals.
   * @param {!print_preview.MeasurementSystem.UnitType} unitType Measurement
   *     unit type of the system.
   * @constructor
   */
  function MeasurementSystem(thousandsDelimeter, decimalDelimeter, unitType) {
    this.thousandsDelimeter_ = thousandsDelimeter || ',';
    this.decimalDelimeter_ = decimalDelimeter || '.';
    this.unitType_ = unitType;
  };

  /**
   * Parses |numberFormat| and extracts the symbols used for the thousands point
   * and decimal point.
   * @param {string} numberFormat The formatted version of the number 12345678.
   * @return {!Array.<string>} The extracted symbols in the order
   *     [thousandsSymbol, decimalSymbol]. For example,
   *     parseNumberFormat("123,456.78") returns [",", "."].
   */
  MeasurementSystem.parseNumberFormat = function(numberFormat) {
    if (!numberFormat) {
      return [',', '.'];
    }
    var regex = /^(\d+)(\W?)(\d+)(\W?)(\d+)$/;
    var matches = numberFormat.match(regex) || ['', '', ',', '', '.'];
    return [matches[2], matches[4]];
  };

  /**
   * Enumeration of measurement unit types.
   * @enum {number}
   */
  MeasurementSystem.UnitType = {
    METRIC: 0, // millimeters
    IMPERIAL: 1 // inches
  };

  /**
   * Maximum resolution of local unit values.
   * @type {!Object.<!print_preview.MeasurementSystem.UnitType, number>}
   * @private
   */
  MeasurementSystem.Precision_ = {};
  MeasurementSystem.Precision_[MeasurementSystem.UnitType.METRIC] = 0.5;
  MeasurementSystem.Precision_[MeasurementSystem.UnitType.IMPERIAL] = 0.01;

  /**
   * Maximum number of decimal places to keep for local unit.
   * @type {!Object.<!print_preview.MeasurementSystem.UnitType, number>}
   * @private
   */
  MeasurementSystem.DecimalPlaces_ = {};
  MeasurementSystem.DecimalPlaces_[MeasurementSystem.UnitType.METRIC] = 1;
  MeasurementSystem.DecimalPlaces_[MeasurementSystem.UnitType.IMPERIAL] = 2;

  /**
   * Number of points per inch.
   * @type {number}
   * @const
   * @private
   */
  MeasurementSystem.PTS_PER_INCH_ = 72.0;

  /**
   * Number of points per millimeter.
   * @type {number}
   * @const
   * @private
   */
  MeasurementSystem.PTS_PER_MM_ = MeasurementSystem.PTS_PER_INCH_ / 25.4;

  MeasurementSystem.prototype = {
    /** @return {string} The unit type symbol of the measurement system. */
    get unitSymbol() {
      if (this.unitType_ == MeasurementSystem.UnitType.METRIC) {
        return 'mm';
      } else if (this.unitType_ == MeasurementSystem.UnitType.IMPERIAL) {
        return '"';
      } else {
        throw Error('Unit type not supported: ' + this.unitType_);
      }
    },

    /**
     * @return {string} The thousands delimeter character of the measurement
     *     system.
     */
    get thousandsDelimeter() {
      return this.thousandsDelimeter_;
    },

    /**
     * @return {string} The decimal delimeter character of the measurement
     *     system.
     */
    get decimalDelimeter() {
      return this.decimalDelimeter_;
    },

    setSystem: function(thousandsDelimeter, decimalDelimeter, unitType) {
      this.thousandsDelimeter_ = thousandsDelimeter;
      this.decimalDelimeter_ = decimalDelimeter;
      this.unitType_ = unitType;
    },

    /**
     * Rounds a value in the local system's units to the appropriate precision.
     * @param {number} value Value to round.
     * @return {number} Rounded value.
     */
    roundValue: function(value) {
      var precision = MeasurementSystem.Precision_[this.unitType_];
      var roundedValue = Math.round(value / precision) * precision;
      // Truncate
      return roundedValue.toFixed(
          MeasurementSystem.DecimalPlaces_[this.unitType_]);
    },

    /**
     * @param {number} pts Value in points to convert to local units.
     * @return {number} Value in local units.
     */
    convertFromPoints: function(pts) {
      if (this.unitType_ == MeasurementSystem.UnitType.METRIC) {
        return pts / MeasurementSystem.PTS_PER_MM_;
      } else {
        return pts / MeasurementSystem.PTS_PER_INCH_;
      }
    },

    /**
     * @param {number} Value in local units to convert to points.
     * @return {number} Value in points.
     */
    convertToPoints: function(localUnits) {
      if (this.unitType_ == MeasurementSystem.UnitType.METRIC) {
        return localUnits * MeasurementSystem.PTS_PER_MM_;
      } else {
        return localUnits * MeasurementSystem.PTS_PER_INCH_;
      }
    }
  };

  // Export
  return {
    MeasurementSystem: MeasurementSystem
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  // TODO(rltoscano): Maybe clear print ticket when destination changes. Or
  // better yet, carry over any print ticket state that is possible. I.e. if
  // destination changes, the new destination might not support duplex anymore,
  // so we should clear the ticket's isDuplexEnabled state.

  /**
   * Storage of the print ticket and document statistics. Dispatches events when
   * the contents of the print ticket or document statistics change. Also
   * handles validation of the print ticket against destination capabilities and
   * against the document.
   * @param {!print_preview.DestinationStore} destinationStore Used to
   *     understand which printer is selected.
   * @param {!print_preview.AppState} appState Print preview application state.
   * @param {!print_preview.DocumentInfo} documentInfo Document data model.
   * @constructor
   * @extends {cr.EventTarget}
   */
  function PrintTicketStore(destinationStore, appState, documentInfo) {
    cr.EventTarget.call(this);

    /**
     * Destination store used to understand which printer is selected.
     * @type {!print_preview.DestinationStore}
     * @private
     */
    this.destinationStore_ = destinationStore;

    /**
     * App state used to persist and load ticket values.
     * @type {!print_preview.AppState}
     * @private
     */
    this.appState_ = appState;

    /**
     * Information about the document to print.
     * @type {!print_preview.DocumentInfo}
     * @private
     */
    this.documentInfo_ = documentInfo;

    /**
     * Printing capabilities of Chromium and the currently selected destination.
     * @type {!print_preview.CapabilitiesHolder}
     * @private
     */
    this.capabilitiesHolder_ = new print_preview.CapabilitiesHolder();

    /**
     * Current measurement system. Used to work with margin measurements.
     * @type {!print_preview.MeasurementSystem}
     * @private
     */
    this.measurementSystem_ = new print_preview.MeasurementSystem(
        ',', '.', print_preview.MeasurementSystem.UnitType.IMPERIAL);

    /**
     * Collate ticket item.
     * @type {!print_preview.ticket_items.Collate}
     * @private
     */
    this.collate_ = new print_preview.ticket_items.Collate(
        this.appState_, this.destinationStore_);

    /**
     * Color ticket item.
     * @type {!print_preview.ticket_items.Color}
     * @private
     */
    this.color_ = new print_preview.ticket_items.Color(
        this.appState_, this.destinationStore_);

    /**
     * Copies ticket item.
     * @type {!print_preview.ticket_items.Copies}
     * @private
     */
    this.copies_ =
        new print_preview.ticket_items.Copies(this.destinationStore_);

    /**
     * Duplex ticket item.
     * @type {!print_preview.ticket_items.Duplex}
     * @private
     */
    this.duplex_ = new print_preview.ticket_items.Duplex(
        this.appState_, this.destinationStore_);

    /**
     * Page range ticket item.
     * @type {!print_preview.ticket_items.PageRange}
     * @private
     */
    this.pageRange_ =
        new print_preview.ticket_items.PageRange(this.documentInfo_);

    /**
     * Custom margins ticket item.
     * @type {!print_preview.ticket_items.CustomMargins}
     * @private
     */
    this.customMargins_ = new print_preview.ticket_items.CustomMargins(
        this.appState_, this.documentInfo_);

    /**
     * Margins type ticket item.
     * @type {!print_preview.ticket_items.MarginsType}
     * @private
     */
    this.marginsType_ = new print_preview.ticket_items.MarginsType(
        this.appState_, this.documentInfo_, this.customMargins_);

    /**
     * Landscape ticket item.
     * @type {!print_preview.ticket_items.Landscape}
     * @private
     */
    this.landscape_ = new print_preview.ticket_items.Landscape(
        this.appState_, this.destinationStore_, this.documentInfo_,
        this.marginsType_, this.customMargins_);

    /**
     * Header-footer ticket item.
     * @type {!print_preview.ticket_items.HeaderFooter}
     * @private
     */
    this.headerFooter_ = new print_preview.ticket_items.HeaderFooter(
        this.appState_, this.documentInfo_, this.marginsType_,
        this.customMargins_);

    /**
     * Fit-to-page ticket item.
     * @type {!print_preview.ticket_items.FitToPage}
     * @private
     */
    this.fitToPage_ = new print_preview.ticket_items.FitToPage(
        this.documentInfo_, this.destinationStore_);

    /**
     * Print CSS backgrounds ticket item.
     * @type {!print_preview.ticket_items.CssBackground}
     * @private
     */
    this.cssBackground_ = new print_preview.ticket_items.CssBackground(
        this.appState_, this.documentInfo_);

    /**
     * Print selection only ticket item.
     * @type {!print_preview.ticket_items.SelectionOnly}
     * @private
     */
    this.selectionOnly_ =
        new print_preview.ticket_items.SelectionOnly(this.documentInfo_);

    /**
     * Keeps track of event listeners for the print ticket store.
     * @type {!EventTracker}
     * @private
     */
    this.tracker_ = new EventTracker();

    /**
     * Whether the print preview has been initialized.
     * @type {boolean}
     * @private
     */
    this.isInitialized_ = false;

    this.addEventListeners_();
  };

  /**
   * Event types dispatched by the print ticket store.
   * @enum {string}
   */
  PrintTicketStore.EventType = {
    CAPABILITIES_CHANGE: 'print_preview.PrintTicketStore.CAPABILITIES_CHANGE',
    DOCUMENT_CHANGE: 'print_preview.PrintTicketStore.DOCUMENT_CHANGE',
    INITIALIZE: 'print_preview.PrintTicketStore.INITIALIZE',
    TICKET_CHANGE: 'print_preview.PrintTicketStore.TICKET_CHANGE'
  };

  PrintTicketStore.prototype = {
    __proto__: cr.EventTarget.prototype,

    /**
     * Whether the print preview has been initialized.
     * @type {boolean}
     */
    get isInitialized() {
      return this.isInitialized_;
    },

    get collate() {
      return this.collate_;
    },

    get color() {
      return this.color_;
    },

    get copies() {
      return this.copies_;
    },

    get cssBackground() {
      return this.cssBackground_;
    },

    get customMargins() {
      return this.customMargins_;
    },

    get duplex() {
      return this.duplex_;
    },

    get fitToPage() {
      return this.fitToPage_;
    },

    get headerFooter() {
      return this.headerFooter_;
    },

    get landscape() {
      return this.landscape_;
    },

    get marginsType() {
      return this.marginsType_;
    },

    get pageRange() {
      return this.pageRange_;
    },

    get selectionOnly() {
      return this.selectionOnly_;
    },

    /**
     * @return {!print_preview.MeasurementSystem} Measurement system of the
     *     local system.
     */
    get measurementSystem() {
      return this.measurementSystem_;
    },

    /**
     * Initializes the print ticket store. Dispatches an INITIALIZE event.
     * @param {string} thousandsDelimeter Delimeter of the thousands place.
     * @param {string} decimalDelimeter Delimeter of the decimal point.
     * @param {!print_preview.MeasurementSystem.UnitType} unitType Type of unit
     *     of the local measurement system.
     * @param {boolean} selectionOnly Whether only selected content should be
     *     printed.
     */
    init: function(
        thousandsDelimeter, decimalDelimeter, unitType, selectionOnly) {
      this.measurementSystem_.setSystem(thousandsDelimeter, decimalDelimeter,
                                        unitType);
      this.selectionOnly_.updateValue(selectionOnly);

      // Initialize ticket with user's previous values.
      if (this.appState_.hasField(
          print_preview.AppState.Field.IS_COLOR_ENABLED)) {
        this.color_.updateValue(this.appState_.getField(
            print_preview.AppState.Field.IS_COLOR_ENABLED));
      }
      if (this.appState_.hasField(
          print_preview.AppState.Field.IS_DUPLEX_ENABLED)) {
        this.duplex_.updateValue(this.appState_.getField(
            print_preview.AppState.Field.IS_DUPLEX_ENABLED));
      }
      if (this.appState_.hasField(
          print_preview.AppState.Field.IS_LANDSCAPE_ENABLED)) {
        this.landscape_.updateValue(this.appState_.getField(
            print_preview.AppState.Field.IS_LANDSCAPE_ENABLED));
      }
      // Initialize margins after landscape because landscape may reset margins.
      if (this.appState_.hasField(print_preview.AppState.Field.MARGINS_TYPE)) {
        this.marginsType_.updateValue(
            this.appState_.getField(print_preview.AppState.Field.MARGINS_TYPE));
      }
      if (this.appState_.hasField(
          print_preview.AppState.Field.CUSTOM_MARGINS)) {
        this.customMargins_.updateValue(this.appState_.getField(
            print_preview.AppState.Field.CUSTOM_MARGINS));
      }
      if (this.appState_.hasField(
          print_preview.AppState.Field.IS_HEADER_FOOTER_ENABLED)) {
        this.headerFooter_.updateValue(this.appState_.getField(
            print_preview.AppState.Field.IS_HEADER_FOOTER_ENABLED));
      }
      if (this.appState_.hasField(
          print_preview.AppState.Field.IS_COLLATE_ENABLED)) {
        this.collate_.updateValue(this.appState_.getField(
            print_preview.AppState.Field.IS_COLLATE_ENABLED));
      }
      if (this.appState_.hasField(
          print_preview.AppState.Field.IS_CSS_BACKGROUND_ENABLED)) {
        this.cssBackground_.updateValue(this.appState_.getField(
            print_preview.AppState.Field.IS_CSS_BACKGROUND_ENABLED));
      }
    },

    /**
     * @return {boolean} {@code true} if the stored print ticket is valid,
     *     {@code false} otherwise.
     */
    isTicketValid: function() {
      return this.isTicketValidForPreview() &&
          (!this.pageRange_.isCapabilityAvailable() ||
              this.pageRange_.isValid());
    },

    /** @return {boolean} Whether the ticket is valid for preview generation. */
    isTicketValidForPreview: function() {
      return (!this.copies.isCapabilityAvailable() || this.copies.isValid()) &&
          (!this.marginsType_.isCapabilityAvailable() ||
              !this.marginsType_.isValueEqual(
                  print_preview.ticket_items.MarginsType.Value.CUSTOM) ||
              this.customMargins_.isValid());
    },

    /**
     * Adds event listeners for the print ticket store.
     * @private
     */
    addEventListeners_: function() {
      this.tracker_.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.
              SELECTED_DESTINATION_CAPABILITIES_READY,
          this.onSelectedDestinationCapabilitiesReady_.bind(this));
      // TODO(rltoscano): Print ticket store shouldn't be re-dispatching these
      // events, the consumers of the print ticket store events should listen
      // for the events from document info instead. Will move this when
      // consumers are all migrated.
      this.tracker_.add(
          this.documentInfo_,
          print_preview.DocumentInfo.EventType.CHANGE,
          this.onDocumentInfoChange_.bind(this));
    },

    /**
     * Called when the capabilities of the selected destination are ready.
     * @private
     */
    onSelectedDestinationCapabilitiesReady_: function() {
      var caps = this.destinationStore_.selectedDestination.capabilities;
      var isFirstUpdate = this.capabilitiesHolder_.get() == null;
      this.capabilitiesHolder_.set(caps);
      if (isFirstUpdate) {
        this.isInitialized_ = true;
        cr.dispatchSimpleEvent(this, PrintTicketStore.EventType.INITIALIZE);
      } else {
        // Reset user selection for certain ticket items.
        this.customMargins_.updateValue(null);

        if (this.marginsType_.getValue() ==
            print_preview.ticket_items.MarginsType.Value.CUSTOM) {
          this.marginsType_.updateValue(
              print_preview.ticket_items.MarginsType.Value.DEFAULT);
        }
        cr.dispatchSimpleEvent(
            this, PrintTicketStore.EventType.CAPABILITIES_CHANGE);
      }
    },

    /**
     * Called when document data model has changed. Dispatches a print ticket
     * store event.
     * @private
     */
    onDocumentInfoChange_: function() {
      cr.dispatchSimpleEvent(this, PrintTicketStore.EventType.DOCUMENT_CHANGE);
    },
  };

  // Export
  return {
    PrintTicketStore: PrintTicketStore
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Immutable two dimensional point in space. The units of the dimensions are
   * undefined.
   * @param {number} x X-dimension of the point.
   * @param {number} y Y-dimension of the point.
   * @constructor
   */
  function Coordinate2d(x, y) {
    /**
     * X-dimension of the point.
     * @type {number}
     * @private
     */
    this.x_ = x;

    /**
     * Y-dimension of the point.
     * @type {number}
     * @private
     */
    this.y_ = y;
  };

  Coordinate2d.prototype = {
    /** @return {number} X-dimension of the point. */
    get x() {
      return this.x_;
    },

    /** @return {number} Y-dimension of the point. */
    get y() {
      return this.y_;
    },

    /**
     * @param {number} x Amount to translate in the X dimension.
     * @param {number} y Amount to translate in the Y dimension.
     * @return {!print_preview.Coordinate2d} A new two-dimensional point
     *     translated along the X and Y dimensions.
     */
    translate: function(x, y) {
      return new Coordinate2d(this.x_ + x, this.y_ + y);
    },

    /**
     * @param {number} factor Amount to scale the X and Y dimensions.
     * @return {!print_preview.Coordinate2d} A new two-dimensional point scaled
     *     by the given factor.
     */
    scale: function(factor) {
      return new Coordinate2d(this.x_ * factor, this.y_ * factor);
    },

    /**
     * @param {print_preview.Coordinate2d} other The point to compare against.
     * @return {boolean} Whether another point is equal to this one.
     */
    equals: function(other) {
      return other != null &&
          this.x_ == other.x_ &&
          this.y_ == other.y_;
    }
  };

  // Export
  return {
    Coordinate2d: Coordinate2d
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Immutable two-dimensional size.
   * @param {number} width Width of the size.
   * @param {number} height Height of the size.
   * @constructor
   */
  function Size(width, height) {
    /**
     * Width of the size.
     * @type {number}
     * @private
     */
    this.width_ = width;

    /**
     * Height of the size.
     * @type {number}
     * @private
     */
    this.height_ = height;
  };

  Size.prototype = {
    /** @return {number} Width of the size. */
    get width() {
      return this.width_;
    },

    /** @return {number} Height of the size. */
    get height() {
      return this.height_;
    },

    /**
     * @param {print_preview.Size} other Other size object to compare against.
     * @return {boolean} Whether this size object is equal to another.
     */
    equals: function(other) {
      return other != null &&
          this.width_ == other.width_ &&
          this.height_ == other.height_;
    }
  };

  // Export
  return {
    Size: Size
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Mutable reference to a CDD object.
   * @constructor
   */
  function CapabilitiesHolder() {
    /**
     * Reference to the capabilities object.
     * @type {print_preview.Cdd}
     * @private
     */
    this.capabilities_ = null;
  };

  CapabilitiesHolder.prototype = {
    /** @return {print_preview.Cdd} The instance held by the holder. */
    get: function() {
      return this.capabilities_;
    },

    /** @param {!print_preview.Cdd} New instance to put into the holder. */
    set: function(capabilities) {
      this.capabilities_ = capabilities;
    }
  };

  // Export
  return {
    CapabilitiesHolder: CapabilitiesHolder
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Repository which stores information about the user. Events are dispatched
   * when the information changes.
   * @constructor
   * @extends {cr.EventTarget}
   */
  function UserInfo() {
    cr.EventTarget.call(this);

    /**
     * Tracker used to keep track of event listeners.
     * @type {!EventTracker}
     * @private
     */
    this.tracker_ = new EventTracker();

    /**
     * Email address of the logged in user or {@code null} if no user is logged
     * in.
     * @type {?string}
     * @private
     */
    this.userEmail_ = null;
  };

  /**
   * Enumeration of event types dispatched by the user info.
   * @enum {string}
   */
  UserInfo.EventType = {
    EMIAL_CHANGE: 'print_preview.UserInfo.EMAIL_CHANGE'
  };

  UserInfo.prototype = {
    __proto__: cr.EventTarget.prototype,

    /**
     * @return {?string} Email address of the logged in user or {@code null} if
     *     no user is logged.
     */
    getUserEmail: function() {
      return this.userEmail_;
    },

    /**
     * @param {!cloudprint.CloudPrintInterface} cloudPrintInterface Interface
     *     to Google Cloud Print that the print preview uses.
     */
    setCloudPrintInterface: function(cloudPrintInterface) {
      this.tracker_.add(
          cloudPrintInterface,
          cloudprint.CloudPrintInterface.EventType.SEARCH_DONE,
          this.onCloudPrintSearchDone_.bind(this));
    },

    /** Removes all event listeners. */
    removeEventListeners: function() {
      this.tracker_.removeAll();
    },

    /**
     * Called when a Google Cloud Print printer search completes. Updates user
     * information.
     * @type {Event} event Contains information about the logged in user.
     * @private
     */
    onCloudPrintSearchDone_: function(event) {
      if (event.origin == print_preview.Destination.Origin.COOKIES) {
        this.userEmail_ = event.email;
        cr.dispatchSimpleEvent(this, UserInfo.EventType.EMAIL_CHANGE);
      }
    }
  };

  return {
    UserInfo: UserInfo
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Object used to get and persist the print preview application state.
   * @constructor
   */
  function AppState() {
    /**
     * Internal representation of application state.
     * @type {Object.<string: Object>}
     * @private
     */
    this.state_ = {};
    this.state_[AppState.Field.VERSION] = AppState.VERSION_;
    this.state_[AppState.Field.IS_GCP_PROMO_DISMISSED] = true;

    /**
     * Whether the app state has been initialized. The app state will ignore all
     * writes until it has been initialized.
     * @type {boolean}
     * @private
     */
    this.isInitialized_ = false;
  };

  /**
   * Enumeration of field names for serialized app state.
   * @enum {string}
   */
  AppState.Field = {
    VERSION: 'version',
    SELECTED_DESTINATION_ID: 'selectedDestinationId',
    SELECTED_DESTINATION_ORIGIN: 'selectedDestinationOrigin',
    IS_SELECTED_DESTINATION_LOCAL: 'isSelectedDestinationLocal',  // Deprecated
    IS_GCP_PROMO_DISMISSED: 'isGcpPromoDismissed',
    MARGINS_TYPE: 'marginsType',
    CUSTOM_MARGINS: 'customMargins',
    IS_COLOR_ENABLED: 'isColorEnabled',
    IS_DUPLEX_ENABLED: 'isDuplexEnabled',
    IS_HEADER_FOOTER_ENABLED: 'isHeaderFooterEnabled',
    IS_LANDSCAPE_ENABLED: 'isLandscapeEnabled',
    IS_COLLATE_ENABLED: 'isCollateEnabled',
    IS_CSS_BACKGROUND_ENABLED: 'isCssBackgroundEnabled'
  };

  /**
   * Current version of the app state. This value helps to understand how to
   * parse earlier versions of the app state.
   * @type {number}
   * @const
   * @private
   */
  AppState.VERSION_ = 2;

  /**
   * Name of C++ layer function to persist app state.
   * @type {string}
   * @const
   * @private
   */
  AppState.NATIVE_FUNCTION_NAME_ = 'saveAppState';

  AppState.prototype = {
    /** @return {?string} ID of the selected destination. */
    get selectedDestinationId() {
      return this.state_[AppState.Field.SELECTED_DESTINATION_ID];
    },

    /** @return {?string} Origin of the selected destination. */
    get selectedDestinationOrigin() {
      return this.state_[AppState.Field.SELECTED_DESTINATION_ORIGIN];
    },

    /** @return {boolean} Whether the GCP promotion has been dismissed. */
    get isGcpPromoDismissed() {
      return this.state_[AppState.Field.IS_GCP_PROMO_DISMISSED];
    },

    /**
     * @param {!print_preview.AppState.Field} field App state field to check if
     *     set.
     * @return {boolean} Whether a field has been set in the app state.
     */
    hasField: function(field) {
      return this.state_.hasOwnProperty(field);
    },

    /**
     * @param {!print_preview.AppState.Field} field App state field to get.
     * @return {Object} Value of the app state field.
     */
    getField: function(field) {
      if (field == AppState.Field.CUSTOM_MARGINS) {
        return this.state_[field] ?
            print_preview.Margins.parse(this.state_[field]) : null;
      } else {
        return this.state_[field];
      }
    },

    /**
     * Initializes the app state from a serialized string returned by the native
     * layer.
     * @param {?string} serializedAppStateStr Serialized string representation
     *     of the app state.
     */
    init: function(serializedAppStateStr) {
      if (serializedAppStateStr) {
        var state = JSON.parse(serializedAppStateStr);
        if (state[AppState.Field.VERSION] == AppState.VERSION_) {
          if (state.hasOwnProperty(
              AppState.Field.IS_SELECTED_DESTINATION_LOCAL)) {
            state[AppState.Field.SELECTED_DESTINATION_ORIGIN] =
                state[AppState.Field.IS_SELECTED_DESTINATION_LOCAL] ?
                print_preview.Destination.Origin.LOCAL :
                print_preview.Destination.Origin.COOKIES;
            delete state[AppState.Field.IS_SELECTED_DESTINATION_LOCAL];
          }
          this.state_ = state;
        }
      } else {
        // Set some state defaults.
        this.state_[AppState.Field.IS_GCP_PROMO_DISMISSED] = false;
      }
    },

    /**
     * Sets to initialized state. Now object will accept persist requests.
     */
    setInitialized: function() {
      this.isInitialized_ = true;
    },

    /**
     * Persists the given value for the given field.
     * @param {!print_preview.AppState.Field} field Field to persist.
     * @param {Object} value Value of field to persist.
     */
    persistField: function(field, value) {
      if (!this.isInitialized_)
        return;
      if (field == AppState.Field.CUSTOM_MARGINS) {
        this.state_[field] = value ? value.serialize() : null;
      } else {
        this.state_[field] = value;
      }
      this.persist_();
    },

    /**
     * Persists the selected destination.
     * @param {!print_preview.Destination} dest Destination to persist.
     */
    persistSelectedDestination: function(dest) {
      if (!this.isInitialized_)
        return;
      this.state_[AppState.Field.SELECTED_DESTINATION_ID] = dest.id;
      this.state_[AppState.Field.SELECTED_DESTINATION_ORIGIN] = dest.origin;
      this.persist_();
    },

    /**
     * Persists whether the GCP promotion has been dismissed.
     * @param {boolean} isGcpPromoDismissed Whether the GCP promotion has been
     *     dismissed.
     */
    persistIsGcpPromoDismissed: function(isGcpPromoDismissed) {
      if (!this.isInitialized_)
        return;
      this.state_[AppState.Field.IS_GCP_PROMO_DISMISSED] = isGcpPromoDismissed;
      this.persist_();
    },

    /**
     * Calls into the native layer to persist the application state.
     * @private
     */
    persist_: function() {
      chrome.send(AppState.NATIVE_FUNCTION_NAME_,
                  [JSON.stringify(this.state_)]);
    }
  };

  return {
    AppState: AppState
  };
});


// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * An object that represents a user modifiable item in a print ticket. Each
   * ticket item has a value which can be set by the user. Ticket items can also
   * be unavailable for modifying if the print destination doesn't support it or
   * if other ticket item constraints are not met.
   * @param {print_preview.AppState=} appState Application state model to update
   *     when ticket items update.
   * @param {print_preview.AppState.Field=} field Field of the app state to
   *     update when ticket item is updated.
   * @param {print_preview.DestinationStore=} destinationStore Used listen for
   *     changes in the currently selected destination's capabilities. Since
   *     this is a common dependency of ticket items, it's handled in the base
   *     class.
   * @param {print_preview.DocumentInfo=} documentInfo Used to listen for
   *     changes in the document. Since this is a common dependency of ticket
   *     items, it's handled in the base class.
   * @constructor
   */
  function TicketItem(appState, field, destinationStore, documentInfo) {
    cr.EventTarget.call(this);

    /**
     * Application state model to update when ticket items update.
     * @type {print_preview.AppState}
     * @private
     */
    this.appState_ = appState || null;

    /**
     * Field of the app state to update when ticket item is updated.
     * @type {print_preview.AppState.Field}
     * @private
     */
    this.field_ = field || null;

    /**
     * Used listen for changes in the currently selected destination's
     * capabilities.
     * @type {print_preview.DestinationStore}
     * @private
     */
    this.destinationStore_ = destinationStore || null;

    /**
     * Used to listen for changes in the document.
     * @type {print_preview.DocumentInfo}
     * @private
     */
    this.documentInfo_ = documentInfo || null;

    /**
     * Backing store of the print ticket item.
     * @type {Object}
     * @private
     */
    this.value_ = null;

    /**
     * Keeps track of event listeners for the ticket item.
     * @type {!EventTracker}
     * @private
     */
    this.tracker_ = new EventTracker();

    this.addEventHandlers_();
  };

  /**
   * Event types dispatched by this class.
   * @enum {string}
   */
  TicketItem.EventType = {
    CHANGE: 'print_preview.ticket_items.TicketItem.CHANGE'
  };

  TicketItem.prototype = {
    __proto__: cr.EventTarget.prototype,

    /**
     * Determines whether a given value is valid for the ticket item.
     * @param {Object} value The value to check for validity.
     * @return {boolean} Whether the given value is valid for the ticket item.
     */
    wouldValueBeValid: function(value) {
      throw Error('Abstract method not overridden');
    },

    /**
     * @return {boolean} Whether the print destination capability is available.
     */
    isCapabilityAvailable: function() {
      throw Error('Abstract method not overridden');
    },

    /** @return {!Object} The value of the ticket item. */
    getValue: function() {
      if (this.isCapabilityAvailable()) {
        if (this.value_ == null) {
          return this.getDefaultValueInternal();
        } else {
          return this.value_;
        }
      } else {
        return this.getCapabilityNotAvailableValueInternal();
      }
    },

    /** @return {boolean} Whether the ticket item was modified by the user. */
    isUserEdited: function() {
      return this.value_ != null;
    },

    /** @return {boolean} Whether the ticket item's value is valid. */
    isValid: function() {
      if (!this.isUserEdited()) {
        return true;
      }
      return this.wouldValueBeValid(this.value_);
    },

    /**
     * @param {Object} value Value to compare to the value of this ticket item.
     * @return {boolean} Whether the given value is equal to the value of the
     *     ticket item.
     */
    isValueEqual: function(value) {
      return this.getValue() == value;
    },

    /** @param {!Object} Value to set as the value of the ticket item. */
    updateValue: function(value) {
      // Use comparison with capabilities for event.
      var sendUpdateEvent = !this.isValueEqual(value);
      // Don't lose requested value if capability is not available.
      this.updateValueInternal(value);
      if (this.appState_) {
        this.appState_.persistField(this.field_, value);
      }
      if (sendUpdateEvent)
        cr.dispatchSimpleEvent(this, TicketItem.EventType.CHANGE);
    },

    /**
     * @return {!Object} Default value of the ticket item if no value was set by
     *     the user.
     * @protected
     */
    getDefaultValueInternal: function() {
      throw Error('Abstract method not overridden');
    },

    /**
     * @return {!Object} Default value of the ticket item if the capability is
     *     not available.
     * @protected
     */
    getCapabilityNotAvailableValueInternal: function() {
      throw Error('Abstract method not overridden');
    },

    /**
     * @return {!EventTracker} Event tracker to keep track of events from
     *     dependencies.
     * @protected
     */
    getTrackerInternal: function() {
      return this.tracker_;
    },

    /**
     * @return {print_preview.Destination} Selected destination from the
     *     destination store, or {@code null} if no destination is selected.
     * @protected
     */
    getSelectedDestInternal: function() {
      return this.destinationStore_ ?
          this.destinationStore_.selectedDestination : null;
    },

    /**
     * @return {print_preview.DocumentInfo} Document data model.
     * @protected
     */
    getDocumentInfoInternal: function() {
      return this.documentInfo_;
    },

    /**
     * Dispatches a CHANGE event.
     * @protected
     */
    dispatchChangeEventInternal: function() {
      cr.dispatchSimpleEvent(
          this, print_preview.ticket_items.TicketItem.EventType.CHANGE);
    },

    /**
     * Updates the value of the ticket item without dispatching any events or
     * persisting the value.
     * @protected
     */
    updateValueInternal: function(value) {
      this.value_ = value;
    },

    /**
     * Adds event handlers for this class.
     * @private
     */
    addEventHandlers_: function() {
      if (this.destinationStore_) {
        this.tracker_.add(
            this.destinationStore_,
            print_preview.DestinationStore.EventType.
                SELECTED_DESTINATION_CAPABILITIES_READY,
            this.dispatchChangeEventInternal.bind(this));
      }
      if (this.documentInfo_) {
        this.tracker_.add(
            this.documentInfo_,
            print_preview.DocumentInfo.EventType.CHANGE,
            this.dispatchChangeEventInternal.bind(this));
      }
    },
  };

  // Export
  return {
    TicketItem: TicketItem
  };
});


// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Custom page margins ticket item whose value is a
   * {@code print_preview.Margins}.
   * @param {!print_preview.AppState} appState App state used to persist custom
   *     margins.
   * @param {!print_preview.DocumentInfo} documentInfo Information about the
   *     document to print.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function CustomMargins(appState, documentInfo) {
    print_preview.ticket_items.TicketItem.call(
        this,
        appState,
        print_preview.AppState.Field.CUSTOM_MARGINS,
        null /*destinationStore*/,
        documentInfo);
  };

  /**
   * Enumeration of the orientations of margins.
   * @enum {string}
   */
  CustomMargins.Orientation = {
    TOP: 'top',
    RIGHT: 'right',
    BOTTOM: 'bottom',
    LEFT: 'left'
  };

  /**
   * Mapping of a margin orientation to its opposite.
   * @type {!Object.<!CustomMargins.Orientation, !CustomMargins.Orientation>}
   * @private
   */
  CustomMargins.OppositeOrientation_ = {};
  CustomMargins.OppositeOrientation_[CustomMargins.Orientation.TOP] =
      CustomMargins.Orientation.BOTTOM;
  CustomMargins.OppositeOrientation_[CustomMargins.Orientation.RIGHT] =
      CustomMargins.Orientation.LEFT;
  CustomMargins.OppositeOrientation_[CustomMargins.Orientation.BOTTOM] =
      CustomMargins.Orientation.TOP;
  CustomMargins.OppositeOrientation_[CustomMargins.Orientation.LEFT] =
      CustomMargins.Orientation.RIGHT;

  /**
   * Minimum distance in points that two margins can be separated by.
   * @type {number}
   * @const
   * @private
   */
  CustomMargins.MINIMUM_MARGINS_DISTANCE_ = 72; // 1 inch.

  CustomMargins.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      var margins = /** @type {!print_preview.Margins} */ (value);
      for (var key in CustomMargins.Orientation) {
        var o = CustomMargins.Orientation[key];
        var max = this.getMarginMax_(
            o, margins.get(CustomMargins.OppositeOrientation_[o]));
        if (margins.get(o) > max || margins.get(o) < 0) {
          return false;
        }
      }
      return true;
    },

    /** @override */
    isCapabilityAvailable: function() {
      return this.getDocumentInfoInternal().isModifiable;
    },

    /** @override */
    isValueEqual: function(value) {
      return this.getValue().equals(value);
    },

    /**
     * @param {!print_preview.ticket_items.CustomMargins.Orientation}
     *     orientation Specifies the margin to get the maximum value for.
     * @return {number} Maximum value in points of the specified margin.
     */
    getMarginMax: function(orientation) {
      var oppositeOrient = CustomMargins.OppositeOrientation_[orientation];
      var margins = /** @type {!print_preview.Margins} */ (this.getValue());
      return this.getMarginMax_(orientation, margins.get(oppositeOrient));
    },

    /** @override */
    updateValue: function(value) {
      var margins = /** @type {!print_preview.Margins} */ (value);
      if (margins != null) {
        margins = new print_preview.Margins(
            Math.round(margins.get(CustomMargins.Orientation.TOP)),
            Math.round(margins.get(CustomMargins.Orientation.RIGHT)),
            Math.round(margins.get(CustomMargins.Orientation.BOTTOM)),
            Math.round(margins.get(CustomMargins.Orientation.LEFT)));
      }
      print_preview.ticket_items.TicketItem.prototype.updateValue.call(
          this, margins);
    },

    /**
     * Updates the specified margin in points while keeping the value within
     * a maximum and minimum.
     * @param {!print_preview.ticket_items.CustomMargins.Orientation}
     *     orientation Specifies the margin to update.
     * @param {number} value Updated margin value in points.
     */
    updateMargin: function(orientation, value) {
      var margins = /** @type {!print_preview.Margins} */ (this.getValue());
      var oppositeOrientation = CustomMargins.OppositeOrientation_[orientation];
      var max =
          this.getMarginMax_(orientation, margins.get(oppositeOrientation));
      value = Math.max(0, Math.min(max, value));
      this.updateValue(margins.set(orientation, value));
    },

    /** @override */
    getDefaultValueInternal: function() {
      return this.getDocumentInfoInternal().margins ||
             new print_preview.Margins(72, 72, 72, 72);
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      return this.getDocumentInfoInternal().margins ||
             new print_preview.Margins(72, 72, 72, 72);
    },

    /**
     * @param {!print_preview.ticket_items.CustomMargins.Orientation}
     *     orientation Specifies which margin to get the maximum value of.
     * @param {number} oppositeMargin Value of the margin in points
     *     opposite the specified margin.
     * @return {number} Maximum value in points of the specified margin.
     * @private
     */
    getMarginMax_: function(orientation, oppositeMargin) {
      var max;
      if (orientation == CustomMargins.Orientation.TOP ||
          orientation == CustomMargins.Orientation.BOTTOM) {
        max = this.getDocumentInfoInternal().pageSize.height - oppositeMargin -
            CustomMargins.MINIMUM_MARGINS_DISTANCE_;
      } else {
        max = this.getDocumentInfoInternal().pageSize.width - oppositeMargin -
            CustomMargins.MINIMUM_MARGINS_DISTANCE_;
      }
      return Math.round(max);
    }
  };

  // Export
  return {
    CustomMargins: CustomMargins
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Collate ticket item whose value is a {@code boolean} that indicates whether
   * collation is enabled.
   * @param {!print_preview.AppState} appState App state used to persist collate
   *     selection.
   * @param {!print_preview.DestinationStore} destinationStore Destination store
   *     used determine if a destination has the collate capability.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function Collate(appState, destinationStore) {
    print_preview.ticket_items.TicketItem.call(
        this,
        appState,
        print_preview.AppState.Field.IS_COLLATE_ENABLED,
        destinationStore);
  };

  Collate.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      return true;
    },

    /** @override */
    isCapabilityAvailable: function() {
      return !!this.getCollateCapability_();
    },

    /** @override */
    getDefaultValueInternal: function() {
      return this.getCollateCapability_().default || false;
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      return false;
    },

    /**
     * @return {Object} Collate capability of the selected destination.
     * @private
     */
    getCollateCapability_: function() {
      var dest = this.getSelectedDestInternal();
      return (dest &&
              dest.capabilities &&
              dest.capabilities.printer &&
              dest.capabilities.printer.collate) ||
             null;
    }
  };

  // Export
  return {
    Collate: Collate
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Color ticket item whose value is a {@code boolean} that indicates whether
   * the document should be printed in color.
   * @param {!print_preview.AppState} appState App state persistence object to
   *     save the state of the color selection.
   * @param {!print_preview.DestinationStore} destinationStore Used to determine
   *     whether color printing should be available.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function Color(appState, destinationStore) {
    print_preview.ticket_items.TicketItem.call(
        this,
        appState,
        print_preview.AppState.Field.IS_COLOR_ENABLED,
        destinationStore);
  };

  Color.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      return true;
    },

    /** @override */
    isCapabilityAvailable: function() {
      var colorCap = this.getColorCapability_();
      if (!colorCap) {
        return false;
      }
      var hasColor = false;
      var hasMonochrome = false;
      colorCap.option.forEach(function(option) {
        hasColor = hasColor || option.type == 'STANDARD_COLOR';
        hasMonochrome = hasMonochrome || option.type == 'STANDARD_MONOCHROME';
      });
      return hasColor && hasMonochrome;
    },

    /** @override */
    getDefaultValueInternal: function() {
      var colorCap = this.getColorCapability_();
      var defaultOption = this.getDefaultColorOption_(colorCap.option);
      return defaultOption && defaultOption.type == 'STANDARD_COLOR';
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      var colorCap = this.getColorCapability_();
      var defaultOption = colorCap ?
          this.getDefaultColorOption_(colorCap.option) : null;

      // TODO(rltoscano): Get rid of this check based on destination ID. These
      // destinations should really update their CDDs to have only one color
      // option that has type 'STANDARD_COLOR'.
      var dest = this.getSelectedDestInternal();
      if (!dest) {
        return false;
      }
      return dest.id == print_preview.Destination.GooglePromotedId.DOCS ||
          dest.id == print_preview.Destination.GooglePromotedId.FEDEX ||
          dest.type == print_preview.Destination.Type.MOBILE ||
          defaultOption && defaultOption.type == 'STANDARD_COLOR';
    },

    /**
     * @return {Object} Color capability of the selected destination.
     * @private
     */
    getColorCapability_: function() {
      var dest = this.getSelectedDestInternal();
      return (dest &&
              dest.capabilities &&
              dest.capabilities.printer &&
              dest.capabilities.printer.color) ||
             null;
    },

    /**
     * @param options {!Array.<!Object.<{type: string=, is_default: boolean=}>>
     * @return {Object.<{type: string=, is_default: boolean=}>} Default color
     *     option of the given list.
     * @private
     */
    getDefaultColorOption_: function(options) {
      var defaultOptions = options.filter(function(option) {
        return option.is_default;
      });
      return (defaultOptions.length == 0) ? null : defaultOptions[0];
    }
  };

  // Export
  return {
    Color: Color
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Copies ticket item whose value is a {@code string} that indicates how many
   * copies of the document should be printed. The ticket item is backed by a
   * string since the user can textually input the copies value.
   * @param {!print_preview.DestinationStore} destinationStore Destination store
   *     used determine if a destination has the copies capability.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function Copies(destinationStore) {
    print_preview.ticket_items.TicketItem.call(
        this, null /*appState*/, null /*field*/, destinationStore);
  };

  Copies.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      if (/[^\d]+/.test(value)) {
        return false;
      }
      var copies = parseInt(value);
      if (copies > 999 || copies < 1) {
        return false;
      }
      return true;
    },

    /** @override */
    isCapabilityAvailable: function() {
      return !!this.getCopiesCapability_();
    },

    /** @return {number} The number of copies indicated by the ticket item. */
    getValueAsNumber: function() {
      return parseInt(this.getValue());
    },

    /** @override */
    getDefaultValueInternal: function() {
      var cap = this.getCopiesCapability_();
      return cap.hasOwnProperty('default') ? cap.default : '';
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      return '1';
    },

    /**
     * @return {Object} Copies capability of the selected destination.
     * @private
     */
    getCopiesCapability_: function() {
      var dest = this.getSelectedDestInternal();
      return (dest &&
              dest.capabilities &&
              dest.capabilities.printer &&
              dest.capabilities.printer.copies) ||
             null;
    }
  };

  // Export
  return {
    Copies: Copies
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Duplex ticket item whose value is a {@code boolean} that indicates whether
   * the document should be duplex printed.
   * @param {!print_preview.AppState} appState App state used to persist collate
   *     selection.
   * @param {!print_preview.DestinationStore} destinationStore Destination store
   *     used determine if a destination has the collate capability.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function Duplex(appState, destinationStore) {
    print_preview.ticket_items.TicketItem.call(
        this,
        appState,
        print_preview.AppState.Field.IS_DUPLEX_ENABLED,
        destinationStore);
  };

  Duplex.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      return true;
    },

    /** @override */
    isCapabilityAvailable: function() {
      var cap = this.getDuplexCapability_();
      if (!cap) {
        return false;
      }
      var hasLongEdgeOption = false;
      var hasSimplexOption = false;
      cap.option.forEach(function(option) {
        hasLongEdgeOption = hasLongEdgeOption || option.type == 'LONG_EDGE';
        hasSimplexOption = hasSimplexOption || option.type == 'NO_DUPLEX';
      });
      return hasLongEdgeOption && hasSimplexOption;
    },

    /** @override */
    getDefaultValueInternal: function() {
      var cap = this.getDuplexCapability_();
      var defaultOptions = cap.option.filter(function(option) {
        return option.is_default;
      });
      return defaultOptions.length == 0 ? false :
                                          defaultOptions[0].type == 'LONG_EDGE';
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      return false;
    },

    /**
     * @return {Object} Duplex capability of the selected destination.
     * @private
     */
    getDuplexCapability_: function() {
      var dest = this.getSelectedDestInternal();
      return (dest &&
              dest.capabilities &&
              dest.capabilities.printer &&
              dest.capabilities.printer.duplex) ||
             null;
    }
  };

  // Export
  return {
    Duplex: Duplex
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Header-footer ticket item whose value is a {@code boolean} that indicates
   * whether the document should be printed with headers and footers.
   * @param {!print_preview.AppState} appState App state used to persist whether
   *     header-footer is enabled.
   * @param {!print_preview.DocumentInfo} documentInfo Information about the
   *     document to print.
   * @param {!print_preview.ticket_items.MarginsType} marginsType Ticket item
   *     that stores which predefined margins to print with.
   * @param {!print_preview.ticket_items.CustomMargins} customMargins Ticket
   *     item that stores custom margin values.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function HeaderFooter(appState, documentInfo, marginsType, customMargins) {
    print_preview.ticket_items.TicketItem.call(
        this,
        appState,
        print_preview.AppState.Field.IS_HEADER_FOOTER_ENABLED,
        null /*destinationStore*/,
        documentInfo);

    /**
     * Ticket item that stores which predefined margins to print with.
     * @type {!print_preview.ticket_items.MarginsType}
     * @private
     */
    this.marginsType_ = marginsType;

    /**
     * Ticket item that stores custom margin values.
     * @type {!print_preview.ticket_items.CustomMargins}
     * @private
     */
    this.customMargins_ = customMargins;

    this.addEventListeners_();
  };

  HeaderFooter.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      return true;
    },

    /** @override */
    isCapabilityAvailable: function() {
      if (!this.getDocumentInfoInternal().isModifiable) {
        return false;
      } else if (this.marginsType_.getValue() ==
          print_preview.ticket_items.MarginsType.Value.NO_MARGINS) {
        return false;
      } else if (this.marginsType_.getValue() ==
          print_preview.ticket_items.MarginsType.Value.MINIMUM) {
        return true;
      }
      var margins;
      if (this.marginsType_.getValue() ==
          print_preview.ticket_items.MarginsType.Value.CUSTOM) {
        if (!this.customMargins_.isValid()) {
          return false;
        }
        margins = this.customMargins_.getValue();
      } else {
        margins = this.getDocumentInfoInternal().margins;
      }
      var orientEnum = print_preview.ticket_items.CustomMargins.Orientation;
      return margins == null ||
             margins.get(orientEnum.TOP) > 0 ||
             margins.get(orientEnum.BOTTOM) > 0;
    },

    /** @override */
    getDefaultValueInternal: function() {
      return true;
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      return false;
    },

    /**
     * Adds CHANGE listeners to dependent ticket items.
     * @private
     */
    addEventListeners_: function() {
      this.getTrackerInternal().add(
          this.marginsType_,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.dispatchChangeEventInternal.bind(this));
      this.getTrackerInternal().add(
          this.customMargins_,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.dispatchChangeEventInternal.bind(this));
    }
  };

  // Export
  return {
    HeaderFooter: HeaderFooter
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Landscape ticket item whose value is a {@code boolean} that indicates
   * whether the document should be printed in landscape orientation.
   * @param {!print_preview.AppState} appState App state object used to persist
   *     ticket item values.
   * @param {!print_preview.DestinationStore} destinationStore Destination store
   *     used to determine the default landscape value and if landscape
   *     printing is available.
   * @param {!print_preview.DocumentInfo} documentInfo Information about the
   *     document to print.
   * @param {!print_preview.ticket_items.MarginsType} marginsType Reset when
   *     landscape value changes.
   * @param {!print_preview.ticket_items.CustomMargins} customMargins Reset when
   *     landscape value changes.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function Landscape(appState, destinationStore, documentInfo, marginsType,
                     customMargins) {
    print_preview.ticket_items.TicketItem.call(
        this,
        appState,
        print_preview.AppState.Field.IS_LANDSCAPE_ENABLED,
        destinationStore,
        documentInfo);

    /**
     * Margins ticket item. Reset when landscape ticket item changes.
     * @type {!print_preview.ticket_items.MarginsType}
     * @private
     */
    this.marginsType_ = marginsType;

    /**
     * Custom margins ticket item. Reset when landscape ticket item changes.
     * @type {!print_preview.ticket_items.CustomMargins}
     * @private
     */
    this.customMargins_ = customMargins;
  };

  Landscape.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      return true;
    },

    /** @override */
    isCapabilityAvailable: function() {
      var cap = this.getPageOrientationCapability_();
      if (!cap) {
        return false;
      }
      var hasAutoOrPortraitOption = false;
      var hasLandscapeOption = false;
      cap.option.forEach(function(option) {
        hasAutoOrPortraitOption = hasAutoOrPortraitOption ||
            option.type == 'AUTO' ||
            option.type == 'PORTRAIT';
        hasLandscapeOption = hasLandscapeOption || option.type == 'LANDSCAPE';
      });
      // TODO(rltoscano): Technically, the print destination can still change
      // the orientation of the print out (at least for cloud printers) if the
      // document is not modifiable. But the preview wouldn't update in this
      // case so it would be a bad user experience.
      return this.getDocumentInfoInternal().isModifiable &&
          !this.getDocumentInfoInternal().hasCssMediaStyles &&
          hasAutoOrPortraitOption &&
          hasLandscapeOption;
    },

    /** @override */
    getDefaultValueInternal: function() {
      var cap = this.getPageOrientationCapability_();
      var defaultOptions = cap.option.filter(function(option) {
        return option.is_default;
      });
      return defaultOptions.length == 0 ? false :
                                          defaultOptions[0].type == 'LANDSCAPE';
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      var doc = this.getDocumentInfoInternal();
      return doc.hasCssMediaStyles ?
          (doc.pageSize.width > doc.pageSize.height) :
          false;
    },

    /** @override */
    updateValueInternal: function(value) {
      var updateMargins = !this.isValueEqual(value);
      print_preview.ticket_items.TicketItem.prototype.updateValueInternal.call(
          this, value);
      if (updateMargins) {
        // Reset the user set margins when page orientation changes.
        this.marginsType_.updateValue(
          print_preview.ticket_items.MarginsType.Value.DEFAULT);
        this.customMargins_.updateValue(null);
      }
    },

    /**
     * @return {Object} Page orientation capability of the selected destination.
     * @private
     */
    getPageOrientationCapability_: function() {
      var dest = this.getSelectedDestInternal();
      return (dest &&
              dest.capabilities &&
              dest.capabilities.printer &&
              dest.capabilities.printer.page_orientation) ||
             null;
    }
  };

  // Export
  return {
    Landscape: Landscape
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Margins type ticket item whose value is a
   * {@link print_preview.ticket_items.MarginsType.Value} that indicates what
   * predefined margins type to use.
   * @param {!print_preview.AppState} appState App state persistence object to
   *     save the state of the margins type selection.
   * @param {!print_preview.DocumentInfo} documentInfo Information about the
   *     document to print.
   * @param {!print_preview.CustomMargins} customMargins Custom margins ticket
   *     item, used to write when margins type changes.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function MarginsType(appState, documentInfo, customMargins) {
    print_preview.ticket_items.TicketItem.call(
        this,
        appState,
        print_preview.AppState.Field.MARGINS_TYPE,
        null /*destinationStore*/,
        documentInfo);

    /**
     * Custom margins ticket item, used to write when margins type changes.
     * @type {!print_preview.ticket_items.CustomMargins}
     * @private
     */
    this.customMargins_ = customMargins;
  };

  /**
   * Enumeration of margin types. Matches enum MarginType in
   * printing/print_job_constants.h.
   * @enum {number}
   */
  MarginsType.Value = {
    DEFAULT: 0,
    NO_MARGINS: 1,
    MINIMUM: 2,
    CUSTOM: 3
  };

  MarginsType.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      return true;
    },

    /** @override */
    isCapabilityAvailable: function() {
      return this.getDocumentInfoInternal().isModifiable;
    },

    /** @override */
    getDefaultValueInternal: function() {
      return MarginsType.Value.DEFAULT;
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      return MarginsType.Value.DEFAULT;
    },

    /** @override */
    updateValueInternal: function(value) {
      print_preview.ticket_items.TicketItem.prototype.updateValueInternal.call(
          this, value);
      if (this.isValueEqual(
          print_preview.ticket_items.MarginsType.Value.CUSTOM)) {
        // If CUSTOM, set the value of the custom margins so that it won't be
        // overridden by the default value.
        this.customMargins_.updateValue(this.customMargins_.getValue());
      }
    }
  };

  // Export
  return {
    MarginsType: MarginsType
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Page range ticket item whose value is a {@code string} that represents
   * which pages in the document should be printed.
   * @param {!print_preview.DocumentInfo} documentInfo Information about the
   *     document to print.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function PageRange(documentInfo) {
    print_preview.ticket_items.TicketItem.call(
        this,
        null /*appState*/,
        null /*field*/,
        null /*destinationStore*/,
        documentInfo);
  };

  /**
   * Impossibly large page number.
   * @type {number}
   * @const
   * @private
   */
  PageRange.MAX_PAGE_NUMBER_ = 1000000000;

  PageRange.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      return null != pageRangeTextToPageRanges(
          value, this.getDocumentInfoInternal().pageCount);
    },

    /**
     * @return {!print_preview.PageNumberSet} Set of page numbers defined by the
     *     page range string.
     */
    getPageNumberSet: function() {
      var pageNumberList = pageRangeTextToPageList(
          this.getValue(), this.getDocumentInfoInternal().pageCount);
      return new print_preview.PageNumberSet(pageNumberList);
    },

    /** @override */
    isCapabilityAvailable: function() {
      return true;
    },

    /** @override */
    getDefaultValueInternal: function() {
      return '';
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      return '';
    },

    /**
     * @return {!Array.<Object.<{from: number, to: number}>>} A list of page
     *     ranges.
     */
    getPageRanges: function() {
      return pageRangeTextToPageRanges(this.getValue()) || [];
    },

    /**
     * @return {!Array.<object.<{from: number, to: number}>>} A list of page
     *     ranges suitable for use in the native layer.
     * TODO(vitalybuka): this should be removed when native layer switched to
     *     page ranges.
     */
    getDocumentPageRanges: function() {
      var pageRanges = pageRangeTextToPageRanges(
          this.getValue(), this.getDocumentInfoInternal().pageCount);
      return pageRanges || [];
    },
  };

  // Export
  return {
    PageRange: PageRange
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Fit-to-page ticket item whose value is a {@code boolean} that indicates
   * whether to scale the document to fit the page.
   * @param {!print_preview.DocumentInfo} documentInfo Information about the
   *     document to print.
   * @param {!print_preview.DestinationStore} destinationStore Used to determine
   *     whether fit to page should be available.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function FitToPage(documentInfo, destinationStore) {
    print_preview.ticket_items.TicketItem.call(
        this,
        null /*appState*/,
        null /*field*/,
        destinationStore,
        documentInfo);
  };

  FitToPage.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      return true;
    },

    /** @override */
    isCapabilityAvailable: function() {
      return !this.getDocumentInfoInternal().isModifiable &&
          (!this.getSelectedDestInternal() ||
              this.getSelectedDestInternal().id !=
                  print_preview.Destination.GooglePromotedId.SAVE_AS_PDF);
    },

    /** @override */
    getDefaultValueInternal: function() {
      return !this.getDocumentInfoInternal().isScalingDisabled;
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      return !this.getSelectedDestInternal() ||
          this.getSelectedDestInternal().id !=
              print_preview.Destination.GooglePromotedId.SAVE_AS_PDF;
    }
  };

  // Export
  return {
    FitToPage: FitToPage
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Ticket item whose value is a {@code boolean} that represents whether to
   * print CSS backgrounds.
   * @param {!print_preview.AppState} appState App state to persist CSS
   *     background value.
   * @param {!print_preview.DocumentInfo} documentInfo Information about the
   *     document to print.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function CssBackground(appState, documentInfo) {
    print_preview.ticket_items.TicketItem.call(
        this,
        appState,
        print_preview.AppState.Field.IS_CSS_BACKGROUND_ENABLED,
        null /*destinationStore*/,
        documentInfo);
  };

  CssBackground.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      return true;
    },

    /** @override */
    isCapabilityAvailable: function() {
      return this.getDocumentInfoInternal().isModifiable;
    },

    /** @override */
    getDefaultValueInternal: function() {
      return false;
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      return false;
    }
  };

  // Export
  return {
    CssBackground: CssBackground
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview.ticket_items', function() {
  'use strict';

  /**
   * Ticket item whose value is a {@code boolean} that represents whether to
   * print selection only.
   * @param {!print_preview.DocumentInfo} documentInfo Information about the
   *     document to print.
   * @constructor
   * @extends {print_preview.ticket_items.TicketItem}
   */
  function SelectionOnly(documentInfo) {
    print_preview.ticket_items.TicketItem.call(
        this,
        null /*appState*/,
        null /*field*/,
        null /*destinationStore*/,
        documentInfo);
  };

  SelectionOnly.prototype = {
    __proto__: print_preview.ticket_items.TicketItem.prototype,

    /** @override */
    wouldValueBeValid: function(value) {
      return true;
    },

    /** @override */
    isCapabilityAvailable: function() {
      return this.getDocumentInfoInternal().isModifiable &&
             this.getDocumentInfoInternal().hasSelection;
    },

    /** @override */
    getDefaultValueInternal: function() {
      return false;
    },

    /** @override */
    getCapabilityNotAvailableValueInternal: function() {
      return false;
    }
  };

  // Export
  return {
    SelectionOnly: SelectionOnly
  };
});


// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * An interface to the native Chromium printing system layer.
   * @constructor
   * @extends {cr.EventTarget}
   */
  function NativeLayer() {
    cr.EventTarget.call(this);

    // Bind global handlers
    global['setInitialSettings'] = this.onSetInitialSettings_.bind(this);
    global['setUseCloudPrint'] = this.onSetUseCloudPrint_.bind(this);
    global['setPrinters'] = this.onSetPrinters_.bind(this);
    global['updateWithPrinterCapabilities'] =
        this.onUpdateWithPrinterCapabilities_.bind(this);
    global['failedToGetPrinterCapabilities'] =
        this.onFailedToGetPrinterCapabilities_.bind(this);
    global['reloadPrintersList'] = this.onReloadPrintersList_.bind(this);
    global['printToCloud'] = this.onPrintToCloud_.bind(this);
    global['fileSelectionCancelled'] =
        this.onFileSelectionCancelled_.bind(this);
    global['fileSelectionCompleted'] =
        this.onFileSelectionCompleted_.bind(this);
    global['printPreviewFailed'] = this.onPrintPreviewFailed_.bind(this);
    global['invalidPrinterSettings'] =
        this.onInvalidPrinterSettings_.bind(this);
    global['onDidGetDefaultPageLayout'] =
        this.onDidGetDefaultPageLayout_.bind(this);
    global['onDidGetPreviewPageCount'] =
        this.onDidGetPreviewPageCount_.bind(this);
    global['reloadPreviewPages'] = this.onReloadPreviewPages_.bind(this);
    global['onDidPreviewPage'] = this.onDidPreviewPage_.bind(this);
    global['updatePrintPreview'] = this.onUpdatePrintPreview_.bind(this);
    global['printScalingDisabledForSourcePDF'] =
        this.onPrintScalingDisabledForSourcePDF_.bind(this);
    global['onDidGetAccessToken'] = this.onDidGetAccessToken_.bind(this);
    global['autoCancelForTesting'] = this.autoCancelForTesting_.bind(this);
  };

  /**
   * Event types dispatched from the Chromium native layer.
   * @enum {string}
   * @const
   */
  NativeLayer.EventType = {
    ACCESS_TOKEN_READY: 'print_preview.NativeLayer.ACCESS_TOKEN_READY',
    CAPABILITIES_SET: 'print_preview.NativeLayer.CAPABILITIES_SET',
    CLOUD_PRINT_ENABLE: 'print_preview.NativeLayer.CLOUD_PRINT_ENABLE',
    DESTINATIONS_RELOAD: 'print_preview.NativeLayer.DESTINATIONS_RELOAD',
    DISABLE_SCALING: 'print_preview.NativeLayer.DISABLE_SCALING',
    FILE_SELECTION_CANCEL: 'print_preview.NativeLayer.FILE_SELECTION_CANCEL',
    FILE_SELECTION_COMPLETE:
        'print_preview.NativeLayer.FILE_SELECTION_COMPLETE',
    GET_CAPABILITIES_FAIL: 'print_preview.NativeLayer.GET_CAPABILITIES_FAIL',
    INITIAL_SETTINGS_SET: 'print_preview.NativeLayer.INITIAL_SETTINGS_SET',
    LOCAL_DESTINATIONS_SET: 'print_preview.NativeLayer.LOCAL_DESTINATIONS_SET',
    PAGE_COUNT_READY: 'print_preview.NativeLayer.PAGE_COUNT_READY',
    PAGE_LAYOUT_READY: 'print_preview.NativeLayer.PAGE_LAYOUT_READY',
    PAGE_PREVIEW_READY: 'print_preview.NativeLayer.PAGE_PREVIEW_READY',
    PREVIEW_GENERATION_DONE:
        'print_preview.NativeLayer.PREVIEW_GENERATION_DONE',
    PREVIEW_GENERATION_FAIL:
        'print_preview.NativeLayer.PREVIEW_GENERATION_FAIL',
    PREVIEW_RELOAD: 'print_preview.NativeLayer.PREVIEW_RELOAD',
    PRINT_TO_CLOUD: 'print_preview.NativeLayer.PRINT_TO_CLOUD',
    SETTINGS_INVALID: 'print_preview.NativeLayer.SETTINGS_INVALID'
  };

  /**
   * Constant values matching printing::DuplexMode enum.
   * @enum {number}
   */
  NativeLayer.DuplexMode = {
    SIMPLEX: 0,
    LONG_EDGE: 1,
    UNKNOWN_DUPLEX_MODE: -1
  };

  /**
   * Enumeration of color modes used by Chromium.
   * @enum {number}
   * @private
   */
  NativeLayer.ColorMode_ = {
    GRAY: 1,
    COLOR: 2
  };

  /**
   * Version of the serialized state of the print preview.
   * @type {number}
   * @const
   * @private
   */
  NativeLayer.SERIALIZED_STATE_VERSION_ = 1;

  NativeLayer.prototype = {
    __proto__: cr.EventTarget.prototype,

    /**
     * Requests access token for cloud print requests.
     * @param {string} authType type of access token.
     */
    startGetAccessToken: function(authType) {
      chrome.send('getAccessToken', [authType]);
    },

    /** Gets the initial settings to initialize the print preview with. */
    startGetInitialSettings: function() {
      chrome.send('getInitialSettings');
    },

    /**
     * Requests the system's local print destinations. A LOCAL_DESTINATIONS_SET
     * event will be dispatched in response.
     */
    startGetLocalDestinations: function() {
      chrome.send('getPrinters');
    },

    /**
     * Requests the destination's printing capabilities. A CAPABILITIES_SET
     * event will be dispatched in response.
     * @param {string} destinationId ID of the destination.
     */
    startGetLocalDestinationCapabilities: function(destinationId) {
      chrome.send('getPrinterCapabilities', [destinationId]);
    },

    /**
     * Requests that a preview be generated. The following events may be
     * dispatched in response:
     *   - PAGE_COUNT_READY
     *   - PAGE_LAYOUT_READY
     *   - PAGE_PREVIEW_READY
     *   - PREVIEW_GENERATION_DONE
     *   - PREVIEW_GENERATION_FAIL
     *   - PREVIEW_RELOAD
     * @param {print_preview.Destination} destination Destination to print to.
     * @param {!print_preview.PrintTicketStore} printTicketStore Used to get the
     *     state of the print ticket.
     * @param {!print_preview.DocumentInfo} documentInfo Document data model.
     * @param {number} ID of the preview request.
     */
    startGetPreview: function(
        destination, printTicketStore, documentInfo, requestId) {
      assert(printTicketStore.isTicketValidForPreview(),
             'Trying to generate preview when ticket is not valid');

      var ticket = {
        'pageRange': printTicketStore.pageRange.getDocumentPageRanges(),
        'landscape': printTicketStore.landscape.getValue(),
        'color': printTicketStore.color.getValue() ?
            NativeLayer.ColorMode_.COLOR : NativeLayer.ColorMode_.GRAY,
        'headerFooterEnabled': printTicketStore.headerFooter.getValue(),
        'marginsType': printTicketStore.marginsType.getValue(),
        'isFirstRequest': requestId == 0,
        'requestID': requestId,
        'previewModifiable': documentInfo.isModifiable,
        'printToPDF':
            destination != null &&
            destination.id ==
                print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
        'printWithCloudPrint': destination != null && !destination.isLocal,
        'deviceName': destination == null ? 'foo' : destination.id,
        'generateDraftData': documentInfo.isModifiable,
        'fitToPageEnabled': printTicketStore.fitToPage.getValue(),

        // NOTE: Even though the following fields don't directly relate to the
        // preview, they still need to be included.
        'duplex': printTicketStore.duplex.getValue() ?
            NativeLayer.DuplexMode.LONG_EDGE : NativeLayer.DuplexMode.SIMPLEX,
        'copies': printTicketStore.copies.getValueAsNumber(),
        'collate': printTicketStore.collate.getValue(),
        'shouldPrintBackgrounds': printTicketStore.cssBackground.getValue(),
        'shouldPrintSelectionOnly': printTicketStore.selectionOnly.getValue()
      };

      // Set 'cloudPrintID' only if the destination is not local.
      if (destination && !destination.isLocal) {
        ticket['cloudPrintID'] = destination.id;
      }

      if (printTicketStore.marginsType.isCapabilityAvailable() &&
          printTicketStore.marginsType.getValue() ==
              print_preview.ticket_items.MarginsType.Value.CUSTOM) {
        var customMargins = printTicketStore.customMargins.getValue();
        var orientationEnum =
            print_preview.ticket_items.CustomMargins.Orientation;
        ticket['marginsCustom'] = {
          'marginTop': customMargins.get(orientationEnum.TOP),
          'marginRight': customMargins.get(orientationEnum.RIGHT),
          'marginBottom': customMargins.get(orientationEnum.BOTTOM),
          'marginLeft': customMargins.get(orientationEnum.LEFT)
        };
      }

      chrome.send(
          'getPreview',
          [JSON.stringify(ticket),
           requestId > 0 ? documentInfo.pageCount : -1,
           documentInfo.isModifiable]);
    },

    /**
     * Requests that the document be printed.
     * @param {!print_preview.Destination} destination Destination to print to.
     * @param {!print_preview.PrintTicketStore} printTicketStore Used to get the
     *     state of the print ticket.
     * @param {print_preview.CloudPrintInterface} cloudPrintInterface Interface
     *     to Google Cloud Print.
     * @param {!print_preview.DocumentInfo} documentInfo Document data model.
     * @param {boolean=} opt_isOpenPdfInPreview Whether to open the PDF in the
     *     system's preview application.
     */
    startPrint: function(destination, printTicketStore, cloudPrintInterface,
                         documentInfo, opt_isOpenPdfInPreview) {
      assert(printTicketStore.isTicketValid(),
             'Trying to print when ticket is not valid');

      var ticket = {
        'pageRange': printTicketStore.pageRange.getDocumentPageRanges(),
        'pageCount': printTicketStore.pageRange.getPageNumberSet().size,
        'landscape': printTicketStore.landscape.getValue(),
        'color': printTicketStore.color.getValue() ?
            NativeLayer.ColorMode_.COLOR : NativeLayer.ColorMode_.GRAY,
        'headerFooterEnabled': printTicketStore.headerFooter.getValue(),
        'marginsType': printTicketStore.marginsType.getValue(),
        'generateDraftData': true, // TODO(rltoscano): What should this be?
        'duplex': printTicketStore.duplex.getValue() ?
            NativeLayer.DuplexMode.LONG_EDGE : NativeLayer.DuplexMode.SIMPLEX,
        'copies': printTicketStore.copies.getValueAsNumber(),
        'collate': printTicketStore.collate.getValue(),
        'shouldPrintBackgrounds': printTicketStore.cssBackground.getValue(),
        'shouldPrintSelectionOnly': printTicketStore.selectionOnly.getValue(),
        'previewModifiable': documentInfo.isModifiable,
        'printToPDF': destination.id ==
            print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
        'printWithCloudPrint': !destination.isLocal,
        'deviceName': destination.id,
        'isFirstRequest': false,
        'requestID': -1,
        'fitToPageEnabled': printTicketStore.fitToPage.getValue()
      };

      if (!destination.isLocal) {
        // We can't set cloudPrintID if the destination is "Print with Cloud
        // Print" because the native system will try to print to Google Cloud
        // Print with this ID instead of opening a Google Cloud Print dialog.
        ticket['cloudPrintID'] = destination.id;
      }

      if (printTicketStore.marginsType.isCapabilityAvailable() &&
          printTicketStore.marginsType.isValueEqual(
              print_preview.ticket_items.MarginsType.Value.CUSTOM)) {
        var customMargins = printTicketStore.customMargins.getValue();
        var orientationEnum =
            print_preview.ticket_items.CustomMargins.Orientation;
        ticket['marginsCustom'] = {
          'marginTop': customMargins.get(orientationEnum.TOP),
          'marginRight': customMargins.get(orientationEnum.RIGHT),
          'marginBottom': customMargins.get(orientationEnum.BOTTOM),
          'marginLeft': customMargins.get(orientationEnum.LEFT)
        };
      }

      if (opt_isOpenPdfInPreview) {
        ticket['OpenPDFInPreview'] = true;
      }

      chrome.send('print', [JSON.stringify(ticket)]);
    },

    /** Requests that the current pending print request be cancelled. */
    startCancelPendingPrint: function() {
      chrome.send('cancelPendingPrintRequest');
    },

    /** Shows the system's native printing dialog. */
    startShowSystemDialog: function() {
      chrome.send('showSystemDialog');
    },

    /** Shows Google Cloud Print's web-based print dialog.
     * @param {number} pageCount Number of pages to print.
     */
    startShowCloudPrintDialog: function(pageCount) {
      chrome.send('printWithCloudPrintDialog', [pageCount]);
    },

    /** Closes the print preview dialog. */
    startCloseDialog: function() {
      chrome.send('closePrintPreviewDialog');
      chrome.send('DialogClose');
    },

    /** Hide the print preview dialog and allow the native layer to close it. */
    startHideDialog: function() {
      chrome.send('hidePreview');
    },

    /**
     * Opens the Google Cloud Print sign-in dialog. The DESTINATIONS_RELOAD
     * event will be dispatched in response.
     */
    startCloudPrintSignIn: function() {
      chrome.send('signIn');
    },

    /** Navigates the user to the system printer settings interface. */
    startManageLocalDestinations: function() {
      chrome.send('manageLocalPrinters');
    },

    /** Navigates the user to the Google Cloud Print management page. */
    startManageCloudDestinations: function() {
      chrome.send('manageCloudPrinters');
    },

    /** Forces browser to open a new tab with the given URL address. */
    startForceOpenNewTab: function(url) {
      chrome.send('forceOpenNewTab', [url]);
    },

    /**
     * @param {!Object} initialSettings Object containing all initial settings.
     */
    onSetInitialSettings_: function(initialSettings) {
      var numberFormatSymbols =
          print_preview.MeasurementSystem.parseNumberFormat(
              initialSettings['numberFormat']);
      var unitType = print_preview.MeasurementSystem.UnitType.IMPERIAL;
      if (initialSettings['measurementSystem'] != null) {
        unitType = initialSettings['measurementSystem'];
      }

      var nativeInitialSettings = new print_preview.NativeInitialSettings(
          initialSettings['printAutomaticallyInKioskMode'] || false,
          initialSettings['hidePrintWithSystemDialogLink'] || false,
          numberFormatSymbols[0] || ',',
          numberFormatSymbols[1] || '.',
          unitType,
          initialSettings['previewModifiable'] || false,
          initialSettings['initiatorTitle'] || '',
          initialSettings['documentHasSelection'] || false,
          initialSettings['shouldPrintSelectionOnly'] || false,
          initialSettings['printerName'] || null,
          initialSettings['appState'] || null);

      var initialSettingsSetEvent = new Event(
          NativeLayer.EventType.INITIAL_SETTINGS_SET);
      initialSettingsSetEvent.initialSettings = nativeInitialSettings;
      this.dispatchEvent(initialSettingsSetEvent);
    },

    /**
     * Turn on the integration of Cloud Print.
     * @param {string} cloudPrintURL The URL to use for cloud print servers.
     * @private
     */
    onSetUseCloudPrint_: function(cloudPrintURL) {
      var cloudPrintEnableEvent = new Event(
          NativeLayer.EventType.CLOUD_PRINT_ENABLE);
      cloudPrintEnableEvent.baseCloudPrintUrl = cloudPrintURL;
      this.dispatchEvent(cloudPrintEnableEvent);
    },

    /**
     * Updates the print preview with local printers.
     * Called from PrintPreviewHandler::SetupPrinterList().
     * @param {Array} printers Array of printer info objects.
     * @private
     */
    onSetPrinters_: function(printers) {
      var localDestsSetEvent = new Event(
          NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
      localDestsSetEvent.destinationInfos = printers;
      this.dispatchEvent(localDestsSetEvent);
    },

    /**
     * Called when native layer gets settings information for a requested local
     * destination.
     * @param {Object} settingsInfo printer setting information.
     * @private
     */
    onUpdateWithPrinterCapabilities_: function(settingsInfo) {
      var capsSetEvent = new Event(NativeLayer.EventType.CAPABILITIES_SET);
      capsSetEvent.settingsInfo = settingsInfo;
      this.dispatchEvent(capsSetEvent);
    },

    /**
     * Called when native layer gets settings information for a requested local
     * destination.
     * @param {string} printerId printer affected by error.
     * @private
     */
    onFailedToGetPrinterCapabilities_: function(destinationId) {
      var getCapsFailEvent = new Event(
          NativeLayer.EventType.GET_CAPABILITIES_FAIL);
      getCapsFailEvent.destinationId = destinationId;
      getCapsFailEvent.destinationOrigin =
          print_preview.Destination.Origin.LOCAL;
      this.dispatchEvent(getCapsFailEvent);
    },

    /** Reloads the printer list. */
    onReloadPrintersList_: function() {
      cr.dispatchSimpleEvent(this, NativeLayer.EventType.DESTINATIONS_RELOAD);
    },

    /**
     * Called from the C++ layer.
     * Take the PDF data handed to us and submit it to the cloud, closing the
     * print preview dialog once the upload is successful.
     * @param {string} data Data to send as the print job.
     * @private
     */
    onPrintToCloud_: function(data) {
      var printToCloudEvent = new Event(
          NativeLayer.EventType.PRINT_TO_CLOUD);
      printToCloudEvent.data = data;
      this.dispatchEvent(printToCloudEvent);
    },

    /**
     * Called from PrintPreviewUI::OnFileSelectionCancelled to notify the print
     * preview dialog regarding the file selection cancel event.
     * @private
     */
    onFileSelectionCancelled_: function() {
      cr.dispatchSimpleEvent(this, NativeLayer.EventType.FILE_SELECTION_CANCEL);
    },

    /**
     * Called from PrintPreviewUI::OnFileSelectionCompleted to notify the print
     * preview dialog regarding the file selection completed event.
     * @private
     */
    onFileSelectionCompleted_: function() {
      // If the file selection is completed and the dialog is not already closed
      // it means that a pending print to pdf request exists.
      cr.dispatchSimpleEvent(
          this, NativeLayer.EventType.FILE_SELECTION_COMPLETE);
    },

    /**
     * Display an error message when print preview fails.
     * Called from PrintPreviewMessageHandler::OnPrintPreviewFailed().
     * @private
     */
    onPrintPreviewFailed_: function() {
      cr.dispatchSimpleEvent(
          this, NativeLayer.EventType.PREVIEW_GENERATION_FAIL);
    },

    /**
     * Display an error message when encountered invalid printer settings.
     * Called from PrintPreviewMessageHandler::OnInvalidPrinterSettings().
     * @private
     */
    onInvalidPrinterSettings_: function() {
      cr.dispatchSimpleEvent(this, NativeLayer.EventType.SETTINGS_INVALID);
    },

    /**
     * @param {{contentWidth: number, contentHeight: number, marginLeft: number,
     *          marginRight: number, marginTop: number, marginBottom: number,
     *          printableAreaX: number, printableAreaY: number,
     *          printableAreaWidth: number, printableAreaHeight: number}}
     *          pageLayout Specifies default page layout details in points.
     * @param {boolean} hasCustomPageSizeStyle Indicates whether the previewed
     *     document has a custom page size style.
     * @private
     */
    onDidGetDefaultPageLayout_: function(pageLayout, hasCustomPageSizeStyle) {
      var pageLayoutChangeEvent = new Event(
          NativeLayer.EventType.PAGE_LAYOUT_READY);
      pageLayoutChangeEvent.pageLayout = pageLayout;
      pageLayoutChangeEvent.hasCustomPageSizeStyle = hasCustomPageSizeStyle;
      this.dispatchEvent(pageLayoutChangeEvent);
    },

    /**
     * Update the page count and check the page range.
     * Called from PrintPreviewUI::OnDidGetPreviewPageCount().
     * @param {number} pageCount The number of pages.
     * @param {number} previewResponseId The preview request id that resulted in
     *      this response.
     * @private
     */
    onDidGetPreviewPageCount_: function(pageCount, previewResponseId) {
      var pageCountChangeEvent = new Event(
          NativeLayer.EventType.PAGE_COUNT_READY);
      pageCountChangeEvent.pageCount = pageCount;
      pageCountChangeEvent.previewResponseId = previewResponseId;
      this.dispatchEvent(pageCountChangeEvent);
    },

    /**
     * Called when no pipelining previewed pages.
     * @param {number} previewUid Preview unique identifier.
     * @param {number} previewResponseId The preview request id that resulted in
     *     this response.
     * @private
     */
    onReloadPreviewPages_: function(previewUid, previewResponseId) {
      var previewReloadEvent = new Event(
          NativeLayer.EventType.PREVIEW_RELOAD);
      previewReloadEvent.previewUid = previewUid;
      previewReloadEvent.previewResponseId = previewResponseId;
      this.dispatchEvent(previewReloadEvent);
    },

    /**
     * Notification that a print preview page has been rendered.
     * Check if the settings have changed and request a regeneration if needed.
     * Called from PrintPreviewUI::OnDidPreviewPage().
     * @param {number} pageNumber The page number, 0-based.
     * @param {number} previewUid Preview unique identifier.
     * @param {number} previewResponseId The preview request id that resulted in
     *     this response.
     * @private
     */
    onDidPreviewPage_: function(pageNumber, previewUid, previewResponseId) {
      var pagePreviewGenEvent = new Event(
          NativeLayer.EventType.PAGE_PREVIEW_READY);
      pagePreviewGenEvent.pageIndex = pageNumber;
      pagePreviewGenEvent.previewUid = previewUid;
      pagePreviewGenEvent.previewResponseId = previewResponseId;
      this.dispatchEvent(pagePreviewGenEvent);
    },

    /**
     * Notification that access token is ready.
     * @param {string} authType Type of access token.
     * @param {string} accessToken Access token.
     * @private
     */
    onDidGetAccessToken_: function(authType, accessToken) {
      var getAccessTokenEvent = new Event(
          NativeLayer.EventType.ACCESS_TOKEN_READY);
      getAccessTokenEvent.authType = authType;
      getAccessTokenEvent.accessToken = accessToken;
      this.dispatchEvent(getAccessTokenEvent);
    },

    /**
     * Update the print preview when new preview data is available.
     * Create the PDF plugin as needed.
     * Called from PrintPreviewUI::PreviewDataIsAvailable().
     * @param {number} previewUid Preview unique identifier.
     * @param {number} previewResponseId The preview request id that resulted in
     *     this response.
     * @private
     */
    onUpdatePrintPreview_: function(previewUid, previewResponseId) {
      var previewGenDoneEvent = new Event(
          NativeLayer.EventType.PREVIEW_GENERATION_DONE);
      previewGenDoneEvent.previewUid = previewUid;
      previewGenDoneEvent.previewResponseId = previewResponseId;
      this.dispatchEvent(previewGenDoneEvent);
    },

    /**
     * Updates the fit to page option state based on the print scaling option of
     * source pdf. PDF's have an option to enable/disable print scaling. When we
     * find out that the print scaling option is disabled for the source pdf, we
     * uncheck the fitToPage_ to page checkbox. This function is called from C++
     * code.
     * @private
     */
    onPrintScalingDisabledForSourcePDF_: function() {
      cr.dispatchSimpleEvent(this, NativeLayer.EventType.DISABLE_SCALING);
    },

    /**
     * Simulates a user click on the print preview dialog cancel button. Used
     * only for testing.
     * @private
     */
    autoCancelForTesting_: function() {
      var properties = {view: window, bubbles: true, cancelable: true};
      var click = new MouseEvent('click', properties);
      document.querySelector('#print-header .cancel').dispatchEvent(click);
    }
  };

  /**
   * Initial settings retrieved from the native layer.
   * @param {boolean} isInKioskAutoPrintMode Whether the print preview should be
   *     in auto-print mode.
   * @param {string} thousandsDelimeter Character delimeter of thousands digits.
   * @param {string} decimalDelimeter Character delimeter of the decimal point.
   * @param {!print_preview.MeasurementSystem.UnitType} unitType Unit type of
   *     local machine's measurement system.
   * @param {boolean} isDocumentModifiable Whether the document to print is
   *     modifiable.
   * @param {string} documentTitle Title of the document.
   * @param {boolean} documentHasSelection Whether the document has selected
   *     content.
   * @param {boolean} selectionOnly Whether only selected content should be
   *     printed.
   * @param {?string} systemDefaultDestinationId ID of the system default
   *     destination.
   * @param {?string} serializedAppStateStr Serialized app state.
   * @constructor
   */
  function NativeInitialSettings(
      isInKioskAutoPrintMode,
      hidePrintWithSystemDialogLink,
      thousandsDelimeter,
      decimalDelimeter,
      unitType,
      isDocumentModifiable,
      documentTitle,
      documentHasSelection,
      selectionOnly,
      systemDefaultDestinationId,
      serializedAppStateStr) {

    /**
     * Whether the print preview should be in auto-print mode.
     * @type {boolean}
     * @private
     */
    this.isInKioskAutoPrintMode_ = isInKioskAutoPrintMode;

    /**
     * Whether we should hide the link which shows the system print dialog.
     * @type {boolean}
     * @private
     */
    this.hidePrintWithSystemDialogLink_ = hidePrintWithSystemDialogLink;

    /**
     * Character delimeter of thousands digits.
     * @type {string}
     * @private
     */
    this.thousandsDelimeter_ = thousandsDelimeter;

    /**
     * Character delimeter of the decimal point.
     * @type {string}
     * @private
     */
    this.decimalDelimeter_ = decimalDelimeter;

    /**
     * Unit type of local machine's measurement system.
     * @type {string}
     * @private
     */
    this.unitType_ = unitType;

    /**
     * Whether the document to print is modifiable.
     * @type {boolean}
     * @private
     */
    this.isDocumentModifiable_ = isDocumentModifiable;

    /**
     * Title of the document.
     * @type {string}
     * @private
     */
    this.documentTitle_ = documentTitle;

    /**
     * Whether the document has selection.
     * @type {string}
     * @private
     */
    this.documentHasSelection_ = documentHasSelection;

    /**
     * Whether selection only should be printed.
     * @type {string}
     * @private
     */
    this.selectionOnly_ = selectionOnly;

    /**
     * ID of the system default destination.
     * @type {?string}
     * @private
     */
    this.systemDefaultDestinationId_ = systemDefaultDestinationId;

    /**
     * Serialized app state.
     * @type {?string}
     * @private
     */
    this.serializedAppStateStr_ = serializedAppStateStr;
  };

  NativeInitialSettings.prototype = {
    /**
     * @return {boolean} Whether the print preview should be in auto-print mode.
     */
    get isInKioskAutoPrintMode() {
      return this.isInKioskAutoPrintMode_;
    },

    /**
     * @return {boolean} Whether we should hide the link which shows the
           system print dialog.
     */
    get hidePrintWithSystemDialogLink() {
      return this.hidePrintWithSystemDialogLink_;
    },

    /** @return {string} Character delimeter of thousands digits. */
    get thousandsDelimeter() {
      return this.thousandsDelimeter_;
    },

    /** @return {string} Character delimeter of the decimal point. */
    get decimalDelimeter() {
      return this.decimalDelimeter_;
    },

    /**
     * @return {!print_preview.MeasurementSystem.UnitType} Unit type of local
     *     machine's measurement system.
     */
    get unitType() {
      return this.unitType_;
    },

    /** @return {boolean} Whether the document to print is modifiable. */
    get isDocumentModifiable() {
      return this.isDocumentModifiable_;
    },

    /** @return {string} Document title. */
    get documentTitle() {
      return this.documentTitle_;
    },

    /** @return {bool} Whether the document has selection. */
    get documentHasSelection() {
      return this.documentHasSelection_;
    },

    /** @return {bool} Whether selection only should be printed. */
    get selectionOnly() {
      return this.selectionOnly_;
    },

    /** @return {?string} ID of the system default destination. */
    get systemDefaultDestinationId() {
      return this.systemDefaultDestinationId_;
    },

    /** @return {?string} Serialized app state. */
    get serializedAppStateStr() {
      return this.serializedAppStateStr_;
    }
  };

  // Export
  return {
    NativeInitialSettings: NativeInitialSettings,
    NativeLayer: NativeLayer
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Counter used to give webkit animations unique names.
var animationCounter = 0;

function addAnimation(code) {
  var name = 'anim' + animationCounter;
  animationCounter++;
  var rules = document.createTextNode(
      '@-webkit-keyframes ' + name + ' {' + code + '}');
  var el = document.createElement('style');
  el.type = 'text/css';
  el.appendChild(rules);
  el.setAttribute('id', name);
  document.body.appendChild(el);

  return name;
}

/**
 * Generates css code for fading in an element by animating the height.
 * @param {number} targetHeight The desired height in pixels after the animation
 *     ends.
 * @return {string} The css code for the fade in animation.
 */
function getFadeInAnimationCode(targetHeight) {
  return '0% { opacity: 0; height: 0; } ' +
      '80% { height: ' + (targetHeight + 4) + 'px; }' +
      '100% { opacity: 1; height: ' + targetHeight + 'px; }';
}

/**
 * Fades in an element. Used for both printing options and error messages
 * appearing underneath the textfields.
 * @param {HTMLElement} el The element to be faded in.
 */
function fadeInElement(el) {
  if (el.classList.contains('visible'))
    return;
  el.classList.remove('closing');
  el.hidden = false;
  el.style.height = 'auto';
  var height = el.offsetHeight;
  el.style.height = height + 'px';
  var animName = addAnimation(getFadeInAnimationCode(height));
  var eventTracker = new EventTracker();
  eventTracker.add(el, 'webkitAnimationEnd',
                   onFadeInAnimationEnd.bind(el, eventTracker),
                   false);
  el.style.webkitAnimationName = animName;
  el.classList.add('visible');
}

/**
 * Fades out an element. Used for both printing options and error messages
 * appearing underneath the textfields.
 * @param {HTMLElement} el The element to be faded out.
 */
function fadeOutElement(el) {
  if (!el.classList.contains('visible'))
    return;
  el.style.height = 'auto';
  var height = el.offsetHeight;
  el.style.height = height + 'px';
  var animName = addAnimation('');
  var eventTracker = new EventTracker();
  eventTracker.add(el, 'webkitTransitionEnd',
                   onFadeOutTransitionEnd.bind(el, animName, eventTracker),
                   false);
  el.classList.add('closing');
  el.classList.remove('visible');
}

/**
 * Executes when a fade out animation ends.
 * @param {string} animationName The name of the animation to be removed.
 * @param {EventTracker} eventTracker The |EventTracker| object that was used
 *     for adding this listener.
 * @param {WebkitTransitionEvent} event The event that triggered this listener.
 * @this {HTMLElement} The element where the transition occurred.
 */
function onFadeOutTransitionEnd(animationName, eventTracker, event) {
  if (event.propertyName != 'height')
    return;
  fadeInOutCleanup(animationName);
  eventTracker.remove(this, 'webkitTransitionEnd');
  this.hidden = true;
}

/**
 * Executes when a fade in animation ends.
 * @param {EventTracker} eventTracker The |EventTracker| object that was used
 *     for adding this listener.
 * @param {WebkitAnimationEvent} event The event that triggered this listener.
 * @this {HTMLElement} The element where the transition occurred.
 */
function onFadeInAnimationEnd(eventTracker, event) {
  this.style.height = '';
  this.style.webkitAnimationName = '';
  fadeInOutCleanup(event.animationName);
  eventTracker.remove(this, 'webkitAnimationEnd');
}

/**
 * Removes the <style> element corrsponding to |animationName| from the DOM.
 * @param {string} animationName The name of the animation to be removed.
 */
function fadeInOutCleanup(animationName) {
  var animEl = document.getElementById(animationName);
  if (animEl)
    animEl.parentNode.removeChild(animEl);
}

/**
 * Fades in a printing option existing under |el|.
 * @param {HTMLElement} el The element to hide.
 */
function fadeInOption(el) {
  if (el.classList.contains('visible'))
    return;

  wrapContentsInDiv(el.querySelector('h1'), ['invisible']);
  var rightColumn = el.querySelector('.right-column');
  wrapContentsInDiv(rightColumn, ['invisible']);

  var toAnimate = el.querySelectorAll('.collapsible');
  for (var i = 0; i < toAnimate.length; i++)
    fadeInElement(toAnimate[i]);
  el.classList.add('visible');
}

/**
 * Fades out a printing option existing under |el|.
 * @param {HTMLElement} el The element to hide.
 */
function fadeOutOption(el) {
  if (!el.classList.contains('visible'))
    return;

  wrapContentsInDiv(el.querySelector('h1'), ['visible']);
  var rightColumn = el.querySelector('.right-column');
  wrapContentsInDiv(rightColumn, ['visible']);

  var toAnimate = el.querySelectorAll('.collapsible');
  for (var i = 0; i < toAnimate.length; i++)
    fadeOutElement(toAnimate[i]);
  el.classList.remove('visible');
}

/**
 * Wraps the contents of |el| in a div element and attaches css classes
 * |classes| in the new div, only if has not been already done. It is neccesary
 * for animating the height of table cells.
 * @param {HTMLElement} el The element to be processed.
 * @param {array} classes The css classes to add.
 */
function wrapContentsInDiv(el, classes) {
  var div = el.querySelector('div');
  if (!div || !div.classList.contains('collapsible')) {
    div = document.createElement('div');
    while (el.childNodes.length > 0)
      div.appendChild(el.firstChild);
    el.appendChild(div);
  }

  div.className = '';
  div.classList.add('collapsible');
  for (var i = 0; i < classes.length; i++)
    div.classList.add(classes[i]);
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('cloudprint', function() {
  'use strict';

  /**
   * API to the Google Cloud Print service.
   * @param {string} baseUrl Base part of the Google Cloud Print service URL
   *     with no trailing slash. For example,
   *     'https://www.google.com/cloudprint'.
   * @param {!print_preview.NativeLayer} nativeLayer Native layer used to get
   *     Auth2 tokens.
   * @constructor
   * @extends {cr.EventTarget}
   */
  function CloudPrintInterface(baseUrl, nativeLayer) {
    /**
     * The base URL of the Google Cloud Print API.
     * @type {string}
     * @private
     */
    this.baseUrl_ = baseUrl;

    /**
     * Used to get Auth2 tokens.
     * @type {!print_preview.NativeLayer}
     * @private
     */
    this.nativeLayer_ = nativeLayer;

    /**
     * Last received XSRF token. Sent as a parameter in every request.
     * @type {string}
     * @private
     */
    this.xsrfToken_ = '';

    /**
     * Pending requests delayed until we get access token.
     * @type {!Array.<!CloudPrintRequest>}
     * @private
     */
    this.requestQueue_ = [];

    /**
     * Number of outstanding cloud destination search requests.
     * @type {number}
     * @private
     */
    this.outstandingCloudSearchRequestCount_ = 0;

    /**
     * Event tracker used to keep track of native layer events.
     * @type {!EventTracker}
     * @private
     */
    this.tracker_ = new EventTracker();

    this.addEventListeners_();
  };

  /**
   * Event types dispatched by the interface.
   * @enum {string}
   */
  CloudPrintInterface.EventType = {
    PRINTER_DONE: 'cloudprint.CloudPrintInterface.PRINTER_DONE',
    PRINTER_FAILED: 'cloudprint.CloudPrintInterface.PRINTER_FAILED',
    SEARCH_DONE: 'cloudprint.CloudPrintInterface.SEARCH_DONE',
    SEARCH_FAILED: 'cloudprint.CloudPrintInterface.SEARCH_FAILED',
    SUBMIT_DONE: 'cloudprint.CloudPrintInterface.SUBMIT_DONE',
    SUBMIT_FAILED: 'cloudprint.CloudPrintInterface.SUBMIT_FAILED',
    UPDATE_PRINTER_TOS_ACCEPTANCE_FAILED:
        'cloudprint.CloudPrintInterface.UPDATE_PRINTER_TOS_ACCEPTANCE_FAILED'
  };

  /**
   * Content type header value for a URL encoded HTTP request.
   * @type {string}
   * @const
   * @private
   */
  CloudPrintInterface.URL_ENCODED_CONTENT_TYPE_ =
      'application/x-www-form-urlencoded';

  /**
   * Multi-part POST request boundary used in communication with Google
   * Cloud Print.
   * @type {string}
   * @const
   * @private
   */
  CloudPrintInterface.MULTIPART_BOUNDARY_ =
      '----CloudPrintFormBoundaryjc9wuprokl8i';

  /**
   * Content type header value for a multipart HTTP request.
   * @type {string}
   * @const
   * @private
   */
  CloudPrintInterface.MULTIPART_CONTENT_TYPE_ =
      'multipart/form-data; boundary=' +
      CloudPrintInterface.MULTIPART_BOUNDARY_;

  /**
   * Regex that extracts Chrome's version from the user-agent string.
   * @type {!RegExp}
   * @const
   * @private
   */
  CloudPrintInterface.VERSION_REGEXP_ = /.*Chrome\/([\d\.]+)/i;

  /**
   * Enumeration of JSON response fields from Google Cloud Print API.
   * @enum {string}
   * @private
   */
  CloudPrintInterface.JsonFields_ = {
    PRINTER: 'printer'
  };

  /**
   * Could Print origins used to search printers.
   * @type {!Array.<!print_preview.Destination.Origin>}
   * @const
   * @private
   */
  CloudPrintInterface.CLOUD_ORIGINS_ = [
      print_preview.Destination.Origin.COOKIES,
      print_preview.Destination.Origin.DEVICE
      // TODO(vitalybuka): Enable when implemented.
      // ready print_preview.Destination.Origin.PROFILE
  ];

  CloudPrintInterface.prototype = {
    __proto__: cr.EventTarget.prototype,

    /** @return {string} Base URL of the Google Cloud Print service. */
    get baseUrl() {
      return this.baseUrl_;
    },

    /**
     * @return {boolean} Whether a search for cloud destinations is in progress.
     */
    get isCloudDestinationSearchInProgress() {
      return this.outstandingCloudSearchRequestCount_ > 0;
    },

    /**
     * Sends a Google Cloud Print search API request.
     * @param {boolean} isRecent Whether to search for only recently used
     *     printers.
     */
    search: function(isRecent) {
      var params = [
        new HttpParam('connection_status', 'ALL'),
        new HttpParam('client', 'chrome'),
        new HttpParam('use_cdd', 'true')
      ];
      if (isRecent) {
        params.push(new HttpParam('q', '^recent'));
      }
      CloudPrintInterface.CLOUD_ORIGINS_.forEach(function(origin) {
        ++this.outstandingCloudSearchRequestCount_;
        var cpRequest =
            this.buildRequest_('GET', 'search', params, origin,
                               this.onSearchDone_.bind(this, isRecent));
        this.sendOrQueueRequest_(cpRequest);
      }, this);
    },

    /**
     * Sends a Google Cloud Print submit API request.
     * @param {!print_preview.Destination} destination Cloud destination to
     *     print to.
     * @param {!print_preview.PrintTicketStore} printTicketStore Contains the
     *     print ticket to print.
     * @param {!print_preview.DocumentInfo} documentInfo Document data model.
     * @param {string} data Base64 encoded data of the document.
     */
    submit: function(destination, printTicketStore, documentInfo, data) {
      var result =
          CloudPrintInterface.VERSION_REGEXP_.exec(navigator.userAgent);
      var chromeVersion = 'unknown';
      if (result && result.length == 2) {
        chromeVersion = result[1];
      }
      var params = [
        new HttpParam('printerid', destination.id),
        new HttpParam('contentType', 'dataUrl'),
        new HttpParam('title', documentInfo.title),
        new HttpParam('ticket',
                      this.createPrintTicket_(destination, printTicketStore)),
        new HttpParam('content', 'data:application/pdf;base64,' + data),
        new HttpParam('tag',
                      '__google__chrome_version=' + chromeVersion),
        new HttpParam('tag', '__google__os=' + navigator.platform)
      ];
      var cpRequest = this.buildRequest_('POST', 'submit', params,
                                         destination.origin,
                                         this.onSubmitDone_.bind(this));
      this.sendOrQueueRequest_(cpRequest);
    },

    /**
     * Sends a Google Cloud Print printer API request.
     * @param {string} printerId ID of the printer to lookup.
     * @param {!print_preview.Destination.Origin} origin Origin of the printer.
     */
    printer: function(printerId, origin) {
      var params = [
        new HttpParam('printerid', printerId),
        new HttpParam('use_cdd', 'true')
      ];
      var cpRequest =
          this.buildRequest_('GET', 'printer', params, origin,
                             this.onPrinterDone_.bind(this, printerId));
      this.sendOrQueueRequest_(cpRequest);
    },

    /**
     * Sends a Google Cloud Print update API request to accept (or reject) the
     * terms-of-service of the given printer.
     * @param {string} printerId ID of the printer to accept the
     *     terms-of-service for.
     * @param {!print_preview.Destination.Origin} origin Origin of the printer.
     * @param {boolean} isAccepted Whether the user accepted the
     *     terms-of-service.
     */
    updatePrinterTosAcceptance: function(printerId, origin, isAccepted) {
      var params = [
        new HttpParam('printerid', printerId),
        new HttpParam('is_tos_accepted', isAccepted)
      ];
      var cpRequest =
          this.buildRequest_('POST', 'update', params, origin,
                             this.onUpdatePrinterTosAcceptanceDone_.bind(this));
      this.sendOrQueueRequest_(cpRequest);
    },

    /**
     * Adds event listeners to the relevant native layer events.
     * @private
     */
    addEventListeners_: function() {
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.ACCESS_TOKEN_READY,
          this.onAccessTokenReady_.bind(this));
    },

    /**
     * Creates an object that represents a Google Cloud Print print ticket.
     * @param {!print_preview.Destination} destination Destination to print to.
     * @param {!print_preview.PrintTicketStore} printTicketStore Used to create
     *     the state of the print ticket.
     * @return {!Object} Google Cloud Print print ticket.
     * @private
     */
    createPrintTicket_: function(destination, printTicketStore) {
      assert(!destination.isLocal,
             'Trying to create a Google Cloud Print print ticket for a local ' +
                 'destination');
      assert(destination.capabilities,
             'Trying to create a Google Cloud Print print ticket for a ' +
                 'destination with no print capabilities');
      var pts = printTicketStore; // For brevity.
      var cjt = {
        version: '1.0',
        print: {}
      };
      if (pts.collate.isCapabilityAvailable() && pts.collate.isUserEdited()) {
        cjt.print.collate = {collate: pts.collate.getValue() == 'true'};
      }
      if (pts.color.isCapabilityAvailable() && pts.color.isUserEdited()) {
        var colorType = pts.color.getValue() ?
            'STANDARD_COLOR' : 'STANDARD_MONOCHROME';
        // Find option with this colorType to read its vendor_id.
        var selectedOptions = destination.capabilities.printer.color.option.
            filter(function(option) {
              return option.type == colorType;
            });
        if (selectedOptions.length == 0) {
          console.error('Could not find correct color option');
        } else {
          cjt.print.color = {type: colorType};
          if (selectedOptions[0].hasOwnProperty('vendor_id')) {
            cjt.print.color.vendor_id = selectedOptions[0].vendor_id;
          }
        }
      }
      if (pts.copies.isCapabilityAvailable() && pts.copies.isUserEdited()) {
        cjt.print.copies = {copies: pts.copies.getValueAsNumber()};
      }
      if (pts.duplex.isCapabilityAvailable() && pts.duplex.isUserEdited()) {
        cjt.print.duplex =
            {type: pts.duplex.getValue() ? 'LONG_EDGE' : 'NO_DUPLEX'};
      }
      if (pts.landscape.isCapabilityAvailable() &&
          pts.landscape.isUserEdited()) {
        cjt.print.page_orientation =
            {type: pts.landscape.getValue() ? 'LANDSCAPE' : 'PORTRAIT'};
      }
      return JSON.stringify(cjt);
    },

    /**
     * Builds request to the Google Cloud Print API.
     * @param {string} method HTTP method of the request.
     * @param {string} action Google Cloud Print action to perform.
     * @param {Array.<!HttpParam>} params HTTP parameters to include in the
     *     request.
     * @param {!print_preview.Destination.Origin} origin Origin for destination.
     * @param {function(number, Object, !print_preview.Destination.Origin)}
     *     callback Callback to invoke when request completes.
     * @return {!CloudPrintRequest} Partially prepared request.
     * @private
     */
    buildRequest_: function(method, action, params, origin, callback) {
      var url = this.baseUrl_ + '/' + action + '?xsrf=';
      if (origin == print_preview.Destination.Origin.COOKIES) {
        if (!this.xsrfToken_) {
          // TODO(rltoscano): Should throw an error if not a read-only action or
          // issue an xsrf token request.
        } else {
          url = url + this.xsrfToken_;
        }
      }
      var body = null;
      if (params) {
        if (method == 'GET') {
          url = params.reduce(function(partialUrl, param) {
            return partialUrl + '&' + param.name + '=' +
                encodeURIComponent(param.value);
          }, url);
        } else if (method == 'POST') {
          body = params.reduce(function(partialBody, param) {
            return partialBody + 'Content-Disposition: form-data; name=\"' +
                param.name + '\"\r\n\r\n' + param.value + '\r\n--' +
                CloudPrintInterface.MULTIPART_BOUNDARY_ + '\r\n';
          }, '--' + CloudPrintInterface.MULTIPART_BOUNDARY_ + '\r\n');
        }
      }

      var headers = {};
      headers['X-CloudPrint-Proxy'] = 'ChromePrintPreview';
      if (method == 'GET') {
        headers['Content-Type'] = CloudPrintInterface.URL_ENCODED_CONTENT_TYPE_;
      } else if (method == 'POST') {
        headers['Content-Type'] = CloudPrintInterface.MULTIPART_CONTENT_TYPE_;
      }

      var xhr = new XMLHttpRequest();
      xhr.open(method, url, true);
      xhr.withCredentials =
          (origin == print_preview.Destination.Origin.COOKIES);
      for (var header in headers) {
        xhr.setRequestHeader(header, headers[header]);
      }

      return new CloudPrintRequest(xhr, body, origin, callback);
    },

    /**
     * Sends a request to the Google Cloud Print API or queues if it needs to
     *     wait OAuth2 access token.
     * @param {!CloudPrintRequest} request Request to send or queue.
     * @private
     */
    sendOrQueueRequest_: function(request) {
      if (request.origin == print_preview.Destination.Origin.COOKIES) {
        return this.sendRequest_(request);
      } else {
        this.requestQueue_.push(request);
        this.nativeLayer_.startGetAccessToken(request.origin);
      }
    },

    /**
     * Sends a request to the Google Cloud Print API.
     * @param {!CloudPrintRequest} request Request to send.
     * @private
     */
    sendRequest_: function(request) {
      request.xhr.onreadystatechange =
          this.onReadyStateChange_.bind(this, request);
      request.xhr.send(request.body);
    },

    /**
     * Creates a Google Cloud Print interface error that is ready to dispatch.
     * @param {!CloudPrintInterface.EventType} type Type of the error.
     * @param {!CloudPrintRequest} request Request that has been completed.
     * @return {!Event} Google Cloud Print interface error event.
     * @private
     */
    createErrorEvent_: function(type, request) {
      var errorEvent = new Event(type);
      errorEvent.status = request.xhr.status;
      if (request.xhr.status == 200) {
        errorEvent.errorCode = request.result['errorCode'];
        errorEvent.message = request.result['message'];
      } else {
        errorEvent.errorCode = 0;
        errorEvent.message = '';
      }
      errorEvent.origin = request.origin;
      return errorEvent;
    },

    /**
     * Called when a native layer receives access token.
     * @param {Event} evt Contains the authetication type and access token.
     * @private
     */
    onAccessTokenReady_: function(event) {
      // TODO(vitalybuka): remove when other Origins implemented.
      assert(event.authType == print_preview.Destination.Origin.DEVICE);
      this.requestQueue_ = this.requestQueue_.filter(function(request) {
        assert(request.origin == print_preview.Destination.Origin.DEVICE);
        if (request.origin != event.authType) {
          return true;
        }
        if (event.accessToken) {
          request.xhr.setRequestHeader('Authorization',
                                       'Bearer ' + event.accessToken);
          this.sendRequest_(request);
        } else {  // No valid token.
          // Without abort status does not exists.
          request.xhr.abort();
          request.callback(request);
        }
        return false;
      }, this);
    },

    /**
     * Called when the ready-state of a XML http request changes.
     * Calls the successCallback with the result or dispatches an ERROR event.
     * @param {!CloudPrintRequest} request Request that was changed.
     * @private
     */
    onReadyStateChange_: function(request) {
      if (request.xhr.readyState == 4) {
        if (request.xhr.status == 200) {
          request.result = JSON.parse(request.xhr.responseText);
          if (request.origin == print_preview.Destination.Origin.COOKIES &&
              request.result['success']) {
            this.xsrfToken_ = request.result['xsrf_token'];
          }
        }
        request.status = request.xhr.status;
        request.callback(request);
      }
    },

    /**
     * Called when the search request completes.
     * @param {boolean} isRecent Whether the search request was for recent
     *     destinations.
     * @param {!CloudPrintRequest} request Request that has been completed.
     * @private
     */
    onSearchDone_: function(isRecent, request) {
      --this.outstandingCloudSearchRequestCount_;
      if (request.xhr.status == 200 && request.result['success']) {
        var printerListJson = request.result['printers'] || [];
        var printerList = [];
        printerListJson.forEach(function(printerJson) {
          try {
            printerList.push(
                cloudprint.CloudDestinationParser.parse(printerJson,
                                                        request.origin));
          } catch (err) {
            console.error('Unable to parse cloud print destination: ' + err);
          }
        });
        var searchDoneEvent =
            new Event(CloudPrintInterface.EventType.SEARCH_DONE);
        searchDoneEvent.printers = printerList;
        searchDoneEvent.origin = request.origin;
        searchDoneEvent.isRecent = isRecent;
        searchDoneEvent.email = request.result['request']['user'];
        this.dispatchEvent(searchDoneEvent);
      } else {
        var errorEvent = this.createErrorEvent_(
            CloudPrintInterface.EventType.SEARCH_FAILED, request);
        this.dispatchEvent(errorEvent);
      }
    },

    /**
     * Called when the submit request completes.
     * @param {!CloudPrintRequest} request Request that has been completed.
     * @private
     */
    onSubmitDone_: function(request) {
      if (request.xhr.status == 200 && request.result['success']) {
        var submitDoneEvent = new Event(
            CloudPrintInterface.EventType.SUBMIT_DONE);
        submitDoneEvent.jobId = request.result['job']['id'];
        this.dispatchEvent(submitDoneEvent);
      } else {
        var errorEvent = this.createErrorEvent_(
            CloudPrintInterface.EventType.SUBMIT_FAILED, request);
        this.dispatchEvent(errorEvent);
      }
    },

    /**
     * Called when the printer request completes.
     * @param {string} destinationId ID of the destination that was looked up.
     * @param {!CloudPrintRequest} request Request that has been completed.
     * @private
     */
    onPrinterDone_: function(destinationId, request) {
      if (request.xhr.status == 200 && request.result['success']) {
        var printerJson = request.result['printers'][0];
        var printer;
        try {
          printer = cloudprint.CloudDestinationParser.parse(printerJson,
                                                            request.origin);
        } catch (err) {
          console.error('Failed to parse cloud print destination: ' +
              JSON.stringify(printerJson));
          return;
        }
        var printerDoneEvent =
            new Event(CloudPrintInterface.EventType.PRINTER_DONE);
        printerDoneEvent.printer = printer;
        this.dispatchEvent(printerDoneEvent);
      } else {
        var errorEvent = this.createErrorEvent_(
            CloudPrintInterface.EventType.PRINTER_FAILED, request);
        errorEvent.destinationId = destinationId;
        errorEvent.destinationOrigin = request.origin;
        this.dispatchEvent(errorEvent, request.origin);
      }
    },

    /**
     * Called when the update printer TOS acceptance request completes.
     * @param {!CloudPrintRequest} request Request that has been completed.
     * @private
     */
    onUpdatePrinterTosAcceptanceDone_: function(request) {
      if (request.xhr.status == 200 && request.result['success']) {
        // Do nothing.
      } else {
        var errorEvent = this.createErrorEvent_(
            CloudPrintInterface.EventType.SUBMIT_FAILED, request);
        this.dispatchEvent(errorEvent);
      }
    }
  };

  /**
   * Data structure that holds data for Cloud Print requests.
   * @param {!XMLHttpRequest} xhr Partially prepared http request.
   * @param {string} body Data to send with POST requests.
   * @param {!print_preview.Destination.Origin} origin Origin for destination.
   * @param {function(!CloudPrintRequest)} callback Callback to invoke when
   *     request completes.
   * @constructor
   */
  function CloudPrintRequest(xhr, body, origin, callback) {
    /**
     * Partially prepared http request.
     * @type {!XMLHttpRequest}
     */
    this.xhr = xhr;

    /**
     * Data to send with POST requests.
     * @type {string}
     */
    this.body = body;

    /**
     * Origin for destination.
     * @type {!print_preview.Destination.Origin}
     */
    this.origin = origin;

    /**
     * Callback to invoke when request completes.
     * @type {function(!CloudPrintRequest)}
     */
    this.callback = callback;

    /**
     * Result for requests.
     * @type {Object} JSON response.
     */
    this.result = null;
  };

  /**
   * Data structure that represents an HTTP parameter.
   * @param {string} name Name of the parameter.
   * @param {string} value Value of the parameter.
   * @constructor
   */
  function HttpParam(name, value) {
    /**
     * Name of the parameter.
     * @type {string}
     */
    this.name = name;

    /**
     * Name of the value.
     * @type {string}
     */
    this.value = value;
  };

  // Export
  return {
    CloudPrintInterface: CloudPrintInterface
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {string} toTest The string to be tested.
 * @return {boolean} True if |toTest| contains only digits. Leading and trailing
 *     whitespace is allowed.
 */
function isInteger(toTest) {
  var numericExp = /^\s*[0-9]+\s*$/;
  return numericExp.test(toTest);
}

/**
 * Returns true if |value| is a valid non zero positive integer.
 * @param {string} value The string to be tested.
 * @return {boolean} true if the |value| is valid non zero positive integer.
 */
function isPositiveInteger(value) {
  return isInteger(value) && parseInt(value, 10) > 0;
}

/**
 * Returns true if the contents of the two arrays are equal.
 * @param {Array.<{from: number, to: number}>} array1 The first array.
 * @param {Array.<{from: number, to: number}>} array2 The second array.
 * @return {boolean} true if the arrays are equal.
 */
function areArraysEqual(array1, array2) {
  if (array1.length != array2.length)
    return false;
  for (var i = 0; i < array1.length; i++)
    if (array1[i] !== array2[i])
      return false;
  return true;
}

/**
 * Returns true if the contents of the two page ranges are equal.
 * @param {Array} array1 The first array.
 * @param {Array} array2 The second array.
 * @return {boolean} true if the arrays are equal.
 */
function areRangesEqual(array1, array2) {
  if (array1.length != array2.length)
    return false;
  for (var i = 0; i < array1.length; i++)
    if (array1[i].from != array2[i].from ||
        array1[i].to != array2[i].to) {
    return false;
  }
  return true;
}

/**
 * Removes duplicate elements from |inArray| and returns a new array.
 * |inArray| is not affected. It assumes that |inArray| is already sorted.
 * @param {Array.<number>} inArray The array to be processed.
 * @return {Array.<number>} The array after processing.
 */
function removeDuplicates(inArray) {
  var out = [];

  if (inArray.length == 0)
    return out;

  out.push(inArray[0]);
  for (var i = 1; i < inArray.length; ++i)
    if (inArray[i] != inArray[i - 1])
      out.push(inArray[i]);
  return out;
}

/**
 * Returns a list of ranges in |pageRangeText|. The ranges are
 * listed in the order they appear in |pageRangeText| and duplicates are not
 * eliminated. If |pageRangeText| is not valid null is returned.
 * A valid selection has a parsable format and every page identifier is
 * greater the 0 and less or equal to |totalPageCount| unless wildcards are
 * used(see examples).
 * If |totalPageCount| is 0 or undefined function uses impossibly large number
 * instead.
 * Wildcard the first number must be larger then 0 and less or equal then
 * |totalPageCount|. If it's missed then 1 is used as the first number.
 * Wildcard the second number must be larger then the first number. If it's
 * missed then |totalPageCount| is used as the second number.
 * Example: "1-4, 9, 3-6, 10, 11" is valid, assuming |totalPageCount| >= 11.
 * Example: "1-4, -6" is valid, assuming |totalPageCount| >= 6.
 * Example: "2-" is valid, assuming |totalPageCount| >= 2, means from 2 to the
 *          end.
 * Example: "4-2, 11, -6" is invalid.
 * Example: "-" is valid, assuming |totalPageCount| >= 1.
 * Example: "1-4dsf, 11" is invalid regardless of |totalPageCount|.
 * @param {string} pageRangeText The text to be checked.
 * @param {number} totalPageCount The total number of pages.
 * @return {Array.<{from: number, to: number}>} An array of page range objects.
 */
function pageRangeTextToPageRanges(pageRangeText, totalPageCount) {
  if (pageRangeText == '') {
    return [];
  }

  var MAX_PAGE_NUMBER = 1000000000;
  totalPageCount = totalPageCount ? totalPageCount : MAX_PAGE_NUMBER;

  var regex = /^\s*([0-9]*)\s*-\s*([0-9]*)\s*$/;
  var parts = pageRangeText.split(/,/);

  var pageRanges = [];
  for (var i = 0; i < parts.length; ++i) {
    var match = parts[i].match(regex);
    if (match) {
      if (!isPositiveInteger(match[1]) && match[1] !== '')
        return null;
      if (!isPositiveInteger(match[2]) && match[2] !== '')
        return null;
      var from = match[1] ? parseInt(match[1], 10) : 1;
      var to = match[2] ? parseInt(match[2], 10) : totalPageCount;
      if (from > to || from > totalPageCount)
        return null;
      pageRanges.push({'from': from, 'to': to});
    } else {
      if (!isPositiveInteger(parts[i]))
        return null;
      var singlePageNumber = parseInt(parts[i], 10);
      if (singlePageNumber > totalPageCount)
        return null;
      pageRanges.push({'from': singlePageNumber, 'to': singlePageNumber});
    }
  }
  return pageRanges;
}

/**
 * Returns a list of pages defined by |pagesRangeText|. The pages are
 * listed in the order they appear in |pageRangeText| and duplicates are not
 * eliminated. If |pageRangeText| is not valid according or
 * |totalPageCount| undefined [1,2,...,totalPageCount] is returned.
 * See pageRangeTextToPageRanges for details.
 * @param {string} pageRangeText The text to be checked.
 * @param {number} totalPageCount The total number of pages.
 * @return {Array.<number>} A list of all pages.
 */
function pageRangeTextToPageList(pageRangeText, totalPageCount) {
  var pageRanges = pageRangeTextToPageRanges(pageRangeText, totalPageCount);
  pageList = [];
  if (pageRanges) {
    for (var i = 0; i < pageRanges.length; ++i) {
      for (var j = pageRanges[i].from; j <= Math.min(pageRanges[i].to,
                                                     totalPageCount); ++j) {
        pageList.push(j);
      }
    }
  }
  if (pageList.length == 0) {
    for (var j = 1; j <= totalPageCount; ++j)
      pageList.push(j);
  }
  return pageList;
}

/**
 * @param {Array.<number>} pageList The list to be processed.
 * @return {Array.<number>} The contents of |pageList| in ascending order and
 *     without any duplicates. |pageList| is not affected.
 */
function pageListToPageSet(pageList) {
  var pageSet = [];
  if (pageList.length == 0)
    return pageSet;
  pageSet = pageList.slice(0);
  pageSet.sort(function(a, b) {
    return (/** @type {number} */ a) - (/** @type {number} */ b);
  });
  pageSet = removeDuplicates(pageSet);
  return pageSet;
}

/**
 * @param {!HTMLElement} element Element to check for visibility.
 * @return {boolean} Whether the given element is visible.
 */
function getIsVisible(element) {
  return !element.hidden;
}

/**
 * Shows or hides an element.
 * @param {!HTMLElement} element Element to show or hide.
 * @param {boolean} isVisible Whether the element should be visible or not.
 */
function setIsVisible(element, isVisible) {
  element.hidden = !isVisible;
}

/**
 * @param {!Array} array Array to check for item.
 * @param {*} item Item to look for in array.
 * @return {boolean} Whether the item is in the array.
 */
function arrayContains(array, item) {
  return array.indexOf(item) != -1;
}

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Creates a PrintHeader object. This object encapsulates all the elements
   * and logic related to the top part of the left pane in print_preview.html.
   * @param {!print_preview.PrintTicketStore} printTicketStore Used to read
   *     information about the document.
   * @param {!print_preview.DestinationStore} destinationStore Used to get the
   *     selected destination.
   * @constructor
   * @extends {print_preview.Component}
   */
  function PrintHeader(printTicketStore, destinationStore) {
    print_preview.Component.call(this);

    /**
     * Used to read information about the document.
     * @type {!print_preview.PrintTicketStore}
     * @private
     */
    this.printTicketStore_ = printTicketStore;

    /**
     * Used to get the selected destination.
     * @type {!print_preview.DestinationStore}
     * @private
     */
    this.destinationStore_ = destinationStore;

    /**
     * Whether the component is enabled.
     * @type {boolean}
     * @private
     */
    this.isEnabled_ = true;

    /**
     * Whether the print button is enabled.
     * @type {boolean}
     * @private
     */
    this.isPrintButtonEnabled_ = true;
  };

  /**
   * Event types dispatched by the print header.
   * @enum {string}
   */
  PrintHeader.EventType = {
    PRINT_BUTTON_CLICK: 'print_preview.PrintHeader.PRINT_BUTTON_CLICK',
    CANCEL_BUTTON_CLICK: 'print_preview.PrintHeader.CANCEL_BUTTON_CLICK'
  };

  PrintHeader.prototype = {
    __proto__: print_preview.Component.prototype,

    set isEnabled(isEnabled) {
      this.isEnabled_ = isEnabled;
      this.updatePrintButtonEnabledState_();
      this.getChildElement('button.cancel').disabled = !isEnabled;
    },

    set isPrintButtonEnabled(isEnabled) {
      this.isPrintButtonEnabled_ = isEnabled;
      this.updatePrintButtonEnabledState_();
    },

    /** @param {string} message Error message to display in the print header. */
    setErrorMessage: function(message) {
      var summaryEl = this.getChildElement('.summary');
      summaryEl.innerHTML = '';
      summaryEl.textContent = message;
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);

      // User events
      this.tracker.add(
          this.getChildElement('button.cancel'),
          'click',
          this.onCancelButtonClick_.bind(this));
      this.tracker.add(
          this.getChildElement('button.print'),
          'click',
          this.onPrintButtonClick_.bind(this));

      // Data events.
      this.tracker.add(
          this.printTicketStore_,
          print_preview.PrintTicketStore.EventType.INITIALIZE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_,
          print_preview.PrintTicketStore.EventType.DOCUMENT_CHANGE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_,
          print_preview.PrintTicketStore.EventType.TICKET_CHANGE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.DESTINATION_SELECT,
          this.onDestinationSelect_.bind(this));
      this.tracker.add(
          this.printTicketStore_.copies,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_.duplex,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_.pageRange,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onTicketChange_.bind(this));
    },

    /**
     * Updates Print Button state.
     * @private
     */
    updatePrintButtonEnabledState_: function() {
      this.getChildElement('button.print').disabled =
          !this.isEnabled_ ||
          !this.isPrintButtonEnabled_ ||
          !this.printTicketStore_.isTicketValid();
    },

    /**
     * Updates the summary element based on the currently selected user options.
     * @private
     */
    updateSummary_: function() {
      if (!this.printTicketStore_.isTicketValid()) {
        this.getChildElement('.summary').innerHTML = '';
        return;
      }

      var summaryLabel =
          localStrings.getString('printPreviewSheetsLabelSingular');
      var pagesLabel = localStrings.getString('printPreviewPageLabelPlural');

      var saveToPdf = this.destinationStore_.selectedDestination &&
          this.destinationStore_.selectedDestination.id ==
              print_preview.Destination.GooglePromotedId.SAVE_AS_PDF;
      if (saveToPdf) {
        summaryLabel = localStrings.getString('printPreviewPageLabelSingular');
      }

      var numPages = this.printTicketStore_.pageRange.getPageNumberSet().size;
      var numSheets = numPages;
      if (!saveToPdf && this.printTicketStore_.duplex.getValue()) {
        numSheets = Math.ceil(numPages / 2);
      }

      var copies = this.printTicketStore_.copies.getValueAsNumber();
      numSheets *= copies;
      numPages *= copies;

      if (numSheets > 1) {
        summaryLabel = saveToPdf ? pagesLabel :
            localStrings.getString('printPreviewSheetsLabelPlural');
      }

      var html;
      if (numPages != numSheets) {
        html = localStrings.getStringF('printPreviewSummaryFormatLong',
                                       '<b>' + numSheets + '</b>',
                                       '<b>' + summaryLabel + '</b>',
                                       numPages,
                                       pagesLabel);
      } else {
        html = localStrings.getStringF('printPreviewSummaryFormatShort',
                                       '<b>' + numSheets + '</b>',
                                       '<b>' + summaryLabel + '</b>');
      }

      // Removing extra spaces from within the string.
      html = html.replace(/\s{2,}/g, ' ');
      this.getChildElement('.summary').innerHTML = html;
    },

    /**
     * Called when the print button is clicked. Dispatches a PRINT_DOCUMENT
     * common event.
     * @private
     */
    onPrintButtonClick_: function() {
      if (this.destinationStore_.selectedDestination.id !=
          print_preview.Destination.GooglePromotedId.SAVE_AS_PDF) {
        this.getChildElement('button.print').classList.add('loading');
        this.getChildElement('button.cancel').classList.add('loading');
        this.getChildElement('.summary').innerHTML =
            localStrings.getString('printing');
      }
      cr.dispatchSimpleEvent(this, PrintHeader.EventType.PRINT_BUTTON_CLICK);
    },

    /**
     * Called when the cancel button is clicked. Dispatches a
     * CLOSE_PRINT_PREVIEW event.
     * @private
     */
    onCancelButtonClick_: function() {
      cr.dispatchSimpleEvent(this, PrintHeader.EventType.CANCEL_BUTTON_CLICK);
    },

    /**
     * Called when a new destination is selected. Updates the text on the print
     * button.
     * @private
     */
    onDestinationSelect_: function() {
      var isSaveLabel = this.destinationStore_.selectedDestination.id ==
          print_preview.Destination.GooglePromotedId.SAVE_AS_PDF ||
          this.destinationStore_.selectedDestination.id ==
              print_preview.Destination.GooglePromotedId.DOCS;
      this.getChildElement('button.print').textContent = isSaveLabel ?
          localStrings.getString('saveButton') :
          localStrings.getString('printButton');
      this.getChildElement('button.print').focus();
    },

    /**
     * Called when the print ticket has changed. Disables the print button if
     * any of the settings are invalid.
     * @private
     */
    onTicketChange_: function() {
      this.updatePrintButtonEnabledState_();
      this.updateSummary_();
      if (document.activeElement == null ||
          document.activeElement == document.body) {
        this.getChildElement('button.print').focus();
      }
    }
  };

  // Export
  return {
    PrintHeader: PrintHeader
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Object used to measure usage statistics.
   * @constructor
   */
  function Metrics() {};

  /**
   * Enumeration of metrics bucket groups. Each group describes a set of events
   * that can happen in order. This implies that an event cannot occur twice and
   * an event that occurs semantically before another event, should not occur
   * after.
   * @enum {number}
   */
  Metrics.BucketGroup = {
    DESTINATION_SEARCH: 0,
    GCP_PROMO: 1
  };

  /**
   * Enumeration of buckets that a user can enter while using the destination
   * search widget.
   * @enum {number}
   */
  Metrics.DestinationSearchBucket = {
    // Used when the print destination search widget is shown.
    SHOWN: 0,
    // Used when the user selects a print destination.
    DESTINATION_SELECTED: 1,
    // Used when the print destination search widget is closed without selecting
    // a print destination.
    CANCELED: 2,
    // Used when the Google Cloud Print promotion (shown in the destination
    // search widget) is shown to the user.
    CLOUDPRINT_PROMO_SHOWN: 3,
    // Used when the user chooses to sign-in to their Google account.
    SIGNIN_TRIGGERED: 4
  };

  /**
   * Enumeration of buckets that a user can enter while using the Google Cloud
   * Print promotion.
   * @enum {number}
   */
  Metrics.GcpPromoBucket = {
    // Used when the Google Cloud Print pomotion (shown above the pdf preview
    // plugin) is shown to the user.
    SHOWN: 0,
    // Used when the user clicks the "Get started" link in the promotion shown
    // in CLOUDPRINT_BIG_PROMO_SHOWN.
    CLICKED: 1,
    // Used when the user dismisses the promotion shown in
    // CLOUDPRINT_BIG_PROMO_SHOWN.
    DISMISSED: 2
  };

  /**
   * Name of the C++ function to call to increment bucket counts.
   * @type {string}
   * @const
   * @private
   */
  Metrics.NATIVE_FUNCTION_NAME_ = 'reportUiEvent';

  Metrics.prototype = {
    /**
     * Increments the counter of a destination-search bucket.
     * @param {!Metrics.DestinationSearchBucket} bucket Bucket to increment.
     */
    incrementDestinationSearchBucket: function(bucket) {
      chrome.send(Metrics.NATIVE_FUNCTION_NAME_,
                  [Metrics.BucketGroup.DESTINATION_SEARCH, bucket]);
    },

    /**
     * Increments the counter of a gcp-promo bucket.
     * @param {!Metrics.GcpPromoBucket} bucket Bucket to increment.
     */
    incrementGcpPromoBucket: function(bucket) {
      chrome.send(Metrics.NATIVE_FUNCTION_NAME_,
                  [Metrics.BucketGroup.GCP_PROMO, bucket]);
    }
  };

  // Export
  return {
    Metrics: Metrics
  };
});


// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Creates a PageSettings object. This object encapsulates all settings and
   * logic related to page selection.
   * @param {!print_preview.ticket_items.PageRange} pageRangeTicketItem Used to
   *     read and write page range settings.
   * @constructor
   * @extends {print_preview.Component}
   */
  function PageSettings(pageRangeTicketItem) {
    print_preview.Component.call(this);

    /**
     * Used to read and write page range settings.
     * @type {!print_preview.ticket_items.PageRange}
     * @private
     */
    this.pageRangeTicketItem_ = pageRangeTicketItem;

    /**
     * Timeout used to delay processing of the custom page range input.
     * @type {?number}
     * @private
     */
    this.customInputTimeout_ = null;

    /**
     * Custom page range input.
     * @type {HTMLInputElement}
     * @private
     */
    this.customInput_ = null;

    /**
     * Custom page range radio button.
     * @type {HTMLInputElement}
     * @private
     */
    this.customRadio_ = null;

    /**
     * All page rage radio button.
     * @type {HTMLInputElement}
     * @private
     */
    this.allRadio_ = null;

    /**
     * Container of a hint to show when the custom page range is invalid.
     * @type {HTMLElement}
     * @private
     */
    this.customHintEl_ = null;
  };

  /**
   * CSS classes used by the page settings.
   * @enum {string}
   * @private
   */
  PageSettings.Classes_ = {
    ALL_RADIO: 'page-settings-all-radio',
    CUSTOM_HINT: 'page-settings-custom-hint',
    CUSTOM_INPUT: 'page-settings-custom-input',
    CUSTOM_RADIO: 'page-settings-custom-radio'
  };

  /**
   * Delay in milliseconds before processing custom page range input.
   * @type {number}
   * @private
   */
  PageSettings.CUSTOM_INPUT_DELAY_ = 500;

  PageSettings.prototype = {
    __proto__: print_preview.Component.prototype,

    set isEnabled(isEnabled) {
      this.customInput_.disabled = !isEnabled;
      this.allRadio_.disabled = !isEnabled;
      this.customRadio_.disabled = !isEnabled;
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);
      this.tracker.add(
          this.allRadio_, 'click', this.onAllRadioClick_.bind(this));
      this.tracker.add(
          this.customRadio_, 'click', this.onCustomRadioClick_.bind(this));
      this.tracker.add(
          this.customInput_, 'blur', this.onCustomInputBlur_.bind(this));
      this.tracker.add(
          this.customInput_, 'focus', this.onCustomInputFocus_.bind(this));
      this.tracker.add(
          this.customInput_, 'keyup', this.onCustomInputKeyUp_.bind(this));
      this.tracker.add(
          this.pageRangeTicketItem_,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onPageRangeTicketItemChange_.bind(this));
    },

    /** @override */
    exitDocument: function() {
      print_preview.Component.prototype.exitDocument.call(this);
      this.customInput_ = null;
      this.customRadio_ = null;
      this.allRadio_ = null;
      this.customHintEl_ = null;
    },

    /** @override */
    decorateInternal: function() {
      this.customInput_ = this.getElement().getElementsByClassName(
          PageSettings.Classes_.CUSTOM_INPUT)[0];
      this.allRadio_ = this.getElement().getElementsByClassName(
          PageSettings.Classes_.ALL_RADIO)[0];
      this.customRadio_ = this.getElement().getElementsByClassName(
          PageSettings.Classes_.CUSTOM_RADIO)[0];
      this.customHintEl_ = this.getElement().getElementsByClassName(
          PageSettings.Classes_.CUSTOM_HINT)[0];
      this.customHintEl_.textContent = localStrings.getStringF(
          'pageRangeInstruction',
          localStrings.getString('examplePageRangeText'));
    },

    /**
     * @param {boolean} Whether the custom hint is visible.
     * @private
     */
    setInvalidStateVisible_: function(isVisible) {
      if (isVisible) {
        this.customInput_.classList.add('invalid');
        this.customHintEl_.setAttribute('aria-hidden', 'false');
        fadeInElement(this.customHintEl_);
      } else {
        this.customInput_.classList.remove('invalid');
        fadeOutElement(this.customHintEl_);
        this.customHintEl_.setAttribute('aria-hidden', 'true');
      }
    },

    /**
     * Called when the all radio button is clicked. Updates the print ticket.
     * @private
     */
    onAllRadioClick_: function() {
      this.pageRangeTicketItem_.updateValue(null);
    },

    /**
     * Called when the custom radio button is clicked. Updates the print ticket.
     * @private
     */
    onCustomRadioClick_: function() {
      this.customInput_.focus();
    },

    /**
     * Called when the custom input is blurred. Enables the all radio button if
     * the custom input is empty.
     * @private
     */
    onCustomInputBlur_: function() {
      if (this.customInput_.value == '') {
        this.allRadio_.checked = true;
      }
    },

    /**
     * Called when the custom input is focused.
     * @private
     */
    onCustomInputFocus_: function() {
      this.customRadio_.checked = true;
      this.pageRangeTicketItem_.updateValue(this.customInput_.value);
    },

    /**
     * Called when a key is pressed on the custom input.
     * @param {Event} event Contains the key that was pressed.
     * @private
     */
    onCustomInputKeyUp_: function(event) {
      if (this.customInputTimeout_) {
        clearTimeout(this.customInputTimeout_);
      }
      if (event.keyIdentifier == 'Enter') {
        this.pageRangeTicketItem_.updateValue(this.customInput_.value);
      } else {
        this.customRadio_.checked = true;
        this.customInputTimeout_ = setTimeout(
            this.onCustomInputTimeout_.bind(this),
            PageSettings.CUSTOM_INPUT_DELAY_);
      }
    },

    /**
     * Called after a delay following a key press in the custom input.
     * @private
     */
    onCustomInputTimeout_: function() {
      this.customInputTimeout_ = null;
      if (this.customRadio_.checked) {
        this.pageRangeTicketItem_.updateValue(this.customInput_.value);
      }
    },

    /**
     * Called when the print ticket changes. Updates the state of the component.
     * @private
     */
    onPageRangeTicketItemChange_: function() {
      if (this.pageRangeTicketItem_.isCapabilityAvailable()) {
        var pageRangeStr = this.pageRangeTicketItem_.getValue();
        if (pageRangeStr || this.customRadio_.checked) {
          if (!document.hasFocus() ||
              document.activeElement != this.customInput_) {
            this.customInput_.value = pageRangeStr;
          }
          this.customRadio_.checked = true;
          this.setInvalidStateVisible_(!this.pageRangeTicketItem_.isValid());
        } else {
          this.allRadio_.checked = true;
          this.setInvalidStateVisible_(false);
        }
        fadeInOption(this.getElement());
      } else {
        fadeOutOption(this.getElement());
      }
    }
  };

  // Export
  return {
    PageSettings: PageSettings
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Component that renders the copies settings UI.
   * @param {!print_preview.ticket_items.Copies} copiesTicketItem Used to read
   *     and write the copies value.
   * @param {!print_preview.ticket_items.Collate} collateTicketItem Used to read
   *     and write the collate value.
   * @constructor
   * @extends {print_preview.Component}
   */
  function CopiesSettings(copiesTicketItem, collateTicketItem) {
    print_preview.Component.call(this);

    /**
     * Used to read and write the copies value.
     * @type {!print_preview.ticket_items.Copies}
     * @private
     */
    this.copiesTicketItem_ = copiesTicketItem;

    /**
     * Used to read and write the collate value.
     * @type {!print_preview.ticket_items.Collate}
     * @private
     */
    this.collateTicketItem_ = collateTicketItem;

    /**
     * Timeout used to delay processing of the copies input.
     * @type {?number}
     * @private
     */
    this.textfieldTimeout_ = null;

    /**
     * Whether this component is enabled or not.
     * @type {boolean}
     * @private
     */
    this.isEnabled_ = true;
  };

  /**
   * Delay in milliseconds before processing the textfield.
   * @type {number}
   * @private
   */
  CopiesSettings.TEXTFIELD_DELAY_ = 250;

  CopiesSettings.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @param {boolean} isEnabled Whether the copies settings is enabled. */
    set isEnabled(isEnabled) {
      this.getChildElement('input.copies').disabled = !isEnabled;
      this.getChildElement('input.collate').disabled = !isEnabled;
      this.isEnabled_ = isEnabled;
      if (isEnabled) {
        this.updateState_();
      } else {
        this.getChildElement('button.increment').disabled = true;
        this.getChildElement('button.decrement').disabled = true;
      }
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);

      this.tracker.add(
          this.getChildElement('input.copies'),
          'keyup',
          this.onTextfieldKeyUp_.bind(this));
      this.tracker.add(
          this.getChildElement('input.copies'),
          'blur',
          this.onTextfieldBlur_.bind(this));
      this.tracker.add(
          this.getChildElement('button.increment'),
          'click',
          this.onButtonClicked_.bind(this, 1));
      this.tracker.add(
          this.getChildElement('button.decrement'),
          'click',
          this.onButtonClicked_.bind(this, -1));
      this.tracker.add(
          this.getChildElement('input.collate'),
          'click',
          this.onCollateCheckboxClick_.bind(this));
      this.tracker.add(
          this.copiesTicketItem_,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.updateState_.bind(this));
      this.tracker.add(
          this.collateTicketItem_,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.updateState_.bind(this));
    },

    /**
     * Updates the state of the copies settings UI controls.
     * @private
     */
    updateState_: function() {
      if (!this.copiesTicketItem_.isCapabilityAvailable()) {
        fadeOutOption(this.getElement());
        return;
      }

      if (this.getChildElement('input.copies').value !=
          this.copiesTicketItem_.getValue()) {
        this.getChildElement('input.copies').value =
            this.copiesTicketItem_.getValue();
      }

      var currentValueGreaterThan1 = false;
      if (this.copiesTicketItem_.isValid()) {
        this.getChildElement('input.copies').classList.remove('invalid');
        fadeOutElement(this.getChildElement('.hint'));
        this.getChildElement('.hint').setAttribute('aria-hidden', true);
        var currentValue = this.copiesTicketItem_.getValueAsNumber();
        var currentValueGreaterThan1 = currentValue > 1;
        this.getChildElement('button.increment').disabled =
            !this.isEnabled_ ||
            !this.copiesTicketItem_.wouldValueBeValid(currentValue + 1);
        this.getChildElement('button.decrement').disabled =
            !this.isEnabled_ ||
            !this.copiesTicketItem_.wouldValueBeValid(currentValue - 1);
      } else {
        this.getChildElement('input.copies').classList.add('invalid');
        this.getChildElement('.hint').setAttribute('aria-hidden', false);
        fadeInElement(this.getChildElement('.hint'));
        this.getChildElement('button.increment').disabled = true;
        this.getChildElement('button.decrement').disabled = true;
      }

      if (!(this.getChildElement('.collate-container').hidden =
             !this.collateTicketItem_.isCapabilityAvailable() ||
             !currentValueGreaterThan1)) {
        this.getChildElement('input.collate').checked =
            this.collateTicketItem_.getValue();
      }

      fadeInOption(this.getElement());
    },

    /**
     * Called whenever the increment/decrement buttons are clicked.
     * @param {number} delta Must be 1 for an increment button click and -1 for
     *     a decrement button click.
     * @private
     */
    onButtonClicked_: function(delta) {
      // Assumes text field has a valid number.
      var newValue =
          parseInt(this.getChildElement('input.copies').value) + delta;
      this.copiesTicketItem_.updateValue(newValue + '');
    },

    /**
     * Called after a timeout after user input into the textfield.
     * @private
     */
    onTextfieldTimeout_: function() {
      var copiesVal = this.getChildElement('input.copies').value;
      if (copiesVal != '') {
        this.copiesTicketItem_.updateValue(copiesVal);
      }
    },

    /**
     * Called when a keyup event occurs on the textfield. Starts an input
     * timeout.
     * @param {Event} event Contains the pressed key.
     * @private
     */
    onTextfieldKeyUp_: function(event) {
      if (this.textfieldTimeout_) {
        clearTimeout(this.textfieldTimeout_);
      }
      this.textfieldTimeout_ = setTimeout(
          this.onTextfieldTimeout_.bind(this), CopiesSettings.TEXTFIELD_DELAY_);
    },

    /**
     * Called when the focus leaves the textfield. If the textfield is empty,
     * its value is set to 1.
     * @private
     */
    onTextfieldBlur_: function() {
      if (this.getChildElement('input.copies').value == '') {
        this.copiesTicketItem_.updateValue('1');
      }
    },

    /**
     * Called when the collate checkbox is clicked. Updates the print ticket.
     * @private
     */
    onCollateCheckboxClick_: function() {
      this.collateTicketItem_.updateValue(
          this.getChildElement('input.collate').checked);
    }
  };

  // Export
  return {
    CopiesSettings: CopiesSettings
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Creates a LayoutSettings object. This object encapsulates all settings and
   * logic related to layout mode (portrait/landscape).
   * @param {!print_preview.ticket_items.Landscape} landscapeTicketItem Used to
   *     get the layout written to the print ticket.
   * @constructor
   * @extends {print_preview.Component}
   */
  function LayoutSettings(landscapeTicketItem) {
    print_preview.Component.call(this);

    /**
     * Used to get the layout written to the print ticket.
     * @type {!print_preview.ticket_items.Landscape}
     * @private
     */
    this.landscapeTicketItem_ = landscapeTicketItem;
  };

  /**
   * CSS classes used by the layout settings.
   * @enum {string}
   * @private
   */
  LayoutSettings.Classes_ = {
    LANDSCAPE_RADIO: 'layout-settings-landscape-radio',
    PORTRAIT_RADIO: 'layout-settings-portrait-radio'
  };

  LayoutSettings.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @param {boolean} isEnabled Whether this component is enabled. */
    set isEnabled(isEnabled) {
      this.landscapeRadioButton_.disabled = !isEnabled;
      this.portraitRadioButton_.disabled = !isEnabled;
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);
      this.tracker.add(
          this.portraitRadioButton_,
          'click',
          this.onLayoutButtonClick_.bind(this));
      this.tracker.add(
          this.landscapeRadioButton_,
          'click',
          this.onLayoutButtonClick_.bind(this));
      this.tracker.add(
          this.landscapeTicketItem_,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onLandscapeTicketItemChange_.bind(this));
    },

    /**
     * @return {HTMLInputElement} The portrait orientation radio button.
     * @private
     */
    get portraitRadioButton_() {
      return this.getElement().getElementsByClassName(
          LayoutSettings.Classes_.PORTRAIT_RADIO)[0];
    },

    /**
     * @return {HTMLInputElement} The landscape orientation radio button.
     * @private
     */
    get landscapeRadioButton_() {
      return this.getElement().getElementsByClassName(
          LayoutSettings.Classes_.LANDSCAPE_RADIO)[0];
    },

    /**
     * Called when one of the radio buttons is clicked. Updates the print ticket
     * store.
     * @private
     */
    onLayoutButtonClick_: function() {
      this.landscapeTicketItem_.updateValue(this.landscapeRadioButton_.checked);
    },

    /**
     * Called when the print ticket store changes state. Updates the state of
     * the radio buttons and hides the setting if necessary.
     * @private
     */
    onLandscapeTicketItemChange_: function() {
      if (this.landscapeTicketItem_.isCapabilityAvailable()) {
        var isLandscapeEnabled = this.landscapeTicketItem_.getValue();
        this.portraitRadioButton_.checked = !isLandscapeEnabled;
        this.landscapeRadioButton_.checked = isLandscapeEnabled;
        fadeInOption(this.getElement());
      } else {
        fadeOutOption(this.getElement());
      }
    }
  };

  // Export
  return {
    LayoutSettings: LayoutSettings
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Creates a ColorSettings object. This object encapsulates all settings and
   * logic related to color selection (color/bw).
   * @param {!print_preview.ticket_item.Color} colorTicketItem Used for writing
   *     and reading color value.
   * @constructor
   * @extends {print_preview.Component}
   */
  function ColorSettings(colorTicketItem) {
    print_preview.Component.call(this);

    /**
     * Used for reading/writing the color value.
     * @type {!print_preview.ticket_items.Color}
     * @private
     */
    this.colorTicketItem_ = colorTicketItem;
  };

  ColorSettings.prototype = {
    __proto__: print_preview.Component.prototype,

    set isEnabled(isEnabled) {
      this.getChildElement('.color-option').disabled = !isEnabled;
      this.getChildElement('.bw-option').disabled = !isEnabled;
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);
      this.tracker.add(
          this.getChildElement('.color-option'),
          'click',
          this.colorTicketItem_.updateValue.bind(this.colorTicketItem_, true));
      this.tracker.add(
          this.getChildElement('.bw-option'),
          'click',
          this.colorTicketItem_.updateValue.bind(this.colorTicketItem_, false));
      this.tracker.add(
          this.colorTicketItem_,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.updateState_.bind(this));
    },

    /**
     * Updates state of the widget.
     * @private
     */
    updateState_: function() {
      var isColorCapAvailable = this.colorTicketItem_.isCapabilityAvailable();
      if (isColorCapAvailable) {
        fadeInOption(this.getElement());
        var isColorEnabled = this.colorTicketItem_.getValue();
        this.getChildElement('.color-option').checked = isColorEnabled;
        this.getChildElement('.bw-option').checked = !isColorEnabled;
      } else {
        fadeOutOption(this.getElement());
      }
      this.getElement().setAttribute('aria-hidden', !isColorCapAvailable);
    }
  };

  // Export
  return {
    ColorSettings: ColorSettings
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Creates a MarginSettings object. This object encapsulates all settings and
   * logic related to the margins mode.
   * @param {!print_preview.ticket_items.MarginsType} marginsTypeTicketItem Used
   *     to read and write the margins type ticket item.
   * @constructor
   * @extends {print_preview.Component}
   */
  function MarginSettings(marginsTypeTicketItem) {
    print_preview.Component.call(this);

    /**
     * Used to read and write the margins type ticket item.
     * @type {!print_preview.ticket_items.MarginsType}
     * @private
     */
    this.marginsTypeTicketItem_ = marginsTypeTicketItem;
  };

  /**
   * CSS classes used by the margin settings component.
   * @enum {string}
   * @private
   */
  MarginSettings.Classes_ = {
    SELECT: 'margin-settings-select'
  };

  MarginSettings.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @param {boolean} isEnabled Whether this component is enabled. */
    set isEnabled(isEnabled) {
      this.select_.disabled = !isEnabled;
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);
      this.tracker.add(
          this.select_, 'change', this.onSelectChange_.bind(this));
      this.tracker.add(
          this.marginsTypeTicketItem_,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onMarginsTypeTicketItemChange_.bind(this));
    },

    /**
     * @return {HTMLSelectElement} Select element containing the margin options.
     * @private
     */
    get select_() {
      return this.getElement().getElementsByClassName(
          MarginSettings.Classes_.SELECT)[0];
    },

    /**
     * Called when the select element is changed. Updates the print ticket
     * margin type.
     * @private
     */
    onSelectChange_: function() {
      var select = this.select_;
      var marginsType =
          /** @type {!print_preview.ticket_items.MarginsType.Value} */ (
              select.selectedIndex);
      this.marginsTypeTicketItem_.updateValue(marginsType);
    },

    /**
     * Called when the print ticket store changes. Selects the corresponding
     * select option.
     * @private
     */
    onMarginsTypeTicketItemChange_: function() {
      if (this.marginsTypeTicketItem_.isCapabilityAvailable()) {
        var select = this.select_;
        var marginsType = this.marginsTypeTicketItem_.getValue();
        var selectedMarginsType =
            /** @type {!print_preview.ticket_items.MarginsType.Value} */ (
                select.selectedIndex);
        if (marginsType != selectedMarginsType) {
          select.options[selectedMarginsType].selected = false;
          select.options[marginsType].selected = true;
        }
        fadeInOption(this.getElement());
      } else {
        fadeOutOption(this.getElement());
      }
    }
  };

  // Export
  return {
    MarginSettings: MarginSettings
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  // TODO(rltoscano): This class needs a throbber while loading the destination
  // or another solution is persist the settings of the printer so that next
  // load is fast.

  /**
   * Component used to render the print destination.
   * @param {!print_preview.DestinationStore} destinationStore Used to determine
   *     the selected destination.
   * @constructor
   * @extends {print_preview.Component}
   */
  function DestinationSettings(destinationStore) {
    print_preview.Component.call(this);

    /**
     * Used to determine the selected destination.
     * @type {!print_preview.DestinationStore}
     * @private
     */
    this.destinationStore_ = destinationStore;

    /**
     * Current CSS class of the destination icon.
     * @type {?DestinationSettings.Classes_}
     * @private
     */
    this.iconClass_ = null;
  };

  /**
   * Event types dispatched by the component.
   * @enum {string}
   */
  DestinationSettings.EventType = {
    CHANGE_BUTTON_ACTIVATE:
        'print_preview.DestinationSettings.CHANGE_BUTTON_ACTIVATE'
  };

  /**
   * CSS classes used by the component.
   * @enum {string}
   * @private
   */
  DestinationSettings.Classes_ = {
    CHANGE_BUTTON: 'destination-settings-change-button',
    ICON: 'destination-settings-icon',
    ICON_CLOUD: 'destination-settings-icon-cloud',
    ICON_CLOUD_SHARED: 'destination-settings-icon-cloud-shared',
    ICON_GOOGLE_PROMOTED: 'destination-settings-icon-google-promoted',
    ICON_LOCAL: 'destination-settings-icon-local',
    ICON_MOBILE: 'destination-settings-icon-mobile',
    ICON_MOBILE_SHARED: 'destination-settings-icon-mobile-shared',
    LOCATION: 'destination-settings-location',
    NAME: 'destination-settings-name'
  };

  DestinationSettings.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @param {boolean} Whether the component is enabled. */
    set isEnabled(isEnabled) {
      var changeButton = this.getElement().getElementsByClassName(
          DestinationSettings.Classes_.CHANGE_BUTTON)[0];
      changeButton.disabled = !isEnabled;
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);
      var changeButton = this.getElement().getElementsByClassName(
          DestinationSettings.Classes_.CHANGE_BUTTON)[0];
      this.tracker.add(
          changeButton, 'click', this.onChangeButtonClick_.bind(this));
      this.tracker.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.DESTINATION_SELECT,
          this.onDestinationSelect_.bind(this));
    },

    /**
     * Called when the "Change" button is clicked. Dispatches the
     * CHANGE_BUTTON_ACTIVATE event.
     * @private
     */
    onChangeButtonClick_: function() {
      cr.dispatchSimpleEvent(
          this, DestinationSettings.EventType.CHANGE_BUTTON_ACTIVATE);
    },

    /**
     * Called when the destination selection has changed. Updates UI elements.
     * @private
     */
    onDestinationSelect_: function() {
      var destination = this.destinationStore_.selectedDestination;
      var nameEl = this.getElement().getElementsByClassName(
          DestinationSettings.Classes_.NAME)[0];
      nameEl.textContent = destination.displayName;
      nameEl.title = destination.displayName;

      var iconEl = this.getElement().getElementsByClassName(
          DestinationSettings.Classes_.ICON)[0];
      iconEl.src = destination.iconUrl;

      var locationEl = this.getElement().getElementsByClassName(
          DestinationSettings.Classes_.LOCATION)[0];
      locationEl.textContent = destination.location;
      locationEl.title = destination.location;

      setIsVisible(this.getElement().querySelector('.throbber'), false);
      setIsVisible(
          this.getElement().querySelector('.destination-settings-box'), true);
    }
  };

  // Export
  return {
    DestinationSettings: DestinationSettings
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * UI component that renders checkboxes for various print options.
   * @param {!print_preview.ticket_items.Duplex} duplex Duplex ticket item.
   * @param {!print_preview.ticket_items.FitToPage} fitToPage Fit-to-page ticket
   *     item.
   * @param {!print_preview.ticket_items.CssBackground} cssBackground CSS
   *     background ticket item.
   * @param {!print_preview.ticket_items.SelectionOnly} selectionOnly Selection
   *     only ticket item.
   * @param {!print_preview.ticket_items.HeaderFooter} headerFooter Header
   *     footer ticket item.
   * @constructor
   * @extends {print_preview.Component}
   */
  function OtherOptionsSettings(
      duplex, fitToPage, cssBackground, selectionOnly, headerFooter) {
    print_preview.Component.call(this);

    /**
     * Duplex ticket item, used to read/write the duplex selection.
     * @type {!print_preview.ticket_items.Duplex}
     * @private
     */
    this.duplexTicketItem_ = duplex;

    /**
     * Fit-to-page ticket item, used to read/write the fit-to-page selection.
     * @type {!print_preview.ticket_items.FitToPage}
     * @private
     */
    this.fitToPageTicketItem_ = fitToPage;

    /**
     * Enable CSS backgrounds ticket item, used to read/write.
     * @type {!print_preview.ticket_items.CssBackground}
     * @private
     */
    this.cssBackgroundTicketItem_ = cssBackground;

    /**
     * Print selection only ticket item, used to read/write.
     * @type {!print_preview.ticket_items.SelectionOnly}
     * @private
     */
    this.selectionOnlyTicketItem_ = selectionOnly;

    /**
     * Header-footer ticket item, used to read/write.
     * @type {!print_preview.ticket_items.HeaderFooter}
     * @private
     */
    this.headerFooterTicketItem_ = headerFooter;

    /**
     * Header footer container element.
     * @type {HTMLElement}
     * @private
     */
    this.headerFooterContainer_ = null;

    /**
     * Header footer checkbox.
     * @type {HTMLInputElement}
     * @private
     */
    this.headerFooterCheckbox_ = null;

    /**
     * Fit-to-page container element.
     * @type {HTMLElement}
     * @private
     */
    this.fitToPageContainer_ = null;

    /**
     * Fit-to-page checkbox.
     * @type {HTMLInputElement}
     * @private
     */
    this.fitToPageCheckbox_ = null;

    /**
     * Duplex container element.
     * @type {HTMLElement}
     * @private
     */
    this.duplexContainer_ = null;

    /**
     * Duplex checkbox.
     * @type {HTMLInputElement}
     * @private
     */
    this.duplexCheckbox_ = null;

    /**
     * Print CSS backgrounds container element.
     * @type {HTMLElement}
     * @private
     */
    this.cssBackgroundContainer_ = null;

    /**
     * Print CSS backgrounds checkbox.
     * @type {HTMLInputElement}
     * @private
     */
    this.cssBackgroundCheckbox_ = null;

    /**
     * Print selection only container element.
     * @type {HTMLElement}
     * @private
     */
    this.selectionOnlyContainer_ = null;

    /**
     * Print selection only checkbox.
     * @type {HTMLInputElement}
     * @private
     */
    this.selectionOnlyCheckbox_ = null;
  };

  OtherOptionsSettings.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @param {boolean} isEnabled Whether the settings is enabled. */
    set isEnabled(isEnabled) {
      this.headerFooterCheckbox_.disabled = !isEnabled;
      this.fitToPageCheckbox_.disabled = !isEnabled;
      this.duplexCheckbox_.disabled = !isEnabled;
      this.cssBackgroundCheckbox_.disabled = !isEnabled;
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);
      this.tracker.add(
          this.headerFooterCheckbox_,
          'click',
          this.onHeaderFooterCheckboxClick_.bind(this));
      this.tracker.add(
          this.fitToPageCheckbox_,
          'click',
          this.onFitToPageCheckboxClick_.bind(this));
      this.tracker.add(
          this.duplexCheckbox_,
          'click',
          this.onDuplexCheckboxClick_.bind(this));
      this.tracker.add(
          this.cssBackgroundCheckbox_,
          'click',
          this.onCssBackgroundCheckboxClick_.bind(this));
      this.tracker.add(
          this.selectionOnlyCheckbox_,
          'click',
          this.onSelectionOnlyCheckboxClick_.bind(this));
      this.tracker.add(
          this.duplexTicketItem_,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onDuplexChange_.bind(this));
      this.tracker.add(
          this.fitToPageTicketItem_,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onFitToPageChange_.bind(this));
      this.tracker.add(
          this.cssBackgroundTicketItem_,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onCssBackgroundChange_.bind(this));
      this.tracker.add(
          this.selectionOnlyTicketItem_,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onSelectionOnlyChange_.bind(this));
      this.tracker.add(
          this.headerFooterTicketItem_,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onHeaderFooterChange_.bind(this));
    },

    /** @override */
    exitDocument: function() {
      print_preview.Component.prototype.exitDocument.call(this);
      this.headerFooterContainer_ = null;
      this.headerFooterCheckbox_ = null;
      this.fitToPageContainer_ = null;
      this.fitToPageCheckbox_ = null;
      this.duplexContainer_ = null;
      this.duplexCheckbox_ = null;
      this.cssBackgroundContainer_ = null;
      this.cssBackgroundCheckbox_ = null;
      this.selectionOnlyContainer_ = null;
      this.selectionOnlyCheckbox_ = null;
    },

    /** @override */
    decorateInternal: function() {
      this.headerFooterContainer_ = this.getElement().querySelector(
          '.header-footer-container');
      this.headerFooterCheckbox_ = this.headerFooterContainer_.querySelector(
          '.header-footer-checkbox');
      this.fitToPageContainer_ = this.getElement().querySelector(
          '.fit-to-page-container');
      this.fitToPageCheckbox_ = this.fitToPageContainer_.querySelector(
          '.fit-to-page-checkbox');
      this.duplexContainer_ = this.getElement().querySelector(
          '.duplex-container');
      this.duplexCheckbox_ = this.duplexContainer_.querySelector(
          '.duplex-checkbox');
      this.cssBackgroundContainer_ = this.getElement().querySelector(
          '.css-background-container');
      this.cssBackgroundCheckbox_ = this.cssBackgroundContainer_.querySelector(
          '.css-background-checkbox');
      this.selectionOnlyContainer_ = this.getElement().querySelector(
          '.selection-only-container');
      this.selectionOnlyCheckbox_ = this.selectionOnlyContainer_.querySelector(
          '.selection-only-checkbox');
    },

    /**
     * Updates the state of the entire other options settings area.
     * @private
     */
    updateContainerState_: function() {
      if (this.headerFooterTicketItem_.isCapabilityAvailable() ||
          this.fitToPageTicketItem_.isCapabilityAvailable() ||
          this.duplexTicketItem_.isCapabilityAvailable() ||
          this.cssBackgroundTicketItem_.isCapabilityAvailable() ||
          this.selectionOnlyTicketItem_.isCapabilityAvailable()) {
        fadeInOption(this.getElement());
      } else {
        fadeOutOption(this.getElement());
      }
    },

    /**
     * Called when the header-footer checkbox is clicked. Updates the print
     * ticket.
     * @private
     */
    onHeaderFooterCheckboxClick_: function() {
      this.headerFooterTicketItem_.updateValue(
          this.headerFooterCheckbox_.checked);
    },

    /**
     * Called when the fit-to-page checkbox is clicked. Updates the print
     * ticket.
     * @private
     */
    onFitToPageCheckboxClick_: function() {
      this.fitToPageTicketItem_.updateValue(this.fitToPageCheckbox_.checked);
    },

    /**
     * Called when the duplex checkbox is clicked. Updates the print ticket.
     * @private
     */
    onDuplexCheckboxClick_: function() {
      this.duplexTicketItem_.updateValue(this.duplexCheckbox_.checked);
    },

    /**
     * Called when the print CSS backgrounds checkbox is clicked. Updates the
     * print ticket store.
     * @private
     */
    onCssBackgroundCheckboxClick_: function() {
      this.cssBackgroundTicketItem_.updateValue(
          this.cssBackgroundCheckbox_.checked);
    },

    /**
     * Called when the print selection only is clicked. Updates the
     * print ticket store.
     * @private
     */
    onSelectionOnlyCheckboxClick_: function() {
      this.selectionOnlyTicketItem_.updateValue(
          this.selectionOnlyCheckbox_.checked);
    },

    /**
     * Called when the duplex ticket item has changed. Updates the duplex
     * checkbox.
     * @private
     */
    onDuplexChange_: function() {
      setIsVisible(this.duplexContainer_,
                   this.duplexTicketItem_.isCapabilityAvailable());
      this.duplexCheckbox_.checked = this.duplexTicketItem_.getValue();
      this.updateContainerState_();
    },

    /**
     * Called when the fit-to-page ticket item has changed. Updates the
     * fit-to-page checkbox.
     * @private
     */
    onFitToPageChange_: function() {
      setIsVisible(this.fitToPageContainer_,
                   this.fitToPageTicketItem_.isCapabilityAvailable());
      this.fitToPageCheckbox_.checked = this.fitToPageTicketItem_.getValue();
      this.updateContainerState_();
    },

    /**
     * Called when the CSS background ticket item has changed. Updates the
     * CSS background checkbox.
     * @private
     */
    onCssBackgroundChange_: function() {
      setIsVisible(this.cssBackgroundContainer_,
                   this.cssBackgroundTicketItem_.isCapabilityAvailable());
      this.cssBackgroundCheckbox_.checked =
          this.cssBackgroundTicketItem_.getValue();
      this.updateContainerState_();
    },

    /**
     * Called when the print selection only ticket item has changed. Updates the
     * CSS background checkbox.
     * @private
     */
    onSelectionOnlyChange_: function() {
      setIsVisible(this.selectionOnlyContainer_,
                   this.selectionOnlyTicketItem_.isCapabilityAvailable());
      this.selectionOnlyCheckbox_.checked =
          this.selectionOnlyTicketItem_.getValue();
      this.updateContainerState_();
    },

    /**
     * Called when the header-footer ticket item has changed. Updates the
     * header-footer checkbox.
     * @private
     */
    onHeaderFooterChange_: function() {
      setIsVisible(this.headerFooterContainer_,
                   this.headerFooterTicketItem_.isCapabilityAvailable());
      this.headerFooterCheckbox_.checked =
          this.headerFooterTicketItem_.getValue();
      this.updateContainerState_();
    }
  };

  // Export
  return {
    OtherOptionsSettings: OtherOptionsSettings
  };
});


// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Draggable control for setting a page margin.
   * @param {!print_preview.ticket_items.CustomMargins.Orientation} orientation
   *     Orientation of the margin control that determines where the margin
   *     textbox will be placed.
   * @constructor
   * @extends {print_preview.Component}
   */
  function MarginControl(orientation) {
    print_preview.Component.call(this);

    /**
     * Determines where the margin textbox will be placed.
     * @type {!print_preview.ticket_items.CustomMargins.Orientation}
     * @private
     */
    this.orientation_ = orientation;

    /**
     * Position of the margin control in points.
     * @type {number}
     * @private
     */
    this.positionInPts_ = 0;

    /**
     * Page size of the document to print.
     * @type {!print_preview.Size}
     * @private
     */
    this.pageSize_ = new print_preview.Size(0, 0);

    /**
     * Amount to scale pixel values by to convert to pixel space.
     * @type {number}
     * @private
     */
    this.scaleTransform_ = 1;

    /**
     * Amount to translate values in pixel space.
     * @type {!print_preview.Coordinate2d}
     * @private
     */
    this.translateTransform_ = new print_preview.Coordinate2d(0, 0);

    /**
     * Position of the margin control when dragging starts.
     * @type {print_preview.Coordinate2d}
     * @private
     */
    this.marginStartPositionInPixels_ = null;

    /**
     * Position of the mouse when the dragging starts.
     * @type {print_preview.Coordinate2d}
     * @private
     */
    this.mouseStartPositionInPixels_ = null;

    /**
     * Processing timeout for the textbox.
     * @type {?number}
     * @private
     */
    this.textTimeout_ = null;

    /**
     * Value of the textbox when the timeout was started.
     * @type {?string}
     * @private
     */
    this.preTimeoutValue_ = null;

    /**
     * Textbox used to display and receive the value of the margin.
     * @type {HTMLInputElement}
     * @private
     */
    this.textbox_ = null;

    /**
     * Element of the margin control line.
     * @type {HTMLElement}
     * @private
     */
    this.marginLineEl_ = null;

    /**
     * Whether this margin control's textbox has keyboard focus.
     * @type {boolean}
     * @private
     */
    this.isFocused_ = false;

    /**
     * Whether the margin control is in an error state.
     * @type {boolean}
     * @private
     */
    this.isInError_ = false;
  };

  /**
   * Event types dispatched by the margin control.
   * @enum {string}
   */
  MarginControl.EventType = {
    // Dispatched when the margin control starts dragging.
    DRAG_START: 'print_preview.MarginControl.DRAG_START',

    // Dispatched when the text in the margin control's textbox changes.
    TEXT_CHANGE: 'print_preview.MarginControl.TEXT_CHANGE'
  };

  /**
   * CSS classes used by this component.
   * @enum {string}
   * @private
   */
  MarginControl.Classes_ = {
    TOP: 'margin-control-top',
    RIGHT: 'margin-control-right',
    BOTTOM: 'margin-control-bottom',
    LEFT: 'margin-control-left',
    TEXTBOX: 'margin-control-textbox',
    INVALID: 'invalid',
    INVISIBLE: 'invisible',
    DISABLED: 'margin-control-disabled',
    DRAGGING: 'margin-control-dragging',
    LINE: 'margin-control-line'
  };

  /**
   * Map from orientation to CSS class name.
   * @type {!Object.<
   *     !print_preview.ticket_items.CustomMargins.Orientation,
   *     !MarginControl.Classes_>}
   * @private
   */
  MarginControl.OrientationToClass_ = {};
  MarginControl.OrientationToClass_[
      print_preview.ticket_items.CustomMargins.Orientation.TOP] =
      MarginControl.Classes_.TOP;
  MarginControl.OrientationToClass_[
      print_preview.ticket_items.CustomMargins.Orientation.RIGHT] =
      MarginControl.Classes_.RIGHT;
  MarginControl.OrientationToClass_[
      print_preview.ticket_items.CustomMargins.Orientation.BOTTOM] =
      MarginControl.Classes_.BOTTOM;
  MarginControl.OrientationToClass_[
      print_preview.ticket_items.CustomMargins.Orientation.LEFT] =
      MarginControl.Classes_.LEFT;

  /**
   * Radius of the margin control in pixels. Padding of control + 1 for border.
   * @type {number}
   * @const
   * @private
   */
  MarginControl.RADIUS_ = 9;

  /**
   * Timeout after a text change after which the text in the textbox is saved to
   * the print ticket. Value in milliseconds.
   * @type {number}
   * @const
   * @private
   */
  MarginControl.TEXTBOX_TIMEOUT_ = 1000;

  MarginControl.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @return {boolean} Whether this margin control is in focus. */
    getIsFocused: function() {
      return this.isFocused_;
    },

    /**
     * @return {!print_preview.ticket_items.CustomMargins.Orientation}
     *     Orientation of the margin control.
     */
    getOrientation: function() {
      return this.orientation_;
    },

    /**
     * @param {number} scaleTransform New scale transform of the margin control.
     */
    setScaleTransform: function(scaleTransform) {
      this.scaleTransform_ = scaleTransform;
      // Reset position
      this.setPositionInPts(this.positionInPts_);
    },

    /**
     * @param {!print_preview.Coordinate2d} translateTransform New translate
     *     transform of the margin control.
     */
    setTranslateTransform: function(translateTransform) {
      this.translateTransform_ = translateTransform;
      // Reset position
      this.setPositionInPts(this.positionInPts_);
    },

    /**
     * @param {!print_preview.Size} pageSize New size of the document's pages.
     */
    setPageSize: function(pageSize) {
      this.pageSize_ = pageSize;
      this.setPositionInPts(this.positionInPts_);
    },

    /** @param {boolean} isVisible Whether the margin control is visible. */
    setIsVisible: function(isVisible) {
      if (isVisible) {
        this.getElement().classList.remove(MarginControl.Classes_.INVISIBLE);
      } else {
        this.getElement().classList.add(MarginControl.Classes_.INVISIBLE);
      }
    },

    /** @return {boolean} Whether the margin control is in an error state. */
    getIsInError: function() {
      return this.isInError_;
    },

    /**
     * @param {boolean} isInError Whether the margin control is in an error
     *     state.
     */
    setIsInError: function(isInError) {
      this.isInError_ = isInError;
      if (isInError) {
        this.textbox_.classList.add(MarginControl.Classes_.INVALID);
      } else {
        this.textbox_.classList.remove(MarginControl.Classes_.INVALID);
      }
    },

    /** @param {boolean} isEnabled Whether to enable the margin control. */
    setIsEnabled: function(isEnabled) {
      this.textbox_.disabled = !isEnabled;
      if (isEnabled) {
        this.getElement().classList.remove(MarginControl.Classes_.DISABLED);
      } else {
        this.getElement().classList.add(MarginControl.Classes_.DISABLED);
      }
    },

    /** @return {number} Current position of the margin control in points. */
    getPositionInPts: function() {
      return this.positionInPts_;
    },

    /**
     * @param {number} posInPts New position of the margin control in points.
     */
    setPositionInPts: function(posInPts) {
      this.positionInPts_ = posInPts;
      var orientationEnum =
          print_preview.ticket_items.CustomMargins.Orientation;
      var x = this.translateTransform_.x;
      var y = this.translateTransform_.y;
      var width = null, height = null;
      if (this.orientation_ == orientationEnum.TOP) {
        y = this.scaleTransform_ * posInPts + this.translateTransform_.y -
            MarginControl.RADIUS_;
        width = this.scaleTransform_ * this.pageSize_.width;
      } else if (this.orientation_ == orientationEnum.RIGHT) {
        x = this.scaleTransform_ * (this.pageSize_.width - posInPts) +
            this.translateTransform_.x - MarginControl.RADIUS_;
        height = this.scaleTransform_ * this.pageSize_.height;
      } else if (this.orientation_ == orientationEnum.BOTTOM) {
        y = this.scaleTransform_ * (this.pageSize_.height - posInPts) +
            this.translateTransform_.y - MarginControl.RADIUS_;
        width = this.scaleTransform_ * this.pageSize_.width;
      } else {
        x = this.scaleTransform_ * posInPts + this.translateTransform_.x -
            MarginControl.RADIUS_;
        height = this.scaleTransform_ * this.pageSize_.height;
      }
      this.getElement().style.left = Math.round(x) + 'px';
      this.getElement().style.top = Math.round(y) + 'px';
      if (width != null) {
        this.getElement().style.width = Math.round(width) + 'px';
      }
      if (height != null) {
        this.getElement().style.height = Math.round(height) + 'px';
      }
    },

    /** @return {string} The value in the margin control's textbox. */
    getTextboxValue: function() {
      return this.textbox_.value;
    },

    /** @param {string} value New value of the margin control's textbox. */
    setTextboxValue: function(value) {
      if (this.textbox_.value != value) {
        this.textbox_.value = value;
      }
    },

    /**
     * Converts a value in pixels to points.
     * @param {number} Pixel value to convert.
     * @return {number} Given value expressed in points.
     */
    convertPixelsToPts: function(pixels) {
      var pts;
      var orientationEnum =
          print_preview.ticket_items.CustomMargins.Orientation;
      if (this.orientation_ == orientationEnum.TOP) {
        pts = pixels - this.translateTransform_.y + MarginControl.RADIUS_;
        pts /= this.scaleTransform_;
      } else if (this.orientation_ == orientationEnum.RIGHT) {
        pts = pixels - this.translateTransform_.x + MarginControl.RADIUS_;
        pts /= this.scaleTransform_;
        pts = this.pageSize_.width - pts;
      } else if (this.orientation_ == orientationEnum.BOTTOM) {
        pts = pixels - this.translateTransform_.y + MarginControl.RADIUS_;
        pts /= this.scaleTransform_;
        pts = this.pageSize_.height - pts;
      } else {
        pts = pixels - this.translateTransform_.x + MarginControl.RADIUS_;
        pts /= this.scaleTransform_;
      }
      return pts;
    },

    /**
     * Translates the position of the margin control relative to the mouse
     * position in pixels.
     * @param {!print_preview.Coordinate2d} mousePosition New position of
     *     the mouse.
     * @return {!print_preview.Coordinate2d} New position of the margin control.
     */
    translateMouseToPositionInPixels: function(mousePosition) {
      return new print_preview.Coordinate2d(
          mousePosition.x - this.mouseStartPositionInPixels_.x +
              this.marginStartPositionInPixels_.x,
          mousePosition.y - this.mouseStartPositionInPixels_.y +
              this.marginStartPositionInPixels_.y);
    },

    /** @override */
    createDom: function() {
      this.setElementInternal(this.cloneTemplateInternal(
          'margin-control-template'));
      this.getElement().classList.add(MarginControl.OrientationToClass_[
          this.orientation_]);
      this.textbox_ = this.getElement().getElementsByClassName(
          MarginControl.Classes_.TEXTBOX)[0];
      this.marginLineEl_ = this.getElement().getElementsByClassName(
          MarginControl.Classes_.LINE)[0];
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);
      this.tracker.add(
          this.getElement(), 'mousedown', this.onMouseDown_.bind(this));
      this.tracker.add(
          this.textbox_, 'keydown', this.onTextboxKeyDown_.bind(this));
      this.tracker.add(
          this.textbox_, 'focus', this.setIsFocused_.bind(this, true));
      this.tracker.add(this.textbox_, 'blur', this.onTexboxBlur_.bind(this));
    },

    /** @override */
    exitDocument: function() {
      print_preview.Component.prototype.exitDocument.call(this);
      this.textbox_ = null;
      this.marginLineEl_ = null;
    },

    /**
     * @param {boolean} isFocused Whether the margin control is in focus.
     * @private
     */
    setIsFocused_: function(isFocused) {
      this.isFocused_ = isFocused;
    },

    /**
     * Called whenever a mousedown event occurs on the component.
     * @param {MouseEvent} event The event that occured.
     * @private
     */
    onMouseDown_: function(event) {
      if (!this.textbox_.disabled &&
          event.button == 0 &&
          (event.target == this.getElement() ||
              event.target == this.marginLineEl_)) {
        this.mouseStartPositionInPixels_ =
            new print_preview.Coordinate2d(event.x, event.y);
        this.marginStartPositionInPixels_ = new print_preview.Coordinate2d(
            this.getElement().offsetLeft, this.getElement().offsetTop);
        this.setIsInError(false);
        cr.dispatchSimpleEvent(this, MarginControl.EventType.DRAG_START);
      }
    },

    /**
     * Called when a key down event occurs on the textbox. Dispatches a
     * TEXT_CHANGE event if the "Enter" key was pressed.
     * @param {Event} event Contains the key that was pressed.
     * @private
     */
    onTextboxKeyDown_: function(event) {
      if (this.textTimeout_) {
        clearTimeout(this.textTimeout_);
        this.textTimeout_ = null;
      }
      if (event.keyIdentifier == 'Enter') {
        this.preTimeoutValue_ = null;
        cr.dispatchSimpleEvent(this, MarginControl.EventType.TEXT_CHANGE);
      } else {
        if (this.preTimeoutValue_ == null) {
          this.preTimeoutValue_ = this.textbox_.value;
        }
        this.textTimeout_ = setTimeout(
            this.onTextboxTimeout_.bind(this), MarginControl.TEXTBOX_TIMEOUT_);
      }
    },

    /**
     * Called after a timeout after the text in the textbox has changed. Saves
     * the textbox's value to the print ticket.
     * @private
     */
    onTextboxTimeout_: function() {
      this.textTimeout_ = null;
      if (this.textbox_.value != this.preTimeoutValue_) {
        cr.dispatchSimpleEvent(this, MarginControl.EventType.TEXT_CHANGE);
      }
      this.preTimeoutValue_ = null;
    },

    /**
     * Called when the textbox loses focus. Dispatches a TEXT_CHANGE event.
     */
    onTexboxBlur_: function() {
      if (this.textTimeout_) {
        clearTimeout(this.textTimeout_);
        this.textTimeout_ = null;
        this.preTimeoutValue_ = null;
      }
      this.setIsFocused_(false);
      cr.dispatchSimpleEvent(this, MarginControl.EventType.TEXT_CHANGE);
    }
  };

  // Export
  return {
    MarginControl: MarginControl
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * UI component used for setting custom print margins.
   * @param {!print_preview.DocumentInfo} documentInfo Document data model.
   * @param {!print_preview.ticket_items.MarginsType} marginsTypeTicketItem
   *     Used to read margins type.
   * @param {!print_preview.ticket_items.CustomMargins} customMarginsTicketItem
   *     Used to read and write custom margin values.
   * @param {!print_preview.MeasurementSystem} measurementSystem Used to convert
   *     between the system's local units and points.
   * @constructor
   * @extends {print_preview.Component}
   */
  function MarginControlContainer(documentInfo, marginsTypeTicketItem,
                                  customMarginsTicketItem, measurementSystem) {
    print_preview.Component.call(this);

    /**
     * Document data model.
     * @type {!print_preview.DocumentInfo}
     * @private
     */
    this.documentInfo_ = documentInfo;

    /**
     * Margins type ticket item used to read predefined margins type.
     */
    this.marginsTypeTicketItem_ = marginsTypeTicketItem;

    /**
     * Custom margins ticket item used to read/write custom margin values.
     * @type {!print_preview.ticket_items.CustomMargins}
     * @private
     */
    this.customMarginsTicketItem_ = customMarginsTicketItem;

    /**
     * Used to convert between the system's local units and points.
     * @type {!print_preview.MeasurementSystem}
     * @private
     */
    this.measurementSystem_ = measurementSystem;

    /**
     * Convenience array that contains all of the margin controls.
     * @type {!Object.<
     *     !print_preview.ticket_items.CustomMargins.Orientation,
     *     !print_preview.MarginControl>}
     * @private
     */
    this.controls_ = {};
    for (var key in print_preview.ticket_items.CustomMargins.Orientation) {
      var orientation = print_preview.ticket_items.CustomMargins.Orientation[
          key];
      var control = new print_preview.MarginControl(orientation);
      this.controls_[orientation] = control;
      this.addChild(control);
    }

    /**
     * Margin control currently being dragged. Null if no control is being
     * dragged.
     * @type {print_preview.MarginControl}
     * @private
     */
    this.draggedControl_ = null;

    /**
     * Translation transformation in pixels to translate from the origin of the
     * custom margins component to the top-left corner of the most visible
     * preview page.
     * @type {!print_preview.Coordinate2d}
     * @private
     */
    this.translateTransform_ = new print_preview.Coordinate2d(0, 0);

    /**
     * Scaling transformation to scale from pixels to the units which the
     * print preview is in. The scaling factor is the same in both dimensions,
     * so this field is just a single number.
     * @type {number}
     * @private
     */
    this.scaleTransform_ = 1;

    /**
     * Clipping size for clipping the margin controls.
     * @type {print_preview.Size}
     * @private
     */
    this.clippingSize_ = null;
  };

  /**
   * CSS classes used by the custom margins component.
   * @enum {string}
   * @private
   */
  MarginControlContainer.Classes_ = {
    DRAGGING_HORIZONTAL: 'margin-control-container-dragging-horizontal',
    DRAGGING_VERTICAL: 'margin-control-container-dragging-vertical'
  };

  /**
   * @param {!print_preview.ticket_items.CustomMargins.Orientation} orientation
   *     Orientation value to test.
   * @return {boolean} Whether the given orientation is TOP or BOTTOM.
   * @private
   */
  MarginControlContainer.isTopOrBottom_ = function(orientation) {
    return orientation ==
        print_preview.ticket_items.CustomMargins.Orientation.TOP ||
        orientation ==
            print_preview.ticket_items.CustomMargins.Orientation.BOTTOM;
  };

  MarginControlContainer.prototype = {
    __proto__: print_preview.Component.prototype,

    /**
     * Updates the translation transformation that translates pixel values in
     * the space of the HTML DOM.
     * @param {print_preview.Coordinate2d} translateTransform Updated value of
     *     the translation transformation.
     */
    updateTranslationTransform: function(translateTransform) {
      if (!translateTransform.equals(this.translateTransform_)) {
        this.translateTransform_ = translateTransform;
        for (var orientation in this.controls_) {
          this.controls_[orientation].setTranslateTransform(translateTransform);
        }
      }
    },

    /**
     * Updates the scaling transform that scales pixels values to point values.
     * @param {number} scaleTransform Updated value of the scale transform.
     */
    updateScaleTransform: function(scaleTransform) {
      if (scaleTransform != this.scaleTransform_) {
        this.scaleTransform_ = scaleTransform;
        for (var orientation in this.controls_) {
          this.controls_[orientation].setScaleTransform(scaleTransform);
        }
      }
    },

    /**
     * Clips margin controls to the given clip size in pixels.
     * @param {print_preview.Size} Size to clip the margin controls to.
     */
    updateClippingMask: function(clipSize) {
      if (!clipSize) {
        return;
      }
      this.clippingSize_ = clipSize;
      for (var orientation in this.controls_) {
        var el = this.controls_[orientation].getElement();
        el.style.clip = 'rect(' +
            (-el.offsetTop) + 'px, ' +
            (clipSize.width - el.offsetLeft) + 'px, ' +
            (clipSize.height - el.offsetTop) + 'px, ' +
            (-el.offsetLeft) + 'px)';
      }
    },

    /** Shows the margin controls if the need to be shown. */
    showMarginControlsIfNeeded: function() {
      if (this.marginsTypeTicketItem_.getValue() ==
          print_preview.ticket_items.MarginsType.Value.CUSTOM) {
        this.setIsMarginControlsVisible_(true);
      }
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);

      // We want to respond to mouse up events even beyond the component's
      // element.
      this.tracker.add(window, 'mouseup', this.onMouseUp_.bind(this));
      this.tracker.add(window, 'mousemove', this.onMouseMove_.bind(this));
      this.tracker.add(
          this.getElement(), 'mouseover', this.onMouseOver_.bind(this));
      this.tracker.add(
          this.getElement(), 'mouseout', this.onMouseOut_.bind(this));

      this.tracker.add(
          this.documentInfo_,
          print_preview.DocumentInfo.EventType.CHANGE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.marginsTypeTicketItem_,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.customMarginsTicketItem_,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onTicketChange_.bind(this));

      for (var orientation in this.controls_) {
        this.tracker.add(
            this.controls_[orientation],
            print_preview.MarginControl.EventType.DRAG_START,
            this.onControlDragStart_.bind(this, this.controls_[orientation]));
        this.tracker.add(
            this.controls_[orientation],
            print_preview.MarginControl.EventType.TEXT_CHANGE,
            this.onControlTextChange_.bind(this, this.controls_[orientation]));
      }
    },

    /** @override */
    decorateInternal: function() {
      for (var orientation in this.controls_) {
        this.controls_[orientation].render(this.getElement());
      }
    },

    /**
     * @param {boolean} isVisible Whether the margin controls are visible.
     * @private
     */
    setIsMarginControlsVisible_: function(isVisible) {
      for (var orientation in this.controls_) {
        this.controls_[orientation].setIsVisible(isVisible);
      }
    },

    /**
     * Moves the position of the given control to the desired position in
     * pixels within some constraint minimum and maximum.
     * @param {!print_preview.MarginControl} control Control to move.
     * @param {!print_preview.Coordinate2d} posInPixels Desired position to move
     *     to in pixels.
     * @private
     */
    moveControlWithConstraints_: function(control, posInPixels) {
      var newPosInPts;
      if (MarginControlContainer.isTopOrBottom_(control.getOrientation())) {
        newPosInPts = control.convertPixelsToPts(posInPixels.y);
      } else {
        newPosInPts = control.convertPixelsToPts(posInPixels.x);
      }
      newPosInPts = Math.min(this.customMarginsTicketItem_.getMarginMax(
                                 control.getOrientation()),
                             newPosInPts);
      newPosInPts = Math.max(0, newPosInPts);
      newPosInPts = Math.round(newPosInPts);
      control.setPositionInPts(newPosInPts);
      control.setTextboxValue(this.serializeValueFromPts_(newPosInPts));
    },

    /**
     * @param {string} value Value to parse to points. E.g. '3.40"' or '200mm'.
     * @return {number} Value in points represented by the input value.
     * @private
     */
    parseValueToPts_: function(value) {
      // Removing whitespace anywhere in the string.
      value = value.replace(/\s*/g, '');
      if (value.length == 0) {
        return null;
      }
      var validationRegex = new RegExp('^(^-?)(\\d)+(\\' +
          this.measurementSystem_.thousandsDelimeter + '\\d{3})*(\\' +
          this.measurementSystem_.decimalDelimeter + '\\d*)?' +
          '(' + this.measurementSystem_.unitSymbol + ')?$');
      if (validationRegex.test(value)) {
        // Replacing decimal point with the dot symbol in order to use
        // parseFloat() properly.
        var replacementRegex =
            new RegExp('\\' + this.measurementSystem_.decimalDelimeter + '{1}');
        value = value.replace(replacementRegex, '.');
        return this.measurementSystem_.convertToPoints(parseFloat(value));
      }
      return null;
    },

    /**
     * @param {number} value Value in points to serialize.
     * @return {string} String representation of the value in the system's local
     *     units.
     * @private
     */
    serializeValueFromPts_: function(value) {
      value = this.measurementSystem_.convertFromPoints(value);
      value = this.measurementSystem_.roundValue(value);
      return value + this.measurementSystem_.unitSymbol;
    },

    /**
     * Called when a margin control starts to drag.
     * @param {print_preview.MarginControl} control The control which started to
     *     drag.
     * @private
     */
    onControlDragStart_: function(control) {
      this.draggedControl_ = control;
      this.getElement().classList.add(
          MarginControlContainer.isTopOrBottom_(control.getOrientation()) ?
              MarginControlContainer.Classes_.DRAGGING_VERTICAL :
              MarginControlContainer.Classes_.DRAGGING_HORIZONTAL);
    },

    /**
     * Called when the mouse moves in the custom margins component. Moves the
     * dragged margin control.
     * @param {MouseEvent} event Contains the position of the mouse.
     * @private
     */
    onMouseMove_: function(event) {
      if (this.draggedControl_) {
        this.moveControlWithConstraints_(
            this.draggedControl_,
            this.draggedControl_.translateMouseToPositionInPixels(
                new print_preview.Coordinate2d(event.x, event.y)));
        this.updateClippingMask(this.clippingSize_);
      }
    },

    /**
     * Called when the mouse is released in the custom margins component.
     * Releases the dragged margin control.
     * @param {MouseEvent} event Contains the position of the mouse.
     * @private
     */
    onMouseUp_: function(event) {
      if (this.draggedControl_) {
        this.getElement().classList.remove(
            MarginControlContainer.Classes_.DRAGGING_VERTICAL);
        this.getElement().classList.remove(
            MarginControlContainer.Classes_.DRAGGING_HORIZONTAL);
        if (event) {
          var posInPixels =
              this.draggedControl_.translateMouseToPositionInPixels(
                  new print_preview.Coordinate2d(event.x, event.y));
          this.moveControlWithConstraints_(this.draggedControl_, posInPixels);
        }
        this.updateClippingMask(this.clippingSize_);
        this.customMarginsTicketItem_.updateMargin(
            this.draggedControl_.getOrientation(),
            this.draggedControl_.getPositionInPts());
        this.draggedControl_ = null;
      }
    },

    /**
     * Called when the mouse moves onto the component. Shows the margin
     * controls.
     * @private
     */
    onMouseOver_: function() {
      var fromElement = event.fromElement;
      while (fromElement != null) {
        if (fromElement == this.getElement()) {
          return;
        }
        fromElement = fromElement.parentElement;
      }
      if (this.marginsTypeTicketItem_.isCapabilityAvailable() &&
          this.marginsTypeTicketItem_.getValue() ==
              print_preview.ticket_items.MarginsType.Value.CUSTOM) {
        this.setIsMarginControlsVisible_(true);
      }
    },

    /**
     * Called when the mouse moves off of the component. Hides the margin
     * controls.
     * @private
     */
    onMouseOut_: function(event) {
      var toElement = event.toElement;
      while (toElement != null) {
        if (toElement == this.getElement()) {
          return;
        }
        toElement = toElement.parentElement;
      }
      if (this.draggedControl_ != null) {
        return;
      }
      for (var orientation in this.controls_) {
        if (this.controls_[orientation].getIsFocused() ||
            this.controls_[orientation].getIsInError()) {
          return;
        }
      }
      this.setIsMarginControlsVisible_(false);
    },

    /**
     * Called when the print ticket changes. Updates the position of the margin
     * controls.
     * @private
     */
    onTicketChange_: function() {
      var margins = this.customMarginsTicketItem_.getValue();
      for (var orientation in this.controls_) {
        var control = this.controls_[orientation];
        control.setPageSize(this.documentInfo_.pageSize);
        control.setTextboxValue(
            this.serializeValueFromPts_(margins.get(orientation)));
        control.setPositionInPts(margins.get(orientation));
        control.setIsInError(false);
        control.setIsEnabled(true);
      }
      this.updateClippingMask(this.clippingSize_);
      if (this.marginsTypeTicketItem_.getValue() !=
          print_preview.ticket_items.MarginsType.Value.CUSTOM) {
        this.setIsMarginControlsVisible_(false);
      }
    },

    /**
     * Called when the text in a textbox of a margin control changes or the
     * textbox loses focus.
     * Updates the print ticket store.
     * @param {!print_preview.MarginControl} control Updated control.
     * @private
     */
    onControlTextChange_: function(control) {
      var marginValue = this.parseValueToPts_(control.getTextboxValue());
      if (marginValue != null) {
        this.customMarginsTicketItem_.updateMargin(
            control.getOrientation(), marginValue);
      } else {
        var enableOtherControls;
        if (!control.getIsFocused()) {
          // If control no longer in focus, revert to previous valid value.
          control.setTextboxValue(
              this.serializeValueFromPts_(control.getPositionInPts()));
          control.setIsInError(false);
          enableOtherControls = true;
        } else {
          control.setIsInError(true);
          enableOtherControls = false;
        }
        // Enable other controls.
        for (var o in this.controls_) {
          if (control.getOrientation() != o) {
            this.controls_[o].setIsEnabled(enableOtherControls);
          }
        }
      }
    }
  };

  // Export
  return {
    MarginControlContainer: MarginControlContainer
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Creates a PreviewArea object. It represents the area where the preview
   * document is displayed.
   * @param {!print_preview.DestinationStore} destinationStore Used to get the
   *     currently selected destination.
   * @param {!print_preview.PrintTicketStore} printTicketStore Used to get
   *     information about how the preview should be displayed.
   * @param {!print_preview.NativeLayer} nativeLayer Needed to communicate with
   *     Chromium's preview generation system.
   * @param {!print_preview.DocumentInfo} documentInfo Document data model.
   * @constructor
   * @extends {print_preview.Component}
   */
  function PreviewArea(
      destinationStore, printTicketStore, nativeLayer, documentInfo) {
    print_preview.Component.call(this);
    // TODO(rltoscano): Understand the dependencies of printTicketStore needed
    // here, and add only those here (not the entire print ticket store).

    /**
     * Used to get the currently selected destination.
     * @type {!print_preview.DestinationStore}
     * @private
     */
    this.destinationStore_ = destinationStore;

    /**
     * Used to get information about how the preview should be displayed.
     * @type {!print_preview.PrintTicketStore}
     * @private
     */
    this.printTicketStore_ = printTicketStore;

    /**
     * Used to contruct the preview generator.
     * @type {!print_preview.NativeLayer}
     * @private
     */
    this.nativeLayer_ = nativeLayer;

    /**
     * Document data model.
     * @type {!print_preview.DocumentInfo}
     * @private
     */
    this.documentInfo_ = documentInfo;

    /**
     * Used to read generated page previews.
     * @type {print_preview.PreviewGenerator}
     * @private
     */
    this.previewGenerator_ = null;

    /**
     * The embedded pdf plugin object. It's value is null if not yet loaded.
     * @type {HTMLEmbedElement}
     * @private
     */
    this.plugin_ = null;

    /**
     * Custom margins component superimposed on the preview plugin.
     * @type {!print_preview.MarginControlContainer}
     * @private
     */
    this.marginControlContainer_ = new print_preview.MarginControlContainer(
        this.documentInfo_,
        this.printTicketStore_.marginsType,
        this.printTicketStore_.customMargins,
        this.printTicketStore_.measurementSystem);
    this.addChild(this.marginControlContainer_);

    /**
     * Current zoom level as a percentage.
     * @type {?number}
     * @private
     */
    this.zoomLevel_ = null;

    /**
     * Current page offset which can be used to calculate scroll amount.
     * @type {print_preview.Coordinate2d}
     * @private
     */
    this.pageOffset_ = null;

    /**
     * Whether the plugin has finished reloading.
     * @type {boolean}
     * @private
     */
    this.isPluginReloaded_ = false;

    /**
     * Whether the document preview is ready.
     * @type {boolean}
     * @private
     */
    this.isDocumentReady_ = false;

    /**
     * Timeout object used to display a loading message if the preview is taking
     * a long time to generate.
     * @type {?number}
     * @private
     */
    this.loadingTimeout_ = null;

    /**
     * Overlay element.
     * @type {HTMLElement}
     * @private
     */
    this.overlayEl_ = null;

    /**
     * The "Open system dialog" button.
     * @type {HTMLButtonElement}
     * @private
     */
    this.openSystemDialogButton_ = null;
  };

  /**
   * Event types dispatched by the preview area.
   * @enum {string}
   */
  PreviewArea.EventType = {
    // Dispatched when the "Open system dialog" button is clicked.
    OPEN_SYSTEM_DIALOG_CLICK:
        'print_preview.PreviewArea.OPEN_SYSTEM_DIALOG_CLICK',

    // Dispatched when the document preview is complete.
    PREVIEW_GENERATION_DONE:
        'print_preview.PreviewArea.PREVIEW_GENERATION_DONE',

    // Dispatched when the document preview failed to be generated.
    PREVIEW_GENERATION_FAIL:
        'print_preview.PreviewArea.PREVIEW_GENERATION_FAIL',

    // Dispatched when a new document preview is being generated.
    PREVIEW_GENERATION_IN_PROGRESS:
        'print_preview.PreviewArea.PREVIEW_GENERATION_IN_PROGRESS'
  };

  /**
   * CSS classes used by the preview area.
   * @enum {string}
   * @private
   */
  PreviewArea.Classes_ = {
    COMPATIBILITY_OBJECT: 'preview-area-compatibility-object',
    CUSTOM_MESSAGE_TEXT: 'preview-area-custom-message-text',
    MESSAGE: 'preview-area-message',
    INVISIBLE: 'invisible',
    OPEN_SYSTEM_DIALOG_BUTTON: 'preview-area-open-system-dialog-button',
    OPEN_SYSTEM_DIALOG_BUTTON_THROBBER:
        'preview-area-open-system-dialog-button-throbber',
    OVERLAY: 'preview-area-overlay-layer'
  };

  /**
   * Enumeration of IDs shown in the preview area.
   * @enum {string}
   * @private
   */
  PreviewArea.MessageId_ = {
    CUSTOM: 'custom',
    LOADING: 'loading',
    PREVIEW_FAILED: 'preview-failed'
  };

  /**
   * Maps message IDs to the CSS class that contains them.
   * @type {object.<PreviewArea.MessageId_, string>}
   * @private
   */
  PreviewArea.MessageIdClassMap_ = {};
  PreviewArea.MessageIdClassMap_[PreviewArea.MessageId_.CUSTOM] =
      'preview-area-custom-message';
  PreviewArea.MessageIdClassMap_[PreviewArea.MessageId_.LOADING] =
      'preview-area-loading-message';
  PreviewArea.MessageIdClassMap_[PreviewArea.MessageId_.PREVIEW_FAILED] =
      'preview-area-preview-failed-message';

  /**
   * Amount of time in milliseconds to wait after issueing a new preview before
   * the loading message is shown.
   * @type {number}
   * @const
   * @private
   */
  PreviewArea.LOADING_TIMEOUT_ = 200;

  PreviewArea.prototype = {
    __proto__: print_preview.Component.prototype,

    /**
     * Should only be called after calling this.render().
     * @return {boolean} Whether the preview area has a compatible plugin to
     *     display the print preview in.
     */
    get hasCompatiblePlugin() {
      return this.previewGenerator_ != null;
    },

    /**
     * Processes a keyboard event that could possibly be used to change state of
     * the preview plugin.
     * @param {MouseEvent} e Mouse event to process.
     */
    handleDirectionalKeyEvent: function(e) {
      // Make sure the PDF plugin is there.
      // We only care about: PageUp, PageDown, Left, Up, Right, Down.
      // If the user is holding a modifier key, ignore.
      if (!this.plugin_ ||
          !arrayContains([33, 34, 37, 38, 39, 40], e.keyCode) ||
          e.metaKey || e.altKey || e.shiftKey || e.ctrlKey) {
        return;
      }

      // Don't handle the key event for these elements.
      var tagName = document.activeElement.tagName;
      if (arrayContains(['INPUT', 'SELECT', 'EMBED'], tagName)) {
        return;
      }

      // For the most part, if any div of header was the last clicked element,
      // then the active element is the body. Starting with the last clicked
      // element, and work up the DOM tree to see if any element has a
      // scrollbar. If there exists a scrollbar, do not handle the key event
      // here.
      var element = e.target;
      while (element) {
        if (element.scrollHeight > element.clientHeight ||
            element.scrollWidth > element.clientWidth) {
          return;
        }
        element = element.parentElement;
      }

      // No scroll bar anywhere, or the active element is something else, like a
      // button. Note: buttons have a bigger scrollHeight than clientHeight.
      this.plugin_.sendKeyEvent(e.keyCode);
      e.preventDefault();
    },

    /**
     * Shows a custom message on the preview area's overlay.
     * @param {string} message Custom message to show.
     */
    showCustomMessage: function(message) {
      this.showMessage_(PreviewArea.MessageId_.CUSTOM, message);
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);
      this.tracker.add(
          this.openSystemDialogButton_,
          'click',
          this.onOpenSystemDialogButtonClick_.bind(this));

      this.tracker.add(
          this.printTicketStore_,
          print_preview.PrintTicketStore.EventType.INITIALIZE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_,
          print_preview.PrintTicketStore.EventType.TICKET_CHANGE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_,
          print_preview.PrintTicketStore.EventType.CAPABILITIES_CHANGE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_,
          print_preview.PrintTicketStore.EventType.DOCUMENT_CHANGE,
          this.onTicketChange_.bind(this));

      this.tracker.add(
          this.printTicketStore_.color,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_.cssBackground,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
        this.printTicketStore_.customMargins,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_.fitToPage,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_.headerFooter,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_.landscape,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_.marginsType,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_.pageRange,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onTicketChange_.bind(this));
      this.tracker.add(
          this.printTicketStore_.selectionOnly,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onTicketChange_.bind(this));

      if (this.checkPluginCompatibility_()) {
        this.previewGenerator_ = new print_preview.PreviewGenerator(
            this.destinationStore_,
            this.printTicketStore_,
            this.nativeLayer_,
            this.documentInfo_);
        this.tracker.add(
            this.previewGenerator_,
            print_preview.PreviewGenerator.EventType.PREVIEW_START,
            this.onPreviewStart_.bind(this));
        this.tracker.add(
            this.previewGenerator_,
            print_preview.PreviewGenerator.EventType.PAGE_READY,
            this.onPagePreviewReady_.bind(this));
        this.tracker.add(
            this.previewGenerator_,
            print_preview.PreviewGenerator.EventType.FAIL,
            this.onPreviewGenerationFail_.bind(this));
        this.tracker.add(
            this.previewGenerator_,
            print_preview.PreviewGenerator.EventType.DOCUMENT_READY,
            this.onDocumentReady_.bind(this));
      } else {
        this.showCustomMessage(localStrings.getString('noPlugin'));
      }
    },

    /** @override */
    exitDocument: function() {
      print_preview.Component.prototype.exitDocument.call(this);
      if (this.previewGenerator_) {
        this.previewGenerator_.removeEventListeners();
      }
      this.overlayEl_ = null;
      this.openSystemDialogButton_ = null;
    },

    /** @override */
    decorateInternal: function() {
      this.marginControlContainer_.decorate(this.getElement());
      this.overlayEl_ = this.getElement().getElementsByClassName(
          PreviewArea.Classes_.OVERLAY)[0];
      this.openSystemDialogButton_ = this.getElement().getElementsByClassName(
          PreviewArea.Classes_.OPEN_SYSTEM_DIALOG_BUTTON)[0];
    },

    /**
     * Checks to see if a suitable plugin for rendering the preview exists. If
     * one does not exist, then an error message will be displayed.
     * @return {boolean} Whether Chromium has a suitable plugin for rendering
     *     the preview.
     * @private
     */
    checkPluginCompatibility_: function() {
      var compatObj = this.getElement().getElementsByClassName(
          PreviewArea.Classes_.COMPATIBILITY_OBJECT)[0];
      var isCompatible =
          compatObj.onload &&
          compatObj.goToPage &&
          compatObj.removePrintButton &&
          compatObj.loadPreviewPage &&
          compatObj.printPreviewPageCount &&
          compatObj.resetPrintPreviewUrl &&
          compatObj.onPluginSizeChanged &&
          compatObj.onScroll &&
          compatObj.pageXOffset &&
          compatObj.pageYOffset &&
          compatObj.setZoomLevel &&
          compatObj.setPageNumbers &&
          compatObj.setPageXOffset &&
          compatObj.setPageYOffset &&
          compatObj.getHorizontalScrollbarThickness &&
          compatObj.getVerticalScrollbarThickness &&
          compatObj.getPageLocationNormalized &&
          compatObj.getHeight &&
          compatObj.getWidth;
      compatObj.parentElement.removeChild(compatObj);
      return isCompatible;
    },

    /**
     * Shows a given message on the overlay.
     * @param {!print_preview.PreviewArea.MessageId_} messageId ID of the
     *     message to show.
     * @param {string=} opt_message Optional message to show that can be used
     *     by some message IDs.
     * @private
     */
    showMessage_: function(messageId, opt_message) {
      // Hide all messages.
      var messageEls = this.getElement().getElementsByClassName(
          PreviewArea.Classes_.MESSAGE);
      for (var i = 0, messageEl; messageEl = messageEls[i]; i++) {
        setIsVisible(messageEl, false);
      }
      // Disable jumping animation to conserve cycles.
      var jumpingDotsEl = this.getElement().querySelector(
          '.preview-area-loading-message-jumping-dots');
      jumpingDotsEl.classList.remove('jumping-dots');

      // Show specific message.
      if (messageId == PreviewArea.MessageId_.CUSTOM) {
        var customMessageTextEl = this.getElement().getElementsByClassName(
            PreviewArea.Classes_.CUSTOM_MESSAGE_TEXT)[0];
        customMessageTextEl.textContent = opt_message;
      } else if (messageId == PreviewArea.MessageId_.LOADING) {
        jumpingDotsEl.classList.add('jumping-dots');
      }
      var messageEl = this.getElement().getElementsByClassName(
            PreviewArea.MessageIdClassMap_[messageId])[0];
      setIsVisible(messageEl, true);

      // Show overlay.
      this.overlayEl_.classList.remove(PreviewArea.Classes_.INVISIBLE);
    },

    /**
     * Hides the message overlay.
     * @private
     */
    hideOverlay_: function() {
      this.overlayEl_.classList.add(PreviewArea.Classes_.INVISIBLE);
      // Disable jumping animation to conserve cycles.
      var jumpingDotsEl = this.getElement().querySelector(
          '.preview-area-loading-message-jumping-dots');
      jumpingDotsEl.classList.remove('jumping-dots');
    },

    /**
     * Creates a preview plugin and adds it to the DOM.
     * @param {string} srcUrl Initial URL of the plugin.
     * @private
     */
    createPlugin_: function(srcUrl) {
      if (this.plugin_) {
        console.warn('Pdf preview plugin already created');
        return;
      }
      this.plugin_ = document.createElement('embed');
      // NOTE: The plugin's 'id' field must be set to 'pdf-viewer' since
      // chrome/renderer/printing/print_web_view_helper.cc actually references
      // it.
      this.plugin_.setAttribute('id', 'pdf-viewer');
      this.plugin_.setAttribute('class', 'preview-area-plugin');
      this.plugin_.setAttribute(
          'type', 'application/x-google-chrome-print-preview-pdf');
      this.plugin_.setAttribute('src', srcUrl);
      this.plugin_.setAttribute('aria-live', 'polite');
      this.plugin_.setAttribute('aria-atomic', 'true');
      this.getChildElement('.preview-area-plugin-wrapper').
          appendChild(this.plugin_);

      global['onPreviewPluginLoad'] = this.onPluginLoad_.bind(this);
      this.plugin_.onload('onPreviewPluginLoad()');

      global['onPreviewPluginVisualStateChange'] =
          this.onPreviewVisualStateChange_.bind(this);
      this.plugin_.onScroll('onPreviewPluginVisualStateChange()');
      this.plugin_.onPluginSizeChanged('onPreviewPluginVisualStateChange()');

      this.plugin_.removePrintButton();
      this.plugin_.grayscale(!this.printTicketStore_.color.getValue());
    },

    /**
     * Dispatches a PREVIEW_GENERATION_DONE event if all conditions are met.
     * @private
     */
    dispatchPreviewGenerationDoneIfReady_: function() {
      if (this.isDocumentReady_ && this.isPluginReloaded_) {
        cr.dispatchSimpleEvent(
            this, PreviewArea.EventType.PREVIEW_GENERATION_DONE);
        this.marginControlContainer_.showMarginControlsIfNeeded();
      }
    },

    /**
     * Called when the open-system-dialog button is clicked. Disables the
     * button, shows the throbber, and dispatches the OPEN_SYSTEM_DIALOG_CLICK
     * event.
     * @private
     */
    onOpenSystemDialogButtonClick_: function() {
      this.openSystemDialogButton_.disabled = true;
      var openSystemDialogThrobber = this.getElement().getElementsByClassName(
          PreviewArea.Classes_.OPEN_SYSTEM_DIALOG_BUTTON_THROBBER)[0];
      setIsVisible(openSystemDialogThrobber, true);
      cr.dispatchSimpleEvent(
          this, PreviewArea.EventType.OPEN_SYSTEM_DIALOG_CLICK);
    },

    /**
     * Called when the print ticket changes. Updates the preview.
     * @private
     */
    onTicketChange_: function() {
      if (this.previewGenerator_ && this.previewGenerator_.requestPreview()) {
        if (this.loadingTimeout_ == null) {
          this.loadingTimeout_ = setTimeout(
              this.showMessage_.bind(this, PreviewArea.MessageId_.LOADING),
              PreviewArea.LOADING_TIMEOUT_);
        }
      } else {
        this.marginControlContainer_.showMarginControlsIfNeeded();
      }
    },

    /**
     * Called when the preview generator begins loading the preview.
     * @param {Event} Contains the URL to initialize the plugin to.
     * @private
     */
    onPreviewStart_: function(event) {
      this.isDocumentReady_ = false;
      this.isPluginReloaded_ = false;
      if (!this.plugin_) {
        this.createPlugin_(event.previewUrl);
      }
      this.plugin_.goToPage('0');
      this.plugin_.resetPrintPreviewUrl(event.previewUrl);
      this.plugin_.reload();
      this.plugin_.grayscale(!this.printTicketStore_.color.getValue());
      cr.dispatchSimpleEvent(
          this, PreviewArea.EventType.PREVIEW_GENERATION_IN_PROGRESS);
    },

    /**
     * Called when a page preview has been generated. Updates the plugin with
     * the new page.
     * @param {Event} event Contains information about the page preview.
     * @private
     */
    onPagePreviewReady_: function(event) {
      this.plugin_.loadPreviewPage(event.previewUrl, event.previewIndex);
    },

    /**
     * Called when the preview generation is complete and the document is ready
     * to print.
     * @private
     */
    onDocumentReady_: function(event) {
      this.isDocumentReady_ = true;
      this.dispatchPreviewGenerationDoneIfReady_();
    },

    /**
     * Called when the generation of a preview fails. Shows an error message.
     * @private
     */
    onPreviewGenerationFail_: function() {
      if (this.loadingTimeout_) {
        clearTimeout(this.loadingTimeout_);
        this.loadingTimeout_ = null;
      }
      this.showMessage_(PreviewArea.MessageId_.PREVIEW_FAILED);
      cr.dispatchSimpleEvent(
          this, PreviewArea.EventType.PREVIEW_GENERATION_FAIL);
    },

    /**
     * Called when the plugin loads. This is a consequence of calling
     * plugin.reload(). Certain plugin state can only be set after the plugin
     * has loaded.
     * @private
     */
    onPluginLoad_: function() {
      if (this.loadingTimeout_) {
        clearTimeout(this.loadingTimeout_);
        this.loadingTimeout_ = null;
      }
      // Setting the plugin's page count can only be called after the plugin is
      // loaded and the document must be modifiable.
      if (this.documentInfo_.isModifiable) {
        this.plugin_.printPreviewPageCount(
            this.printTicketStore_.pageRange.getPageNumberSet().size);
      }
      this.plugin_.setPageNumbers(JSON.stringify(
          this.printTicketStore_.pageRange.getPageNumberSet().asArray()));
      if (this.zoomLevel_ != null && this.pageOffset_ != null) {
        this.plugin_.setZoomLevel(this.zoomLevel_);
        this.plugin_.setPageXOffset(this.pageOffset_.x);
        this.plugin_.setPageYOffset(this.pageOffset_.y);
      } else {
        this.plugin_.fitToHeight();
      }
      this.hideOverlay_();
      this.isPluginReloaded_ = true;
      this.dispatchPreviewGenerationDoneIfReady_();
    },

    /**
     * Called when the preview plugin's visual state has changed. This is a
     * consequence of scrolling or zooming the plugin. Updates the custom
     * margins component if shown.
     * @private
     */
    onPreviewVisualStateChange_: function() {
      if (this.isPluginReloaded_) {
        this.zoomLevel_ = this.plugin_.getZoomLevel();
        this.pageOffset_ = new print_preview.Coordinate2d(
            this.plugin_.pageXOffset(), this.plugin_.pageYOffset());
      }
      var pageLocationNormalizedStr = this.plugin_.getPageLocationNormalized();
      if (!pageLocationNormalizedStr) {
        return;
      }
      var normalized = pageLocationNormalizedStr.split(';');
      var pluginWidth = this.plugin_.getWidth();
      var pluginHeight = this.plugin_.getHeight();
      var translationTransform = new print_preview.Coordinate2d(
          parseFloat(normalized[0]) * pluginWidth,
          parseFloat(normalized[1]) * pluginHeight);
      this.marginControlContainer_.updateTranslationTransform(
          translationTransform);
      var pageWidthInPixels = parseFloat(normalized[2]) * pluginWidth;
      this.marginControlContainer_.updateScaleTransform(
          pageWidthInPixels / this.documentInfo_.pageSize.width);
      this.marginControlContainer_.updateClippingMask(
          new print_preview.Size(
              pluginWidth - this.plugin_.getVerticalScrollbarThickness(),
              pluginHeight - this.plugin_.getHorizontalScrollbarThickness()));
    }
  };

  // Export
  return {
    PreviewArea: PreviewArea
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Interface to the Chromium print preview generator.
   * @param {!print_preview.DestinationStore} destinationStore Used to get the
   *     currently selected destination.
   * @param {!print_preview.PrintTicketStore} printTicketStore Used to read the
   *     state of the ticket and write document information.
   * @param {!print_preview.NativeLayer} nativeLayer Used to communicate to
   *     Chromium's preview rendering system.
   * @param {!print_preview.DocumentInfo} documentInfo Document data model.
   * @constructor
   * @extends {cr.EventTarget}
   */
  function PreviewGenerator(
       destinationStore, printTicketStore, nativeLayer, documentInfo) {
    cr.EventTarget.call(this);

    /**
     * Used to get the currently selected destination.
     * @type {!print_preview.DestinationStore}
     * @private
     */
    this.destinationStore_ = destinationStore;

    /**
     * Used to read the state of the ticket and write document information.
     * @type {!print_preview.PrintTicketStore}
     * @private
     */
    this.printTicketStore_ = printTicketStore;

    /**
     * Interface to the Chromium native layer.
     * @type {!print_preview.NativeLayer}
     * @private
     */
    this.nativeLayer_ = nativeLayer;

    /**
     * Document data model.
     * @type {!print_preview.DocumentInfo}
     * @private
     */
    this.documentInfo_ = documentInfo;

    /**
     * ID of current in-flight request. Requests that do not share this ID will
     * be ignored.
     * @type {number}
     * @private
     */
    this.inFlightRequestId_ = -1;

    /**
     * Whether the previews are being generated in landscape mode.
     * @type {boolean}
     * @private
     */
    this.isLandscapeEnabled_ = false;

    /**
     * Whether the previews are being generated with a header and footer.
     * @type {boolean}
     * @private
     */
    this.isHeaderFooterEnabled_ = false;

    /**
     * Whether the previews are being generated in color.
     * @type {boolean}
     * @private
     */
    this.colorValue_ = false;

    /**
     * Whether the document should be fitted to the page.
     * @type {boolean}
     * @private
     */
    this.isFitToPageEnabled_ = false;

    /**
     * Page ranges setting used used to generate the last preview.
     * @type {!Array.<object.<{from: number, to: number}>>}
     * @private
     */
    this.pageRanges_ = null;

    /**
     * Margins type used to generate the last preview.
     * @type {!print_preview.ticket_items.MarginsType.Value}
     * @private
     */
    this.marginsType_ = print_preview.ticket_items.MarginsType.Value.DEFAULT;

    /**
     * Whether the document should have element CSS backgrounds printed.
     * @type {boolean}
     * @private
     */
    this.isCssBackgroundEnabled_ = false;

    /**
     * Destination that was selected for the last preview.
     * @type {print_preview.Destination}
     * @private
     */
    this.selectedDestination_ = null;

    /**
     * Event tracker used to keep track of native layer events.
     * @type {!EventTracker}
     * @private
     */
    this.tracker_ = new EventTracker();

    this.addEventListeners_();
  };

  /**
   * Event types dispatched by the preview generator.
   * @enum {string}
   */
  PreviewGenerator.EventType = {
    // Dispatched when the document can be printed.
    DOCUMENT_READY: 'print_preview.PreviewGenerator.DOCUMENT_READY',

    // Dispatched when a page preview is ready. The previewIndex field of the
    // event is the index of the page in the modified document, not the
    // original. So page 4 of the original document might be previewIndex = 0 of
    // the modified document.
    PAGE_READY: 'print_preview.PreviewGenerator.PAGE_READY',

    // Dispatched when the document preview starts to be generated.
    PREVIEW_START: 'print_preview.PreviewGenerator.PREVIEW_START',

    // Dispatched when the current print preview request fails.
    FAIL: 'print_preview.PreviewGenerator.FAIL'
  };

  PreviewGenerator.prototype = {
    __proto__: cr.EventTarget.prototype,

    /**
     * Request that new preview be generated. A preview request will not be
     * generated if the print ticket has not changed sufficiently.
     * @return {boolean} Whether a new preview was actually requested.
     */
    requestPreview: function() {
      if (!this.printTicketStore_.isTicketValidForPreview() ||
          !this.printTicketStore_.isInitialized) {
        return false;
      }
      if (!this.hasPreviewChanged_()) {
        // Changes to these ticket items might not trigger a new preview, but
        // they still need to be recorded.
        this.marginsType_ = this.printTicketStore_.marginsType.getValue();
        return false;
      }
      this.isLandscapeEnabled_ = this.printTicketStore_.landscape.getValue();
      this.isHeaderFooterEnabled_ =
          this.printTicketStore_.headerFooter.getValue();
      this.colorValue_ = this.printTicketStore_.color.getValue();
      this.isFitToPageEnabled_ = this.printTicketStore_.fitToPage.getValue();
      this.pageRanges_ = this.printTicketStore_.pageRange.getPageRanges();
      this.marginsType_ = this.printTicketStore_.marginsType.getValue();
      this.isCssBackgroundEnabled_ =
          this.printTicketStore_.cssBackground.getValue();
      this.isSelectionOnlyEnabled_ =
          this.printTicketStore_.selectionOnly.getValue();
      this.selectedDestination_ = this.destinationStore_.selectedDestination;

      this.inFlightRequestId_++;
      this.nativeLayer_.startGetPreview(
          this.destinationStore_.selectedDestination,
          this.printTicketStore_,
          this.documentInfo_,
          this.inFlightRequestId_);
      return true;
    },

    /** Removes all event listeners that the preview generator has attached. */
    removeEventListeners: function() {
      this.tracker_.removeAll();
    },

    /**
     * Adds event listeners to the relevant native layer events.
     * @private
     */
    addEventListeners_: function() {
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.PAGE_LAYOUT_READY,
          this.onPageLayoutReady_.bind(this));
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.PAGE_COUNT_READY,
          this.onPageCountReady_.bind(this));
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.PREVIEW_RELOAD,
          this.onPreviewReload_.bind(this));
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.PAGE_PREVIEW_READY,
          this.onPagePreviewReady_.bind(this));
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.PREVIEW_GENERATION_DONE,
          this.onPreviewGenerationDone_.bind(this));
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.PREVIEW_GENERATION_FAIL,
          this.onPreviewGenerationFail_.bind(this));
    },

    /**
     * Dispatches a PAGE_READY event to signal that a page preview is ready.
     * @param {number} previewIndex Index of the page with respect to the pages
     *     shown in the preview. E.g an index of 0 is the first displayed page,
     *     but not necessarily the first original document page.
     * @param {number} pageNumber Number of the page with respect to the
     *     document. A value of 3 means it's the third page of the original
     *     document.
     * @param {number} previewUid Unique identifier of the preview.
     * @private
     */
    dispatchPageReadyEvent_: function(previewIndex, pageNumber, previewUid) {
      var pageGenEvent = new Event(PreviewGenerator.EventType.PAGE_READY);
      pageGenEvent.previewIndex = previewIndex;
      pageGenEvent.previewUrl = 'chrome://print/' + previewUid.toString() +
          '/' + (pageNumber - 1) + '/print.pdf';
      this.dispatchEvent(pageGenEvent);
    },

    /**
     * Dispatches a PREVIEW_START event. Signals that the preview should be
     * reloaded.
     * @param {number} previewUid Unique identifier of the preview.
     * @param {number} index Index of the first page of the preview.
     * @private
     */
    dispatchPreviewStartEvent_: function(previewUid, index) {
      var previewStartEvent = new Event(
          PreviewGenerator.EventType.PREVIEW_START);
      if (!this.documentInfo_.isModifiable) {
        index = -1;
      }
      previewStartEvent.previewUrl = 'chrome://print/' +
          previewUid.toString() + '/' + index + '/print.pdf';
      this.dispatchEvent(previewStartEvent);
    },

    /**
     * @return {boolean} Whether the print ticket has changed sufficiently to
     *     determine whether a new preview request should be issued.
     * @private
     */
    hasPreviewChanged_: function() {
      var ticketStore = this.printTicketStore_;
      return this.inFlightRequestId_ == -1 ||
          !ticketStore.landscape.isValueEqual(this.isLandscapeEnabled_) ||
          !ticketStore.headerFooter.isValueEqual(this.isHeaderFooterEnabled_) ||
          !ticketStore.color.isValueEqual(this.colorValue_) ||
          !ticketStore.fitToPage.isValueEqual(this.isFitToPageEnabled_) ||
          this.pageRanges_ == null ||
          !areRangesEqual(ticketStore.pageRange.getPageRanges(),
                          this.pageRanges_) ||
          (!ticketStore.marginsType.isValueEqual(this.marginsType_) &&
              !ticketStore.marginsType.isValueEqual(
                  print_preview.ticket_items.MarginsType.Value.CUSTOM)) ||
          (ticketStore.marginsType.isValueEqual(
              print_preview.ticket_items.MarginsType.Value.CUSTOM) &&
              !ticketStore.customMargins.isValueEqual(
                  this.documentInfo_.margins)) ||
          !ticketStore.cssBackground.isValueEqual(
              this.isCssBackgroundEnabled_) ||
          !ticketStore.selectionOnly.isValueEqual(
              this.isSelectionOnlyEnabled_) ||
          (this.selectedDestination_ !=
              this.destinationStore_.selectedDestination);
    },

    /**
     * Called when the page layout of the document is ready. Always occurs
     * as a result of a preview request.
     * @param {Event} event Contains layout info about the document.
     * @private
     */
    onPageLayoutReady_: function(event) {
      // NOTE: A request ID is not specified, so assuming its for the current
      // in-flight request.

      var origin = new print_preview.Coordinate2d(
          event.pageLayout.printableAreaX,
          event.pageLayout.printableAreaY);
      var size = new print_preview.Size(
          event.pageLayout.printableAreaWidth,
          event.pageLayout.printableAreaHeight);

      var margins = new print_preview.Margins(
          Math.round(event.pageLayout.marginTop),
          Math.round(event.pageLayout.marginRight),
          Math.round(event.pageLayout.marginBottom),
          Math.round(event.pageLayout.marginLeft));

      var o = print_preview.ticket_items.CustomMargins.Orientation;
      var pageSize = new print_preview.Size(
          event.pageLayout.contentWidth +
              margins.get(o.LEFT) + margins.get(o.RIGHT),
          event.pageLayout.contentHeight +
              margins.get(o.TOP) + margins.get(o.BOTTOM));

      this.documentInfo_.updatePageInfo(
          new print_preview.PrintableArea(origin, size),
          pageSize,
          event.hasCustomPageSizeStyle,
          margins);
    },

    /**
     * Called when the document page count is received from the native layer.
     * Always occurs as a result of a preview request.
     * @param {Event} event Contains the document's page count.
     * @private
     */
    onPageCountReady_: function(event) {
      if (this.inFlightRequestId_ != event.previewResponseId) {
        return; // Ignore old response.
      }
      this.documentInfo_.updatePageCount(event.pageCount);
      this.pageRanges_ = this.printTicketStore_.pageRange.getPageRanges();
    },

    /**
     * Called when the print preview should be reloaded.
     * @param {Event} event Contains the preview UID and request ID.
     * @private
     */
    onPreviewReload_: function(event) {
      if (this.inFlightRequestId_ != event.previewResponseId) {
        return; // Ignore old response.
      }
      var pageNumberSet = this.printTicketStore_.pageRange.getPageNumberSet();
      this.dispatchPreviewStartEvent_(
          event.previewUid, pageNumberSet.getPageNumberAt(0) - 1);
      for (var i = 0; i < pageNumberSet.size; i++) {
        var pageNumber = pageNumberSet.getPageNumberAt(i);
        this.dispatchPageReadyEvent_(i, pageNumber, event.previewUid);
      }
      cr.dispatchSimpleEvent(this, PreviewGenerator.EventType.DOCUMENT_READY);
    },

    /**
     * Called when a page's preview has been generated. Dispatches a
     * PAGE_READY event.
     * @param {Event} event Contains the page index and preview UID.
     * @private
     */
    onPagePreviewReady_: function(event) {
      if (this.inFlightRequestId_ != event.previewResponseId) {
        return; // Ignore old response.
      }
      var pageNumber = event.pageIndex + 1;
      var pageNumberSet = this.printTicketStore_.pageRange.getPageNumberSet();
      if (pageNumberSet.hasPageNumber(pageNumber)) {
        var previewIndex = pageNumberSet.getPageNumberIndex(pageNumber);
        if (previewIndex == 0) {
          this.dispatchPreviewStartEvent_(event.previewUid, event.pageIndex);
        }
        this.dispatchPageReadyEvent_(
            previewIndex, pageNumber, event.previewUid);
      }
    },

    /**
     * Called when the preview generation is complete. Dispatches a
     * DOCUMENT_READY event.
     * @param {Event} event Contains the preview UID and response ID.
     * @private
     */
    onPreviewGenerationDone_: function(event) {
      if (this.inFlightRequestId_ != event.previewResponseId) {
        return; // Ignore old response.
      }
      // Dispatch a PREVIEW_START event since non-modifiable documents don't
      // trigger PAGE_READY events.
      if (!this.documentInfo_.isModifiable) {
        this.dispatchPreviewStartEvent_(event.previewUid, 0);
      }
      cr.dispatchSimpleEvent(this, PreviewGenerator.EventType.DOCUMENT_READY);
    },

    /**
     * Called when the preview generation fails.
     * @private
     */
    onPreviewGenerationFail_: function() {
      // NOTE: No request ID is returned from Chromium so its assumed its the
      // current one.
      cr.dispatchSimpleEvent(this, PreviewGenerator.EventType.FAIL);
    }
  };

  // Export
  return {
    PreviewGenerator: PreviewGenerator
  };
});


// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Component that displays a list of destinations with a heading, action link,
   * and "Show All..." button. An event is dispatched when the action link is
   * activated.
   * @param {!cr.EventTarget} eventTarget Event target to pass to destination
   *     items for dispatching SELECT events.
   * @param {string} title Title of the destination list.
   * @param {string=} opt_actionLinkLabel Optional label of the action link. If
   *     no label is provided, the action link will not be shown.
   * @constructor
   * @extends {print_preview.Component}
   */
  function DestinationList(eventTarget, title, opt_actionLinkLabel) {
    print_preview.Component.call(this);

    /**
     * Event target to pass to destination items for dispatching SELECT events.
     * @type {!cr.EventTarget}
     * @private
     */
    this.eventTarget_ = eventTarget;

    /**
     * Title of the destination list.
     * @type {string}
     * @private
     */
    this.title_ = title;

    /**
     * Label of the action link.
     * @type {?string}
     * @private
     */
    this.actionLinkLabel_ = opt_actionLinkLabel || null;

    /**
     * Backing store for the destination list.
     * @type {!Array.<print_preview.Destination>}
     * @private
     */
    this.destinations_ = [];

    /**
     * Current query used for filtering.
     * @type {?string}
     * @private
     */
    this.query_ = null;

    /**
     * Whether the destination list is fully expanded.
     * @type {boolean}
     * @private
     */
    this.isShowAll_ = false;

    /**
     * Maximum number of destinations before showing the "Show All..." button.
     * @type {number}
     * @private
     */
    this.shortListSize_ = DestinationList.DEFAULT_SHORT_LIST_SIZE_;
  };

  /**
   * Enumeration of event types dispatched by the destination list.
   * @enum {string}
   */
  DestinationList.EventType = {
    // Dispatched when the action linked is activated.
    ACTION_LINK_ACTIVATED: 'print_preview.DestinationList.ACTION_LINK_ACTIVATED'
  };

  /**
   * Default maximum number of destinations before showing the "Show All..."
   * button.
   * @type {number}
   * @const
   * @private
   */
  DestinationList.DEFAULT_SHORT_LIST_SIZE_ = 4;

  /**
   * Height of a destination list item in pixels.
   * @type {number}
   * @const
   * @private
   */
  DestinationList.HEIGHT_OF_ITEM_ = 30;

  DestinationList.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @param {boolean} isShowAll Whether the show-all button is activated. */
    setIsShowAll: function(isShowAll) {
      this.isShowAll_ = isShowAll;
      this.renderDestinations_();
    },

    /**
     * @return {number} Size of list when destination list is in collapsed
     *     mode (a.k.a non-show-all mode).
     */
    getShortListSize: function() {
      return this.shortListSize_;
    },

    /** @return {number} Count of the destinations in the list. */
    getDestinationsCount: function() {
      return this.destinations_.length;
    },

    /**
     * Gets estimated height of the destination list for the given number of
     * items.
     * @param {number} Number of items to render in the destination list.
     * @return {number} Height (in pixels) of the destination list.
     */
    getEstimatedHeightInPixels: function(numItems) {
      numItems = Math.min(numItems, this.destinations_.length);
      var headerHeight =
          this.getChildElement('.destination-list > header').offsetHeight;
      var throbberHeight =
          getIsVisible(this.getChildElement('.throbber-container')) ?
              DestinationList.HEIGHT_OF_ITEM_ : 0;
      return headerHeight + numItems * DestinationList.HEIGHT_OF_ITEM_ +
          throbberHeight;
    },

    /** @param {boolean} isVisible Whether the throbber is visible. */
    setIsThrobberVisible: function(isVisible) {
      setIsVisible(this.getChildElement('.throbber-container'), isVisible);
    },

    /**
     * @param {number} size Size of list when destination list is in collapsed
     *     mode (a.k.a non-show-all mode).
     */
    updateShortListSize: function(size) {
      size = Math.max(1, Math.min(size, this.destinations_.length));
      if (size == 1 && this.destinations_.length > 1) {
        // If this is the case, we will only show the "Show All" button and
        // nothing else. Increment the short list size by one so that we can see
        // at least one print destination.
        size++;
      }
      this.setShortListSizeInternal(size);
    },

    /** @override */
    createDom: function() {
      this.setElementInternal(this.cloneTemplateInternal(
          'destination-list-template'));
      this.getChildElement('.title').textContent = this.title_;
      if (this.actionLinkLabel_) {
        var actionLinkEl = this.getChildElement('.action-link');
        actionLinkEl.textContent = this.actionLinkLabel_;
        setIsVisible(actionLinkEl, true);
      }
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);
      this.tracker.add(
          this.getChildElement('.action-link'),
          'click',
          this.onActionLinkClick_.bind(this));
      this.tracker.add(
          this.getChildElement('.show-all-button'),
          'click',
          this.setIsShowAll.bind(this, true));
    },

    /**
     * Updates the destinations to render in the destination list.
     * @param {!Array.<print_preview.Destination>} destinations Destinations to
     *     render.
     */
    updateDestinations: function(destinations) {
      this.destinations_ = destinations;
      this.renderDestinations_();
    },

    /** @param {?string} query Query to update the filter with. */
    updateSearchQuery: function(query) {
      this.query_ = query;
      this.renderDestinations_();
    },

    /**
     * @param {string} text Text to set the action link to.
     * @protected
     */
    setActionLinkTextInternal: function(text) {
      this.actionLinkLabel_ = text;
      this.getChildElement('.action-link').textContent = text;
    },

    /**
     * Sets the short list size without constraints.
     * @protected
     */
    setShortListSizeInternal: function(size) {
      this.shortListSize_ = size;
      this.renderDestinations_();
    },

    /**
     * Renders all destinations in the given list.
     * @param {!Array.<print_preview.Destination>} destinations List of
     *     destinations to render.
     * @protected
     */
    renderListInternal: function(destinations) {
      setIsVisible(this.getChildElement('.no-destinations-message'),
                   destinations.length == 0);
      setIsVisible(this.getChildElement('.destination-list > footer'), false);
      var numItems = destinations.length;
      if (destinations.length > this.shortListSize_ && !this.isShowAll_) {
        numItems = this.shortListSize_ - 1;
        this.getChildElement('.total').textContent =
            localStrings.getStringF('destinationCount', destinations.length);
        setIsVisible(this.getChildElement('.destination-list > footer'), true);
      }
      for (var i = 0; i < numItems; i++) {
        var destListItem = new print_preview.DestinationListItem(
            this.eventTarget_, destinations[i]);
        this.addChild(destListItem);
        destListItem.render(this.getChildElement('.destination-list > ul'));
      }
    },

    /**
     * Renders all destinations in the list that match the current query. For
     * each render, all old destination items are first removed.
     * @private
     */
    renderDestinations_: function() {
      this.removeChildren();

      var filteredDests = [];
      this.destinations_.forEach(function(destination) {
        if (!this.query_ || destination.matches(this.query_)) {
          filteredDests.push(destination);
        }
      }, this);

      this.renderListInternal(filteredDests);
    },

    /**
     * Called when the action link is clicked. Dispatches an
     * ACTION_LINK_ACTIVATED event.
     * @private
     */
    onActionLinkClick_: function() {
      cr.dispatchSimpleEvent(this,
                             DestinationList.EventType.ACTION_LINK_ACTIVATED);
    }
  };

  // Export
  return {
    DestinationList: DestinationList
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Sub-class of a destination list that shows cloud-based destinations.
   * @param {!cr.EventTarget} eventTarget Event target to pass to destination
   *     items for dispatching SELECT events.
   * @constructor
   * @extends {print_preview.DestinationList}
   */
  function CloudDestinationList(eventTarget) {
    print_preview.DestinationList.call(
        this,
        eventTarget,
        localStrings.getString('cloudDestinationsTitle'),
        localStrings.getString('manage'));
  };

  CloudDestinationList.prototype = {
    __proto__: print_preview.DestinationList.prototype,

    /** @override */
    updateDestinations: function(destinations) {
      // Change the action link from "Manage..." to "Setup..." if user only has
      // Docs and FedEx printers.
      var docsId = print_preview.Destination.GooglePromotedId.DOCS;
      var fedexId = print_preview.Destination.GooglePromotedId.FEDEX;
      if ((destinations.length == 1 && destinations[0].id == docsId) ||
          (destinations.length == 2 &&
           ((destinations[0].id == docsId && destinations[1].id == fedexId) ||
            (destinations[0].id == fedexId && destinations[1].id == docsId)))) {
        this.setActionLinkTextInternal(
            localStrings.getString('setupCloudPrinters'));
      } else {
        this.setActionLinkTextInternal(localStrings.getString('manage'));
      }
      print_preview.DestinationList.prototype.updateDestinations.call(
          this, destinations);
    }
  };

  return {
    CloudDestinationList: CloudDestinationList
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Sub-class of a destination list that shows recent destinations. This list
   * does not render a "Show all" button.
   * @param {!cr.EventTarget} eventTarget Event target to pass to destination
   *     items for dispatching SELECT events.
   * @constructor
   * @extends {print_preview.DestinationList}
   */
  function RecentDestinationList(eventTarget) {
    print_preview.DestinationList.call(
        this,
        eventTarget,
        localStrings.getString('recentDestinationsTitle'));
  };

  RecentDestinationList.prototype = {
    __proto__: print_preview.DestinationList.prototype,

    /** @override */
    updateShortListSize: function(size) {
      this.setShortListSizeInternal(
          Math.max(1, Math.min(size, this.getDestinationsCount())));
    },

    /** @override */
    renderListInternal: function(destinations) {
      setIsVisible(this.getChildElement('.no-destinations-message'),
                   destinations.length == 0);
      setIsVisible(this.getChildElement('.destination-list > footer'), false);
      var numItems = Math.min(destinations.length, this.shortListSize_);
      for (var i = 0; i < numItems; i++) {
        var destListItem = new print_preview.DestinationListItem(
            this.eventTarget_, destinations[i]);
        this.addChild(destListItem);
        destListItem.render(this.getChildElement('.destination-list > ul'));
      }
    }
  };

  return {
    RecentDestinationList: RecentDestinationList
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Component that renders a destination item in a destination list.
   * @param {!cr.EventTarget} eventTarget Event target to dispatch selection
   *     events to.
   * @param {!print_preview.Destination} destination Destination data object to
   *     render.
   * @constructor
   * @extends {print_preview.Component}
   */
  function DestinationListItem(eventTarget, destination) {
    print_preview.Component.call(this);

    /**
     * Event target to dispatch selection events to.
     * @type {!cr.EventTarget}
     * @private
     */
    this.eventTarget_ = eventTarget;

    /**
     * Destination that the list item renders.
     * @type {!print_preview.Destination}
     * @private
     */
    this.destination_ = destination;

    /**
     * FedEx terms-of-service widget or {@code null} if this list item does not
     * render the FedEx Office print destination.
     * @type {print_preview.FedexTos}
     * @private
     */
    this.fedexTos_ = null;
  };

  /**
   * Event types dispatched by the destination list item.
   * @enum {string}
   */
  DestinationListItem.EventType = {
    // Dispatched when the list item is activated.
    SELECT: 'print_preview.DestinationListItem.SELECT'
  };

  /**
   * CSS classes used by the destination list item.
   * @enum {string}
   * @private
   */
  DestinationListItem.Classes_ = {
    ICON: 'destination-list-item-icon',
    NAME: 'destination-list-item-name',
    STALE: 'stale'
  };

  DestinationListItem.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @override */
    createDom: function() {
      this.setElementInternal(this.cloneTemplateInternal(
          'destination-list-item-template'));

      var iconImg = this.getElement().getElementsByClassName(
          print_preview.DestinationListItem.Classes_.ICON)[0];
      iconImg.src = this.destination_.iconUrl;

      var nameEl = this.getElement().getElementsByClassName(
          DestinationListItem.Classes_.NAME)[0];
      nameEl.textContent = this.destination_.displayName;
      nameEl.title = this.destination_.displayName;

      this.initializeOfflineStatusElement_();
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);
      this.tracker.add(this.getElement(), 'click', this.onActivate_.bind(this));
    },

    /**
     * Initializes the element which renders the print destination's
     * offline status.
     * @private
     */
    initializeOfflineStatusElement_: function() {
      if (arrayContains([print_preview.Destination.ConnectionStatus.OFFLINE,
                         print_preview.Destination.ConnectionStatus.DORMANT],
                        this.destination_.connectionStatus)) {
        this.getElement().classList.add(DestinationListItem.Classes_.STALE);
        var offlineDurationMs = Date.now() - this.destination_.lastAccessTime;
        var offlineMessageId;
        if (offlineDurationMs > 31622400000.0) { // One year.
          offlineMessageId = 'offlineForYear';
        } else if (offlineDurationMs > 2678400000.0) { // One month.
          offlineMessageId = 'offlineForMonth';
        } else if (offlineDurationMs > 604800000.0) { // One week.
          offlineMessageId = 'offlineForWeek';
        } else {
          offlineMessageId = 'offline';
        }
        var offlineStatusEl = this.getElement().querySelector(
            '.offline-status');
        offlineStatusEl.textContent = localStrings.getString(offlineMessageId);
        setIsVisible(offlineStatusEl, true);
      }
    },

    /**
     * Called when the destination item is activated. Dispatches a SELECT event
     * on the given event target.
     * @private
     */
    onActivate_: function() {
      if (this.destination_.id ==
              print_preview.Destination.GooglePromotedId.FEDEX &&
          !this.destination_.isTosAccepted) {
        if (!this.fedexTos_) {
          this.fedexTos_ = new print_preview.FedexTos();
          this.fedexTos_.render(this.getElement());
          this.tracker.add(
              this.fedexTos_,
              print_preview.FedexTos.EventType.AGREE,
              this.onTosAgree_.bind(this));
        }
        this.fedexTos_.setIsVisible(true);
      } else {
        var selectEvt = new Event(DestinationListItem.EventType.SELECT);
        selectEvt.destination = this.destination_;
        this.eventTarget_.dispatchEvent(selectEvt);
      }
    },

    /**
     * Called when the user agrees to the print destination's terms-of-service.
     * Selects the print destination that was agreed to.
     * @private
     */
    onTosAgree_: function() {
      var selectEvt = new Event(DestinationListItem.EventType.SELECT);
      selectEvt.destination = this.destination_;
      this.eventTarget_.dispatchEvent(selectEvt);
    }
  };

  // Export
  return {
    DestinationListItem: DestinationListItem
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Component used for searching for a print destination.
   * This is a modal dialog that allows the user to search and select a
   * destination to print to. When a destination is selected, it is written to
   * the destination store.
   * @param {!print_preview.DestinationStore} destinationStore Data store
   *     containing the destinations to search through.
   * @param {!print_preview.UserInfo} userInfo Event target that contains
   *     information about the logged in user.
   * @param {!print_preview.Metrics} metrics Used to record usage statistics.
   * @constructor
   * @extends {print_preview.Component}
   */
  function DestinationSearch(destinationStore, userInfo, metrics) {
    print_preview.Component.call(this);

    /**
     * Data store containing the destinations to search through.
     * @type {!print_preview.DestinationStore}
     * @private
     */
    this.destinationStore_ = destinationStore;

    /**
     * Event target that contains information about the logged in user.
     * @type {!print_preview.UserInfo}
     * @private
     */
    this.userInfo_ = userInfo;

    /**
     * Used to record usage statistics.
     * @type {!print_preview.Metrics}
     * @private
     */
    this.metrics_ = metrics;

    /**
     * Search box used to search through the destination lists.
     * @type {!print_preview.SearchBox}
     * @private
     */
    this.searchBox_ = new print_preview.SearchBox();
    this.addChild(this.searchBox_);

    /**
     * Destination list containing recent destinations.
     * @type {!print_preview.DestinationList}
     * @private
     */
    this.recentList_ = new print_preview.RecentDestinationList(this);
    this.addChild(this.recentList_);

    /**
     * Destination list containing local destinations.
     * @type {!print_preview.DestinationList}
     * @private
     */
    this.localList_ = new print_preview.DestinationList(
        this,
        localStrings.getString('localDestinationsTitle'),
        cr.isChromeOS ? null : localStrings.getString('manage'));
    this.addChild(this.localList_);

    /**
     * Destination list containing cloud destinations.
     * @type {!print_preview.DestinationList}
     * @private
     */
    this.cloudList_ = new print_preview.CloudDestinationList(this);
    this.addChild(this.cloudList_);
  };

  /**
   * Event types dispatched by the component.
   * @enum {string}
   */
  DestinationSearch.EventType = {
    // Dispatched when the user requests to manage their cloud destinations.
    MANAGE_CLOUD_DESTINATIONS:
        'print_preview.DestinationSearch.MANAGE_CLOUD_DESTINATIONS',

    // Dispatched when the user requests to manage their local destinations.
    MANAGE_LOCAL_DESTINATIONS:
        'print_preview.DestinationSearch.MANAGE_LOCAL_DESTINATIONS',

    // Dispatched when the user requests to sign-in to their Google account.
    SIGN_IN: 'print_preview.DestinationSearch.SIGN_IN'
  };

  /**
   * Padding at the bottom of a destination list in pixels.
   * @type {number}
   * @const
   * @private
   */
  DestinationSearch.LIST_BOTTOM_PADDING_ = 18;

  DestinationSearch.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @return {boolean} Whether the component is visible. */
    getIsVisible: function() {
      return !this.getElement().classList.contains('transparent');
    },

    /** @param {boolean} isVisible Whether the component is visible. */
    setIsVisible: function(isVisible) {
      if (isVisible) {
        this.searchBox_.focus();
        this.getElement().classList.remove('transparent');
        var promoEl = this.getChildElement('.cloudprint-promo');
        if (getIsVisible(promoEl)) {
          this.metrics_.incrementDestinationSearchBucket(
              print_preview.Metrics.DestinationSearchBucket.
                  CLOUDPRINT_PROMO_SHOWN);
        }
        this.reflowLists_();
      } else {
        this.getElement().classList.add('transparent');
        // Collapse all destination lists
        this.localList_.setIsShowAll(false);
        this.cloudList_.setIsShowAll(false);
        this.searchBox_.setQuery('');
        this.filterLists_(null);
      }
    },

    /** @param {string} email Email of the logged-in user. */
    setCloudPrintEmail: function(email) {
      var userInfoEl = this.getChildElement('.user-info');
      userInfoEl.textContent = localStrings.getStringF('userInfo', email);
      userInfoEl.title = localStrings.getStringF('userInfo', email);
      setIsVisible(userInfoEl, true);
      setIsVisible(this.getChildElement('.cloud-list'), true);
      setIsVisible(this.getChildElement('.cloudprint-promo'), false);
      this.reflowLists_();
    },

    /** Shows the Google Cloud Print promotion banner. */
    showCloudPrintPromo: function() {
      setIsVisible(this.getChildElement('.cloudprint-promo'), true);
      if (this.getIsVisible()) {
        this.metrics_.incrementDestinationSearchBucket(
            print_preview.Metrics.DestinationSearchBucket.
                CLOUDPRINT_PROMO_SHOWN);
      }
      this.reflowLists_();
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);
      this.tracker.add(
          this.getChildElement('.page > .close-button'),
          'click',
          this.onCloseClick_.bind(this));

      this.tracker.add(
          this.getChildElement('.sign-in'),
          'click',
          this.onSignInActivated_.bind(this));

      this.tracker.add(
          this.getChildElement('.cloudprint-promo > .close-button'),
          'click',
          this.onCloudprintPromoCloseButtonClick_.bind(this));
      this.tracker.add(
          this.searchBox_,
          print_preview.SearchBox.EventType.SEARCH,
          this.onSearch_.bind(this));
      this.tracker.add(
          this,
          print_preview.DestinationListItem.EventType.SELECT,
          this.onDestinationSelect_.bind(this));

      this.tracker.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.DESTINATIONS_INSERTED,
          this.onDestinationsInserted_.bind(this));
      this.tracker.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.DESTINATION_SELECT,
          this.onDestinationStoreSelect_.bind(this));
      this.tracker.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.DESTINATION_SEARCH_STARTED,
          this.updateThrobbers_.bind(this));
      this.tracker.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.DESTINATION_SEARCH_DONE,
          this.updateThrobbers_.bind(this));

      this.tracker.add(
          this.localList_,
          print_preview.DestinationList.EventType.ACTION_LINK_ACTIVATED,
          this.onManageLocalDestinationsActivated_.bind(this));
      this.tracker.add(
          this.cloudList_,
          print_preview.DestinationList.EventType.ACTION_LINK_ACTIVATED,
          this.onManageCloudDestinationsActivated_.bind(this));

      this.tracker.add(
          this.getElement(), 'click', this.onClick_.bind(this));
      this.tracker.add(
          this.getChildElement('.page'),
          'webkitAnimationEnd',
          this.onAnimationEnd_.bind(this));

      this.tracker.add(
          this.userInfo_,
          print_preview.UserInfo.EventType.EMAIL_CHANGE,
          this.onEmailChange_.bind(this));

      this.tracker.add(window, 'resize', this.onWindowResize_.bind(this));

      this.updateThrobbers_();

      // Render any destinations already in the store.
      this.renderDestinations_();
    },

    /** @override */
    decorateInternal: function() {
      this.searchBox_.decorate($('search-box'));
      this.recentList_.render(this.getChildElement('.recent-list'));
      this.localList_.render(this.getChildElement('.local-list'));
      this.cloudList_.render(this.getChildElement('.cloud-list'));
      this.getChildElement('.promo-text').innerHTML = localStrings.getStringF(
          'cloudPrintPromotion',
          '<span class="sign-in link-button">',
          '</span>');
    },

    /**
     * @return {number} Height available for destination lists, in pixels.
     * @private
     */
    getAvailableListsHeight_: function() {
      var elStyle = window.getComputedStyle(this.getElement());
      return this.getElement().offsetHeight -
          parseInt(elStyle.getPropertyValue('padding-top')) -
          parseInt(elStyle.getPropertyValue('padding-bottom')) -
          this.getChildElement('.lists').offsetTop -
          this.getChildElement('.cloudprint-promo').offsetHeight;
    },

    /**
     * Filters all destination lists with the given query.
     * @param {?string} query Query to filter destination lists by.
     * @private
     */
    filterLists_: function(query) {
      this.recentList_.updateSearchQuery(query);
      this.localList_.updateSearchQuery(query);
      this.cloudList_.updateSearchQuery(query);
    },

    /**
     * Resets the search query.
     * @private
     */
    resetSearch_: function() {
      this.searchBox_.setQuery(null);
      this.filterLists_(null);
    },

    /**
     * Renders all of the destinations in the destination store.
     * @private
     */
    renderDestinations_: function() {
      var recentDestinations = [];
      var localDestinations = [];
      var cloudDestinations = [];
      this.destinationStore_.destinations.forEach(function(destination) {
        if (destination.isRecent) {
          recentDestinations.push(destination);
        }
        if (destination.isLocal ||
            destination.origin == print_preview.Destination.Origin.DEVICE) {
          localDestinations.push(destination);
        } else {
          cloudDestinations.push(destination);
        }
      });
      this.recentList_.updateDestinations(recentDestinations);
      this.localList_.updateDestinations(localDestinations);
      this.cloudList_.updateDestinations(cloudDestinations);
    },

    /**
     * Reflows the destination lists according to the available height.
     * @private
     */
    reflowLists_: function() {
      if (!this.getIsVisible()) {
        return;
      }

      var hasCloudList = getIsVisible(this.getChildElement('.cloud-list'));
      var lists = [this.recentList_, this.localList_];
      if (hasCloudList) {
        lists.push(this.cloudList_);
      }

      var availableHeight = this.getAvailableListsHeight_();
      this.getChildElement('.lists').style.maxHeight = availableHeight + 'px';

      var maxListLength = lists.reduce(function(prevCount, list) {
        return Math.max(prevCount, list.getDestinationsCount());
      }, 0);
      for (var i = 1; i <= maxListLength; i++) {
        var listsHeight = lists.reduce(function(sum, list) {
          return sum + list.getEstimatedHeightInPixels(i) +
              DestinationSearch.LIST_BOTTOM_PADDING_;
        }, 0);
        if (listsHeight > availableHeight) {
          i -= 1;
          break;
        }
      }

      lists.forEach(function(list) {
        list.updateShortListSize(i);
      });

      // Set height of the list manually so that search filter doesn't change
      // lists height.
      this.getChildElement('.lists').style.height =
          lists.reduce(function(sum, list) {
            return sum + list.getEstimatedHeightInPixels(i) +
                DestinationSearch.LIST_BOTTOM_PADDING_;
          }, 0) + 'px';
    },

    /**
     * Updates whether the throbbers for the various destination lists should be
     * shown or hidden.
     * @private
     */
    updateThrobbers_: function() {
      this.localList_.setIsThrobberVisible(
          this.destinationStore_.isLocalDestinationSearchInProgress);
      this.cloudList_.setIsThrobberVisible(
          this.destinationStore_.isCloudDestinationSearchInProgress);
      this.recentList_.setIsThrobberVisible(
          this.destinationStore_.isLocalDestinationSearchInProgress &&
          this.destinationStore_.isCloudDestinationSearchInProgress);
      this.reflowLists_();
    },

    /**
     * Called when a destination search should be executed. Filters the
     * destination lists with the given query.
     * @param {Event} evt Contains the search query.
     * @private
     */
    onSearch_: function(evt) {
      this.filterLists_(evt.query);
    },

    /**
     * Called when the close button is clicked. Hides the search widget.
     * @private
     */
    onCloseClick_: function() {
      this.setIsVisible(false);
      this.resetSearch_();
      this.metrics_.incrementDestinationSearchBucket(
          print_preview.Metrics.DestinationSearchBucket.CANCELED);
    },

    /**
     * Called when a destination is selected. Clears the search and hides the
     * widget.
     * @param {Event} evt Contains the selected destination.
     * @private
     */
    onDestinationSelect_: function(evt) {
      this.setIsVisible(false);
      this.resetSearch_();
      this.destinationStore_.selectDestination(evt.destination);
      this.metrics_.incrementDestinationSearchBucket(
          print_preview.Metrics.DestinationSearchBucket.DESTINATION_SELECTED);
    },

    /**
     * Called when a destination is selected. Selected destination are marked as
     * recent, so we have to update our recent destinations list.
     * @private
     */
    onDestinationStoreSelect_: function() {
      var destinations = this.destinationStore_.destinations;
      var recentDestinations = [];
      destinations.forEach(function(destination) {
        if (destination.isRecent) {
          recentDestinations.push(destination);
        }
      });
      this.recentList_.updateDestinations(recentDestinations);
      this.reflowLists_();
    },

    /**
     * Called when destinations are inserted into the store. Rerenders
     * destinations.
     * @private
     */
    onDestinationsInserted_: function() {
      this.renderDestinations_();
      this.reflowLists_();
    },

    /**
     * Called when the manage cloud printers action is activated.
     * @private
     */
    onManageCloudDestinationsActivated_: function() {
      cr.dispatchSimpleEvent(
          this,
          print_preview.DestinationSearch.EventType.MANAGE_CLOUD_DESTINATIONS);
    },

    /**
     * Called when the manage local printers action is activated.
     * @private
     */
    onManageLocalDestinationsActivated_: function() {
      cr.dispatchSimpleEvent(
          this,
          print_preview.DestinationSearch.EventType.MANAGE_LOCAL_DESTINATIONS);
    },

    /**
     * Called when the "Sign in" link on the Google Cloud Print promo is
     * activated.
     * @private
     */
    onSignInActivated_: function() {
      cr.dispatchSimpleEvent(this, DestinationSearch.EventType.SIGN_IN);
      this.metrics_.incrementDestinationSearchBucket(
          print_preview.Metrics.DestinationSearchBucket.SIGNIN_TRIGGERED);
    },

    /**
     * Called when the close button on the cloud print promo is clicked. Hides
     * the promo.
     * @private
     */
    onCloudprintPromoCloseButtonClick_: function() {
      setIsVisible(this.getChildElement('.cloudprint-promo'), false);
    },

    /**
     * Called when the overlay is clicked. Pulses the page.
     * @param {Event} event Contains the element that was clicked.
     * @private
     */
    onClick_: function(event) {
      if (event.target == this.getElement()) {
        this.getChildElement('.page').classList.add('pulse');
      }
    },

    /**
     * Called when an animation ends on the page.
     * @private
     */
    onAnimationEnd_: function() {
      this.getChildElement('.page').classList.remove('pulse');
    },

    /**
     * Called when the user's email field has changed. Updates the UI.
     * @private
     */
    onEmailChange_: function() {
      var userEmail = this.userInfo_.getUserEmail();
      if (userEmail) {
        this.setCloudPrintEmail(userEmail);
      }
    },

    /**
     * Called when the window is resized. Reflows layout of destination lists.
     * @private
     */
    onWindowResize_: function() {
      this.reflowLists_();
    }
  };

  // Export
  return {
    DestinationSearch: DestinationSearch
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Component that renders a search box for searching through destinations.
   * @constructor
   * @extends {print_preview.Component}
   */
  function SearchBox() {
    print_preview.Component.call(this);

    /**
     * Timeout used to control incremental search.
     * @type {?number}
     * @private
     */
     this.timeout_ = null;

    /**
     * Input box where the query is entered.
     * @type {HTMLInputElement}
     * @private
     */
    this.input_ = null;
  };

  /**
   * Enumeration of event types dispatched from the search box.
   * @enum {string}
   */
  SearchBox.EventType = {
    SEARCH: 'print_preview.SearchBox.SEARCH'
  };

  /**
   * CSS classes used by the search box.
   * @enum {string}
   * @private
   */
  SearchBox.Classes_ = {
    INPUT: 'search-box-input'
  };

  /**
   * Delay in milliseconds before dispatching a SEARCH event.
   * @type {number}
   * @const
   * @private
   */
  SearchBox.SEARCH_DELAY_ = 150;

  SearchBox.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @param {string} New query to set the search box's query to. */
    setQuery: function(query) {
      query = query || '';
      this.input_.value = query.trim();
    },

    /** Sets the input element of the search box in focus. */
    focus: function() {
      this.input_.focus();
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);
      this.tracker.add(this.input_, 'keydown', this.onInputKeyDown_.bind(this));
    },

    /** @override */
    exitDocument: function() {
      print_preview.Component.prototype.exitDocument.call(this);
      this.input_ = null;
    },

    /** @override */
    decorateInternal: function() {
      this.input_ = this.getElement().getElementsByClassName(
          SearchBox.Classes_.INPUT)[0];
    },

    /**
     * @return {string} The current query of the search box.
     * @private
     */
    getQuery_: function() {
      return this.input_.value.trim();
    },

    /**
     * Dispatches a SEARCH event.
     * @private
     */
    dispatchSearchEvent_: function() {
      this.timeout_ = null;
      var searchEvent = new Event(SearchBox.EventType.SEARCH);
      searchEvent.query = this.getQuery_();
      this.dispatchEvent(searchEvent);
    },

    /**
     * Called when the input element's value changes. Dispatches a search event.
     * @private
     */
    onInputKeyDown_: function() {
      if (this.timeout_) {
        clearTimeout(this.timeout_);
      }
      this.timeout_ = setTimeout(
          this.dispatchSearchEvent_.bind(this), SearchBox.SEARCH_DELAY_);
    }
  };

  // Export
  return {
    SearchBox: SearchBox
  };
});

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Widget that renders a terms-of-service agreement for using the FedEx Office
   * print destination.
   * @constructor
   * @extends {print_preview.Component}
   */
  function FedexTos() {
    print_preview.Component.call(this);
  };

  /**
   * Enumeration of event types dispatched by the widget.
   * @enum {string}
   */
  FedexTos.EventType = {
    // Dispatched when the user agrees to the terms-of-service.
    AGREE: 'print_preview.FedexTos.AGREE'
  };

  FedexTos.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @param {boolean} isVisible Whether the widget is visible. */
    setIsVisible: function(isVisible) {
      if (isVisible) {
        var heightHelperEl = this.getElement().querySelector('.height-helper');
        this.getElement().style.height = heightHelperEl.offsetHeight + 'px';
      } else {
        this.getElement().style.height = 0;
      }
    },

    /** @override */
    createDom: function() {
      this.setElementInternal(this.cloneTemplateInternal('fedex-tos-template'));
      var tosTextEl = this.getElement().querySelector('.tos-text');
      tosTextEl.innerHTML = localStrings.getStringF(
          'fedexTos',
          '<a href="http://www.fedex.com/us/office/copyprint/online/' +
              'googlecloudprint/termsandconditions">',
          '</a>');
    },

    /** @override */
    enterDocument: function() {
      var agreeCheckbox = this.getElement().querySelector('.agree-checkbox');
      this.tracker.add(
          agreeCheckbox, 'click', this.onAgreeCheckboxClick_.bind(this));
    },

    /**
     * Called when the agree checkbox is clicked. Dispatches a AGREE event.
     * @private
     */
    onAgreeCheckboxClick_: function() {
      cr.dispatchSimpleEvent(this, FedexTos.EventType.AGREE);
    }
  };

  // Export
  return {
    FedexTos: FedexTos
  };
});


window.addEventListener('DOMContentLoaded', function() {
  printPreview = new print_preview.PrintPreview();
  printPreview.initialize();
});
