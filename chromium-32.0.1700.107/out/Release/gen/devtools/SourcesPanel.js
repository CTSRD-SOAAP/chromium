WebInspector.JavaScriptBreakpointsSidebarPane=function(breakpointManager,showSourceLineDelegate)
{WebInspector.SidebarPane.call(this,WebInspector.UIString("Breakpoints"));this.registerRequiredCSS("breakpointsList.css");this._breakpointManager=breakpointManager;this._showSourceLineDelegate=showSourceLineDelegate;this.listElement=document.createElement("ol");this.listElement.className="breakpoint-list";this.emptyElement=document.createElement("div");this.emptyElement.className="info";this.emptyElement.textContent=WebInspector.UIString("No Breakpoints");this.bodyElement.appendChild(this.emptyElement);this._items=new Map();var breakpointLocations=this._breakpointManager.allBreakpointLocations();for(var i=0;i<breakpointLocations.length;++i)
this._addBreakpoint(breakpointLocations[i].breakpoint,breakpointLocations[i].uiLocation);this._breakpointManager.addEventListener(WebInspector.BreakpointManager.Events.BreakpointAdded,this._breakpointAdded,this);this._breakpointManager.addEventListener(WebInspector.BreakpointManager.Events.BreakpointRemoved,this._breakpointRemoved,this);this.emptyElement.addEventListener("contextmenu",this._emptyElementContextMenu.bind(this),true);}
WebInspector.JavaScriptBreakpointsSidebarPane.prototype={_emptyElementContextMenu:function(event)
{var contextMenu=new WebInspector.ContextMenu(event);var breakpointActive=WebInspector.debuggerModel.breakpointsActive();var breakpointActiveTitle=breakpointActive?WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Deactivate breakpoints":"Deactivate Breakpoints"):WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Activate breakpoints":"Activate Breakpoints");contextMenu.appendItem(breakpointActiveTitle,WebInspector.debuggerModel.setBreakpointsActive.bind(WebInspector.debuggerModel,!breakpointActive));contextMenu.show();},_breakpointAdded:function(event)
{this._breakpointRemoved(event);var breakpoint=(event.data.breakpoint);var uiLocation=(event.data.uiLocation);this._addBreakpoint(breakpoint,uiLocation);},_addBreakpoint:function(breakpoint,uiLocation)
{var element=document.createElement("li");element.addStyleClass("cursor-pointer");element.addEventListener("contextmenu",this._breakpointContextMenu.bind(this,breakpoint),true);element.addEventListener("click",this._breakpointClicked.bind(this,uiLocation),false);var checkbox=document.createElement("input");checkbox.className="checkbox-elem";checkbox.type="checkbox";checkbox.checked=breakpoint.enabled();checkbox.addEventListener("click",this._breakpointCheckboxClicked.bind(this,breakpoint),false);element.appendChild(checkbox);var labelElement=document.createTextNode(uiLocation.linkText());element.appendChild(labelElement);var snippetElement=document.createElement("div");snippetElement.className="source-text monospace";element.appendChild(snippetElement);function didRequestContent(content)
{var lineEndings=content.lineEndings();if(uiLocation.lineNumber<lineEndings.length)
snippetElement.textContent=content.substring(lineEndings[uiLocation.lineNumber-1],lineEndings[uiLocation.lineNumber]);}
uiLocation.uiSourceCode.requestContent(didRequestContent.bind(this));element._data=uiLocation;var currentElement=this.listElement.firstChild;while(currentElement){if(currentElement._data&&this._compareBreakpoints(currentElement._data,element._data)>0)
break;currentElement=currentElement.nextSibling;}
this._addListElement(element,currentElement);var breakpointItem={};breakpointItem.element=element;breakpointItem.checkbox=checkbox;this._items.put(breakpoint,breakpointItem);this.expand();},_breakpointRemoved:function(event)
{var breakpoint=(event.data.breakpoint);var uiLocation=(event.data.uiLocation);var breakpointItem=this._items.get(breakpoint);if(!breakpointItem)
return;this._items.remove(breakpoint);this._removeListElement(breakpointItem.element);},highlightBreakpoint:function(breakpoint)
{var breakpointItem=this._items.get(breakpoint);if(!breakpointItem)
return;breakpointItem.element.addStyleClass("breakpoint-hit");this._highlightedBreakpointItem=breakpointItem;},clearBreakpointHighlight:function()
{if(this._highlightedBreakpointItem){this._highlightedBreakpointItem.element.removeStyleClass("breakpoint-hit");delete this._highlightedBreakpointItem;}},_breakpointClicked:function(uiLocation,event)
{this._showSourceLineDelegate(uiLocation.uiSourceCode,uiLocation.lineNumber);},_breakpointCheckboxClicked:function(breakpoint,event)
{event.consume();breakpoint.setEnabled(event.target.checked);},_breakpointContextMenu:function(breakpoint,event)
{var breakpoints=this._items.values();var contextMenu=new WebInspector.ContextMenu(event);contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Remove breakpoint":"Remove Breakpoint"),breakpoint.remove.bind(breakpoint));if(breakpoints.length>1){var removeAllTitle=WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Remove all breakpoints":"Remove All Breakpoints");contextMenu.appendItem(removeAllTitle,this._breakpointManager.removeAllBreakpoints.bind(this._breakpointManager));}
contextMenu.appendSeparator();var breakpointActive=WebInspector.debuggerModel.breakpointsActive();var breakpointActiveTitle=breakpointActive?WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Deactivate breakpoints":"Deactivate Breakpoints"):WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Activate breakpoints":"Activate Breakpoints");contextMenu.appendItem(breakpointActiveTitle,WebInspector.debuggerModel.setBreakpointsActive.bind(WebInspector.debuggerModel,!breakpointActive));function enabledBreakpointCount(breakpoints)
{var count=0;for(var i=0;i<breakpoints.length;++i){if(breakpoints[i].checkbox.checked)
count++;}
return count;}
if(breakpoints.length>1){var enableBreakpointCount=enabledBreakpointCount(breakpoints);var enableTitle=WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Enable all breakpoints":"Enable All Breakpoints");var disableTitle=WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Disable all breakpoints":"Disable All Breakpoints");contextMenu.appendSeparator();contextMenu.appendItem(enableTitle,this._breakpointManager.toggleAllBreakpoints.bind(this._breakpointManager,true),!(enableBreakpointCount!=breakpoints.length));contextMenu.appendItem(disableTitle,this._breakpointManager.toggleAllBreakpoints.bind(this._breakpointManager,false),!(enableBreakpointCount>1));}
contextMenu.show();},_addListElement:function(element,beforeElement)
{if(beforeElement)
this.listElement.insertBefore(element,beforeElement);else{if(!this.listElement.firstChild){this.bodyElement.removeChild(this.emptyElement);this.bodyElement.appendChild(this.listElement);}
this.listElement.appendChild(element);}},_removeListElement:function(element)
{this.listElement.removeChild(element);if(!this.listElement.firstChild){this.bodyElement.removeChild(this.listElement);this.bodyElement.appendChild(this.emptyElement);}},_compare:function(x,y)
{if(x!==y)
return x<y?-1:1;return 0;},_compareBreakpoints:function(b1,b2)
{return this._compare(b1.uiSourceCode.originURL(),b2.uiSourceCode.originURL())||this._compare(b1.lineNumber,b2.lineNumber);},reset:function()
{this.listElement.removeChildren();if(this.listElement.parentElement){this.bodyElement.removeChild(this.listElement);this.bodyElement.appendChild(this.emptyElement);}
this._items.clear();},__proto__:WebInspector.SidebarPane.prototype}
WebInspector.XHRBreakpointsSidebarPane=function()
{WebInspector.NativeBreakpointsSidebarPane.call(this,WebInspector.UIString("XHR Breakpoints"));this._breakpointElements={};var addButton=document.createElement("button");addButton.className="pane-title-button add";addButton.addEventListener("click",this._addButtonClicked.bind(this),false);addButton.title=WebInspector.UIString("Add XHR breakpoint");this.titleElement.appendChild(addButton);this.emptyElement.addEventListener("contextmenu",this._emptyElementContextMenu.bind(this),true);this._restoreBreakpoints();}
WebInspector.XHRBreakpointsSidebarPane.prototype={_emptyElementContextMenu:function(event)
{var contextMenu=new WebInspector.ContextMenu(event);contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Add breakpoint":"Add Breakpoint"),this._addButtonClicked.bind(this));contextMenu.show();},_addButtonClicked:function(event)
{if(event)
event.consume();this.expand();var inputElementContainer=document.createElement("p");inputElementContainer.className="breakpoint-condition";var inputElement=document.createElement("span");inputElementContainer.textContent=WebInspector.UIString("Break when URL contains:");inputElement.className="editing";inputElement.id="breakpoint-condition-input";inputElementContainer.appendChild(inputElement);this._addListElement(inputElementContainer,this.listElement.firstChild);function finishEditing(accept,e,text)
{this._removeListElement(inputElementContainer);if(accept){this._setBreakpoint(text,true);this._saveBreakpoints();}}
var config=new WebInspector.EditingConfig(finishEditing.bind(this,true),finishEditing.bind(this,false));WebInspector.startEditing(inputElement,config);},_setBreakpoint:function(url,enabled)
{if(url in this._breakpointElements)
return;var element=document.createElement("li");element._url=url;element.addEventListener("contextmenu",this._contextMenu.bind(this,url),true);var checkboxElement=document.createElement("input");checkboxElement.className="checkbox-elem";checkboxElement.type="checkbox";checkboxElement.checked=enabled;checkboxElement.addEventListener("click",this._checkboxClicked.bind(this,url),false);element._checkboxElement=checkboxElement;element.appendChild(checkboxElement);var labelElement=document.createElement("span");if(!url)
labelElement.textContent=WebInspector.UIString("Any XHR");else
labelElement.textContent=WebInspector.UIString("URL contains \"%s\"",url);labelElement.addStyleClass("cursor-auto");labelElement.addEventListener("dblclick",this._labelClicked.bind(this,url),false);element.appendChild(labelElement);var currentElement=this.listElement.firstChild;while(currentElement){if(currentElement._url&&currentElement._url<element._url)
break;currentElement=currentElement.nextSibling;}
this._addListElement(element,currentElement);this._breakpointElements[url]=element;if(enabled)
DOMDebuggerAgent.setXHRBreakpoint(url);},_removeBreakpoint:function(url)
{var element=this._breakpointElements[url];if(!element)
return;this._removeListElement(element);delete this._breakpointElements[url];if(element._checkboxElement.checked)
DOMDebuggerAgent.removeXHRBreakpoint(url);},_contextMenu:function(url,event)
{var contextMenu=new WebInspector.ContextMenu(event);function removeBreakpoint()
{this._removeBreakpoint(url);this._saveBreakpoints();}
function removeAllBreakpoints()
{for(var url in this._breakpointElements)
this._removeBreakpoint(url);this._saveBreakpoints();}
var removeAllTitle=WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Remove all breakpoints":"Remove All Breakpoints");contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Add breakpoint":"Add Breakpoint"),this._addButtonClicked.bind(this));contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Remove breakpoint":"Remove Breakpoint"),removeBreakpoint.bind(this));contextMenu.appendItem(removeAllTitle,removeAllBreakpoints.bind(this));contextMenu.show();},_checkboxClicked:function(url,event)
{if(event.target.checked)
DOMDebuggerAgent.setXHRBreakpoint(url);else
DOMDebuggerAgent.removeXHRBreakpoint(url);this._saveBreakpoints();},_labelClicked:function(url)
{var element=this._breakpointElements[url];var inputElement=document.createElement("span");inputElement.className="breakpoint-condition editing";inputElement.textContent=url;this.listElement.insertBefore(inputElement,element);element.addStyleClass("hidden");function finishEditing(accept,e,text)
{this._removeListElement(inputElement);if(accept){this._removeBreakpoint(url);this._setBreakpoint(text,element._checkboxElement.checked);this._saveBreakpoints();}else
element.removeStyleClass("hidden");}
WebInspector.startEditing(inputElement,new WebInspector.EditingConfig(finishEditing.bind(this,true),finishEditing.bind(this,false)));},highlightBreakpoint:function(url)
{var element=this._breakpointElements[url];if(!element)
return;this.expand();element.addStyleClass("breakpoint-hit");this._highlightedElement=element;},clearBreakpointHighlight:function()
{if(this._highlightedElement){this._highlightedElement.removeStyleClass("breakpoint-hit");delete this._highlightedElement;}},_saveBreakpoints:function()
{var breakpoints=[];for(var url in this._breakpointElements)
breakpoints.push({url:url,enabled:this._breakpointElements[url]._checkboxElement.checked});WebInspector.settings.xhrBreakpoints.set(breakpoints);},_restoreBreakpoints:function()
{var breakpoints=WebInspector.settings.xhrBreakpoints.get();for(var i=0;i<breakpoints.length;++i){var breakpoint=breakpoints[i];if(breakpoint&&typeof breakpoint.url==="string")
this._setBreakpoint(breakpoint.url,breakpoint.enabled);}},__proto__:WebInspector.NativeBreakpointsSidebarPane.prototype}
WebInspector.EventListenerBreakpointsSidebarPane=function()
{WebInspector.SidebarPane.call(this,WebInspector.UIString("Event Listener Breakpoints"));this.registerRequiredCSS("breakpointsList.css");this.categoriesElement=document.createElement("ol");this.categoriesElement.tabIndex=0;this.categoriesElement.addStyleClass("properties-tree");this.categoriesElement.addStyleClass("event-listener-breakpoints");this.categoriesTreeOutline=new TreeOutline(this.categoriesElement);this.bodyElement.appendChild(this.categoriesElement);this._breakpointItems={};this._createCategory(WebInspector.UIString("Animation"),false,["requestAnimationFrame","cancelAnimationFrame","animationFrameFired"]);this._createCategory(WebInspector.UIString("Control"),true,["resize","scroll","zoom","focus","blur","select","change","submit","reset"]);this._createCategory(WebInspector.UIString("Clipboard"),true,["copy","cut","paste","beforecopy","beforecut","beforepaste"]);this._createCategory(WebInspector.UIString("DOM Mutation"),true,["DOMActivate","DOMFocusIn","DOMFocusOut","DOMAttrModified","DOMCharacterDataModified","DOMNodeInserted","DOMNodeInsertedIntoDocument","DOMNodeRemoved","DOMNodeRemovedFromDocument","DOMSubtreeModified","DOMContentLoaded"]);this._createCategory(WebInspector.UIString("Device"),true,["deviceorientation","devicemotion"]);this._createCategory(WebInspector.UIString("Keyboard"),true,["keydown","keyup","keypress","input"]);this._createCategory(WebInspector.UIString("Load"),true,["load","unload","abort","error","hashchange"]);this._createCategory(WebInspector.UIString("Mouse"),true,["click","dblclick","mousedown","mouseup","mouseover","mousemove","mouseout","mousewheel"]);this._createCategory(WebInspector.UIString("Timer"),false,["setTimer","clearTimer","timerFired"]);this._createCategory(WebInspector.UIString("Touch"),true,["touchstart","touchmove","touchend","touchcancel"]);this._createCategory(WebInspector.UIString("WebGL"),false,["webglErrorFired","webglWarningFired"]);this._restoreBreakpoints();}
WebInspector.EventListenerBreakpointsSidebarPane.categotyListener="listener:";WebInspector.EventListenerBreakpointsSidebarPane.categotyInstrumentation="instrumentation:";WebInspector.EventListenerBreakpointsSidebarPane.eventNameForUI=function(eventName,auxData)
{if(!WebInspector.EventListenerBreakpointsSidebarPane._eventNamesForUI){WebInspector.EventListenerBreakpointsSidebarPane._eventNamesForUI={"instrumentation:setTimer":WebInspector.UIString("Set Timer"),"instrumentation:clearTimer":WebInspector.UIString("Clear Timer"),"instrumentation:timerFired":WebInspector.UIString("Timer Fired"),"instrumentation:requestAnimationFrame":WebInspector.UIString("Request Animation Frame"),"instrumentation:cancelAnimationFrame":WebInspector.UIString("Cancel Animation Frame"),"instrumentation:animationFrameFired":WebInspector.UIString("Animation Frame Fired"),"instrumentation:webglErrorFired":WebInspector.UIString("WebGL Error Fired"),"instrumentation:webglWarningFired":WebInspector.UIString("WebGL Warning Fired")};}
if(auxData){if(eventName==="instrumentation:webglErrorFired"&&auxData["webglErrorName"]){var errorName=auxData["webglErrorName"];errorName=errorName.replace(/^.*(0x[0-9a-f]+).*$/i,"$1");return WebInspector.UIString("WebGL Error Fired (%s)",errorName);}}
return WebInspector.EventListenerBreakpointsSidebarPane._eventNamesForUI[eventName]||eventName.substring(eventName.indexOf(":")+1);}
WebInspector.EventListenerBreakpointsSidebarPane.prototype={_createCategory:function(name,isDOMEvent,eventNames)
{var categoryItem={};categoryItem.element=new TreeElement(name);this.categoriesTreeOutline.appendChild(categoryItem.element);categoryItem.element.listItemElement.addStyleClass("event-category");categoryItem.element.selectable=true;categoryItem.checkbox=this._createCheckbox(categoryItem.element);categoryItem.checkbox.addEventListener("click",this._categoryCheckboxClicked.bind(this,categoryItem),true);categoryItem.children={};for(var i=0;i<eventNames.length;++i){var eventName=(isDOMEvent?WebInspector.EventListenerBreakpointsSidebarPane.categotyListener:WebInspector.EventListenerBreakpointsSidebarPane.categotyInstrumentation)+eventNames[i];var breakpointItem={};var title=WebInspector.EventListenerBreakpointsSidebarPane.eventNameForUI(eventName);breakpointItem.element=new TreeElement(title);categoryItem.element.appendChild(breakpointItem.element);var hitMarker=document.createElement("div");hitMarker.className="breakpoint-hit-marker";breakpointItem.element.listItemElement.appendChild(hitMarker);breakpointItem.element.listItemElement.addStyleClass("source-code");breakpointItem.element.selectable=false;breakpointItem.checkbox=this._createCheckbox(breakpointItem.element);breakpointItem.checkbox.addEventListener("click",this._breakpointCheckboxClicked.bind(this,eventName),true);breakpointItem.parent=categoryItem;this._breakpointItems[eventName]=breakpointItem;categoryItem.children[eventName]=breakpointItem;}},_createCheckbox:function(treeElement)
{var checkbox=document.createElement("input");checkbox.className="checkbox-elem";checkbox.type="checkbox";treeElement.listItemElement.insertBefore(checkbox,treeElement.listItemElement.firstChild);return checkbox;},_categoryCheckboxClicked:function(categoryItem)
{var checked=categoryItem.checkbox.checked;for(var eventName in categoryItem.children){var breakpointItem=categoryItem.children[eventName];if(breakpointItem.checkbox.checked===checked)
continue;if(checked)
this._setBreakpoint(eventName);else
this._removeBreakpoint(eventName);}
this._saveBreakpoints();},_breakpointCheckboxClicked:function(eventName,event)
{if(event.target.checked)
this._setBreakpoint(eventName);else
this._removeBreakpoint(eventName);this._saveBreakpoints();},_setBreakpoint:function(eventName)
{var breakpointItem=this._breakpointItems[eventName];if(!breakpointItem)
return;breakpointItem.checkbox.checked=true;if(eventName.startsWith(WebInspector.EventListenerBreakpointsSidebarPane.categotyListener))
DOMDebuggerAgent.setEventListenerBreakpoint(eventName.substring(WebInspector.EventListenerBreakpointsSidebarPane.categotyListener.length));else if(eventName.startsWith(WebInspector.EventListenerBreakpointsSidebarPane.categotyInstrumentation))
DOMDebuggerAgent.setInstrumentationBreakpoint(eventName.substring(WebInspector.EventListenerBreakpointsSidebarPane.categotyInstrumentation.length));this._updateCategoryCheckbox(breakpointItem.parent);},_removeBreakpoint:function(eventName)
{var breakpointItem=this._breakpointItems[eventName];if(!breakpointItem)
return;breakpointItem.checkbox.checked=false;if(eventName.startsWith(WebInspector.EventListenerBreakpointsSidebarPane.categotyListener))
DOMDebuggerAgent.removeEventListenerBreakpoint(eventName.substring(WebInspector.EventListenerBreakpointsSidebarPane.categotyListener.length));else if(eventName.startsWith(WebInspector.EventListenerBreakpointsSidebarPane.categotyInstrumentation))
DOMDebuggerAgent.removeInstrumentationBreakpoint(eventName.substring(WebInspector.EventListenerBreakpointsSidebarPane.categotyInstrumentation.length));this._updateCategoryCheckbox(breakpointItem.parent);},_updateCategoryCheckbox:function(categoryItem)
{var hasEnabled=false,hasDisabled=false;for(var eventName in categoryItem.children){var breakpointItem=categoryItem.children[eventName];if(breakpointItem.checkbox.checked)
hasEnabled=true;else
hasDisabled=true;}
categoryItem.checkbox.checked=hasEnabled;categoryItem.checkbox.indeterminate=hasEnabled&&hasDisabled;},highlightBreakpoint:function(eventName)
{var breakpointItem=this._breakpointItems[eventName];if(!breakpointItem)
return;this.expand();breakpointItem.parent.element.expand();breakpointItem.element.listItemElement.addStyleClass("breakpoint-hit");this._highlightedElement=breakpointItem.element.listItemElement;},clearBreakpointHighlight:function()
{if(this._highlightedElement){this._highlightedElement.removeStyleClass("breakpoint-hit");delete this._highlightedElement;}},_saveBreakpoints:function()
{var breakpoints=[];for(var eventName in this._breakpointItems){if(this._breakpointItems[eventName].checkbox.checked)
breakpoints.push({eventName:eventName});}
WebInspector.settings.eventListenerBreakpoints.set(breakpoints);},_restoreBreakpoints:function()
{var breakpoints=WebInspector.settings.eventListenerBreakpoints.get();for(var i=0;i<breakpoints.length;++i){var breakpoint=breakpoints[i];if(breakpoint&&typeof breakpoint.eventName==="string")
this._setBreakpoint(breakpoint.eventName);}},__proto__:WebInspector.SidebarPane.prototype};WebInspector.CallStackSidebarPane=function()
{WebInspector.SidebarPane.call(this,WebInspector.UIString("Call Stack"));this._model=WebInspector.debuggerModel;this.bodyElement.addEventListener("keydown",this._keyDown.bind(this),true);this.bodyElement.tabIndex=0;}
WebInspector.CallStackSidebarPane.Events={CallFrameSelected:"CallFrameSelected"}
WebInspector.CallStackSidebarPane.prototype={update:function(callFrames)
{this.bodyElement.removeChildren();delete this._statusMessageElement;this.placards=[];if(!callFrames){var infoElement=document.createElement("div");infoElement.className="info";infoElement.textContent=WebInspector.UIString("Not Paused");this.bodyElement.appendChild(infoElement);return;}
for(var i=0;i<callFrames.length;++i){var callFrame=callFrames[i];var placard=new WebInspector.CallStackSidebarPane.Placard(callFrame,this);placard.element.addEventListener("click",this._placardSelected.bind(this,placard),false);this.placards.push(placard);this.bodyElement.appendChild(placard.element);}},setSelectedCallFrame:function(x)
{for(var i=0;i<this.placards.length;++i){var placard=this.placards[i];placard.selected=(placard._callFrame===x);}},_selectNextCallFrameOnStack:function(event)
{var index=this._selectedCallFrameIndex();if(index==-1)
return true;this._selectedPlacardByIndex(index+1);return true;},_selectPreviousCallFrameOnStack:function(event)
{var index=this._selectedCallFrameIndex();if(index==-1)
return true;this._selectedPlacardByIndex(index-1);return true;},_selectedPlacardByIndex:function(index)
{if(index<0||index>=this.placards.length)
return;this._placardSelected(this.placards[index])},_selectedCallFrameIndex:function()
{if(!this._model.selectedCallFrame())
return-1;for(var i=0;i<this.placards.length;++i){var placard=this.placards[i];if(placard._callFrame===this._model.selectedCallFrame())
return i;}
return-1;},_placardSelected:function(placard)
{this.dispatchEventToListeners(WebInspector.CallStackSidebarPane.Events.CallFrameSelected,placard._callFrame);},_copyStackTrace:function()
{var text="";for(var i=0;i<this.placards.length;++i)
text+=this.placards[i].title+" ("+this.placards[i].subtitle+")\n";InspectorFrontendHost.copyText(text);},registerShortcuts:function(registerShortcutDelegate)
{registerShortcutDelegate(WebInspector.SourcesPanelDescriptor.ShortcutKeys.NextCallFrame,this._selectNextCallFrameOnStack.bind(this));registerShortcutDelegate(WebInspector.SourcesPanelDescriptor.ShortcutKeys.PrevCallFrame,this._selectPreviousCallFrameOnStack.bind(this));},setStatus:function(status)
{if(!this._statusMessageElement){this._statusMessageElement=document.createElement("div");this._statusMessageElement.className="info";this.bodyElement.appendChild(this._statusMessageElement);}
if(typeof status==="string")
this._statusMessageElement.textContent=status;else{this._statusMessageElement.removeChildren();this._statusMessageElement.appendChild(status);}},_keyDown:function(event)
{if(event.altKey||event.shiftKey||event.metaKey||event.ctrlKey)
return;if(event.keyIdentifier==="Up"){this._selectPreviousCallFrameOnStack();event.consume();}else if(event.keyIdentifier==="Down"){this._selectNextCallFrameOnStack();event.consume();}},__proto__:WebInspector.SidebarPane.prototype}
WebInspector.CallStackSidebarPane.Placard=function(callFrame,pane)
{WebInspector.Placard.call(this,callFrame.functionName||WebInspector.UIString("(anonymous function)"),"");callFrame.createLiveLocation(this._update.bind(this));this.element.addEventListener("contextmenu",this._placardContextMenu.bind(this),true);this._callFrame=callFrame;this._pane=pane;}
WebInspector.CallStackSidebarPane.Placard.prototype={_update:function(uiLocation)
{this.subtitle=uiLocation.linkText().trimMiddle(100);},_placardContextMenu:function(event)
{var contextMenu=new WebInspector.ContextMenu(event);if(WebInspector.debuggerModel.canSetScriptSource()){contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Restart frame":"Restart Frame"),this._restartFrame.bind(this));contextMenu.appendSeparator();}
contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Copy stack trace":"Copy Stack Trace"),this._pane._copyStackTrace.bind(this._pane));contextMenu.show();},_restartFrame:function()
{this._callFrame.restart(undefined);},__proto__:WebInspector.Placard.prototype};WebInspector.FilePathScoreFunction=function(query)
{this._query=query;this._queryUpperCase=query.toUpperCase();this._score=null;this._sequence=null;this._dataUpperCase="";this._fileNameIndex=0;}
WebInspector.FilePathScoreFunction.filterRegex=function(query)
{const toEscape=String.regexSpecialCharacters();var regexString="";for(var i=0;i<query.length;++i){var c=query.charAt(i);if(toEscape.indexOf(c)!==-1)
c="\\"+c;if(i)
regexString+="[^"+c+"]*";regexString+=c;}
return new RegExp(regexString,"i");}
WebInspector.FilePathScoreFunction.prototype={score:function(data,matchIndexes)
{if(!data||!this._query)
return 0;var n=this._query.length;var m=data.length;if(!this._score||this._score.length<n*m){this._score=new Int32Array(n*m*2);this._sequence=new Int32Array(n*m*2);}
var score=this._score;var sequence=this._sequence;this._dataUpperCase=data.toUpperCase();this._fileNameIndex=data.lastIndexOf("/");for(var i=0;i<n;++i){for(var j=0;j<m;++j){var skipCharScore=j===0?0:score[i*m+j-1];var prevCharScore=i===0||j===0?0:score[(i-1)*m+j-1];var consecutiveMatch=i===0||j===0?0:sequence[(i-1)*m+j-1];var pickCharScore=this._match(this._query,data,i,j,consecutiveMatch);if(pickCharScore&&prevCharScore+pickCharScore>skipCharScore){sequence[i*m+j]=consecutiveMatch+1;score[i*m+j]=(prevCharScore+pickCharScore);}else{sequence[i*m+j]=0;score[i*m+j]=skipCharScore;}}}
if(matchIndexes)
this._restoreMatchIndexes(sequence,n,m,matchIndexes);return score[n*m-1];},_testWordStart:function(data,j)
{var prevChar=data.charAt(j-1);return j===0||prevChar==="_"||prevChar==="-"||prevChar==="/"||(data[j-1]!==this._dataUpperCase[j-1]&&data[j]===this._dataUpperCase[j]);},_restoreMatchIndexes:function(sequence,n,m,out)
{var i=n-1,j=m-1;while(i>=0&&j>=0){switch(sequence[i*m+j]){case 0:--j;break;default:out.push(j);--i;--j;break;}}
out.reverse();},_singleCharScore:function(query,data,i,j)
{var isWordStart=this._testWordStart(data,j);var isFileName=j>this._fileNameIndex;var isPathTokenStart=j===0||data[j-1]==="/";var isCapsMatch=query[i]===data[j]&&query[i]==this._queryUpperCase[i];var score=10;if(isPathTokenStart)
score+=4;if(isWordStart)
score+=2;if(isCapsMatch)
score+=6;if(isFileName)
score+=4;if(j===this._fileNameIndex+1&&i===0)
score+=5;if(isFileName&&isWordStart)
score+=3;return score;},_sequenceCharScore:function(query,data,i,j,sequenceLength)
{var isFileName=j>this._fileNameIndex;var isPathTokenStart=j===0||data[j-1]==="/";var score=10;if(isFileName)
score+=4;if(isPathTokenStart)
score+=5;score+=sequenceLength*4;return score;},_match:function(query,data,i,j,consecutiveMatch)
{if(this._queryUpperCase[i]!==this._dataUpperCase[j])
return 0;if(!consecutiveMatch)
return this._singleCharScore(query,data,i,j);else
return this._sequenceCharScore(query,data,i,j-consecutiveMatch,consecutiveMatch);},};WebInspector.FilteredItemSelectionDialog=function(delegate)
{WebInspector.DialogDelegate.call(this);var xhr=new XMLHttpRequest();xhr.open("GET","filteredItemSelectionDialog.css",false);xhr.send(null);this.element=document.createElement("div");this.element.className="filtered-item-list-dialog";this.element.addEventListener("keydown",this._onKeyDown.bind(this),false);var styleElement=this.element.createChild("style");styleElement.type="text/css";styleElement.textContent=xhr.responseText;this._promptElement=this.element.createChild("input","monospace");this._promptElement.addEventListener("input",this._onInput.bind(this),false);this._promptElement.type="text";this._promptElement.setAttribute("spellcheck","false");this._filteredItems=[];this._viewportControl=new WebInspector.ViewportControl(this);this._itemElementsContainer=this._viewportControl.element;this._itemElementsContainer.addStyleClass("container");this._itemElementsContainer.addStyleClass("monospace");this._itemElementsContainer.addEventListener("click",this._onClick.bind(this),false);this.element.appendChild(this._itemElementsContainer);this._delegate=delegate;this._delegate.setRefreshCallback(this._itemsLoaded.bind(this));this._itemsLoaded();this._shouldShowMatchingItems=true;}
WebInspector.FilteredItemSelectionDialog.prototype={position:function(element,relativeToElement)
{const minWidth=500;const minHeight=204;var width=Math.max(relativeToElement.offsetWidth*2/3,minWidth);var height=Math.max(relativeToElement.offsetHeight*2/3,minHeight);this.element.style.width=width+"px";const shadowPadding=20;element.positionAt(relativeToElement.totalOffsetLeft()+Math.max((relativeToElement.offsetWidth-width-2*shadowPadding)/2,shadowPadding),relativeToElement.totalOffsetTop()+Math.max((relativeToElement.offsetHeight-height-2*shadowPadding)/2,shadowPadding));this._dialogHeight=height;this._updateShowMatchingItems();},focus:function()
{WebInspector.setCurrentFocusElement(this._promptElement);if(this._filteredItems.length&&this._viewportControl.lastVisibleIndex()===-1)
this._viewportControl.refresh();},willHide:function()
{if(this._isHiding)
return;this._isHiding=true;this._delegate.dispose();if(this._filterTimer)
clearTimeout(this._filterTimer);},renderAsTwoRows:function()
{this._renderAsTwoRows=true;},onEnter:function()
{if(!this._delegate.itemCount())
return;this._delegate.selectItem(this._filteredItems[this._selectedIndexInFiltered],this._promptElement.value.trim());},_itemsLoaded:function()
{if(this._loadTimeout)
return;this._loadTimeout=setTimeout(this._updateAfterItemsLoaded.bind(this),0);},_updateAfterItemsLoaded:function()
{delete this._loadTimeout;this._filterItems();},_createItemElement:function(index)
{var itemElement=document.createElement("div");itemElement.className="filtered-item-list-dialog-item "+(this._renderAsTwoRows?"two-rows":"one-row");itemElement._titleElement=itemElement.createChild("span");itemElement._titleSuffixElement=itemElement.createChild("span");itemElement._subtitleElement=itemElement.createChild("div","filtered-item-list-dialog-subtitle");itemElement._subtitleElement.textContent="\u200B";itemElement._index=index;this._delegate.renderItem(index,this._promptElement.value.trim(),itemElement._titleElement,itemElement._subtitleElement);return itemElement;},setQuery:function(query)
{this._promptElement.value=query;this._scheduleFilter();},_filterItems:function()
{delete this._filterTimer;if(this._scoringTimer){clearTimeout(this._scoringTimer);delete this._scoringTimer;}
var query=this._delegate.rewriteQuery(this._promptElement.value.trim());this._query=query;var queryLength=query.length;var filterRegex=query?WebInspector.FilePathScoreFunction.filterRegex(query):null;var oldSelectedAbsoluteIndex=this._selectedIndexInFiltered?this._filteredItems[this._selectedIndexInFiltered]:null;var filteredItems=[];this._selectedIndexInFiltered=0;var bestScores=[];var bestItems=[];var bestItemsToCollect=100;var minBestScore=0;var overflowItems=[];scoreItems.call(this,0);function compareIntegers(a,b)
{return b-a;}
function scoreItems(fromIndex)
{var maxWorkItems=1000;var workDone=0;for(var i=fromIndex;i<this._delegate.itemCount()&&workDone<maxWorkItems;++i){if(filterRegex&&!filterRegex.test(this._delegate.itemKeyAt(i)))
continue;var score=this._delegate.itemScoreAt(i,query);if(query)
workDone++;if(score>minBestScore||bestScores.length<bestItemsToCollect){var index=insertionIndexForObjectInListSortedByFunction(score,bestScores,compareIntegers,true);bestScores.splice(index,0,score);bestItems.splice(index,0,i);if(bestScores.length>bestItemsToCollect){overflowItems.push(bestItems.peekLast());bestScores.length=bestItemsToCollect;bestItems.length=bestItemsToCollect;}
minBestScore=bestScores.peekLast();}else
filteredItems.push(i);}
if(i<this._delegate.itemCount()){this._scoringTimer=setTimeout(scoreItems.bind(this,i),0);return;}
delete this._scoringTimer;this._filteredItems=bestItems.concat(overflowItems).concat(filteredItems);for(var i=0;i<this._filteredItems.length;++i){if(this._filteredItems[i]===oldSelectedAbsoluteIndex){this._selectedIndexInFiltered=i;break;}}
this._viewportControl.refresh();if(!query)
this._selectedIndexInFiltered=0;this._updateSelection(this._selectedIndexInFiltered,false);}},_onInput:function(event)
{this._shouldShowMatchingItems=this._delegate.shouldShowMatchingItems(this._promptElement.value);this._updateShowMatchingItems();this._scheduleFilter();},_updateShowMatchingItems:function()
{this._itemElementsContainer.enableStyleClass("hidden",!this._shouldShowMatchingItems);this.element.style.height=this._shouldShowMatchingItems?this._dialogHeight+"px":"auto";},_onKeyDown:function(event)
{var newSelectedIndex=this._selectedIndexInFiltered;switch(event.keyCode){case WebInspector.KeyboardShortcut.Keys.Down.code:if(++newSelectedIndex>=this._filteredItems.length)
newSelectedIndex=this._filteredItems.length-1;this._updateSelection(newSelectedIndex,true);event.consume(true);break;case WebInspector.KeyboardShortcut.Keys.Up.code:if(--newSelectedIndex<0)
newSelectedIndex=0;this._updateSelection(newSelectedIndex,false);event.consume(true);break;case WebInspector.KeyboardShortcut.Keys.PageDown.code:newSelectedIndex=Math.min(newSelectedIndex+this._viewportControl.rowsPerViewport(),this._filteredItems.length-1);this._updateSelection(newSelectedIndex,true);event.consume(true);break;case WebInspector.KeyboardShortcut.Keys.PageUp.code:newSelectedIndex=Math.max(newSelectedIndex-this._viewportControl.rowsPerViewport(),0);this._updateSelection(newSelectedIndex,false);event.consume(true);break;default:}},_scheduleFilter:function()
{if(this._filterTimer)
return;this._filterTimer=setTimeout(this._filterItems.bind(this),0);},_updateSelection:function(index,makeLast)
{var element=this._viewportControl.renderedElementAt(this._selectedIndexInFiltered);if(element)
element.removeStyleClass("selected");this._viewportControl.scrollItemIntoView(index,makeLast);this._selectedIndexInFiltered=index;element=this._viewportControl.renderedElementAt(index);if(element)
element.addStyleClass("selected");},_onClick:function(event)
{var itemElement=event.target.enclosingNodeOrSelfWithClass("filtered-item-list-dialog-item");if(!itemElement)
return;this._delegate.selectItem(itemElement._index,this._promptElement.value.trim());WebInspector.Dialog.hide();},itemCount:function()
{return this._filteredItems.length;},itemElement:function(index)
{var delegateIndex=this._filteredItems[index];var element=this._createItemElement(delegateIndex);if(index===this._selectedIndexInFiltered)
element.addStyleClass("selected");return element;},__proto__:WebInspector.DialogDelegate.prototype}
WebInspector.SelectionDialogContentProvider=function()
{}
WebInspector.SelectionDialogContentProvider.prototype={setRefreshCallback:function(refreshCallback)
{this._refreshCallback=refreshCallback;},shouldShowMatchingItems:function(query)
{return true;},itemCount:function()
{return 0;},itemKeyAt:function(itemIndex)
{return"";},itemScoreAt:function(itemIndex,query)
{return 1;},renderItem:function(itemIndex,query,titleElement,subtitleElement)
{},highlightRanges:function(element,query)
{if(!query)
return false;function rangesForMatch(text,query)
{var sm=new difflib.SequenceMatcher(query,text);var opcodes=sm.get_opcodes();var ranges=[];for(var i=0;i<opcodes.length;++i){var opcode=opcodes[i];if(opcode[0]==="equal")
ranges.push({offset:opcode[3],length:opcode[4]-opcode[3]});else if(opcode[0]!=="insert")
return null;}
return ranges;}
var text=element.textContent;var ranges=rangesForMatch(text,query);if(!ranges)
ranges=rangesForMatch(text.toUpperCase(),query.toUpperCase());if(ranges){WebInspector.highlightRangesWithStyleClass(element,ranges,"highlight");return true;}
return false;},selectItem:function(itemIndex,promptValue)
{},refresh:function()
{this._refreshCallback();},rewriteQuery:function(query)
{return query;},dispose:function()
{}}
WebInspector.JavaScriptOutlineDialog=function(view,contentProvider)
{WebInspector.SelectionDialogContentProvider.call(this);this._functionItems=[];this._view=view;contentProvider.requestContent(this._contentAvailable.bind(this));}
WebInspector.JavaScriptOutlineDialog.show=function(view,contentProvider)
{if(WebInspector.Dialog.currentInstance())
return null;var filteredItemSelectionDialog=new WebInspector.FilteredItemSelectionDialog(new WebInspector.JavaScriptOutlineDialog(view,contentProvider));WebInspector.Dialog.show(view.element,filteredItemSelectionDialog);}
WebInspector.JavaScriptOutlineDialog.prototype={_contentAvailable:function(content)
{this._outlineWorker=new Worker("ScriptFormatterWorker.js");this._outlineWorker.onmessage=this._didBuildOutlineChunk.bind(this);const method="outline";this._outlineWorker.postMessage({method:method,params:{content:content}});},_didBuildOutlineChunk:function(event)
{var data=event.data;var chunk=data["chunk"];for(var i=0;i<chunk.length;++i)
this._functionItems.push(chunk[i]);if(data.total===data.index)
this.dispose();this.refresh();},itemCount:function()
{return this._functionItems.length;},itemKeyAt:function(itemIndex)
{return this._functionItems[itemIndex].name;},itemScoreAt:function(itemIndex,query)
{var item=this._functionItems[itemIndex];return-item.line;},renderItem:function(itemIndex,query,titleElement,subtitleElement)
{var item=this._functionItems[itemIndex];titleElement.textContent=item.name+(item.arguments?item.arguments:"");this.highlightRanges(titleElement,query);subtitleElement.textContent=":"+(item.line+1);},selectItem:function(itemIndex,promptValue)
{var lineNumber=this._functionItems[itemIndex].line;if(!isNaN(lineNumber)&&lineNumber>=0)
this._view.highlightPosition(lineNumber,this._functionItems[itemIndex].column);this._view.focus();},dispose:function()
{if(this._outlineWorker){this._outlineWorker.terminate();delete this._outlineWorker;}},__proto__:WebInspector.SelectionDialogContentProvider.prototype}
WebInspector.SelectUISourceCodeDialog=function(defaultScores)
{WebInspector.SelectionDialogContentProvider.call(this);this._uiSourceCodes=[];var projects=WebInspector.workspace.projects().filter(this.filterProject.bind(this));for(var i=0;i<projects.length;++i)
this._uiSourceCodes=this._uiSourceCodes.concat(projects[i].uiSourceCodes());this._defaultScores=defaultScores;this._scorer=new WebInspector.FilePathScoreFunction("");WebInspector.workspace.addEventListener(WebInspector.Workspace.Events.UISourceCodeAdded,this._uiSourceCodeAdded,this);}
WebInspector.SelectUISourceCodeDialog.prototype={uiSourceCodeSelected:function(uiSourceCode,lineNumber)
{},filterProject:function(project)
{return true;},itemCount:function()
{return this._uiSourceCodes.length;},itemKeyAt:function(itemIndex)
{return this._uiSourceCodes[itemIndex].fullDisplayName();},itemScoreAt:function(itemIndex,query)
{var uiSourceCode=this._uiSourceCodes[itemIndex];var score=this._defaultScores?(this._defaultScores.get(uiSourceCode)||0):0;if(!query||query.length<2)
return score;if(this._query!==query){this._query=query;this._scorer=new WebInspector.FilePathScoreFunction(query);}
var path=uiSourceCode.fullDisplayName();return score+10*this._scorer.score(path,null);},renderItem:function(itemIndex,query,titleElement,subtitleElement)
{query=this.rewriteQuery(query);var uiSourceCode=this._uiSourceCodes[itemIndex];titleElement.textContent=uiSourceCode.displayName()+(this._queryLineNumber?this._queryLineNumber:"");subtitleElement.textContent=uiSourceCode.fullDisplayName().trimEnd(100);var indexes=[];var score=new WebInspector.FilePathScoreFunction(query).score(subtitleElement.textContent,indexes);var fileNameIndex=subtitleElement.textContent.lastIndexOf("/");var ranges=[];for(var i=0;i<indexes.length;++i)
ranges.push({offset:indexes[i],length:1});if(indexes[0]>fileNameIndex){for(var i=0;i<ranges.length;++i)
ranges[i].offset-=fileNameIndex+1;return WebInspector.highlightRangesWithStyleClass(titleElement,ranges,"highlight");}else{return WebInspector.highlightRangesWithStyleClass(subtitleElement,ranges,"highlight");}},selectItem:function(itemIndex,promptValue)
{if(/^:\d+$/.test(promptValue.trimRight())){var lineNumber=parseInt(promptValue.trimRight().substring(1),10)-1;if(!isNaN(lineNumber)&&lineNumber>=0)
this.uiSourceCodeSelected(null,lineNumber);return;}
var lineNumberMatch=promptValue.match(/[^:]+\:([\d]*)$/);var lineNumber=lineNumberMatch?Math.max(parseInt(lineNumberMatch[1],10)-1,0):undefined;this.uiSourceCodeSelected(this._uiSourceCodes[itemIndex],lineNumber);},rewriteQuery:function(query)
{if(!query)
return query;query=query.trim();var lineNumberMatch=query.match(/([^:]+)(\:[\d]*)$/);this._queryLineNumber=lineNumberMatch?lineNumberMatch[2]:"";return lineNumberMatch?lineNumberMatch[1]:query;},_uiSourceCodeAdded:function(event)
{var uiSourceCode=(event.data);if(!this.filterProject(uiSourceCode.project()))
return;this._uiSourceCodes.push(uiSourceCode)
this.refresh();},dispose:function()
{WebInspector.workspace.removeEventListener(WebInspector.Workspace.Events.UISourceCodeAdded,this._uiSourceCodeAdded,this);},__proto__:WebInspector.SelectionDialogContentProvider.prototype}
WebInspector.OpenResourceDialog=function(panel,defaultScores)
{WebInspector.SelectUISourceCodeDialog.call(this,defaultScores);this._panel=panel;}
WebInspector.OpenResourceDialog.prototype={uiSourceCodeSelected:function(uiSourceCode,lineNumber)
{if(!uiSourceCode)
uiSourceCode=this._panel.currentUISourceCode();if(!uiSourceCode)
return;this._panel.showUISourceCode(uiSourceCode,lineNumber);},shouldShowMatchingItems:function(query)
{return!query.startsWith(":");},filterProject:function(project)
{return!project.isServiceProject();},__proto__:WebInspector.SelectUISourceCodeDialog.prototype}
WebInspector.OpenResourceDialog.show=function(panel,relativeToElement,name,defaultScores)
{if(WebInspector.Dialog.currentInstance())
return;var filteredItemSelectionDialog=new WebInspector.FilteredItemSelectionDialog(new WebInspector.OpenResourceDialog(panel,defaultScores));filteredItemSelectionDialog.renderAsTwoRows();if(name)
filteredItemSelectionDialog.setQuery(name);WebInspector.Dialog.show(relativeToElement,filteredItemSelectionDialog);}
WebInspector.SelectUISourceCodeForProjectTypeDialog=function(type,callback)
{this._type=type;WebInspector.SelectUISourceCodeDialog.call(this);this._callback=callback;}
WebInspector.SelectUISourceCodeForProjectTypeDialog.prototype={uiSourceCodeSelected:function(uiSourceCode,lineNumber)
{this._callback(uiSourceCode);},filterProject:function(project)
{return project.type()===this._type;},__proto__:WebInspector.SelectUISourceCodeDialog.prototype}
WebInspector.SelectUISourceCodeForProjectTypeDialog.show=function(name,type,callback,relativeToElement)
{if(WebInspector.Dialog.currentInstance())
return;var filteredItemSelectionDialog=new WebInspector.FilteredItemSelectionDialog(new WebInspector.SelectUISourceCodeForProjectTypeDialog(type,callback));filteredItemSelectionDialog.setQuery(name);filteredItemSelectionDialog.renderAsTwoRows();WebInspector.Dialog.show(relativeToElement,filteredItemSelectionDialog);};WebInspector.UISourceCodeFrame=function(uiSourceCode)
{this._uiSourceCode=uiSourceCode;WebInspector.SourceFrame.call(this,this._uiSourceCode);WebInspector.settings.textEditorAutocompletion.addChangeListener(this._enableAutocompletionIfNeeded,this);this._enableAutocompletionIfNeeded();this._uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.FormattedChanged,this._onFormattedChanged,this);this._uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.WorkingCopyChanged,this._onWorkingCopyChanged,this);this._uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.WorkingCopyCommitted,this._onWorkingCopyCommitted,this);this._updateStyle();}
WebInspector.UISourceCodeFrame.prototype={_enableAutocompletionIfNeeded:function()
{this.textEditor.setCompletionDictionary(WebInspector.settings.textEditorAutocompletion.get()?new WebInspector.SampleCompletionDictionary():null);},wasShown:function()
{WebInspector.SourceFrame.prototype.wasShown.call(this);this._boundWindowFocused=this._windowFocused.bind(this);window.addEventListener("focus",this._boundWindowFocused,false);this._checkContentUpdated();},willHide:function()
{WebInspector.SourceFrame.prototype.willHide.call(this);window.removeEventListener("focus",this._boundWindowFocused,false);delete this._boundWindowFocused;this._uiSourceCode.removeWorkingCopyGetter();},canEditSource:function()
{return this._uiSourceCode.isEditable();},_windowFocused:function(event)
{this._checkContentUpdated();},_checkContentUpdated:function()
{if(!this.loaded||!this.isShowing())
return;this._uiSourceCode.checkContentUpdated();},commitEditing:function(text)
{if(!this._uiSourceCode.isDirty())
return;this._muteSourceCodeEvents=true;this._uiSourceCode.commitWorkingCopy(this._didEditContent.bind(this));delete this._muteSourceCodeEvents;},onTextChanged:function(oldRange,newRange)
{WebInspector.SourceFrame.prototype.onTextChanged.call(this,oldRange,newRange);if(this._isSettingContent)
return;this._muteSourceCodeEvents=true;if(this._textEditor.isClean())
this._uiSourceCode.resetWorkingCopy();else
this._uiSourceCode.setWorkingCopyGetter(this._textEditor.text.bind(this._textEditor));delete this._muteSourceCodeEvents;},_didEditContent:function(error)
{if(error){WebInspector.log(error,WebInspector.ConsoleMessage.MessageLevel.Error,true);return;}},_onFormattedChanged:function(event)
{var content=(event.data.content);this._textEditor.setReadOnly(this._uiSourceCode.formatted());var selection=this._textEditor.selection();this._innerSetContent(content);var start=null;var end=null;if(this._uiSourceCode.formatted()){start=event.data.newFormatter.originalToFormatted(selection.startLine,selection.startColumn);end=event.data.newFormatter.originalToFormatted(selection.endLine,selection.endColumn);}else{start=event.data.oldFormatter.formattedToOriginal(selection.startLine,selection.startColumn);end=event.data.oldFormatter.formattedToOriginal(selection.endLine,selection.endColumn);}
this.textEditor.setSelection(new WebInspector.TextRange(start[0],start[1],end[0],end[1]));this.textEditor.revealLine(start[0]);},_onWorkingCopyChanged:function(event)
{if(this._muteSourceCodeEvents)
return;this._innerSetContent(this._uiSourceCode.workingCopy());this.onUISourceCodeContentChanged();},_onWorkingCopyCommitted:function(event)
{if(!this._muteSourceCodeEvents){this._innerSetContent(this._uiSourceCode.workingCopy());this.onUISourceCodeContentChanged();}
this._textEditor.markClean();this._updateStyle();},_updateStyle:function()
{this.element.enableStyleClass("source-frame-unsaved-committed-changes",this._uiSourceCode.hasUnsavedCommittedChanges());},onUISourceCodeContentChanged:function()
{},_innerSetContent:function(content)
{this._isSettingContent=true;this.setContent(content);delete this._isSettingContent;},populateTextAreaContextMenu:function(contextMenu,lineNumber)
{WebInspector.SourceFrame.prototype.populateTextAreaContextMenu.call(this,contextMenu,lineNumber);contextMenu.appendApplicableItems(this._uiSourceCode);contextMenu.appendSeparator();},dispose:function()
{this.detach();},__proto__:WebInspector.SourceFrame.prototype};WebInspector.JavaScriptSourceFrame=function(scriptsPanel,uiSourceCode)
{this._scriptsPanel=scriptsPanel;this._breakpointManager=WebInspector.breakpointManager;this._uiSourceCode=uiSourceCode;WebInspector.UISourceCodeFrame.call(this,uiSourceCode);if(uiSourceCode.project().type()===WebInspector.projectTypes.Debugger)
this.element.addStyleClass("source-frame-debugger-script");this._popoverHelper=new WebInspector.ObjectPopoverHelper(this.textEditor.element,this._getPopoverAnchor.bind(this),this._resolveObjectForPopover.bind(this),this._onHidePopover.bind(this),true);this.textEditor.element.addEventListener("keydown",this._onKeyDown.bind(this),true);this.textEditor.addEventListener(WebInspector.TextEditor.Events.GutterClick,this._handleGutterClick.bind(this),this);this.textEditor.element.addEventListener("mousedown",this._onMouseDownAndClick.bind(this,true),true);this.textEditor.element.addEventListener("click",this._onMouseDownAndClick.bind(this,false),true);this._breakpointManager.addEventListener(WebInspector.BreakpointManager.Events.BreakpointAdded,this._breakpointAdded,this);this._breakpointManager.addEventListener(WebInspector.BreakpointManager.Events.BreakpointRemoved,this._breakpointRemoved,this);this._uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.ConsoleMessageAdded,this._consoleMessageAdded,this);this._uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.ConsoleMessageRemoved,this._consoleMessageRemoved,this);this._uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.ConsoleMessagesCleared,this._consoleMessagesCleared,this);this._uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.SourceMappingChanged,this._onSourceMappingChanged,this);this._uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.WorkingCopyChanged,this._workingCopyChanged,this);this._uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.WorkingCopyCommitted,this._workingCopyCommitted,this);this._registerShortcuts();this._updateScriptFile();}
WebInspector.JavaScriptSourceFrame.prototype={_registerShortcuts:function()
{var shortcutKeys=WebInspector.SourcesPanelDescriptor.ShortcutKeys;for(var i=0;i<shortcutKeys.EvaluateSelectionInConsole.length;++i){var keyDescriptor=shortcutKeys.EvaluateSelectionInConsole[i];this.addShortcut(keyDescriptor.key,this._evaluateSelectionInConsole.bind(this));}
for(var i=0;i<shortcutKeys.AddSelectionToWatch.length;++i){var keyDescriptor=shortcutKeys.AddSelectionToWatch[i];this.addShortcut(keyDescriptor.key,this._addCurrentSelectionToWatch.bind(this));}},_addCurrentSelectionToWatch:function()
{var textSelection=this.textEditor.selection();if(textSelection&&!textSelection.isEmpty())
this._innerAddToWatch(this.textEditor.copyRange(textSelection));},_innerAddToWatch:function(expression)
{this._scriptsPanel.addToWatch(expression);},_evaluateSelectionInConsole:function(event)
{var selection=this.textEditor.selection();if(!selection||selection.isEmpty())
return false;WebInspector.evaluateInConsole(this.textEditor.copyRange(selection));return true;},wasShown:function()
{WebInspector.UISourceCodeFrame.prototype.wasShown.call(this);},willHide:function()
{WebInspector.UISourceCodeFrame.prototype.willHide.call(this);this._popoverHelper.hidePopover();},onUISourceCodeContentChanged:function()
{this._removeAllBreakpoints();WebInspector.UISourceCodeFrame.prototype.onUISourceCodeContentChanged.call(this);},populateLineGutterContextMenu:function(contextMenu,lineNumber)
{contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Continue to here":"Continue to Here"),this._continueToLine.bind(this,lineNumber));var breakpoint=this._breakpointManager.findBreakpoint(this._uiSourceCode,lineNumber);if(!breakpoint){contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Add breakpoint":"Add Breakpoint"),this._setBreakpoint.bind(this,lineNumber,"",true));contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Add conditional breakpoint…":"Add Conditional Breakpoint…"),this._editBreakpointCondition.bind(this,lineNumber));}else{contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Remove breakpoint":"Remove Breakpoint"),breakpoint.remove.bind(breakpoint));contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Edit breakpoint…":"Edit Breakpoint…"),this._editBreakpointCondition.bind(this,lineNumber,breakpoint));if(breakpoint.enabled())
contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Disable breakpoint":"Disable Breakpoint"),breakpoint.setEnabled.bind(breakpoint,false));else
contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Enable breakpoint":"Enable Breakpoint"),breakpoint.setEnabled.bind(breakpoint,true));}},populateTextAreaContextMenu:function(contextMenu,lineNumber)
{var textSelection=this.textEditor.selection();if(textSelection&&!textSelection.isEmpty()){var selection=this.textEditor.copyRange(textSelection);var addToWatchLabel=WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Add to watch":"Add to Watch");contextMenu.appendItem(addToWatchLabel,this._innerAddToWatch.bind(this,selection));var evaluateLabel=WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Evaluate in console":"Evaluate in Console");contextMenu.appendItem(evaluateLabel,WebInspector.evaluateInConsole.bind(WebInspector,selection));contextMenu.appendSeparator();}else if(!this._uiSourceCode.isEditable()&&this._uiSourceCode.contentType()===WebInspector.resourceTypes.Script){function liveEdit(event)
{var liveEditUISourceCode=WebInspector.liveEditSupport.uiSourceCodeForLiveEdit(this._uiSourceCode);this._scriptsPanel.showUISourceCode(liveEditUISourceCode,lineNumber)}
var liveEditLabel=WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Live edit":"Live Edit");contextMenu.appendItem(liveEditLabel,liveEdit.bind(this));contextMenu.appendSeparator();}
WebInspector.UISourceCodeFrame.prototype.populateTextAreaContextMenu.call(this,contextMenu,lineNumber);},_workingCopyChanged:function(event)
{if(this._supportsEnabledBreakpointsWhileEditing()||this._scriptFile)
return;if(this._uiSourceCode.isDirty())
this._muteBreakpointsWhileEditing();else
this._restoreBreakpointsAfterEditing();},_workingCopyCommitted:function(event)
{if(this._supportsEnabledBreakpointsWhileEditing()||this._scriptFile)
return;this._restoreBreakpointsAfterEditing();},_didMergeToVM:function()
{if(this._supportsEnabledBreakpointsWhileEditing())
return;this._restoreBreakpointsAfterEditing();},_didDivergeFromVM:function()
{if(this._supportsEnabledBreakpointsWhileEditing())
return;this._muteBreakpointsWhileEditing();},_muteBreakpointsWhileEditing:function()
{if(this._muted)
return;for(var lineNumber=0;lineNumber<this._textEditor.linesCount;++lineNumber){var breakpointDecoration=this._textEditor.getAttribute(lineNumber,"breakpoint");if(!breakpointDecoration)
continue;this._removeBreakpointDecoration(lineNumber);this._addBreakpointDecoration(lineNumber,breakpointDecoration.condition,breakpointDecoration.enabled,true);}
this._muted=true;},_supportsEnabledBreakpointsWhileEditing:function()
{return this._uiSourceCode.project().type()===WebInspector.projectTypes.Snippets;},_restoreBreakpointsAfterEditing:function()
{delete this._muted;var breakpoints={};for(var lineNumber=0;lineNumber<this._textEditor.linesCount;++lineNumber){var breakpointDecoration=this._textEditor.getAttribute(lineNumber,"breakpoint");if(breakpointDecoration){breakpoints[lineNumber]=breakpointDecoration;this._removeBreakpointDecoration(lineNumber);}}
this._removeAllBreakpoints();for(var lineNumberString in breakpoints){var lineNumber=parseInt(lineNumberString,10);if(isNaN(lineNumber))
continue;var breakpointDecoration=breakpoints[lineNumberString];this._setBreakpoint(lineNumber,breakpointDecoration.condition,breakpointDecoration.enabled);}},_removeAllBreakpoints:function()
{var breakpoints=this._breakpointManager.breakpointsForUISourceCode(this._uiSourceCode);for(var i=0;i<breakpoints.length;++i)
breakpoints[i].remove();},_getPopoverAnchor:function(element,event)
{if(!WebInspector.debuggerModel.isPaused())
return null;var textPosition=this.textEditor.coordinatesToCursorPosition(event.x,event.y);if(!textPosition)
return null;var mouseLine=textPosition.startLine;var mouseColumn=textPosition.startColumn;var textSelection=this.textEditor.selection().normalize();if(textSelection&&!textSelection.isEmpty()){if(textSelection.startLine!==textSelection.endLine||textSelection.startLine!==mouseLine||mouseColumn<textSelection.startColumn||mouseColumn>textSelection.endColumn)
return null;var leftCorner=this.textEditor.cursorPositionToCoordinates(textSelection.startLine,textSelection.startColumn);var rightCorner=this.textEditor.cursorPositionToCoordinates(textSelection.endLine,textSelection.endColumn);var anchorBox=new AnchorBox(leftCorner.x,leftCorner.y,rightCorner.x-leftCorner.x,leftCorner.height);anchorBox.highlight={lineNumber:textSelection.startLine,startColumn:textSelection.startColumn,endColumn:textSelection.endColumn-1};anchorBox.forSelection=true;return anchorBox;}
var token=this.textEditor.tokenAtTextPosition(textPosition.startLine,textPosition.startColumn);if(!token)
return null;var lineNumber=textPosition.startLine;var line=this.textEditor.line(lineNumber);var tokenContent=line.substring(token.startColumn,token.endColumn+1);if(token.type!=="javascript-ident"&&(token.type!=="javascript-keyword"||tokenContent!=="this"))
return null;var leftCorner=this.textEditor.cursorPositionToCoordinates(lineNumber,token.startColumn);var rightCorner=this.textEditor.cursorPositionToCoordinates(lineNumber,token.endColumn+1);var anchorBox=new AnchorBox(leftCorner.x,leftCorner.y,rightCorner.x-leftCorner.x,leftCorner.height);anchorBox.highlight={lineNumber:lineNumber,startColumn:token.startColumn,endColumn:token.endColumn};return anchorBox;},_resolveObjectForPopover:function(anchorBox,showCallback,objectGroupName)
{function showObjectPopover(result,wasThrown)
{if(!WebInspector.debuggerModel.isPaused()){this._popoverHelper.hidePopover();return;}
this._popoverAnchorBox=anchorBox;showCallback(WebInspector.RemoteObject.fromPayload(result),wasThrown,this._popoverAnchorBox);if(this._popoverAnchorBox){var highlightRange=new WebInspector.TextRange(lineNumber,startHighlight,lineNumber,endHighlight);this._popoverAnchorBox._highlightDescriptor=this.textEditor.highlightRange(highlightRange,"source-frame-eval-expression");}}
if(!WebInspector.debuggerModel.isPaused()){this._popoverHelper.hidePopover();return;}
var lineNumber=anchorBox.highlight.lineNumber;var startHighlight=anchorBox.highlight.startColumn;var endHighlight=anchorBox.highlight.endColumn;var line=this.textEditor.line(lineNumber);if(!anchorBox.forSelection){while(startHighlight>1&&line.charAt(startHighlight-1)==='.')
startHighlight=this.textEditor.tokenAtTextPosition(lineNumber,startHighlight-2).startColumn;}
var evaluationText=line.substring(startHighlight,endHighlight+1);var selectedCallFrame=WebInspector.debuggerModel.selectedCallFrame();selectedCallFrame.evaluate(evaluationText,objectGroupName,false,true,false,false,showObjectPopover.bind(this));},_onHidePopover:function()
{if(!this._popoverAnchorBox)
return;if(this._popoverAnchorBox._highlightDescriptor)
this.textEditor.removeHighlight(this._popoverAnchorBox._highlightDescriptor);delete this._popoverAnchorBox;},_addBreakpointDecoration:function(lineNumber,condition,enabled,mutedWhileEditing)
{var breakpoint={condition:condition,enabled:enabled};this.textEditor.setAttribute(lineNumber,"breakpoint",breakpoint);var disabled=!enabled||mutedWhileEditing;this.textEditor.addBreakpoint(lineNumber,disabled,!!condition);},_removeBreakpointDecoration:function(lineNumber)
{this.textEditor.removeAttribute(lineNumber,"breakpoint");this.textEditor.removeBreakpoint(lineNumber);},_onKeyDown:function(event)
{if(event.keyIdentifier==="U+001B"){if(this._popoverHelper.isPopoverVisible()){this._popoverHelper.hidePopover();event.consume();return;}
if(this._stepIntoMarkup&&WebInspector.KeyboardShortcut.eventHasCtrlOrMeta(event)){this._stepIntoMarkup.stoptIteratingSelection();event.consume();return;}}},_editBreakpointCondition:function(lineNumber,breakpoint)
{this._conditionElement=this._createConditionElement(lineNumber);this.textEditor.addDecoration(lineNumber,this._conditionElement);function finishEditing(committed,element,newText)
{this.textEditor.removeDecoration(lineNumber,this._conditionElement);delete this._conditionEditorElement;delete this._conditionElement;if(!committed)
return;if(breakpoint)
breakpoint.setCondition(newText);else
this._setBreakpoint(lineNumber,newText,true);}
var config=new WebInspector.EditingConfig(finishEditing.bind(this,true),finishEditing.bind(this,false));WebInspector.startEditing(this._conditionEditorElement,config);this._conditionEditorElement.value=breakpoint?breakpoint.condition():"";this._conditionEditorElement.select();},_createConditionElement:function(lineNumber)
{var conditionElement=document.createElement("div");conditionElement.className="source-frame-breakpoint-condition";var labelElement=document.createElement("label");labelElement.className="source-frame-breakpoint-message";labelElement.htmlFor="source-frame-breakpoint-condition";labelElement.appendChild(document.createTextNode(WebInspector.UIString("The breakpoint on line %d will stop only if this expression is true:",lineNumber)));conditionElement.appendChild(labelElement);var editorElement=document.createElement("input");editorElement.id="source-frame-breakpoint-condition";editorElement.className="monospace";editorElement.type="text";conditionElement.appendChild(editorElement);this._conditionEditorElement=editorElement;return conditionElement;},setExecutionLine:function(lineNumber,callFrame)
{this._executionLineNumber=lineNumber;this._executionCallFrame=callFrame;if(this.loaded){this.textEditor.setExecutionLine(lineNumber);if(WebInspector.experimentsSettings.stepIntoSelection.isEnabled()){function locationsCallback(locations)
{if(this._executionCallFrame!==callFrame||this._stepIntoMarkup)
return;this._stepIntoMarkup=WebInspector.JavaScriptSourceFrame.StepIntoMarkup.create(this,locations);if(this._stepIntoMarkup)
this._stepIntoMarkup.show();}
callFrame.getStepIntoLocations(locationsCallback.bind(this));}}},clearExecutionLine:function()
{if(this._stepIntoMarkup){this._stepIntoMarkup.dispose();delete this._stepIntoMarkup;}
if(this.loaded&&typeof this._executionLineNumber==="number")
this.textEditor.clearExecutionLine();delete this._executionLineNumber;delete this._executionCallFrame;},_lineNumberAfterEditing:function(lineNumber,oldRange,newRange)
{var shiftOffset=lineNumber<=oldRange.startLine?0:newRange.linesCount-oldRange.linesCount;if(lineNumber===oldRange.startLine){var whiteSpacesRegex=/^[\s\xA0]*$/;for(var i=0;lineNumber+i<=newRange.endLine;++i){if(!whiteSpacesRegex.test(this.textEditor.line(lineNumber+i))){shiftOffset=i;break;}}}
var newLineNumber=Math.max(0,lineNumber+shiftOffset);if(oldRange.startLine<lineNumber&&lineNumber<oldRange.endLine)
newLineNumber=oldRange.startLine;return newLineNumber;},_onMouseDownAndClick:function(isMouseDown,event)
{var markup=this._stepIntoMarkup;if(!markup)
return;var index=markup.findItemByCoordinates(event.x,event.y);if(typeof index==="undefined")
return;if(isMouseDown){event.consume();}else{var rawLocation=markup.getRawPosition(index);this._scriptsPanel.doStepIntoSelection(rawLocation);}},_shouldIgnoreExternalBreakpointEvents:function()
{if(this._supportsEnabledBreakpointsWhileEditing())
return false;if(this._muted)
return true;return this._scriptFile&&(this._scriptFile.isDivergingFromVM()||this._scriptFile.isMergingToVM());},_breakpointAdded:function(event)
{var uiLocation=(event.data.uiLocation);if(uiLocation.uiSourceCode!==this._uiSourceCode)
return;if(this._shouldIgnoreExternalBreakpointEvents())
return;var breakpoint=(event.data.breakpoint);if(this.loaded)
this._addBreakpointDecoration(uiLocation.lineNumber,breakpoint.condition(),breakpoint.enabled(),false);},_breakpointRemoved:function(event)
{var uiLocation=(event.data.uiLocation);if(uiLocation.uiSourceCode!==this._uiSourceCode)
return;if(this._shouldIgnoreExternalBreakpointEvents())
return;var breakpoint=(event.data.breakpoint);var remainingBreakpoint=this._breakpointManager.findBreakpoint(this._uiSourceCode,uiLocation.lineNumber);if(!remainingBreakpoint&&this.loaded)
this._removeBreakpointDecoration(uiLocation.lineNumber);},_consoleMessageAdded:function(event)
{var message=(event.data);if(this.loaded)
this.addMessageToSource(message.lineNumber,message.originalMessage);},_consoleMessageRemoved:function(event)
{var message=(event.data);if(this.loaded)
this.removeMessageFromSource(message.lineNumber,message.originalMessage);},_consoleMessagesCleared:function(event)
{this.clearMessages();},_onSourceMappingChanged:function(event)
{this._updateScriptFile();},_updateScriptFile:function()
{if(this._scriptFile){this._scriptFile.removeEventListener(WebInspector.ScriptFile.Events.DidMergeToVM,this._didMergeToVM,this);this._scriptFile.removeEventListener(WebInspector.ScriptFile.Events.DidDivergeFromVM,this._didDivergeFromVM,this);if(this._muted&&!this._uiSourceCode.isDirty())
this._restoreBreakpointsAfterEditing();}
this._scriptFile=this._uiSourceCode.scriptFile();if(this._scriptFile){this._scriptFile.addEventListener(WebInspector.ScriptFile.Events.DidMergeToVM,this._didMergeToVM,this);this._scriptFile.addEventListener(WebInspector.ScriptFile.Events.DidDivergeFromVM,this._didDivergeFromVM,this);if(this.loaded)
this._scriptFile.checkMapping();}},onTextEditorContentLoaded:function()
{if(typeof this._executionLineNumber==="number")
this.setExecutionLine(this._executionLineNumber,this._executionCallFrame);var breakpointLocations=this._breakpointManager.breakpointLocationsForUISourceCode(this._uiSourceCode);for(var i=0;i<breakpointLocations.length;++i)
this._breakpointAdded({data:breakpointLocations[i]});var messages=this._uiSourceCode.consoleMessages();for(var i=0;i<messages.length;++i){var message=messages[i];this.addMessageToSource(message.lineNumber,message.originalMessage);}
if(this._scriptFile)
this._scriptFile.checkMapping();},_handleGutterClick:function(event)
{if(this._muted)
return;var eventData=(event.data);var lineNumber=eventData.lineNumber;var eventObject=(eventData.event);if(eventObject.button!=0||eventObject.altKey||eventObject.ctrlKey||eventObject.metaKey)
return;this._toggleBreakpoint(lineNumber,eventObject.shiftKey);eventObject.consume(true);},_toggleBreakpoint:function(lineNumber,onlyDisable)
{var breakpoint=this._breakpointManager.findBreakpoint(this._uiSourceCode,lineNumber);if(breakpoint){if(onlyDisable)
breakpoint.setEnabled(!breakpoint.enabled());else
breakpoint.remove();}else
this._setBreakpoint(lineNumber,"",true);},toggleBreakpointOnCurrentLine:function()
{if(this._muted)
return;var selection=this.textEditor.selection();if(!selection)
return;this._toggleBreakpoint(selection.startLine,false);},_setBreakpoint:function(lineNumber,condition,enabled)
{this._breakpointManager.setBreakpoint(this._uiSourceCode,lineNumber,condition,enabled);WebInspector.notifications.dispatchEventToListeners(WebInspector.UserMetrics.UserAction,{action:WebInspector.UserMetrics.UserActionNames.SetBreakpoint,url:this._uiSourceCode.originURL(),line:lineNumber,enabled:enabled});},_continueToLine:function(lineNumber)
{var rawLocation=(this._uiSourceCode.uiLocationToRawLocation(lineNumber,0));this._scriptsPanel.continueToLocation(rawLocation);},stepIntoMarkup:function()
{return this._stepIntoMarkup;},dispose:function()
{this._breakpointManager.removeEventListener(WebInspector.BreakpointManager.Events.BreakpointAdded,this._breakpointAdded,this);this._breakpointManager.removeEventListener(WebInspector.BreakpointManager.Events.BreakpointRemoved,this._breakpointRemoved,this);this._uiSourceCode.removeEventListener(WebInspector.UISourceCode.Events.ConsoleMessageAdded,this._consoleMessageAdded,this);this._uiSourceCode.removeEventListener(WebInspector.UISourceCode.Events.ConsoleMessageRemoved,this._consoleMessageRemoved,this);this._uiSourceCode.removeEventListener(WebInspector.UISourceCode.Events.ConsoleMessagesCleared,this._consoleMessagesCleared,this);this._uiSourceCode.removeEventListener(WebInspector.UISourceCode.Events.SourceMappingChanged,this._onSourceMappingChanged,this);this._uiSourceCode.removeEventListener(WebInspector.UISourceCode.Events.WorkingCopyChanged,this._workingCopyChanged,this);this._uiSourceCode.removeEventListener(WebInspector.UISourceCode.Events.WorkingCopyCommitted,this._workingCopyCommitted,this);WebInspector.UISourceCodeFrame.prototype.dispose.call(this);},__proto__:WebInspector.UISourceCodeFrame.prototype}
WebInspector.JavaScriptSourceFrame.StepIntoMarkup=function(rawPositions,editorRanges,firstToExecute,sourceFrame)
{this._positions=rawPositions;this._editorRanges=editorRanges;this._highlightDescriptors=new Array(rawPositions.length);this._currentHighlight=undefined;this._firstToExecute=firstToExecute;this._currentSelection=undefined;this._sourceFrame=sourceFrame;};WebInspector.JavaScriptSourceFrame.StepIntoMarkup.prototype={show:function()
{var highlight=this._getVisibleHighlight();for(var i=0;i<this._positions.length;++i)
this._highlightItem(i,i===highlight);this._shownVisibleHighlight=highlight;},startIteratingSelection:function()
{this._currentSelection=this._positions.length
this._redrawHighlight();},stopIteratingSelection:function()
{this._currentSelection=undefined;this._redrawHighlight();},iterateSelection:function(backward)
{if(typeof this._currentSelection==="undefined")
return;var nextSelection=backward?this._currentSelection-1:this._currentSelection+1;var modulo=this._positions.length+1;nextSelection=(nextSelection+modulo)%modulo;this._currentSelection=nextSelection;this._redrawHighlight();},_redrawHighlight:function()
{var visibleHighlight=this._getVisibleHighlight();if(this._shownVisibleHighlight===visibleHighlight)
return;this._hideItemHighlight(this._shownVisibleHighlight);this._hideItemHighlight(visibleHighlight);this._highlightItem(this._shownVisibleHighlight,false);this._highlightItem(visibleHighlight,true);this._shownVisibleHighlight=visibleHighlight;},_getVisibleHighlight:function()
{return typeof this._currentSelection==="undefined"?this._firstToExecute:this._currentSelection;},_highlightItem:function(position,selected)
{if(position===this._positions.length)
return;var styleName=selected?"source-frame-stepin-mark-highlighted":"source-frame-stepin-mark";var textEditor=this._sourceFrame.textEditor;var highlightDescriptor=textEditor.highlightRange(this._editorRanges[position],styleName);this._highlightDescriptors[position]=highlightDescriptor;},_hideItemHighlight:function(position)
{if(position===this._positions.length)
return;var highlightDescriptor=this._highlightDescriptors[position];console.assert(highlightDescriptor);var textEditor=this._sourceFrame.textEditor;textEditor.removeHighlight(highlightDescriptor);this._highlightDescriptors[position]=undefined;},dispose:function()
{for(var i=0;i<this._positions.length;++i)
this._hideItemHighlight(i);},findItemByCoordinates:function(x,y)
{var textPosition=this._sourceFrame.textEditor.coordinatesToCursorPosition(x,y);if(!textPosition)
return;var ranges=this._editorRanges;for(var i=0;i<ranges.length;++i){var nextRange=ranges[i];if(nextRange.startLine==textPosition.startLine&&nextRange.startColumn<=textPosition.startColumn&&nextRange.endColumn>=textPosition.startColumn)
return i;}},getSelectedItemIndex:function()
{if(this._currentSelection===this._positions.length)
return undefined;return this._currentSelection;},getRawPosition:function(position)
{return(this._positions[position]);}};WebInspector.JavaScriptSourceFrame.StepIntoMarkup.create=function(sourceFrame,stepIntoRawLocations)
{if(!stepIntoRawLocations.length)
return null;var firstToExecute=stepIntoRawLocations[0];stepIntoRawLocations.sort(WebInspector.JavaScriptSourceFrame.StepIntoMarkup._Comparator);var firstToExecuteIndex=stepIntoRawLocations.indexOf(firstToExecute);var textEditor=sourceFrame.textEditor;var uiRanges=[];for(var i=0;i<stepIntoRawLocations.length;++i){var uiLocation=WebInspector.debuggerModel.rawLocationToUILocation((stepIntoRawLocations[i]));var token=textEditor.tokenAtTextPosition(uiLocation.lineNumber,uiLocation.columnNumber);var startColumn;var endColumn;if(token){startColumn=token.startColumn;endColumn=token.endColumn;}else{startColumn=uiLocation.columnNumber;endColumn=uiLocation.columnNumber;}
var range=new WebInspector.TextRange(uiLocation.lineNumber,startColumn,uiLocation.lineNumber,endColumn);uiRanges.push(range);}
return new WebInspector.JavaScriptSourceFrame.StepIntoMarkup(stepIntoRawLocations,uiRanges,firstToExecuteIndex,sourceFrame);};WebInspector.JavaScriptSourceFrame.StepIntoMarkup._Comparator=function(locationA,locationB)
{if(locationA.lineNumber===locationB.lineNumber)
return locationA.columnNumber-locationB.columnNumber;else
return locationA.lineNumber-locationB.lineNumber;};;WebInspector.CSSSourceFrame=function(uiSourceCode)
{WebInspector.UISourceCodeFrame.call(this,uiSourceCode);this._registerShortcuts();}
WebInspector.CSSSourceFrame.prototype={_registerShortcuts:function()
{var shortcutKeys=WebInspector.SourcesPanelDescriptor.ShortcutKeys;for(var i=0;i<shortcutKeys.IncreaseCSSUnitByOne.length;++i)
this.addShortcut(shortcutKeys.IncreaseCSSUnitByOne[i].key,this._handleUnitModification.bind(this,1));for(var i=0;i<shortcutKeys.DecreaseCSSUnitByOne.length;++i)
this.addShortcut(shortcutKeys.DecreaseCSSUnitByOne[i].key,this._handleUnitModification.bind(this,-1));for(var i=0;i<shortcutKeys.IncreaseCSSUnitByTen.length;++i)
this.addShortcut(shortcutKeys.IncreaseCSSUnitByTen[i].key,this._handleUnitModification.bind(this,10));for(var i=0;i<shortcutKeys.DecreaseCSSUnitByTen.length;++i)
this.addShortcut(shortcutKeys.DecreaseCSSUnitByTen[i].key,this._handleUnitModification.bind(this,-10));},_modifyUnit:function(unit,change)
{var unitValue=parseInt(unit,10);if(isNaN(unitValue))
return null;var tail=unit.substring((unitValue).toString().length);return String.sprintf("%d%s",unitValue+change,tail);},_handleUnitModification:function(change)
{var selection=this.textEditor.selection().normalize();var token=this.textEditor.tokenAtTextPosition(selection.startLine,selection.startColumn);if(!token){if(selection.startColumn>0)
token=this.textEditor.tokenAtTextPosition(selection.startLine,selection.startColumn-1);if(!token)
return false;}
if(token.type!=="css-number")
return false;var cssUnitRange=new WebInspector.TextRange(selection.startLine,token.startColumn,selection.startLine,token.endColumn+1);var cssUnitText=this.textEditor.copyRange(cssUnitRange);var newUnitText=this._modifyUnit(cssUnitText,change);if(!newUnitText)
return false;this.textEditor.editRange(cssUnitRange,newUnitText);selection.startColumn=token.startColumn;selection.endColumn=selection.startColumn+newUnitText.length;this.textEditor.setSelection(selection);return true;},__proto__:WebInspector.UISourceCodeFrame.prototype};WebInspector.NavigatorOverlayController=function(parentSidebarView,navigatorView,editorView)
{this._parentSidebarView=parentSidebarView;this._navigatorView=navigatorView;this._editorView=editorView;this._navigatorSidebarResizeWidgetElement=this._navigatorView.element.createChild("div","resizer-widget");this._parentSidebarView.installResizer(this._navigatorSidebarResizeWidgetElement);this._navigatorShowHideButton=new WebInspector.StatusBarButton(WebInspector.UIString("Hide navigator"),"left-sidebar-show-hide-button scripts-navigator-show-hide-button",3);this._navigatorShowHideButton.state="left";this._navigatorShowHideButton.addEventListener("click",this._toggleNavigator,this);parentSidebarView.mainElement.appendChild(this._navigatorShowHideButton.element);WebInspector.settings.navigatorHidden=WebInspector.settings.createSetting("navigatorHidden",true);if(WebInspector.settings.navigatorHidden.get())
this._toggleNavigator();}
WebInspector.NavigatorOverlayController.prototype={wasShown:function()
{window.setTimeout(this._maybeShowNavigatorOverlay.bind(this),0);},_maybeShowNavigatorOverlay:function()
{if(WebInspector.settings.navigatorHidden.get()&&!WebInspector.settings.navigatorWasOnceHidden.get())
this.showNavigatorOverlay();},_toggleNavigator:function()
{if(this._navigatorShowHideButton.state==="overlay")
this._pinNavigator();else if(this._navigatorShowHideButton.state==="right")
this.showNavigatorOverlay();else
this._hidePinnedNavigator();},_hidePinnedNavigator:function()
{this._navigatorShowHideButton.state="right";this._navigatorShowHideButton.title=WebInspector.UIString("Show navigator");this._parentSidebarView.element.appendChild(this._navigatorShowHideButton.element);this._editorView.element.addStyleClass("navigator-hidden");this._navigatorSidebarResizeWidgetElement.addStyleClass("hidden");this._parentSidebarView.hideSidebarElement();this._navigatorView.detach();this._editorView.focus();WebInspector.settings.navigatorWasOnceHidden.set(true);WebInspector.settings.navigatorHidden.set(true);},_pinNavigator:function()
{this._navigatorShowHideButton.state="left";this._navigatorShowHideButton.title=WebInspector.UIString("Hide navigator");this._editorView.element.removeStyleClass("navigator-hidden");this._navigatorSidebarResizeWidgetElement.removeStyleClass("hidden");this._editorView.element.appendChild(this._navigatorShowHideButton.element);this._innerHideNavigatorOverlay();this._parentSidebarView.showSidebarElement();this._navigatorView.show(this._parentSidebarView.sidebarElement);this._navigatorView.focus();WebInspector.settings.navigatorHidden.set(false);},showNavigatorOverlay:function()
{if(this._navigatorShowHideButton.state==="overlay")
return;this._navigatorShowHideButton.state="overlay";this._navigatorShowHideButton.title=WebInspector.UIString("Pin navigator");this._sidebarOverlay=new WebInspector.SidebarOverlay(this._navigatorView,"scriptsPanelNavigatorOverlayWidth",Preferences.minScriptsSidebarWidth);this._boundKeyDown=this._keyDown.bind(this);this._sidebarOverlay.element.addEventListener("keydown",this._boundKeyDown,false);var navigatorOverlayResizeWidgetElement=document.createElement("div");navigatorOverlayResizeWidgetElement.addStyleClass("resizer-widget");this._sidebarOverlay.resizerWidgetElement=navigatorOverlayResizeWidgetElement;this._navigatorView.element.appendChild(this._navigatorShowHideButton.element);this._boundContainingElementFocused=this._containingElementFocused.bind(this);this._parentSidebarView.element.addEventListener("mousedown",this._boundContainingElementFocused,false);this._sidebarOverlay.show(this._parentSidebarView.element);this._navigatorView.focus();},_keyDown:function(event)
{if(event.handled)
return;if(event.keyCode===WebInspector.KeyboardShortcut.Keys.Esc.code){this.hideNavigatorOverlay();event.consume(true);}},hideNavigatorOverlay:function()
{if(this._navigatorShowHideButton.state!=="overlay")
return;this._navigatorShowHideButton.state="right";this._navigatorShowHideButton.title=WebInspector.UIString("Show navigator");this._parentSidebarView.element.appendChild(this._navigatorShowHideButton.element);this._innerHideNavigatorOverlay();this._editorView.focus();},_innerHideNavigatorOverlay:function()
{this._parentSidebarView.element.removeEventListener("mousedown",this._boundContainingElementFocused,false);this._sidebarOverlay.element.removeEventListener("keydown",this._boundKeyDown,false);this._sidebarOverlay.hide();},_containingElementFocused:function(event)
{if(!event.target.isSelfOrDescendant(this._sidebarOverlay.element))
this.hideNavigatorOverlay();},isNavigatorPinned:function()
{return this._navigatorShowHideButton.state==="left";},isNavigatorHidden:function()
{return this._navigatorShowHideButton.state==="right";}};WebInspector.NavigatorView=function()
{WebInspector.View.call(this);this.registerRequiredCSS("navigatorView.css");var scriptsTreeElement=document.createElement("ol");this._scriptsTree=new WebInspector.NavigatorTreeOutline(scriptsTreeElement);this._scriptsTree.childrenListElement.addEventListener("keypress",this._treeKeyPress.bind(this),true);var scriptsOutlineElement=document.createElement("div");scriptsOutlineElement.addStyleClass("outline-disclosure");scriptsOutlineElement.addStyleClass("navigator");scriptsOutlineElement.appendChild(scriptsTreeElement);this.element.addStyleClass("fill");this.element.addStyleClass("navigator-container");this.element.appendChild(scriptsOutlineElement);this.setDefaultFocusedElement(this._scriptsTree.element);this._uiSourceCodeNodes=new Map();this._subfolderNodes=new Map();this._rootNode=new WebInspector.NavigatorRootTreeNode(this);this._rootNode.populate();WebInspector.resourceTreeModel.addEventListener(WebInspector.ResourceTreeModel.EventTypes.InspectedURLChanged,this._inspectedURLChanged,this);this.element.addEventListener("contextmenu",this.handleContextMenu.bind(this),false);}
WebInspector.NavigatorView.Events={ItemSelected:"ItemSelected",ItemSearchStarted:"ItemSearchStarted",ItemRenamingRequested:"ItemRenamingRequested",ItemCreationRequested:"ItemCreationRequested"}
WebInspector.NavigatorView.iconClassForType=function(type)
{if(type===WebInspector.NavigatorTreeOutline.Types.Domain)
return"navigator-domain-tree-item";if(type===WebInspector.NavigatorTreeOutline.Types.FileSystem)
return"navigator-folder-tree-item";return"navigator-folder-tree-item";}
WebInspector.NavigatorView.prototype={addUISourceCode:function(uiSourceCode)
{var projectNode=this._projectNode(uiSourceCode.project());var folderNode=this._folderNode(projectNode,uiSourceCode.parentPath());var uiSourceCodeNode=new WebInspector.NavigatorUISourceCodeTreeNode(this,uiSourceCode);this._uiSourceCodeNodes.put(uiSourceCode,uiSourceCodeNode);folderNode.appendChild(uiSourceCodeNode);if(uiSourceCode.url===WebInspector.inspectedPageURL)
this.revealUISourceCode(uiSourceCode);},_inspectedURLChanged:function(event)
{var nodes=this._uiSourceCodeNodes.values();for(var i=0;i<nodes.length;++i){var uiSourceCode=nodes[i].uiSourceCode();if(uiSourceCode.url===WebInspector.inspectedPageURL)
this.revealUISourceCode(uiSourceCode);}},_projectNode:function(project)
{if(!project.displayName())
return this._rootNode;var projectNode=this._rootNode.child(project.id());if(!projectNode){var type=project.type()===WebInspector.projectTypes.FileSystem?WebInspector.NavigatorTreeOutline.Types.FileSystem:WebInspector.NavigatorTreeOutline.Types.Domain;projectNode=new WebInspector.NavigatorFolderTreeNode(this,project,project.id(),type,"",project.displayName());this._rootNode.appendChild(projectNode);}
return projectNode;},_folderNode:function(projectNode,folderPath)
{if(!folderPath)
return projectNode;var subfolderNodes=this._subfolderNodes.get(projectNode);if(!subfolderNodes){subfolderNodes=(new StringMap());this._subfolderNodes.put(projectNode,subfolderNodes);}
var folderNode=subfolderNodes.get(folderPath);if(folderNode)
return folderNode;var parentNode=projectNode;var index=folderPath.lastIndexOf("/");if(index!==-1)
parentNode=this._folderNode(projectNode,folderPath.substring(0,index));var name=folderPath.substring(index+1);folderNode=new WebInspector.NavigatorFolderTreeNode(this,null,name,WebInspector.NavigatorTreeOutline.Types.Folder,folderPath,name);subfolderNodes.put(folderPath,folderNode);parentNode.appendChild(folderNode);return folderNode;},revealUISourceCode:function(uiSourceCode,select)
{var node=this._uiSourceCodeNodes.get(uiSourceCode);if(!node)
return null;if(this._scriptsTree.selectedTreeElement)
this._scriptsTree.selectedTreeElement.deselect();this._lastSelectedUISourceCode=uiSourceCode;node.reveal(select);},_sourceSelected:function(uiSourceCode,focusSource)
{this._lastSelectedUISourceCode=uiSourceCode;var data={uiSourceCode:uiSourceCode,focusSource:focusSource};this.dispatchEventToListeners(WebInspector.NavigatorView.Events.ItemSelected,data);},sourceDeleted:function(uiSourceCode)
{},removeUISourceCode:function(uiSourceCode)
{var node=this._uiSourceCodeNodes.get(uiSourceCode);if(!node)
return;var projectNode=this._projectNode(uiSourceCode.project());var subfolderNodes=this._subfolderNodes.get(projectNode);var parentNode=node.parent;this._uiSourceCodeNodes.remove(uiSourceCode);parentNode.removeChild(node);node=parentNode;while(node){parentNode=node.parent;if(!parentNode||!node.isEmpty())
break;if(subfolderNodes)
subfolderNodes.remove(node._folderPath);parentNode.removeChild(node);node=parentNode;}},updateIcon:function(uiSourceCode)
{var node=this._uiSourceCodeNodes.get(uiSourceCode);node.updateIcon();},requestRename:function(uiSourceCode)
{this.dispatchEventToListeners(WebInspector.SourcesNavigator.Events.ItemRenamingRequested,uiSourceCode);},rename:function(uiSourceCode,callback)
{var node=this._uiSourceCodeNodes.get(uiSourceCode);if(!node)
return null;node.rename(callback);},reset:function()
{var nodes=this._uiSourceCodeNodes.values();for(var i=0;i<nodes.length;++i)
nodes[i].dispose();this._scriptsTree.removeChildren();this._uiSourceCodeNodes.clear();this._subfolderNodes.clear();this._rootNode.reset();},handleContextMenu:function(event)
{var contextMenu=new WebInspector.ContextMenu(event);this._appendAddFolderItem(contextMenu);contextMenu.show();},_appendAddFolderItem:function(contextMenu)
{function addFolder()
{WebInspector.isolatedFileSystemManager.addFileSystem();}
var addFolderLabel=WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Add folder to workspace":"Add Folder to Workspace");contextMenu.appendItem(addFolderLabel,addFolder);},_handleContextMenuRefresh:function(project,path)
{project.refresh(path);},_handleContextMenuCreate:function(project,path,uiSourceCode)
{var data={};data.project=project;data.path=path;data.uiSourceCode=uiSourceCode;this.dispatchEventToListeners(WebInspector.NavigatorView.Events.ItemCreationRequested,data);},_handleContextMenuExclude:function(project,path)
{var shouldExclude=window.confirm(WebInspector.UIString("Are you sure you want to exclude this folder?"));if(shouldExclude){WebInspector.startBatchUpdate();project.excludeFolder(path);WebInspector.endBatchUpdate();}},_handleContextMenuDelete:function(uiSourceCode)
{var shouldDelete=window.confirm(WebInspector.UIString("Are you sure you want to delete this file?"));if(shouldDelete)
uiSourceCode.project().deleteFile(uiSourceCode.path());},handleFileContextMenu:function(event,uiSourceCode)
{var contextMenu=new WebInspector.ContextMenu(event);contextMenu.appendApplicableItems(uiSourceCode);contextMenu.appendSeparator();var project=uiSourceCode.project();var path=uiSourceCode.parentPath();contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Refresh parent":"Refresh Parent"),this._handleContextMenuRefresh.bind(this,project,path));contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Duplicate file":"Duplicate File"),this._handleContextMenuCreate.bind(this,project,path,uiSourceCode));contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Exclude parent folder":"Exclude Parent Folder"),this._handleContextMenuExclude.bind(this,project,path));contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Delete file":"Delete File"),this._handleContextMenuDelete.bind(this,uiSourceCode));contextMenu.appendSeparator();this._appendAddFolderItem(contextMenu);contextMenu.show();},handleFolderContextMenu:function(event,node)
{var contextMenu=new WebInspector.ContextMenu(event);var path="/";var projectNode=node;while(projectNode.parent!==this._rootNode){path="/"+projectNode.id+path;projectNode=projectNode.parent;}
var project=projectNode._project;if(project.type()===WebInspector.projectTypes.FileSystem){contextMenu.appendItem(WebInspector.UIString("Refresh"),this._handleContextMenuRefresh.bind(this,project,path));contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"New file":"New File"),this._handleContextMenuCreate.bind(this,project,path));contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Exclude folder":"Exclude Folder"),this._handleContextMenuExclude.bind(this,project,path));}
contextMenu.appendSeparator();this._appendAddFolderItem(contextMenu);if(project.type()===WebInspector.projectTypes.FileSystem&&node===projectNode){function removeFolder()
{var shouldRemove=window.confirm(WebInspector.UIString("Are you sure you want to remove this folder?"));if(shouldRemove)
project.remove();}
var removeFolderLabel=WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Remove folder from workspace":"Remove Folder from Workspace");contextMenu.appendItem(removeFolderLabel,removeFolder);}
contextMenu.show();},_treeKeyPress:function(event)
{if(WebInspector.isBeingEdited(this._scriptsTree.childrenListElement))
return;var searchText=String.fromCharCode(event.charCode);if(searchText.trim()!==searchText)
return;this.dispatchEventToListeners(WebInspector.NavigatorView.Events.ItemSearchStarted,searchText);event.consume(true);},__proto__:WebInspector.View.prototype}
WebInspector.NavigatorTreeOutline=function(element)
{TreeOutline.call(this,element);this.element=element;this.comparator=WebInspector.NavigatorTreeOutline._treeElementsCompare;}
WebInspector.NavigatorTreeOutline.Types={Root:"Root",Domain:"Domain",Folder:"Folder",UISourceCode:"UISourceCode",FileSystem:"FileSystem"}
WebInspector.NavigatorTreeOutline._treeElementsCompare=function compare(treeElement1,treeElement2)
{function typeWeight(treeElement)
{var type=treeElement.type();if(type===WebInspector.NavigatorTreeOutline.Types.Domain){if(treeElement.titleText===WebInspector.inspectedPageDomain)
return 1;return 2;}
if(type===WebInspector.NavigatorTreeOutline.Types.FileSystem)
return 3;if(type===WebInspector.NavigatorTreeOutline.Types.Folder)
return 4;return 5;}
var typeWeight1=typeWeight(treeElement1);var typeWeight2=typeWeight(treeElement2);var result;if(typeWeight1>typeWeight2)
result=1;else if(typeWeight1<typeWeight2)
result=-1;else{var title1=treeElement1.titleText;var title2=treeElement2.titleText;result=title1.compareTo(title2);}
return result;}
WebInspector.NavigatorTreeOutline.prototype={scriptTreeElements:function()
{var result=[];if(this.children.length){for(var treeElement=this.children[0];treeElement;treeElement=treeElement.traverseNextTreeElement(false,this,true)){if(treeElement instanceof WebInspector.NavigatorSourceTreeElement)
result.push(treeElement.uiSourceCode);}}
return result;},__proto__:TreeOutline.prototype}
WebInspector.BaseNavigatorTreeElement=function(type,title,iconClasses,hasChildren,noIcon)
{this._type=type;TreeElement.call(this,"",null,hasChildren);this._titleText=title;this._iconClasses=iconClasses;this._noIcon=noIcon;}
WebInspector.BaseNavigatorTreeElement.prototype={onattach:function()
{this.listItemElement.removeChildren();if(this._iconClasses){for(var i=0;i<this._iconClasses.length;++i)
this.listItemElement.addStyleClass(this._iconClasses[i]);}
var selectionElement=document.createElement("div");selectionElement.className="selection";this.listItemElement.appendChild(selectionElement);if(!this._noIcon){this.imageElement=document.createElement("img");this.imageElement.className="icon";this.listItemElement.appendChild(this.imageElement);}
this.titleElement=document.createElement("div");this.titleElement.className="base-navigator-tree-element-title";this._titleTextNode=document.createTextNode("");this._titleTextNode.textContent=this._titleText;this.titleElement.appendChild(this._titleTextNode);this.listItemElement.appendChild(this.titleElement);},updateIconClasses:function(iconClasses)
{for(var i=0;i<this._iconClasses.length;++i)
this.listItemElement.removeStyleClass(this._iconClasses[i]);this._iconClasses=iconClasses;for(var i=0;i<this._iconClasses.length;++i)
this.listItemElement.addStyleClass(this._iconClasses[i]);},onreveal:function()
{if(this.listItemElement)
this.listItemElement.scrollIntoViewIfNeeded(true);},get titleText()
{return this._titleText;},set titleText(titleText)
{if(this._titleText===titleText)
return;this._titleText=titleText||"";if(this.titleElement)
this.titleElement.textContent=this._titleText;},type:function()
{return this._type;},__proto__:TreeElement.prototype}
WebInspector.NavigatorFolderTreeElement=function(navigatorView,type,title)
{var iconClass=WebInspector.NavigatorView.iconClassForType(type);WebInspector.BaseNavigatorTreeElement.call(this,type,title,[iconClass],true);this._navigatorView=navigatorView;}
WebInspector.NavigatorFolderTreeElement.prototype={onpopulate:function()
{this._node.populate();},onattach:function()
{WebInspector.BaseNavigatorTreeElement.prototype.onattach.call(this);this.collapse();this.listItemElement.addEventListener("contextmenu",this._handleContextMenuEvent.bind(this),false);},setNode:function(node)
{this._node=node;var paths=[];while(node&&!node.isRoot()){paths.push(node._title);node=node.parent;}
paths.reverse();this.tooltip=paths.join("/");},_handleContextMenuEvent:function(event)
{if(!this._node)
return;this.select();this._navigatorView.handleFolderContextMenu(event,this._node);},__proto__:WebInspector.BaseNavigatorTreeElement.prototype}
WebInspector.NavigatorSourceTreeElement=function(navigatorView,uiSourceCode,title)
{this._navigatorView=navigatorView;this._uiSourceCode=uiSourceCode;WebInspector.BaseNavigatorTreeElement.call(this,WebInspector.NavigatorTreeOutline.Types.UISourceCode,title,this._calculateIconClasses(),false);this.tooltip=uiSourceCode.originURL();}
WebInspector.NavigatorSourceTreeElement.prototype={get uiSourceCode()
{return this._uiSourceCode;},_calculateIconClasses:function()
{return["navigator-"+this._uiSourceCode.contentType().name()+"-tree-item"];},updateIcon:function()
{this.updateIconClasses(this._calculateIconClasses());},onattach:function()
{WebInspector.BaseNavigatorTreeElement.prototype.onattach.call(this);this.listItemElement.draggable=true;this.listItemElement.addEventListener("click",this._onclick.bind(this),false);this.listItemElement.addEventListener("contextmenu",this._handleContextMenuEvent.bind(this),false);this.listItemElement.addEventListener("mousedown",this._onmousedown.bind(this),false);this.listItemElement.addEventListener("dragstart",this._ondragstart.bind(this),false);},_onmousedown:function(event)
{if(event.which===1)
this._uiSourceCode.requestContent(callback.bind(this));function callback(content)
{this._warmedUpContent=content;}},_shouldRenameOnMouseDown:function()
{if(!this._uiSourceCode.canRename())
return false;var isSelected=this===this.treeOutline.selectedTreeElement;var isFocused=this.treeOutline.childrenListElement.isSelfOrAncestor(document.activeElement);return isSelected&&isFocused&&!WebInspector.isBeingEdited(this.treeOutline.element);},selectOnMouseDown:function(event)
{if(event.which!==1||!this._shouldRenameOnMouseDown()){TreeElement.prototype.selectOnMouseDown.call(this,event);return;}
setTimeout(rename.bind(this),300);function rename()
{if(this._shouldRenameOnMouseDown())
this._navigatorView.requestRename(this._uiSourceCode);}},_ondragstart:function(event)
{event.dataTransfer.setData("text/plain",this._warmedUpContent);event.dataTransfer.effectAllowed="copy";return true;},onspace:function()
{this._navigatorView._sourceSelected(this.uiSourceCode,true);return true;},_onclick:function(event)
{this._navigatorView._sourceSelected(this.uiSourceCode,false);},ondblclick:function(event)
{var middleClick=event.button===1;this._navigatorView._sourceSelected(this.uiSourceCode,!middleClick);},onenter:function()
{this._navigatorView._sourceSelected(this.uiSourceCode,true);return true;},ondelete:function()
{this._navigatorView.sourceDeleted(this.uiSourceCode);return true;},_handleContextMenuEvent:function(event)
{this.select();this._navigatorView.handleFileContextMenu(event,this._uiSourceCode);},__proto__:WebInspector.BaseNavigatorTreeElement.prototype}
WebInspector.NavigatorTreeNode=function(id)
{this.id=id;this._children=new StringMap();}
WebInspector.NavigatorTreeNode.prototype={treeElement:function(){},dispose:function(){},isRoot:function()
{return false;},hasChildren:function()
{return true;},populate:function()
{if(this.isPopulated())
return;if(this.parent)
this.parent.populate();this._populated=true;this.wasPopulated();},wasPopulated:function()
{var children=this.children();for(var i=0;i<children.length;++i)
this.treeElement().appendChild(children[i].treeElement());},didAddChild:function(node)
{if(this.isPopulated())
this.treeElement().appendChild(node.treeElement());},willRemoveChild:function(node)
{if(this.isPopulated())
this.treeElement().removeChild(node.treeElement());},isPopulated:function()
{return this._populated;},isEmpty:function()
{return!this._children.size();},child:function(id)
{return this._children.get(id);},children:function()
{return this._children.values();},appendChild:function(node)
{this._children.put(node.id,node);node.parent=this;this.didAddChild(node);},removeChild:function(node)
{this.willRemoveChild(node);this._children.remove(node.id);delete node.parent;node.dispose();},reset:function()
{this._children.clear();}}
WebInspector.NavigatorRootTreeNode=function(navigatorView)
{WebInspector.NavigatorTreeNode.call(this,"");this._navigatorView=navigatorView;}
WebInspector.NavigatorRootTreeNode.prototype={isRoot:function()
{return true;},treeElement:function()
{return this._navigatorView._scriptsTree;},__proto__:WebInspector.NavigatorTreeNode.prototype}
WebInspector.NavigatorUISourceCodeTreeNode=function(navigatorView,uiSourceCode)
{WebInspector.NavigatorTreeNode.call(this,uiSourceCode.name());this._navigatorView=navigatorView;this._uiSourceCode=uiSourceCode;this._treeElement=null;}
WebInspector.NavigatorUISourceCodeTreeNode.prototype={uiSourceCode:function()
{return this._uiSourceCode;},updateIcon:function()
{if(this._treeElement)
this._treeElement.updateIcon();},treeElement:function()
{if(this._treeElement)
return this._treeElement;this._treeElement=new WebInspector.NavigatorSourceTreeElement(this._navigatorView,this._uiSourceCode,"");this.updateTitle();this._uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.TitleChanged,this._titleChanged,this);this._uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.WorkingCopyChanged,this._workingCopyChanged,this);this._uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.WorkingCopyCommitted,this._workingCopyCommitted,this);this._uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.FormattedChanged,this._formattedChanged,this);return this._treeElement;},updateTitle:function(ignoreIsDirty)
{if(!this._treeElement)
return;var titleText=this._uiSourceCode.displayName();if(!ignoreIsDirty&&(this._uiSourceCode.isDirty()||this._uiSourceCode.hasUnsavedCommittedChanges()))
titleText="*"+titleText;this._treeElement.titleText=titleText;},hasChildren:function()
{return false;},dispose:function()
{if(!this._treeElement)
return;this._uiSourceCode.removeEventListener(WebInspector.UISourceCode.Events.TitleChanged,this._titleChanged,this);this._uiSourceCode.removeEventListener(WebInspector.UISourceCode.Events.WorkingCopyChanged,this._workingCopyChanged,this);this._uiSourceCode.removeEventListener(WebInspector.UISourceCode.Events.WorkingCopyCommitted,this._workingCopyCommitted,this);this._uiSourceCode.removeEventListener(WebInspector.UISourceCode.Events.FormattedChanged,this._formattedChanged,this);},_titleChanged:function(event)
{this.updateTitle();},_workingCopyChanged:function(event)
{this.updateTitle();},_workingCopyCommitted:function(event)
{this.updateTitle();},_formattedChanged:function(event)
{this.updateTitle();},reveal:function(select)
{this.parent.populate();this.parent.treeElement().expand();this._treeElement.reveal();if(select)
this._treeElement.select();},rename:function(callback)
{if(!this._treeElement)
return;var treeOutlineElement=this._treeElement.treeOutline.element;WebInspector.markBeingEdited(treeOutlineElement,true);function commitHandler(element,newTitle,oldTitle)
{if(newTitle!==oldTitle){this._treeElement.titleText=newTitle;this._uiSourceCode.rename(newTitle,renameCallback.bind(this));return;}
afterEditing.call(this,true);}
function renameCallback(success)
{if(!success){WebInspector.markBeingEdited(treeOutlineElement,false);this.updateTitle();this.rename(callback);return;}
afterEditing.call(this,true);}
function cancelHandler()
{afterEditing.call(this,false);}
function afterEditing(committed)
{WebInspector.markBeingEdited(treeOutlineElement,false);this.updateTitle();this._treeElement.treeOutline.childrenListElement.focus();if(callback)
callback(committed);}
var editingConfig=new WebInspector.EditingConfig(commitHandler.bind(this),cancelHandler.bind(this));this.updateTitle(true);WebInspector.startEditing(this._treeElement.titleElement,editingConfig);window.getSelection().setBaseAndExtent(this._treeElement.titleElement,0,this._treeElement.titleElement,1);},__proto__:WebInspector.NavigatorTreeNode.prototype}
WebInspector.NavigatorFolderTreeNode=function(navigatorView,project,id,type,folderPath,title)
{WebInspector.NavigatorTreeNode.call(this,id);this._navigatorView=navigatorView;this._project=project;this._type=type;this._folderPath=folderPath;this._title=title;}
WebInspector.NavigatorFolderTreeNode.prototype={treeElement:function()
{if(this._treeElement)
return this._treeElement;this._treeElement=this._createTreeElement(this._title,this);return this._treeElement;},_createTreeElement:function(title,node)
{var treeElement=new WebInspector.NavigatorFolderTreeElement(this._navigatorView,this._type,title);treeElement.setNode(node);return treeElement;},wasPopulated:function()
{if(!this._treeElement||this._treeElement._node!==this)
return;this._addChildrenRecursive();},_addChildrenRecursive:function()
{var children=this.children();for(var i=0;i<children.length;++i){var child=children[i];this.didAddChild(child);if(child instanceof WebInspector.NavigatorFolderTreeNode)
child._addChildrenRecursive();}},_shouldMerge:function(node)
{return this._type!==WebInspector.NavigatorTreeOutline.Types.Domain&&node instanceof WebInspector.NavigatorFolderTreeNode;},didAddChild:function(node)
{function titleForNode(node)
{return node._title;}
if(!this._treeElement)
return;var children=this.children();if(children.length===1&&this._shouldMerge(node)){node._isMerged=true;this._treeElement.titleText=this._treeElement.titleText+"/"+node._title;node._treeElement=this._treeElement;this._treeElement.setNode(node);return;}
var oldNode;if(children.length===2)
oldNode=children[0]!==node?children[0]:children[1];if(oldNode&&oldNode._isMerged){delete oldNode._isMerged;var mergedToNodes=[];mergedToNodes.push(this);var treeNode=this;while(treeNode._isMerged){treeNode=treeNode.parent;mergedToNodes.push(treeNode);}
mergedToNodes.reverse();var titleText=mergedToNodes.map(titleForNode).join("/");var nodes=[];treeNode=oldNode;do{nodes.push(treeNode);children=treeNode.children();treeNode=children.length===1?children[0]:null;}while(treeNode&&treeNode._isMerged);if(!this.isPopulated()){this._treeElement.titleText=titleText;this._treeElement.setNode(this);for(var i=0;i<nodes.length;++i){delete nodes[i]._treeElement;delete nodes[i]._isMerged;}
return;}
var oldTreeElement=this._treeElement;var treeElement=this._createTreeElement(titleText,this);for(var i=0;i<mergedToNodes.length;++i)
mergedToNodes[i]._treeElement=treeElement;oldTreeElement.parent.appendChild(treeElement);oldTreeElement.setNode(nodes[nodes.length-1]);oldTreeElement.titleText=nodes.map(titleForNode).join("/");oldTreeElement.parent.removeChild(oldTreeElement);this._treeElement.appendChild(oldTreeElement);if(oldTreeElement.expanded)
treeElement.expand();}
if(this.isPopulated())
this._treeElement.appendChild(node.treeElement());},willRemoveChild:function(node)
{if(node._isMerged||!this.isPopulated())
return;this._treeElement.removeChild(node._treeElement);},__proto__:WebInspector.NavigatorTreeNode.prototype};WebInspector.RevisionHistoryView=function()
{WebInspector.View.call(this);this.registerRequiredCSS("revisionHistory.css");this.element.addStyleClass("revision-history-drawer");this.element.addStyleClass("fill");this.element.addStyleClass("outline-disclosure");this._uiSourceCodeItems=new Map();var olElement=this.element.createChild("ol");this._treeOutline=new TreeOutline(olElement);function populateRevisions(uiSourceCode)
{if(uiSourceCode.history.length)
this._createUISourceCodeItem(uiSourceCode);}
WebInspector.workspace.uiSourceCodes().forEach(populateRevisions.bind(this));WebInspector.workspace.addEventListener(WebInspector.Workspace.Events.UISourceCodeContentCommitted,this._revisionAdded,this);WebInspector.workspace.addEventListener(WebInspector.Workspace.Events.UISourceCodeRemoved,this._uiSourceCodeRemoved,this);WebInspector.workspace.addEventListener(WebInspector.Workspace.Events.ProjectWillReset,this._projectWillReset,this);}
WebInspector.RevisionHistoryView.showHistory=function(uiSourceCode)
{if(!WebInspector.RevisionHistoryView._view)
WebInspector.RevisionHistoryView._view=new WebInspector.RevisionHistoryView();var view=WebInspector.RevisionHistoryView._view;WebInspector.inspectorView.showCloseableViewInDrawer("history",WebInspector.UIString("History"),view);view._revealUISourceCode(uiSourceCode);}
WebInspector.RevisionHistoryView.prototype={_createUISourceCodeItem:function(uiSourceCode)
{var uiSourceCodeItem=new TreeElement(uiSourceCode.displayName(),null,true);uiSourceCodeItem.selectable=false;for(var i=0;i<this._treeOutline.children.length;++i){if(this._treeOutline.children[i].title.localeCompare(uiSourceCode.displayName())>0){this._treeOutline.insertChild(uiSourceCodeItem,i);break;}}
if(i===this._treeOutline.children.length)
this._treeOutline.appendChild(uiSourceCodeItem);this._uiSourceCodeItems.put(uiSourceCode,uiSourceCodeItem);var revisionCount=uiSourceCode.history.length;for(var i=revisionCount-1;i>=0;--i){var revision=uiSourceCode.history[i];var historyItem=new WebInspector.RevisionHistoryTreeElement(revision,uiSourceCode.history[i-1],i!==revisionCount-1);uiSourceCodeItem.appendChild(historyItem);}
var linkItem=new TreeElement("",null,false);linkItem.selectable=false;uiSourceCodeItem.appendChild(linkItem);var revertToOriginal=linkItem.listItemElement.createChild("span","revision-history-link revision-history-link-row");revertToOriginal.textContent=WebInspector.UIString("apply original content");revertToOriginal.addEventListener("click",uiSourceCode.revertToOriginal.bind(uiSourceCode));var clearHistoryElement=uiSourceCodeItem.listItemElement.createChild("span","revision-history-link");clearHistoryElement.textContent=WebInspector.UIString("revert");clearHistoryElement.addEventListener("click",this._clearHistory.bind(this,uiSourceCode));return uiSourceCodeItem;},_clearHistory:function(uiSourceCode)
{uiSourceCode.revertAndClearHistory(this._removeUISourceCode.bind(this));},_revisionAdded:function(event)
{var uiSourceCode=(event.data.uiSourceCode);var uiSourceCodeItem=this._uiSourceCodeItems.get(uiSourceCode);if(!uiSourceCodeItem){uiSourceCodeItem=this._createUISourceCodeItem(uiSourceCode);return;}
var historyLength=uiSourceCode.history.length;var historyItem=new WebInspector.RevisionHistoryTreeElement(uiSourceCode.history[historyLength-1],uiSourceCode.history[historyLength-2],false);if(uiSourceCodeItem.children.length)
uiSourceCodeItem.children[0].allowRevert();uiSourceCodeItem.insertChild(historyItem,0);},_revealUISourceCode:function(uiSourceCode)
{var uiSourceCodeItem=this._uiSourceCodeItems.get(uiSourceCode);if(uiSourceCodeItem){uiSourceCodeItem.reveal();uiSourceCodeItem.expand();}},_uiSourceCodeRemoved:function(event)
{var uiSourceCode=(event.data);this._removeUISourceCode(uiSourceCode);},_removeUISourceCode:function(uiSourceCode)
{var uiSourceCodeItem=this._uiSourceCodeItems.get(uiSourceCode);if(!uiSourceCodeItem)
return;this._treeOutline.removeChild(uiSourceCodeItem);this._uiSourceCodeItems.remove(uiSourceCode);},_projectWillReset:function(event)
{var project=event.data;project.uiSourceCodes().forEach(this._removeUISourceCode.bind(this));},__proto__:WebInspector.View.prototype}
WebInspector.RevisionHistoryTreeElement=function(revision,baseRevision,allowRevert)
{TreeElement.call(this,revision.timestamp.toLocaleTimeString(),null,true);this.selectable=false;this._revision=revision;this._baseRevision=baseRevision;this._revertElement=document.createElement("span");this._revertElement.className="revision-history-link";this._revertElement.textContent=WebInspector.UIString("apply revision content");this._revertElement.addEventListener("click",this._revision.revertToThis.bind(this._revision),false);if(!allowRevert)
this._revertElement.addStyleClass("hidden");}
WebInspector.RevisionHistoryTreeElement.prototype={onattach:function()
{this.listItemElement.addStyleClass("revision-history-revision");},onexpand:function()
{this.listItemElement.appendChild(this._revertElement);if(this._wasExpandedOnce)
return;this._wasExpandedOnce=true;this.childrenListElement.addStyleClass("source-code");if(this._baseRevision)
this._baseRevision.requestContent(step1.bind(this));else
this._revision.uiSourceCode.requestOriginalContent(step1.bind(this));function step1(baseContent)
{this._revision.requestContent(step2.bind(this,baseContent));}
function step2(baseContent,newContent)
{var baseLines=difflib.stringAsLines(baseContent);var newLines=difflib.stringAsLines(newContent);var sm=new difflib.SequenceMatcher(baseLines,newLines);var opcodes=sm.get_opcodes();var lastWasSeparator=false;for(var idx=0;idx<opcodes.length;idx++){var code=opcodes[idx];var change=code[0];var b=code[1];var be=code[2];var n=code[3];var ne=code[4];var rowCount=Math.max(be-b,ne-n);var topRows=[];var bottomRows=[];for(var i=0;i<rowCount;i++){if(change==="delete"||(change==="replace"&&b<be)){var lineNumber=b++;this._createLine(lineNumber,null,baseLines[lineNumber],"removed");lastWasSeparator=false;}
if(change==="insert"||(change==="replace"&&n<ne)){var lineNumber=n++;this._createLine(null,lineNumber,newLines[lineNumber],"added");lastWasSeparator=false;}
if(change==="equal"){b++;n++;if(!lastWasSeparator)
this._createLine(null,null,"    \u2026","separator");lastWasSeparator=true;}}}}},oncollapse:function()
{this._revertElement.remove();},_createLine:function(baseLineNumber,newLineNumber,lineContent,changeType)
{var child=new TreeElement("",null,false);child.selectable=false;this.appendChild(child);var lineElement=document.createElement("span");function appendLineNumber(lineNumber)
{var numberString=lineNumber!==null?numberToStringWithSpacesPadding(lineNumber+1,4):"    ";var lineNumberSpan=document.createElement("span");lineNumberSpan.addStyleClass("webkit-line-number");lineNumberSpan.textContent=numberString;child.listItemElement.appendChild(lineNumberSpan);}
appendLineNumber(baseLineNumber);appendLineNumber(newLineNumber);var contentSpan=document.createElement("span");contentSpan.textContent=lineContent;child.listItemElement.appendChild(contentSpan);child.listItemElement.addStyleClass("revision-history-line");child.listItemElement.addStyleClass("revision-history-line-"+changeType);},allowRevert:function()
{this._revertElement.removeStyleClass("hidden");},__proto__:TreeElement.prototype};WebInspector.ScopeChainSidebarPane=function()
{WebInspector.SidebarPane.call(this,WebInspector.UIString("Scope Variables"));this._sections=[];this._expandedSections={};this._expandedProperties=[];}
WebInspector.ScopeChainSidebarPane.prototype={update:function(callFrame)
{this.bodyElement.removeChildren();if(!callFrame){var infoElement=document.createElement("div");infoElement.className="info";infoElement.textContent=WebInspector.UIString("Not Paused");this.bodyElement.appendChild(infoElement);return;}
for(var i=0;i<this._sections.length;++i){var section=this._sections[i];if(!section.title)
continue;if(section.expanded)
this._expandedSections[section.title]=true;else
delete this._expandedSections[section.title];}
this._sections=[];var foundLocalScope=false;var scopeChain=callFrame.scopeChain;for(var i=0;i<scopeChain.length;++i){var scope=scopeChain[i];var title=null;var subtitle=scope.object.description;var emptyPlaceholder=null;var extraProperties=null;var declarativeScope;switch(scope.type){case"local":foundLocalScope=true;title=WebInspector.UIString("Local");emptyPlaceholder=WebInspector.UIString("No Variables");subtitle=null;if(callFrame.this)
extraProperties=[new WebInspector.RemoteObjectProperty("this",WebInspector.RemoteObject.fromPayload(callFrame.this))];if(i==0){var details=WebInspector.debuggerModel.debuggerPausedDetails();var exception=details.reason===WebInspector.DebuggerModel.BreakReason.Exception?details.auxData:0;if(exception){extraProperties=extraProperties||[];var exceptionObject=(exception);extraProperties.push(new WebInspector.RemoteObjectProperty("<exception>",WebInspector.RemoteObject.fromPayload(exceptionObject)));}}
declarativeScope=true;break;case"closure":title=WebInspector.UIString("Closure");emptyPlaceholder=WebInspector.UIString("No Variables");subtitle=null;declarativeScope=true;break;case"catch":title=WebInspector.UIString("Catch");subtitle=null;declarativeScope=true;break;case"with":title=WebInspector.UIString("With Block");declarativeScope=false;break;case"global":title=WebInspector.UIString("Global");declarativeScope=false;break;}
if(!title||title===subtitle)
subtitle=null;var scopeRef;if(declarativeScope)
scopeRef=new WebInspector.ScopeRef(i,callFrame.id,undefined);else
scopeRef=undefined;var section=new WebInspector.ObjectPropertiesSection(WebInspector.ScopeRemoteObject.fromPayload(scope.object,scopeRef),title,subtitle,emptyPlaceholder,true,extraProperties,WebInspector.ScopeVariableTreeElement);section.editInSelectedCallFrameWhenPaused=true;section.pane=this;if(scope.type==="global")
section.expanded=false;else if(!foundLocalScope||scope.type==="local"||title in this._expandedSections)
section.expanded=true;this._sections.push(section);this.bodyElement.appendChild(section.element);}},__proto__:WebInspector.SidebarPane.prototype}
WebInspector.ScopeVariableTreeElement=function(property)
{WebInspector.ObjectPropertyTreeElement.call(this,property);}
WebInspector.ScopeVariableTreeElement.prototype={onattach:function()
{WebInspector.ObjectPropertyTreeElement.prototype.onattach.call(this);if(this.hasChildren&&this.propertyIdentifier in this.treeOutline.section.pane._expandedProperties)
this.expand();},onexpand:function()
{this.treeOutline.section.pane._expandedProperties[this.propertyIdentifier]=true;},oncollapse:function()
{delete this.treeOutline.section.pane._expandedProperties[this.propertyIdentifier];},get propertyIdentifier()
{if("_propertyIdentifier"in this)
return this._propertyIdentifier;var section=this.treeOutline.section;this._propertyIdentifier=section.title+":"+(section.subtitle?section.subtitle+":":"")+this.propertyPath();return this._propertyIdentifier;},__proto__:WebInspector.ObjectPropertyTreeElement.prototype};WebInspector.SourcesNavigator=function()
{WebInspector.Object.call(this);this._tabbedPane=new WebInspector.TabbedPane();this._tabbedPane.shrinkableTabs=true;this._tabbedPane.element.addStyleClass("navigator-tabbed-pane");this._sourcesView=new WebInspector.NavigatorView();this._sourcesView.addEventListener(WebInspector.NavigatorView.Events.ItemSelected,this._sourceSelected,this);this._sourcesView.addEventListener(WebInspector.NavigatorView.Events.ItemSearchStarted,this._itemSearchStarted,this);this._sourcesView.addEventListener(WebInspector.NavigatorView.Events.ItemRenamingRequested,this._itemRenamingRequested,this);this._sourcesView.addEventListener(WebInspector.NavigatorView.Events.ItemCreationRequested,this._itemCreationRequested,this);this._contentScriptsView=new WebInspector.NavigatorView();this._contentScriptsView.addEventListener(WebInspector.NavigatorView.Events.ItemSelected,this._sourceSelected,this);this._contentScriptsView.addEventListener(WebInspector.NavigatorView.Events.ItemSearchStarted,this._itemSearchStarted,this);this._contentScriptsView.addEventListener(WebInspector.NavigatorView.Events.ItemRenamingRequested,this._itemRenamingRequested,this);this._contentScriptsView.addEventListener(WebInspector.NavigatorView.Events.ItemCreationRequested,this._itemCreationRequested,this);this._snippetsView=new WebInspector.SnippetsNavigatorView();this._snippetsView.addEventListener(WebInspector.NavigatorView.Events.ItemSelected,this._sourceSelected,this);this._snippetsView.addEventListener(WebInspector.NavigatorView.Events.ItemSearchStarted,this._itemSearchStarted,this);this._snippetsView.addEventListener(WebInspector.NavigatorView.Events.ItemRenamingRequested,this._itemRenamingRequested,this);this._snippetsView.addEventListener(WebInspector.NavigatorView.Events.ItemCreationRequested,this._itemCreationRequested,this);this._tabbedPane.appendTab(WebInspector.SourcesNavigator.SourcesTab,WebInspector.UIString("Sources"),this._sourcesView);this._tabbedPane.selectTab(WebInspector.SourcesNavigator.SourcesTab);this._tabbedPane.appendTab(WebInspector.SourcesNavigator.ContentScriptsTab,WebInspector.UIString("Content scripts"),this._contentScriptsView);this._tabbedPane.appendTab(WebInspector.SourcesNavigator.SnippetsTab,WebInspector.UIString("Snippets"),this._snippetsView);}
WebInspector.SourcesNavigator.Events={SourceSelected:"SourceSelected",ItemCreationRequested:"ItemCreationRequested",ItemRenamingRequested:"ItemRenamingRequested",ItemSearchStarted:"ItemSearchStarted",}
WebInspector.SourcesNavigator.SourcesTab="sources";WebInspector.SourcesNavigator.ContentScriptsTab="contentScripts";WebInspector.SourcesNavigator.SnippetsTab="snippets";WebInspector.SourcesNavigator.prototype={get view()
{return this._tabbedPane;},_navigatorViewForUISourceCode:function(uiSourceCode)
{if(uiSourceCode.isContentScript)
return this._contentScriptsView;else if(uiSourceCode.project().type()===WebInspector.projectTypes.Snippets)
return this._snippetsView;else
return this._sourcesView;},addUISourceCode:function(uiSourceCode)
{this._navigatorViewForUISourceCode(uiSourceCode).addUISourceCode(uiSourceCode);},removeUISourceCode:function(uiSourceCode)
{this._navigatorViewForUISourceCode(uiSourceCode).removeUISourceCode(uiSourceCode);},revealUISourceCode:function(uiSourceCode,select)
{this._navigatorViewForUISourceCode(uiSourceCode).revealUISourceCode(uiSourceCode,select);if(uiSourceCode.isContentScript)
this._tabbedPane.selectTab(WebInspector.SourcesNavigator.ContentScriptsTab);else if(uiSourceCode.project().type()!==WebInspector.projectTypes.Snippets)
this._tabbedPane.selectTab(WebInspector.SourcesNavigator.SourcesTab);},updateIcon:function(uiSourceCode)
{this._navigatorViewForUISourceCode(uiSourceCode).updateIcon(uiSourceCode);},rename:function(uiSourceCode,callback)
{this._navigatorViewForUISourceCode(uiSourceCode).rename(uiSourceCode,callback);},_sourceSelected:function(event)
{this.dispatchEventToListeners(WebInspector.SourcesNavigator.Events.SourceSelected,event.data);},_itemSearchStarted:function(event)
{this.dispatchEventToListeners(WebInspector.SourcesNavigator.Events.ItemSearchStarted,event.data);},_itemRenamingRequested:function(event)
{this.dispatchEventToListeners(WebInspector.SourcesNavigator.Events.ItemRenamingRequested,event.data);},_itemCreationRequested:function(event)
{this.dispatchEventToListeners(WebInspector.SourcesNavigator.Events.ItemCreationRequested,event.data);},__proto__:WebInspector.Object.prototype}
WebInspector.SnippetsNavigatorView=function()
{WebInspector.NavigatorView.call(this);}
WebInspector.SnippetsNavigatorView.prototype={handleContextMenu:function(event)
{var contextMenu=new WebInspector.ContextMenu(event);contextMenu.appendItem(WebInspector.UIString("New"),this._handleCreateSnippet.bind(this));contextMenu.show();},handleFileContextMenu:function(event,uiSourceCode)
{var contextMenu=new WebInspector.ContextMenu(event);contextMenu.appendItem(WebInspector.UIString("Run"),this._handleEvaluateSnippet.bind(this,uiSourceCode));contextMenu.appendItem(WebInspector.UIString("Rename"),this.requestRename.bind(this,uiSourceCode));contextMenu.appendItem(WebInspector.UIString("Remove"),this._handleRemoveSnippet.bind(this,uiSourceCode));contextMenu.appendSeparator();contextMenu.appendItem(WebInspector.UIString("New"),this._handleCreateSnippet.bind(this));contextMenu.show();},_handleEvaluateSnippet:function(uiSourceCode)
{if(uiSourceCode.project().type()!==WebInspector.projectTypes.Snippets)
return;WebInspector.scriptSnippetModel.evaluateScriptSnippet(uiSourceCode);},_handleRemoveSnippet:function(uiSourceCode)
{if(uiSourceCode.project().type()!==WebInspector.projectTypes.Snippets)
return;uiSourceCode.project().deleteFile(uiSourceCode.path());},_handleCreateSnippet:function()
{var data={};data.project=WebInspector.scriptSnippetModel.project();data.path="";this.dispatchEventToListeners(WebInspector.NavigatorView.Events.ItemCreationRequested,data);},sourceDeleted:function(uiSourceCode)
{this._handleRemoveSnippet(uiSourceCode);},__proto__:WebInspector.NavigatorView.prototype};WebInspector.SourcesSearchScope=function(workspace)
{WebInspector.SearchScope.call(this)
this._searchId=0;this._workspace=workspace;}
WebInspector.SourcesSearchScope.prototype={performIndexing:function(progress,indexingFinishedCallback)
{this.stopSearch();function filterOutServiceProjects(project)
{return!project.isServiceProject();}
var projects=this._workspace.projects().filter(filterOutServiceProjects);var barrier=new CallbackBarrier();var compositeProgress=new WebInspector.CompositeProgress(progress);progress.addEventListener(WebInspector.Progress.Events.Canceled,indexingCanceled.bind(this));for(var i=0;i<projects.length;++i){var project=projects[i];var projectProgress=compositeProgress.createSubProgress(project.uiSourceCodes().length);project.indexContent(projectProgress,barrier.createCallback());}
barrier.callWhenDone(indexingFinishedCallback.bind(this,true));function indexingCanceled()
{indexingFinishedCallback(false);progress.done();}},performSearch:function(searchConfig,progress,searchResultCallback,searchFinishedCallback)
{this.stopSearch();function filterOutServiceProjects(project)
{return!project.isServiceProject();}
var projects=this._workspace.projects().filter(filterOutServiceProjects);var barrier=new CallbackBarrier();var compositeProgress=new WebInspector.CompositeProgress(progress);for(var i=0;i<projects.length;++i){var project=projects[i];var projectProgress=compositeProgress.createSubProgress(project.uiSourceCodes().length);var callback=barrier.createCallback(searchCallbackWrapper.bind(this,this._searchId,project));project.searchInContent(searchConfig.query,!searchConfig.ignoreCase,searchConfig.isRegex,projectProgress,callback);}
barrier.callWhenDone(searchFinishedCallback.bind(this,true));function searchCallbackWrapper(searchId,project,searchMatches)
{if(searchId!==this._searchId){searchFinishedCallback(false);return;}
var paths=searchMatches.keys();for(var i=0;i<paths.length;++i){var uiSourceCode=project.uiSourceCode(paths[i]);var searchResult=new WebInspector.FileBasedSearchResultsPane.SearchResult(uiSourceCode,searchMatches.get(paths[i]));searchResultCallback(searchResult);}}},stopSearch:function()
{++this._searchId;},createSearchResultsPane:function(searchConfig)
{return new WebInspector.FileBasedSearchResultsPane(searchConfig);},__proto__:WebInspector.SearchScope.prototype};WebInspector.StyleSheetOutlineDialog=function(view,uiSourceCode)
{WebInspector.SelectionDialogContentProvider.call(this);this._rules=[];this._view=view;this._uiSourceCode=uiSourceCode;this._requestItems();}
WebInspector.StyleSheetOutlineDialog.show=function(view,uiSourceCode)
{if(WebInspector.Dialog.currentInstance())
return null;var delegate=new WebInspector.StyleSheetOutlineDialog(view,uiSourceCode);var filteredItemSelectionDialog=new WebInspector.FilteredItemSelectionDialog(delegate);WebInspector.Dialog.show(view.element,filteredItemSelectionDialog);}
WebInspector.StyleSheetOutlineDialog.prototype={itemCount:function()
{return this._rules.length;},itemKeyAt:function(itemIndex)
{return this._rules[itemIndex].selectorText;},itemScoreAt:function(itemIndex,query)
{var rule=this._rules[itemIndex];return-rule.rawLocation.lineNumber;},renderItem:function(itemIndex,query,titleElement,subtitleElement)
{var rule=this._rules[itemIndex];titleElement.textContent=rule.selectorText;this.highlightRanges(titleElement,query);subtitleElement.textContent=":"+(rule.rawLocation.lineNumber+1);},_requestItems:function()
{function didGetAllStyleSheets(error,infos)
{if(error)
return;for(var i=0;i<infos.length;++i){var info=infos[i];if(info.sourceURL===this._uiSourceCode.url){WebInspector.CSSStyleSheet.createForId(info.styleSheetId,didGetStyleSheet.bind(this));return;}}}
CSSAgent.getAllStyleSheets(didGetAllStyleSheets.bind(this));function didGetStyleSheet(styleSheet)
{if(!styleSheet)
return;this._rules=styleSheet.rules;this.refresh();}},selectItem:function(itemIndex,promptValue)
{var rule=this._rules[itemIndex];var lineNumber=rule.rawLocation.lineNumber;if(!isNaN(lineNumber)&&lineNumber>=0)
this._view.highlightPosition(lineNumber,rule.rawLocation.columnNumber);this._view.focus();},__proto__:WebInspector.SelectionDialogContentProvider.prototype};WebInspector.TabbedEditorContainerDelegate=function(){}
WebInspector.TabbedEditorContainerDelegate.prototype={viewForFile:function(uiSourceCode){}}
WebInspector.TabbedEditorContainer=function(delegate,settingName,placeholderText)
{WebInspector.Object.call(this);this._delegate=delegate;this._tabbedPane=new WebInspector.TabbedPane();this._tabbedPane.setPlaceholderText(placeholderText);this._tabbedPane.setTabDelegate(new WebInspector.EditorContainerTabDelegate(this));this._tabbedPane.closeableTabs=true;this._tabbedPane.element.id="sources-editor-container-tabbed-pane";this._tabbedPane.addEventListener(WebInspector.TabbedPane.EventTypes.TabClosed,this._tabClosed,this);this._tabbedPane.addEventListener(WebInspector.TabbedPane.EventTypes.TabSelected,this._tabSelected,this);this._tabIds=new Map();this._files={};this._previouslyViewedFilesSetting=WebInspector.settings.createSetting(settingName,[]);this._history=WebInspector.TabbedEditorContainer.History.fromObject(this._previouslyViewedFilesSetting.get());}
WebInspector.TabbedEditorContainer.Events={EditorSelected:"EditorSelected",EditorClosed:"EditorClosed"}
WebInspector.TabbedEditorContainer._tabId=0;WebInspector.TabbedEditorContainer.maximalPreviouslyViewedFilesCount=30;WebInspector.TabbedEditorContainer.prototype={get view()
{return this._tabbedPane;},get visibleView()
{return this._tabbedPane.visibleView;},show:function(parentElement)
{this._tabbedPane.show(parentElement);},showFile:function(uiSourceCode)
{this._innerShowFile(uiSourceCode,true);},historyUISourceCodes:function()
{var uriToUISourceCode={};for(var id in this._files){var uiSourceCode=this._files[id];uriToUISourceCode[uiSourceCode.uri()]=uiSourceCode;}
var result=[];var uris=this._history._urls();for(var i=0;i<uris.length;++i){var uiSourceCode=uriToUISourceCode[uris[i]];if(uiSourceCode)
result.push(uiSourceCode);}
return result;},_addScrollAndSelectionListeners:function()
{if(!this._currentView)
return;this._currentView.addEventListener(WebInspector.SourceFrame.Events.ScrollChanged,this._scrollChanged,this);this._currentView.addEventListener(WebInspector.SourceFrame.Events.SelectionChanged,this._selectionChanged,this);},_removeScrollAndSelectionListeners:function()
{if(!this._currentView)
return;this._currentView.removeEventListener(WebInspector.SourceFrame.Events.ScrollChanged,this._scrollChanged,this);this._currentView.removeEventListener(WebInspector.SourceFrame.Events.SelectionChanged,this._selectionChanged,this);},_scrollChanged:function(event)
{var lineNumber=(event.data);this._history.updateScrollLineNumber(this._currentFile.uri(),lineNumber);this._history.save(this._previouslyViewedFilesSetting);},_selectionChanged:function(event)
{var range=(event.data);this._history.updateSelectionRange(this._currentFile.uri(),range);this._history.save(this._previouslyViewedFilesSetting);},_innerShowFile:function(uiSourceCode,userGesture)
{if(this._currentFile===uiSourceCode)
return;this._removeScrollAndSelectionListeners();this._currentFile=uiSourceCode;var tabId=this._tabIds.get(uiSourceCode)||this._appendFileTab(uiSourceCode,userGesture);this._tabbedPane.selectTab(tabId,userGesture);if(userGesture)
this._editorSelectedByUserAction();this._currentView=this.visibleView;this._addScrollAndSelectionListeners();this.dispatchEventToListeners(WebInspector.TabbedEditorContainer.Events.EditorSelected,this._currentFile);},_titleForFile:function(uiSourceCode)
{var maxDisplayNameLength=30;var title=uiSourceCode.displayName(true).trimMiddle(maxDisplayNameLength);if(uiSourceCode.isDirty()||uiSourceCode.hasUnsavedCommittedChanges())
title+="*";return title;},_maybeCloseTab:function(id,nextTabId)
{var uiSourceCode=this._files[id];var shouldPrompt=uiSourceCode.isDirty()&&uiSourceCode.project().canSetFileContent();if(!shouldPrompt||confirm(WebInspector.UIString("Are you sure you want to close unsaved file: %s?",uiSourceCode.name()))){uiSourceCode.resetWorkingCopy();if(nextTabId)
this._tabbedPane.selectTab(nextTabId,true);this._tabbedPane.closeTab(id,true);return true;}
return false;},_closeTabs:function(ids)
{var dirtyTabs=[];var cleanTabs=[];for(var i=0;i<ids.length;++i){var id=ids[i];var uiSourceCode=this._files[id];if(uiSourceCode.isDirty())
dirtyTabs.push(id);else
cleanTabs.push(id);}
if(dirtyTabs.length)
this._tabbedPane.selectTab(dirtyTabs[0],true);this._tabbedPane.closeTabs(cleanTabs,true);for(var i=0;i<dirtyTabs.length;++i){var nextTabId=i+1<dirtyTabs.length?dirtyTabs[i+1]:null;if(!this._maybeCloseTab(dirtyTabs[i],nextTabId))
break;}},addUISourceCode:function(uiSourceCode)
{var uri=uiSourceCode.uri();if(this._userSelectedFiles)
return;var index=this._history.index(uri)
if(index===-1)
return;var tabId=this._tabIds.get(uiSourceCode)||this._appendFileTab(uiSourceCode,false);if(!this._currentFile)
return;if(!index){this._innerShowFile(uiSourceCode,false);return;}
var currentProjectType=this._currentFile.project().type();var addedProjectType=uiSourceCode.project().type();var snippetsProjectType=WebInspector.projectTypes.Snippets;if(this._history.index(this._currentFile.uri())&&currentProjectType===snippetsProjectType&&addedProjectType!==snippetsProjectType)
this._innerShowFile(uiSourceCode,false);},removeUISourceCode:function(uiSourceCode)
{this.removeUISourceCodes([uiSourceCode]);},removeUISourceCodes:function(uiSourceCodes)
{var tabIds=[];for(var i=0;i<uiSourceCodes.length;++i){var uiSourceCode=uiSourceCodes[i];var tabId=this._tabIds.get(uiSourceCode);if(tabId)
tabIds.push(tabId);}
this._tabbedPane.closeTabs(tabIds);},_editorClosedByUserAction:function(uiSourceCode)
{this._userSelectedFiles=true;this._history.remove(uiSourceCode.uri());this._updateHistory();},_editorSelectedByUserAction:function()
{this._userSelectedFiles=true;this._updateHistory();},_updateHistory:function()
{var tabIds=this._tabbedPane.lastOpenedTabIds(WebInspector.TabbedEditorContainer.maximalPreviouslyViewedFilesCount);function tabIdToURI(tabId)
{return this._files[tabId].uri();}
this._history.update(tabIds.map(tabIdToURI.bind(this)));this._history.save(this._previouslyViewedFilesSetting);},_tooltipForFile:function(uiSourceCode)
{return uiSourceCode.originURL();},_appendFileTab:function(uiSourceCode,userGesture)
{var view=this._delegate.viewForFile(uiSourceCode);var title=this._titleForFile(uiSourceCode);var tooltip=this._tooltipForFile(uiSourceCode);var tabId=this._generateTabId();this._tabIds.put(uiSourceCode,tabId);this._files[tabId]=uiSourceCode;var savedSelectionRange=this._history.selectionRange(uiSourceCode.uri());if(savedSelectionRange)
view.setSelection(savedSelectionRange);var savedScrollLineNumber=this._history.scrollLineNumber(uiSourceCode.uri());if(savedScrollLineNumber)
view.scrollToLine(savedScrollLineNumber);this._tabbedPane.appendTab(tabId,title,view,tooltip,userGesture);this._updateFileTitle(uiSourceCode);this._addUISourceCodeListeners(uiSourceCode);return tabId;},_tabClosed:function(event)
{var tabId=(event.data.tabId);var userGesture=(event.data.isUserGesture);var uiSourceCode=this._files[tabId];if(this._currentFile===uiSourceCode){this._removeScrollAndSelectionListeners();delete this._currentView;delete this._currentFile;}
this._tabIds.remove(uiSourceCode);delete this._files[tabId];this._removeUISourceCodeListeners(uiSourceCode);this.dispatchEventToListeners(WebInspector.TabbedEditorContainer.Events.EditorClosed,uiSourceCode);if(userGesture)
this._editorClosedByUserAction(uiSourceCode);},_tabSelected:function(event)
{var tabId=(event.data.tabId);var userGesture=(event.data.isUserGesture);var uiSourceCode=this._files[tabId];this._innerShowFile(uiSourceCode,userGesture);},_addUISourceCodeListeners:function(uiSourceCode)
{uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.TitleChanged,this._uiSourceCodeTitleChanged,this);uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.WorkingCopyChanged,this._uiSourceCodeWorkingCopyChanged,this);uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.WorkingCopyCommitted,this._uiSourceCodeWorkingCopyCommitted,this);uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.SavedStateUpdated,this._uiSourceCodeSavedStateUpdated,this);uiSourceCode.addEventListener(WebInspector.UISourceCode.Events.FormattedChanged,this._uiSourceCodeFormattedChanged,this);},_removeUISourceCodeListeners:function(uiSourceCode)
{uiSourceCode.removeEventListener(WebInspector.UISourceCode.Events.TitleChanged,this._uiSourceCodeTitleChanged,this);uiSourceCode.removeEventListener(WebInspector.UISourceCode.Events.WorkingCopyChanged,this._uiSourceCodeWorkingCopyChanged,this);uiSourceCode.removeEventListener(WebInspector.UISourceCode.Events.WorkingCopyCommitted,this._uiSourceCodeWorkingCopyCommitted,this);uiSourceCode.removeEventListener(WebInspector.UISourceCode.Events.SavedStateUpdated,this._uiSourceCodeSavedStateUpdated,this);uiSourceCode.removeEventListener(WebInspector.UISourceCode.Events.FormattedChanged,this._uiSourceCodeFormattedChanged,this);},_updateFileTitle:function(uiSourceCode)
{var tabId=this._tabIds.get(uiSourceCode);if(tabId){var title=this._titleForFile(uiSourceCode);this._tabbedPane.changeTabTitle(tabId,title);if(uiSourceCode.hasUnsavedCommittedChanges())
this._tabbedPane.setTabIcon(tabId,"editor-container-unsaved-committed-changes-icon",WebInspector.UIString("Changes to this file were not saved to file system."));else
this._tabbedPane.setTabIcon(tabId,"");}},_uiSourceCodeTitleChanged:function(event)
{var uiSourceCode=(event.target);this._updateFileTitle(uiSourceCode);this._updateHistory();},_uiSourceCodeWorkingCopyChanged:function(event)
{var uiSourceCode=(event.target);this._updateFileTitle(uiSourceCode);},_uiSourceCodeWorkingCopyCommitted:function(event)
{var uiSourceCode=(event.target);this._updateFileTitle(uiSourceCode);},_uiSourceCodeSavedStateUpdated:function(event)
{var uiSourceCode=(event.target);this._updateFileTitle(uiSourceCode);},_uiSourceCodeFormattedChanged:function(event)
{var uiSourceCode=(event.target);this._updateFileTitle(uiSourceCode);},reset:function()
{delete this._userSelectedFiles;},_generateTabId:function()
{return"tab_"+(WebInspector.TabbedEditorContainer._tabId++);},currentFile:function()
{return this._currentFile;},__proto__:WebInspector.Object.prototype}
WebInspector.TabbedEditorContainer.HistoryItem=function(url,selectionRange,scrollLineNumber)
{this.url=url;this._isSerializable=url.length<WebInspector.TabbedEditorContainer.HistoryItem.serializableUrlLengthLimit;this.selectionRange=selectionRange;this.scrollLineNumber=scrollLineNumber;}
WebInspector.TabbedEditorContainer.HistoryItem.serializableUrlLengthLimit=4096;WebInspector.TabbedEditorContainer.HistoryItem.fromObject=function(serializedHistoryItem)
{var selectionRange=serializedHistoryItem.selectionRange?WebInspector.TextRange.fromObject(serializedHistoryItem.selectionRange):null;return new WebInspector.TabbedEditorContainer.HistoryItem(serializedHistoryItem.url,selectionRange,serializedHistoryItem.scrollLineNumber);}
WebInspector.TabbedEditorContainer.HistoryItem.prototype={serializeToObject:function()
{if(!this._isSerializable)
return null;var serializedHistoryItem={};serializedHistoryItem.url=this.url;serializedHistoryItem.selectionRange=this.selectionRange;serializedHistoryItem.scrollLineNumber=this.scrollLineNumber;return serializedHistoryItem;},__proto__:WebInspector.Object.prototype}
WebInspector.TabbedEditorContainer.History=function(items)
{this._items=items;this._rebuildItemIndex();}
WebInspector.TabbedEditorContainer.History.fromObject=function(serializedHistory)
{var items=[];for(var i=0;i<serializedHistory.length;++i)
items.push(WebInspector.TabbedEditorContainer.HistoryItem.fromObject(serializedHistory[i]));return new WebInspector.TabbedEditorContainer.History(items);}
WebInspector.TabbedEditorContainer.History.prototype={index:function(url)
{var index=this._itemsIndex[url];if(typeof index==="number")
return index;return-1;},_rebuildItemIndex:function()
{this._itemsIndex={};for(var i=0;i<this._items.length;++i){console.assert(!this._itemsIndex.hasOwnProperty(this._items[i].url));this._itemsIndex[this._items[i].url]=i;}},selectionRange:function(url)
{var index=this.index(url);return index!==-1?this._items[index].selectionRange:undefined;},updateSelectionRange:function(url,selectionRange)
{if(!selectionRange)
return;var index=this.index(url);if(index===-1)
return;this._items[index].selectionRange=selectionRange;},scrollLineNumber:function(url)
{var index=this.index(url);return index!==-1?this._items[index].scrollLineNumber:undefined;},updateScrollLineNumber:function(url,scrollLineNumber)
{var index=this.index(url);if(index===-1)
return;this._items[index].scrollLineNumber=scrollLineNumber;},update:function(urls)
{for(var i=urls.length-1;i>=0;--i){var index=this.index(urls[i]);var item;if(index!==-1){item=this._items[index];this._items.splice(index,1);}else
item=new WebInspector.TabbedEditorContainer.HistoryItem(urls[i]);this._items.unshift(item);this._rebuildItemIndex();}},remove:function(url)
{var index=this.index(url);if(index!==-1){this._items.splice(index,1);this._rebuildItemIndex();}},save:function(setting)
{setting.set(this._serializeToObject());},_serializeToObject:function()
{var serializedHistory=[];for(var i=0;i<this._items.length;++i){var serializedItem=this._items[i].serializeToObject();if(serializedItem)
serializedHistory.push(serializedItem);if(serializedHistory.length===WebInspector.TabbedEditorContainer.maximalPreviouslyViewedFilesCount)
break;}
return serializedHistory;},_urls:function()
{var result=[];for(var i=0;i<this._items.length;++i)
result.push(this._items[i].url);return result;},__proto__:WebInspector.Object.prototype}
WebInspector.EditorContainerTabDelegate=function(editorContainer)
{this._editorContainer=editorContainer;}
WebInspector.EditorContainerTabDelegate.prototype={closeTabs:function(tabbedPane,ids)
{this._editorContainer._closeTabs(ids);}};WebInspector.WatchExpressionsSidebarPane=function()
{WebInspector.SidebarPane.call(this,WebInspector.UIString("Watch Expressions"));this.section=new WebInspector.WatchExpressionsSection();this.bodyElement.appendChild(this.section.element);var refreshButton=document.createElement("button");refreshButton.className="pane-title-button refresh";refreshButton.addEventListener("click",this._refreshButtonClicked.bind(this),false);refreshButton.title=WebInspector.UIString("Refresh");this.titleElement.appendChild(refreshButton);var addButton=document.createElement("button");addButton.className="pane-title-button add";addButton.addEventListener("click",this._addButtonClicked.bind(this),false);this.titleElement.appendChild(addButton);addButton.title=WebInspector.UIString("Add watch expression");this._requiresUpdate=true;}
WebInspector.WatchExpressionsSidebarPane.prototype={wasShown:function()
{this._refreshExpressionsIfNeeded();},reset:function()
{this.refreshExpressions();},refreshExpressions:function()
{this._requiresUpdate=true;this._refreshExpressionsIfNeeded();},addExpression:function(expression)
{this.section.addExpression(expression);this.expand();},_refreshExpressionsIfNeeded:function()
{if(this._requiresUpdate&&this.isShowing()){this.section.update();delete this._requiresUpdate;}else
this._requiresUpdate=true;},_addButtonClicked:function(event)
{event.consume();this.expand();this.section.addNewExpressionAndEdit();},_refreshButtonClicked:function(event)
{event.consume();this.refreshExpressions();},__proto__:WebInspector.SidebarPane.prototype}
WebInspector.WatchExpressionsSection=function()
{this._watchObjectGroupId="watch-group";WebInspector.ObjectPropertiesSection.call(this,WebInspector.RemoteObject.fromPrimitiveValue(""));this.treeElementConstructor=WebInspector.WatchedPropertyTreeElement;this._expandedExpressions={};this._expandedProperties={};this.emptyElement=document.createElement("div");this.emptyElement.className="info";this.emptyElement.textContent=WebInspector.UIString("No Watch Expressions");this.watchExpressions=WebInspector.settings.watchExpressions.get();this.headerElement.className="hidden";this.editable=true;this.expanded=true;this.propertiesElement.addStyleClass("watch-expressions");this.element.addEventListener("mousemove",this._mouseMove.bind(this),true);this.element.addEventListener("mouseout",this._mouseOut.bind(this),true);this.element.addEventListener("dblclick",this._sectionDoubleClick.bind(this),false);this.emptyElement.addEventListener("contextmenu",this._emptyElementContextMenu.bind(this),false);}
WebInspector.WatchExpressionsSection.NewWatchExpression="\xA0";WebInspector.WatchExpressionsSection.prototype={update:function(e)
{if(e)
e.consume();function appendResult(expression,watchIndex,result,wasThrown)
{if(!result)
return;var property=new WebInspector.RemoteObjectProperty(expression,result);property.watchIndex=watchIndex;property.wasThrown=wasThrown;properties.push(property);if(properties.length==propertyCount){this.updateProperties(properties,[],WebInspector.WatchExpressionTreeElement,WebInspector.WatchExpressionsSection.CompareProperties);if(this._newExpressionAdded){delete this._newExpressionAdded;var treeElement=this.findAddedTreeElement();if(treeElement)
treeElement.startEditing();}
if(this._lastMouseMovePageY)
this._updateHoveredElement(this._lastMouseMovePageY);}}
RuntimeAgent.releaseObjectGroup(this._watchObjectGroupId)
var properties=[];var propertyCount=0;for(var i=0;i<this.watchExpressions.length;++i){if(!this.watchExpressions[i])
continue;++propertyCount;}
for(var i=0;i<this.watchExpressions.length;++i){var expression=this.watchExpressions[i];if(!expression)
continue;WebInspector.runtimeModel.evaluate(expression,this._watchObjectGroupId,false,true,false,false,appendResult.bind(this,expression,i));}
if(!propertyCount){if(!this.emptyElement.parentNode)
this.element.appendChild(this.emptyElement);}else{if(this.emptyElement.parentNode)
this.element.removeChild(this.emptyElement);}
this.expanded=(propertyCount!=0);},addExpression:function(expression)
{this.watchExpressions.push(expression);this.saveExpressions();this.update();},addNewExpressionAndEdit:function()
{this._newExpressionAdded=true;this.watchExpressions.push(WebInspector.WatchExpressionsSection.NewWatchExpression);this.update();},_sectionDoubleClick:function(event)
{if(event.target!==this.element&&event.target!==this.propertiesElement&&event.target!==this.emptyElement)
return;event.consume();this.addNewExpressionAndEdit();},updateExpression:function(element,value)
{if(value===null){var index=element.property.watchIndex;this.watchExpressions.splice(index,1);}
else
this.watchExpressions[element.property.watchIndex]=value;this.saveExpressions();this.update();},_deleteAllExpressions:function()
{this.watchExpressions=[];this.saveExpressions();this.update();},findAddedTreeElement:function()
{var children=this.propertiesTreeOutline.children;for(var i=0;i<children.length;++i){if(children[i].property.name===WebInspector.WatchExpressionsSection.NewWatchExpression)
return children[i];}},saveExpressions:function()
{var toSave=[];for(var i=0;i<this.watchExpressions.length;i++)
if(this.watchExpressions[i])
toSave.push(this.watchExpressions[i]);WebInspector.settings.watchExpressions.set(toSave);return toSave.length;},_mouseMove:function(e)
{if(this.propertiesElement.firstChild)
this._updateHoveredElement(e.pageY);},_mouseOut:function()
{if(this._hoveredElement){this._hoveredElement.removeStyleClass("hovered");delete this._hoveredElement;}
delete this._lastMouseMovePageY;},_updateHoveredElement:function(pageY)
{var candidateElement=this.propertiesElement.firstChild;while(true){var next=candidateElement.nextSibling;while(next&&!next.clientHeight)
next=next.nextSibling;if(!next||next.totalOffsetTop()>pageY)
break;candidateElement=next;}
if(this._hoveredElement!==candidateElement){if(this._hoveredElement)
this._hoveredElement.removeStyleClass("hovered");if(candidateElement)
candidateElement.addStyleClass("hovered");this._hoveredElement=candidateElement;}
this._lastMouseMovePageY=pageY;},_emptyElementContextMenu:function(event)
{var contextMenu=new WebInspector.ContextMenu(event);contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Add watch expression":"Add Watch Expression"),this.addNewExpressionAndEdit.bind(this));contextMenu.show();},__proto__:WebInspector.ObjectPropertiesSection.prototype}
WebInspector.WatchExpressionsSection.CompareProperties=function(propertyA,propertyB)
{if(propertyA.watchIndex==propertyB.watchIndex)
return 0;else if(propertyA.watchIndex<propertyB.watchIndex)
return-1;else
return 1;}
WebInspector.WatchExpressionTreeElement=function(property)
{WebInspector.ObjectPropertyTreeElement.call(this,property);}
WebInspector.WatchExpressionTreeElement.prototype={onexpand:function()
{WebInspector.ObjectPropertyTreeElement.prototype.onexpand.call(this);this.treeOutline.section._expandedExpressions[this._expression()]=true;},oncollapse:function()
{WebInspector.ObjectPropertyTreeElement.prototype.oncollapse.call(this);delete this.treeOutline.section._expandedExpressions[this._expression()];},onattach:function()
{WebInspector.ObjectPropertyTreeElement.prototype.onattach.call(this);if(this.treeOutline.section._expandedExpressions[this._expression()])
this.expanded=true;},_expression:function()
{return this.property.name;},update:function()
{WebInspector.ObjectPropertyTreeElement.prototype.update.call(this);if(this.property.wasThrown){this.valueElement.textContent=WebInspector.UIString("<not available>");this.listItemElement.addStyleClass("dimmed");}else
this.listItemElement.removeStyleClass("dimmed");var deleteButton=document.createElement("input");deleteButton.type="button";deleteButton.title=WebInspector.UIString("Delete watch expression.");deleteButton.addStyleClass("enabled-button");deleteButton.addStyleClass("delete-button");deleteButton.addEventListener("click",this._deleteButtonClicked.bind(this),false);this.listItemElement.addEventListener("contextmenu",this._contextMenu.bind(this),false);this.listItemElement.insertBefore(deleteButton,this.listItemElement.firstChild);},populateContextMenu:function(contextMenu)
{if(!this.isEditing()){contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Add watch expression":"Add Watch Expression"),this.treeOutline.section.addNewExpressionAndEdit.bind(this.treeOutline.section));contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Delete watch expression":"Delete Watch Expression"),this._deleteButtonClicked.bind(this));}
if(this.treeOutline.section.watchExpressions.length>1)
contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Delete all watch expressions":"Delete All Watch Expressions"),this._deleteAllButtonClicked.bind(this));},_contextMenu:function(event)
{var contextMenu=new WebInspector.ContextMenu(event);this.populateContextMenu(contextMenu);contextMenu.show();},_deleteAllButtonClicked:function()
{this.treeOutline.section._deleteAllExpressions();},_deleteButtonClicked:function()
{this.treeOutline.section.updateExpression(this,null);},renderPromptAsBlock:function()
{return true;},elementAndValueToEdit:function(event)
{return[this.nameElement,this.property.name.trim()];},editingCancelled:function(element,context)
{if(!context.elementToEdit.textContent)
this.treeOutline.section.updateExpression(this,null);WebInspector.ObjectPropertyTreeElement.prototype.editingCancelled.call(this,element,context);},applyExpression:function(expression,updateInterface)
{expression=expression.trim();if(!expression)
expression=null;this.property.name=expression;this.treeOutline.section.updateExpression(this,expression);},__proto__:WebInspector.ObjectPropertyTreeElement.prototype}
WebInspector.WatchedPropertyTreeElement=function(property)
{WebInspector.ObjectPropertyTreeElement.call(this,property);}
WebInspector.WatchedPropertyTreeElement.prototype={onattach:function()
{WebInspector.ObjectPropertyTreeElement.prototype.onattach.call(this);if(this.hasChildren&&this.propertyPath()in this.treeOutline.section._expandedProperties)
this.expand();},onexpand:function()
{WebInspector.ObjectPropertyTreeElement.prototype.onexpand.call(this);this.treeOutline.section._expandedProperties[this.propertyPath()]=true;},oncollapse:function()
{WebInspector.ObjectPropertyTreeElement.prototype.oncollapse.call(this);delete this.treeOutline.section._expandedProperties[this.propertyPath()];},__proto__:WebInspector.ObjectPropertyTreeElement.prototype};WebInspector.Worker=function(id,url,shared)
{this.id=id;this.url=url;this.shared=shared;}
WebInspector.WorkersSidebarPane=function(workerManager)
{WebInspector.SidebarPane.call(this,WebInspector.UIString("Workers"));this._enableWorkersCheckbox=new WebInspector.Checkbox(WebInspector.UIString("Pause on start"),"sidebar-label",WebInspector.UIString("Automatically attach to new workers and pause them. Enabling this option will force opening inspector for all new workers."));this._enableWorkersCheckbox.element.id="pause-workers-checkbox";this.bodyElement.appendChild(this._enableWorkersCheckbox.element);this._enableWorkersCheckbox.addEventListener(this._autoattachToWorkersClicked.bind(this));this._enableWorkersCheckbox.checked=false;var note=this.bodyElement.createChild("div");note.id="shared-workers-list";note.addStyleClass("sidebar-label")
note.textContent=WebInspector.UIString("Shared workers can be inspected in the Task Manager");var separator=this.bodyElement.createChild("div","sidebar-separator");separator.textContent=WebInspector.UIString("Dedicated worker inspectors");this._workerListElement=document.createElement("ol");this._workerListElement.tabIndex=0;this._workerListElement.addStyleClass("properties-tree");this._workerListElement.addStyleClass("sidebar-label");this.bodyElement.appendChild(this._workerListElement);this._idToWorkerItem={};this._workerManager=workerManager;workerManager.addEventListener(WebInspector.WorkerManager.Events.WorkerAdded,this._workerAdded,this);workerManager.addEventListener(WebInspector.WorkerManager.Events.WorkerRemoved,this._workerRemoved,this);workerManager.addEventListener(WebInspector.WorkerManager.Events.WorkersCleared,this._workersCleared,this);}
WebInspector.WorkersSidebarPane.prototype={_workerAdded:function(event)
{this._addWorker(event.data.workerId,event.data.url,event.data.inspectorConnected);},_workerRemoved:function(event)
{this._idToWorkerItem[event.data].remove();delete this._idToWorkerItem[event.data];},_workersCleared:function(event)
{this._idToWorkerItem={};this._workerListElement.removeChildren();},_addWorker:function(workerId,url,inspectorConnected)
{var item=this._workerListElement.createChild("div","dedicated-worker-item");var link=item.createChild("a");link.textContent=url;link.href="#";link.target="_blank";link.addEventListener("click",this._workerItemClicked.bind(this,workerId),true);this._idToWorkerItem[workerId]=item;},_workerItemClicked:function(workerId,event)
{event.preventDefault();this._workerManager.openWorkerInspector(workerId);},_autoattachToWorkersClicked:function(event)
{WorkerAgent.setAutoconnectToWorkers(this._enableWorkersCheckbox.checked);},__proto__:WebInspector.SidebarPane.prototype};WebInspector.SourcesPanel=function(workspaceForTest)
{WebInspector.Panel.call(this,"sources");this.registerRequiredCSS("sourcesPanel.css");this.registerRequiredCSS("textPrompt.css");WebInspector.settings.navigatorWasOnceHidden=WebInspector.settings.createSetting("navigatorWasOnceHidden",false);WebInspector.settings.debuggerSidebarHidden=WebInspector.settings.createSetting("debuggerSidebarHidden",false);WebInspector.settings.showEditorInDrawer=WebInspector.settings.createSetting("showEditorInDrawer",true);this._workspace=workspaceForTest||WebInspector.workspace;function viewGetter()
{return this.visibleView;}
WebInspector.GoToLineDialog.install(this,viewGetter.bind(this));var helpSection=WebInspector.shortcutsScreen.section(WebInspector.UIString("Sources Panel"));this.debugToolbar=this._createDebugToolbar();const initialDebugSidebarWidth=225;const minimumDebugSidebarWidthPercent=0.5;this.createSidebarView(this.element,WebInspector.SidebarView.SidebarPosition.End,initialDebugSidebarWidth);this.splitView.element.id="scripts-split-view";this.splitView.setSidebarElementConstraints(Preferences.minScriptsSidebarWidth);this.splitView.setMainElementConstraints(minimumDebugSidebarWidthPercent);const initialNavigatorWidth=225;const minimumViewsContainerWidthPercent=0.5;this.editorView=new WebInspector.SidebarView(WebInspector.SidebarView.SidebarPosition.Start,"scriptsPanelNavigatorSidebarWidth",initialNavigatorWidth);this.editorView.element.id="scripts-editor-split-view";this.editorView.element.tabIndex=0;this.editorView.setSidebarElementConstraints(Preferences.minScriptsSidebarWidth);this.editorView.setMainElementConstraints(minimumViewsContainerWidthPercent);this.editorView.show(this.splitView.mainElement);this._navigator=new WebInspector.SourcesNavigator();this._navigator.view.show(this.editorView.sidebarElement);var tabbedEditorPlaceholderText=WebInspector.isMac()?WebInspector.UIString("Hit Cmd+O to open a file"):WebInspector.UIString("Hit Ctrl+O to open a file");this.editorView.mainElement.addStyleClass("vbox");this.editorView.sidebarElement.addStyleClass("vbox");this.sourcesView=new WebInspector.SourcesView();this._editorContainer=new WebInspector.TabbedEditorContainer(this,"previouslyViewedFiles",tabbedEditorPlaceholderText);this._editorContainer.show(this.sourcesView.element);this._editorFooterElement=this.sourcesView.element.createChild("div","inspector-footer status-bar hidden");this._navigatorController=new WebInspector.NavigatorOverlayController(this.editorView,this._navigator.view,this._editorContainer.view);this._navigator.addEventListener(WebInspector.SourcesNavigator.Events.SourceSelected,this._sourceSelected,this);this._navigator.addEventListener(WebInspector.SourcesNavigator.Events.ItemSearchStarted,this._itemSearchStarted,this);this._navigator.addEventListener(WebInspector.SourcesNavigator.Events.ItemCreationRequested,this._itemCreationRequested,this);this._navigator.addEventListener(WebInspector.SourcesNavigator.Events.ItemRenamingRequested,this._itemRenamingRequested,this);this._editorContainer.addEventListener(WebInspector.TabbedEditorContainer.Events.EditorSelected,this._editorSelected,this);this._editorContainer.addEventListener(WebInspector.TabbedEditorContainer.Events.EditorClosed,this._editorClosed,this);this._debugSidebarResizeWidgetElement=this.splitView.mainElement.createChild("div","resizer-widget");this._debugSidebarResizeWidgetElement.id="scripts-debug-sidebar-resizer-widget";this.splitView.installResizer(this._debugSidebarResizeWidgetElement);this.sidebarPanes={};this.sidebarPanes.watchExpressions=new WebInspector.WatchExpressionsSidebarPane();this.sidebarPanes.callstack=new WebInspector.CallStackSidebarPane();this.sidebarPanes.callstack.addEventListener(WebInspector.CallStackSidebarPane.Events.CallFrameSelected,this._callFrameSelectedInSidebar.bind(this));this.sidebarPanes.scopechain=new WebInspector.ScopeChainSidebarPane();this.sidebarPanes.jsBreakpoints=new WebInspector.JavaScriptBreakpointsSidebarPane(WebInspector.breakpointManager,this._showSourceLocation.bind(this));this.sidebarPanes.domBreakpoints=WebInspector.domBreakpointsSidebarPane.createProxy(this);this.sidebarPanes.xhrBreakpoints=new WebInspector.XHRBreakpointsSidebarPane();this.sidebarPanes.eventListenerBreakpoints=new WebInspector.EventListenerBreakpointsSidebarPane();if(Capabilities.canInspectWorkers&&!WebInspector.WorkerManager.isWorkerFrontend()){WorkerAgent.enable();this.sidebarPanes.workerList=new WebInspector.WorkersSidebarPane(WebInspector.workerManager);}
this.sidebarPanes.callstack.registerShortcuts(this.registerShortcuts.bind(this));this.registerShortcuts(WebInspector.SourcesPanelDescriptor.ShortcutKeys.GoToMember,this._showOutlineDialog.bind(this));this.registerShortcuts(WebInspector.SourcesPanelDescriptor.ShortcutKeys.ToggleBreakpoint,this._toggleBreakpoint.bind(this));this._extensionSidebarPanes=[];this._toggleFormatSourceButton=new WebInspector.StatusBarButton(WebInspector.UIString("Pretty print"),"sources-toggle-pretty-print-status-bar-item");this._toggleFormatSourceButton.toggled=false;this._toggleFormatSourceButton.addEventListener("click",this._toggleFormatSource,this);this._scriptViewStatusBarItemsContainer=document.createElement("div");this._scriptViewStatusBarItemsContainer.className="inline-block";this._scriptViewStatusBarTextContainer=document.createElement("div");this._scriptViewStatusBarTextContainer.className="inline-block";var statusBarContainerElement=this.sourcesView.element.createChild("div","sources-status-bar");statusBarContainerElement.appendChild(this._toggleFormatSourceButton.element);statusBarContainerElement.appendChild(this._scriptViewStatusBarItemsContainer);statusBarContainerElement.appendChild(this._scriptViewStatusBarTextContainer);this._installDebuggerSidebarController();WebInspector.dockController.addEventListener(WebInspector.DockController.Events.DockSideChanged,this._dockSideChanged.bind(this));WebInspector.settings.splitVerticallyWhenDockedToRight.addChangeListener(this._dockSideChanged.bind(this));this._dockSideChanged();this._sourceFramesByUISourceCode=new Map();this._updateDebuggerButtons();this._pauseOnExceptionStateChanged();if(WebInspector.debuggerModel.isPaused())
this._showDebuggerPausedDetails();WebInspector.settings.pauseOnExceptionStateString.addChangeListener(this._pauseOnExceptionStateChanged,this);WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.DebuggerWasEnabled,this._debuggerWasEnabled,this);WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.DebuggerWasDisabled,this._debuggerWasDisabled,this);WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.DebuggerPaused,this._debuggerPaused,this);WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.DebuggerResumed,this._debuggerResumed,this);WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.CallFrameSelected,this._callFrameSelected,this);WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.ConsoleCommandEvaluatedInSelectedCallFrame,this._consoleCommandEvaluatedInSelectedCallFrame,this);WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.BreakpointsActiveStateChanged,this._breakpointsActiveStateChanged,this);WebInspector.startBatchUpdate();this._workspace.uiSourceCodes().forEach(this._addUISourceCode.bind(this));WebInspector.endBatchUpdate();this._workspace.addEventListener(WebInspector.Workspace.Events.UISourceCodeAdded,this._uiSourceCodeAdded,this);this._workspace.addEventListener(WebInspector.Workspace.Events.UISourceCodeRemoved,this._uiSourceCodeRemoved,this);this._workspace.addEventListener(WebInspector.Workspace.Events.ProjectWillReset,this._projectWillReset.bind(this),this);WebInspector.debuggerModel.addEventListener(WebInspector.DebuggerModel.Events.GlobalObjectCleared,this._debuggerReset,this);WebInspector.advancedSearchController.registerSearchScope(new WebInspector.SourcesSearchScope(this._workspace));this._boundOnKeyUp=this._onKeyUp.bind(this);this._boundOnKeyDown=this._onKeyDown.bind(this);}
WebInspector.SourcesPanel.prototype={defaultFocusedElement:function()
{return this._editorContainer.view.defaultFocusedElement()||this._navigator.view.defaultFocusedElement();},get paused()
{return this._paused;},wasShown:function()
{WebInspector.inspectorView.closeViewInDrawer("editor");this.sourcesView.show(this.editorView.mainElement);WebInspector.Panel.prototype.wasShown.call(this);this._navigatorController.wasShown();this.element.addEventListener("keydown",this._boundOnKeyDown,false);this.element.addEventListener("keyup",this._boundOnKeyUp,false);},willHide:function()
{this.element.removeEventListener("keydown",this._boundOnKeyDown,false);this.element.removeEventListener("keyup",this._boundOnKeyUp,false);WebInspector.Panel.prototype.willHide.call(this);},_uiSourceCodeAdded:function(event)
{var uiSourceCode=(event.data);this._addUISourceCode(uiSourceCode);},_addUISourceCode:function(uiSourceCode)
{if(this._toggleFormatSourceButton.toggled)
uiSourceCode.setFormatted(true);if(uiSourceCode.project().isServiceProject())
return;this._navigator.addUISourceCode(uiSourceCode);this._editorContainer.addUISourceCode(uiSourceCode);var currentUISourceCode=this._currentUISourceCode;if(currentUISourceCode&&currentUISourceCode.project().isServiceProject()&&currentUISourceCode!==uiSourceCode&&currentUISourceCode.url===uiSourceCode.url){this._showFile(uiSourceCode);this._editorContainer.removeUISourceCode(currentUISourceCode);}},_uiSourceCodeRemoved:function(event)
{var uiSourceCode=(event.data);this._removeUISourceCodes([uiSourceCode]);},_removeUISourceCodes:function(uiSourceCodes)
{for(var i=0;i<uiSourceCodes.length;++i){this._navigator.removeUISourceCode(uiSourceCodes[i]);this._removeSourceFrame(uiSourceCodes[i]);}
this._editorContainer.removeUISourceCodes(uiSourceCodes);},_consoleCommandEvaluatedInSelectedCallFrame:function(event)
{this.sidebarPanes.scopechain.update(WebInspector.debuggerModel.selectedCallFrame());},_debuggerPaused:function()
{WebInspector.inspectorView.setCurrentPanel(this);this._showDebuggerPausedDetails();},_showDebuggerPausedDetails:function()
{var details=WebInspector.debuggerModel.debuggerPausedDetails();this._paused=true;this._waitingToPause=false;this._stepping=false;this._updateDebuggerButtons();this.sidebarPanes.callstack.update(details.callFrames);if(details.reason===WebInspector.DebuggerModel.BreakReason.DOM){WebInspector.domBreakpointsSidebarPane.highlightBreakpoint(details.auxData);function didCreateBreakpointHitStatusMessage(element)
{this.sidebarPanes.callstack.setStatus(element);}
WebInspector.domBreakpointsSidebarPane.createBreakpointHitStatusMessage(details.auxData,didCreateBreakpointHitStatusMessage.bind(this));}else if(details.reason===WebInspector.DebuggerModel.BreakReason.EventListener){var eventName=details.auxData.eventName;this.sidebarPanes.eventListenerBreakpoints.highlightBreakpoint(details.auxData.eventName);var eventNameForUI=WebInspector.EventListenerBreakpointsSidebarPane.eventNameForUI(eventName,details.auxData);this.sidebarPanes.callstack.setStatus(WebInspector.UIString("Paused on a \"%s\" Event Listener.",eventNameForUI));}else if(details.reason===WebInspector.DebuggerModel.BreakReason.XHR){this.sidebarPanes.xhrBreakpoints.highlightBreakpoint(details.auxData["breakpointURL"]);this.sidebarPanes.callstack.setStatus(WebInspector.UIString("Paused on a XMLHttpRequest."));}else if(details.reason===WebInspector.DebuggerModel.BreakReason.Exception)
this.sidebarPanes.callstack.setStatus(WebInspector.UIString("Paused on exception: '%s'.",details.auxData.description));else if(details.reason===WebInspector.DebuggerModel.BreakReason.Assert)
this.sidebarPanes.callstack.setStatus(WebInspector.UIString("Paused on assertion."));else if(details.reason===WebInspector.DebuggerModel.BreakReason.CSPViolation)
this.sidebarPanes.callstack.setStatus(WebInspector.UIString("Paused on a script blocked due to Content Security Policy directive: \"%s\".",details.auxData["directiveText"]));else if(details.reason===WebInspector.DebuggerModel.BreakReason.DebugCommand)
this.sidebarPanes.callstack.setStatus(WebInspector.UIString("Paused on a debugged function"));else{function didGetUILocation(uiLocation)
{var breakpoint=WebInspector.breakpointManager.findBreakpoint(uiLocation.uiSourceCode,uiLocation.lineNumber);if(!breakpoint)
return;this.sidebarPanes.jsBreakpoints.highlightBreakpoint(breakpoint);this.sidebarPanes.callstack.setStatus(WebInspector.UIString("Paused on a JavaScript breakpoint."));}
if(details.callFrames.length)
details.callFrames[0].createLiveLocation(didGetUILocation.bind(this));else
console.warn("ScriptsPanel paused, but callFrames.length is zero.");}
this._enableDebuggerSidebar(true);this._toggleDebuggerSidebarButton.setEnabled(false);window.focus();InspectorFrontendHost.bringToFront();},_debuggerResumed:function()
{this._paused=false;this._waitingToPause=false;this._stepping=false;this._clearInterface();this._toggleDebuggerSidebarButton.setEnabled(true);},_debuggerWasEnabled:function()
{this._updateDebuggerButtons();},_debuggerWasDisabled:function()
{this._debuggerReset();},_debuggerReset:function()
{this._debuggerResumed();this.sidebarPanes.watchExpressions.reset();delete this._skipExecutionLineRevealing;},_projectWillReset:function(event)
{var project=event.data;var uiSourceCodes=project.uiSourceCodes();this._removeUISourceCodes(uiSourceCodes);if(project.type()===WebInspector.projectTypes.Network)
this._editorContainer.reset();},get visibleView()
{return this._editorContainer.visibleView;},_updateScriptViewStatusBarItems:function()
{this._scriptViewStatusBarItemsContainer.removeChildren();this._scriptViewStatusBarTextContainer.removeChildren();var sourceFrame=this.visibleView;if(sourceFrame){var statusBarItems=sourceFrame.statusBarItems()||[];for(var i=0;i<statusBarItems.length;++i)
this._scriptViewStatusBarItemsContainer.appendChild(statusBarItems[i]);var statusBarText=sourceFrame.statusBarText();if(statusBarText)
this._scriptViewStatusBarTextContainer.appendChild(statusBarText);}},showAnchorLocation:function(anchor)
{if(!anchor.uiSourceCode){var uiSourceCode=WebInspector.workspace.uiSourceCodeForURL(anchor.href);if(uiSourceCode)
anchor.uiSourceCode=uiSourceCode;}
if(!anchor.uiSourceCode)
return false;this._showSourceLocation(anchor.uiSourceCode,anchor.lineNumber,anchor.columnNumber);return true;},showUISourceCode:function(uiSourceCode,lineNumber,columnNumber,forceShowInPanel)
{this._showSourceLocation(uiSourceCode,lineNumber,columnNumber,forceShowInPanel);},_showEditor:function(forceShowInPanel)
{if(this.sourcesView.isShowing())
return;if(this._canShowEditorInDrawer()&&!forceShowInPanel){var drawerEditorView=new WebInspector.DrawerEditorView();this.sourcesView.show(drawerEditorView.element);WebInspector.inspectorView.showCloseableViewInDrawer("editor",WebInspector.UIString("Editor"),drawerEditorView);}else{WebInspector.showPanel("sources");}},currentUISourceCode:function()
{return this._currentUISourceCode;},showUILocation:function(uiLocation,forceShowInPanel)
{this._showSourceLocation(uiLocation.uiSourceCode,uiLocation.lineNumber,uiLocation.columnNumber,forceShowInPanel);},_canShowEditorInDrawer:function()
{return WebInspector.experimentsSettings.showEditorInDrawer.isEnabled()&&WebInspector.settings.showEditorInDrawer.get();},_showSourceLocation:function(uiSourceCode,lineNumber,columnNumber,forceShowInPanel)
{this._showEditor(forceShowInPanel);var sourceFrame=this._showFile(uiSourceCode);if(typeof lineNumber==="number")
sourceFrame.highlightPosition(lineNumber,columnNumber);sourceFrame.focus();WebInspector.notifications.dispatchEventToListeners(WebInspector.UserMetrics.UserAction,{action:WebInspector.UserMetrics.UserActionNames.OpenSourceLink,url:uiSourceCode.originURL(),lineNumber:lineNumber});},_showFile:function(uiSourceCode)
{var sourceFrame=this._getOrCreateSourceFrame(uiSourceCode);if(this._currentUISourceCode===uiSourceCode)
return sourceFrame;this._currentUISourceCode=uiSourceCode;if(!uiSourceCode.project().isServiceProject())
this._navigator.revealUISourceCode(uiSourceCode,true);this._editorContainer.showFile(uiSourceCode);this._updateScriptViewStatusBarItems();if(this._currentUISourceCode.project().type()===WebInspector.projectTypes.Snippets)
this._runSnippetButton.element.removeStyleClass("hidden");else
this._runSnippetButton.element.addStyleClass("hidden");return sourceFrame;},_createSourceFrame:function(uiSourceCode)
{var sourceFrame;switch(uiSourceCode.contentType()){case WebInspector.resourceTypes.Script:sourceFrame=new WebInspector.JavaScriptSourceFrame(this,uiSourceCode);break;case WebInspector.resourceTypes.Document:sourceFrame=new WebInspector.JavaScriptSourceFrame(this,uiSourceCode);break;case WebInspector.resourceTypes.Stylesheet:sourceFrame=new WebInspector.CSSSourceFrame(uiSourceCode);break;default:sourceFrame=new WebInspector.UISourceCodeFrame(uiSourceCode);break;}
sourceFrame.setHighlighterType(uiSourceCode.highlighterType());this._sourceFramesByUISourceCode.put(uiSourceCode,sourceFrame);return sourceFrame;},_getOrCreateSourceFrame:function(uiSourceCode)
{return this._sourceFramesByUISourceCode.get(uiSourceCode)||this._createSourceFrame(uiSourceCode);},_sourceFrameMatchesUISourceCode:function(sourceFrame,uiSourceCode)
{switch(uiSourceCode.contentType()){case WebInspector.resourceTypes.Script:case WebInspector.resourceTypes.Document:return sourceFrame instanceof WebInspector.JavaScriptSourceFrame;case WebInspector.resourceTypes.Stylesheet:return sourceFrame instanceof WebInspector.CSSSourceFrame;default:return!(sourceFrame instanceof WebInspector.JavaScriptSourceFrame);}},_recreateSourceFrameIfNeeded:function(uiSourceCode)
{var oldSourceFrame=this._sourceFramesByUISourceCode.get(uiSourceCode);if(!oldSourceFrame)
return;if(this._sourceFrameMatchesUISourceCode(oldSourceFrame,uiSourceCode)){oldSourceFrame.setHighlighterType(uiSourceCode.highlighterType());}else{this._editorContainer.removeUISourceCode(uiSourceCode);this._removeSourceFrame(uiSourceCode);}},viewForFile:function(uiSourceCode)
{return this._getOrCreateSourceFrame(uiSourceCode);},_removeSourceFrame:function(uiSourceCode)
{var sourceFrame=this._sourceFramesByUISourceCode.get(uiSourceCode);if(!sourceFrame)
return;this._sourceFramesByUISourceCode.remove(uiSourceCode);sourceFrame.dispose();},_clearCurrentExecutionLine:function()
{if(this._executionSourceFrame)
this._executionSourceFrame.clearExecutionLine();delete this._executionSourceFrame;},_setExecutionLine:function(uiLocation)
{var callFrame=WebInspector.debuggerModel.selectedCallFrame()
var sourceFrame=this._getOrCreateSourceFrame(uiLocation.uiSourceCode);sourceFrame.setExecutionLine(uiLocation.lineNumber,callFrame);this._executionSourceFrame=sourceFrame;},_executionLineChanged:function(uiLocation)
{this._clearCurrentExecutionLine();this._setExecutionLine(uiLocation);var uiSourceCode=uiLocation.uiSourceCode;var scriptFile=this._currentUISourceCode?this._currentUISourceCode.scriptFile():null;if(this._skipExecutionLineRevealing)
return;this._skipExecutionLineRevealing=true;var sourceFrame=this._showFile(uiSourceCode);sourceFrame.revealLine(uiLocation.lineNumber);if(sourceFrame.canEditSource())
sourceFrame.setSelection(WebInspector.TextRange.createFromLocation(uiLocation.lineNumber,0));sourceFrame.focus();},_callFrameSelected:function(event)
{var callFrame=event.data;if(!callFrame)
return;this.sidebarPanes.scopechain.update(callFrame);this.sidebarPanes.watchExpressions.refreshExpressions();this.sidebarPanes.callstack.setSelectedCallFrame(callFrame);callFrame.createLiveLocation(this._executionLineChanged.bind(this));},_editorClosed:function(event)
{this._navigatorController.hideNavigatorOverlay();var uiSourceCode=(event.data);if(this._currentUISourceCode===uiSourceCode)
delete this._currentUISourceCode;this._updateScriptViewStatusBarItems();WebInspector.searchController.resetSearch();},_editorSelected:function(event)
{var uiSourceCode=(event.data);var sourceFrame=this._showFile(uiSourceCode);this._navigatorController.hideNavigatorOverlay();if(!this._navigatorController.isNavigatorPinned())
sourceFrame.focus();WebInspector.searchController.resetSearch();},_sourceSelected:function(event)
{var uiSourceCode=(event.data.uiSourceCode);var sourceFrame=this._showFile(uiSourceCode);this._navigatorController.hideNavigatorOverlay();if(sourceFrame&&(!this._navigatorController.isNavigatorPinned()||event.data.focusSource))
sourceFrame.focus();},_itemSearchStarted:function(event)
{var searchText=(event.data);WebInspector.OpenResourceDialog.show(this,this.editorView.mainElement,searchText);},_pauseOnExceptionStateChanged:function()
{var pauseOnExceptionsState=WebInspector.settings.pauseOnExceptionStateString.get();switch(pauseOnExceptionsState){case WebInspector.DebuggerModel.PauseOnExceptionsState.DontPauseOnExceptions:this._pauseOnExceptionButton.title=WebInspector.UIString("Don't pause on exceptions.\nClick to Pause on all exceptions.");break;case WebInspector.DebuggerModel.PauseOnExceptionsState.PauseOnAllExceptions:this._pauseOnExceptionButton.title=WebInspector.UIString("Pause on all exceptions.\nClick to Pause on uncaught exceptions.");break;case WebInspector.DebuggerModel.PauseOnExceptionsState.PauseOnUncaughtExceptions:this._pauseOnExceptionButton.title=WebInspector.UIString("Pause on uncaught exceptions.\nClick to Not pause on exceptions.");break;}
this._pauseOnExceptionButton.state=pauseOnExceptionsState;},_updateDebuggerButtons:function()
{if(this._paused){this._updateButtonTitle(this._pauseButton,WebInspector.UIString("Resume script execution (%s)."))
this._pauseButton.state=true;this._pauseButton.setLongClickOptionsEnabled((function(){return[this._longResumeButton]}).bind(this));this._pauseButton.setEnabled(true);this._stepOverButton.setEnabled(true);this._stepIntoButton.setEnabled(true);this._stepOutButton.setEnabled(true);}else{this._updateButtonTitle(this._pauseButton,WebInspector.UIString("Pause script execution (%s)."))
this._pauseButton.state=false;this._pauseButton.setLongClickOptionsEnabled(null);this._pauseButton.setEnabled(!this._waitingToPause);this._stepOverButton.setEnabled(false);this._stepIntoButton.setEnabled(false);this._stepOutButton.setEnabled(false);}},_clearInterface:function()
{this.sidebarPanes.callstack.update(null);this.sidebarPanes.scopechain.update(null);this.sidebarPanes.jsBreakpoints.clearBreakpointHighlight();WebInspector.domBreakpointsSidebarPane.clearBreakpointHighlight();this.sidebarPanes.eventListenerBreakpoints.clearBreakpointHighlight();this.sidebarPanes.xhrBreakpoints.clearBreakpointHighlight();this._clearCurrentExecutionLine();this._updateDebuggerButtons();},_togglePauseOnExceptions:function()
{var nextStateMap={};var stateEnum=WebInspector.DebuggerModel.PauseOnExceptionsState;nextStateMap[stateEnum.DontPauseOnExceptions]=stateEnum.PauseOnAllExceptions;nextStateMap[stateEnum.PauseOnAllExceptions]=stateEnum.PauseOnUncaughtExceptions;nextStateMap[stateEnum.PauseOnUncaughtExceptions]=stateEnum.DontPauseOnExceptions;WebInspector.settings.pauseOnExceptionStateString.set(nextStateMap[this._pauseOnExceptionButton.state]);},_runSnippet:function(event)
{if(this._currentUISourceCode.project().type()!==WebInspector.projectTypes.Snippets)
return false;WebInspector.scriptSnippetModel.evaluateScriptSnippet(this._currentUISourceCode);return true;},_togglePause:function(event)
{if(this._paused){delete this._skipExecutionLineRevealing;this._paused=false;this._waitingToPause=false;WebInspector.debuggerModel.resume();}else{this._stepping=false;this._waitingToPause=true;WebInspector.debuggerModel.skipAllPauses(false);DebuggerAgent.pause();}
this._clearInterface();return true;},_longResume:function(event)
{if(!this._paused)
return true;this._paused=false;this._waitingToPause=false;WebInspector.debuggerModel.skipAllPausesUntilReloadOrTimeout(500);WebInspector.debuggerModel.resume();this._clearInterface();return true;},_stepOverClicked:function(event)
{if(!this._paused)
return true;delete this._skipExecutionLineRevealing;this._paused=false;this._stepping=true;this._clearInterface();WebInspector.debuggerModel.stepOver();return true;},_stepIntoClicked:function(event)
{if(!this._paused)
return true;delete this._skipExecutionLineRevealing;this._paused=false;this._stepping=true;this._clearInterface();WebInspector.debuggerModel.stepInto();return true;},_stepIntoSelectionClicked:function(event)
{if(!this._paused)
return true;if(this._executionSourceFrame){var stepIntoMarkup=this._executionSourceFrame.stepIntoMarkup();if(stepIntoMarkup)
stepIntoMarkup.iterateSelection(event.shiftKey);}
return true;},doStepIntoSelection:function(rawLocation)
{if(!this._paused)
return;delete this._skipExecutionLineRevealing;this._paused=false;this._stepping=true;this._clearInterface();WebInspector.debuggerModel.stepIntoSelection(rawLocation);},_stepOutClicked:function(event)
{if(!this._paused)
return true;delete this._skipExecutionLineRevealing;this._paused=false;this._stepping=true;this._clearInterface();WebInspector.debuggerModel.stepOut();return true;},_callFrameSelectedInSidebar:function(event)
{var callFrame=(event.data);delete this._skipExecutionLineRevealing;WebInspector.debuggerModel.setSelectedCallFrame(callFrame);},continueToLocation:function(rawLocation)
{if(!this._paused)
return;delete this._skipExecutionLineRevealing;this._paused=false;this._stepping=true;this._clearInterface();WebInspector.debuggerModel.continueToLocation(rawLocation);},_toggleBreakpointsClicked:function(event)
{WebInspector.debuggerModel.setBreakpointsActive(!WebInspector.debuggerModel.breakpointsActive());},_breakpointsActiveStateChanged:function(event)
{var active=event.data;this._toggleBreakpointsButton.toggled=!active;if(active){this._toggleBreakpointsButton.title=WebInspector.UIString("Deactivate breakpoints.");WebInspector.inspectorView.element.removeStyleClass("breakpoints-deactivated");this.sidebarPanes.jsBreakpoints.listElement.removeStyleClass("breakpoints-list-deactivated");}else{this._toggleBreakpointsButton.title=WebInspector.UIString("Activate breakpoints.");WebInspector.inspectorView.element.addStyleClass("breakpoints-deactivated");this.sidebarPanes.jsBreakpoints.listElement.addStyleClass("breakpoints-list-deactivated");}},_createDebugToolbar:function()
{var debugToolbar=document.createElement("div");debugToolbar.className="status-bar";debugToolbar.id="scripts-debug-toolbar";var title,handler;var platformSpecificModifier=WebInspector.KeyboardShortcut.Modifiers.CtrlOrMeta;title=WebInspector.UIString("Run snippet (%s).");handler=this._runSnippet.bind(this);this._runSnippetButton=this._createButtonAndRegisterShortcuts("scripts-run-snippet",title,handler,WebInspector.SourcesPanelDescriptor.ShortcutKeys.RunSnippet);debugToolbar.appendChild(this._runSnippetButton.element);this._runSnippetButton.element.addStyleClass("hidden");handler=this._togglePause.bind(this);this._pauseButton=this._createButtonAndRegisterShortcuts("scripts-pause","",handler,WebInspector.SourcesPanelDescriptor.ShortcutKeys.PauseContinue);debugToolbar.appendChild(this._pauseButton.element);title=WebInspector.UIString("Resume with all pauses blocked for 500 ms");this._longResumeButton=new WebInspector.StatusBarButton(title,"scripts-long-resume");this._longResumeButton.addEventListener("click",this._longResume.bind(this),this);title=WebInspector.UIString("Step over next function call (%s).");handler=this._stepOverClicked.bind(this);this._stepOverButton=this._createButtonAndRegisterShortcuts("scripts-step-over",title,handler,WebInspector.SourcesPanelDescriptor.ShortcutKeys.StepOver);debugToolbar.appendChild(this._stepOverButton.element);title=WebInspector.UIString("Step into next function call (%s).");handler=this._stepIntoClicked.bind(this);this._stepIntoButton=this._createButtonAndRegisterShortcuts("scripts-step-into",title,handler,WebInspector.SourcesPanelDescriptor.ShortcutKeys.StepInto);debugToolbar.appendChild(this._stepIntoButton.element);this.registerShortcuts(WebInspector.SourcesPanelDescriptor.ShortcutKeys.StepIntoSelection,this._stepIntoSelectionClicked.bind(this))
title=WebInspector.UIString("Step out of current function (%s).");handler=this._stepOutClicked.bind(this);this._stepOutButton=this._createButtonAndRegisterShortcuts("scripts-step-out",title,handler,WebInspector.SourcesPanelDescriptor.ShortcutKeys.StepOut);debugToolbar.appendChild(this._stepOutButton.element);this._toggleBreakpointsButton=new WebInspector.StatusBarButton(WebInspector.UIString("Deactivate breakpoints."),"scripts-toggle-breakpoints");this._toggleBreakpointsButton.toggled=false;this._toggleBreakpointsButton.addEventListener("click",this._toggleBreakpointsClicked,this);debugToolbar.appendChild(this._toggleBreakpointsButton.element);this._pauseOnExceptionButton=new WebInspector.StatusBarButton("","scripts-pause-on-exceptions-status-bar-item",3);this._pauseOnExceptionButton.addEventListener("click",this._togglePauseOnExceptions,this);debugToolbar.appendChild(this._pauseOnExceptionButton.element);return debugToolbar;},_updateButtonTitle:function(button,buttonTitle)
{var hasShortcuts=button.shortcuts&&button.shortcuts.length;if(hasShortcuts)
button.title=String.vsprintf(buttonTitle,[button.shortcuts[0].name]);else
button.title=buttonTitle;},_createButtonAndRegisterShortcuts:function(buttonId,buttonTitle,handler,shortcuts)
{var button=new WebInspector.StatusBarButton(buttonTitle,buttonId);button.element.addEventListener("click",handler,false);button.shortcuts=shortcuts;this._updateButtonTitle(button,buttonTitle);this.registerShortcuts(shortcuts,handler);return button;},searchCanceled:function()
{if(this._searchView)
this._searchView.searchCanceled();delete this._searchView;delete this._searchQuery;},performSearch:function(query,shouldJump)
{WebInspector.searchController.updateSearchMatchesCount(0,this);if(!this.visibleView)
return;this._searchView=this.visibleView;this._searchQuery=query;function finishedCallback(view,searchMatches)
{if(!searchMatches)
return;WebInspector.searchController.updateSearchMatchesCount(searchMatches,this);}
function currentMatchChanged(currentMatchIndex)
{WebInspector.searchController.updateCurrentMatchIndex(currentMatchIndex,this);}
this._searchView.performSearch(query,shouldJump,finishedCallback.bind(this),currentMatchChanged.bind(this));},minimalSearchQuerySize:function()
{return 0;},jumpToNextSearchResult:function()
{if(!this._searchView)
return;if(this._searchView!==this.visibleView){this.performSearch(this._searchQuery,true);return;}
this._searchView.jumpToNextSearchResult();return true;},jumpToPreviousSearchResult:function()
{if(!this._searchView)
return;if(this._searchView!==this.visibleView){this.performSearch(this._searchQuery,true);if(this._searchView)
this._searchView.jumpToLastSearchResult();return;}
this._searchView.jumpToPreviousSearchResult();},canSearchAndReplace:function()
{var view=(this.visibleView);return!!view&&view.canEditSource();},replaceSelectionWith:function(text)
{var view=(this.visibleView);view.replaceSearchMatchWith(text);},replaceAllWith:function(query,text)
{var view=(this.visibleView);view.replaceAllWith(query,text);},_onKeyDown:function(event)
{if(event.keyCode!==WebInspector.KeyboardShortcut.Keys.CtrlOrMeta.code)
return;if(!this._paused||!this._executionSourceFrame)
return;var stepIntoMarkup=this._executionSourceFrame.stepIntoMarkup();if(stepIntoMarkup)
stepIntoMarkup.startIteratingSelection();},_onKeyUp:function(event)
{if(event.keyCode!==WebInspector.KeyboardShortcut.Keys.CtrlOrMeta.code)
return;if(!this._paused||!this._executionSourceFrame)
return;var stepIntoMarkup=this._executionSourceFrame.stepIntoMarkup();if(!stepIntoMarkup)
return;var currentPosition=stepIntoMarkup.getSelectedItemIndex();if(typeof currentPosition==="undefined"){stepIntoMarkup.stopIteratingSelection();}else{var rawLocation=stepIntoMarkup.getRawPosition(currentPosition);this.doStepIntoSelection(rawLocation);}},_toggleFormatSource:function()
{delete this._skipExecutionLineRevealing;this._toggleFormatSourceButton.toggled=!this._toggleFormatSourceButton.toggled;var uiSourceCodes=this._workspace.uiSourceCodes();for(var i=0;i<uiSourceCodes.length;++i)
uiSourceCodes[i].setFormatted(this._toggleFormatSourceButton.toggled);var currentFile=this._editorContainer.currentFile();WebInspector.notifications.dispatchEventToListeners(WebInspector.UserMetrics.UserAction,{action:WebInspector.UserMetrics.UserActionNames.TogglePrettyPrint,enabled:this._toggleFormatSourceButton.toggled,url:currentFile?currentFile.originURL():null});},addToWatch:function(expression)
{this.sidebarPanes.watchExpressions.addExpression(expression);},_toggleBreakpoint:function()
{var sourceFrame=this.visibleView;if(!sourceFrame)
return false;if(sourceFrame instanceof WebInspector.JavaScriptSourceFrame){var javaScriptSourceFrame=(sourceFrame);javaScriptSourceFrame.toggleBreakpointOnCurrentLine();return true;}
return false;},_showOutlineDialog:function(event)
{var uiSourceCode=this._editorContainer.currentFile();if(!uiSourceCode)
return false;switch(uiSourceCode.contentType()){case WebInspector.resourceTypes.Document:case WebInspector.resourceTypes.Script:WebInspector.JavaScriptOutlineDialog.show(this.visibleView,uiSourceCode);return true;case WebInspector.resourceTypes.Stylesheet:WebInspector.StyleSheetOutlineDialog.show(this.visibleView,uiSourceCode);return true;}
return false;},_installDebuggerSidebarController:function()
{this._toggleDebuggerSidebarButton=new WebInspector.StatusBarButton("","right-sidebar-show-hide-button scripts-debugger-show-hide-button",3);this._toggleDebuggerSidebarButton.addEventListener("click",clickHandler,this);this.editorView.element.appendChild(this._toggleDebuggerSidebarButton.element);this._enableDebuggerSidebar(!WebInspector.settings.debuggerSidebarHidden.get());function clickHandler()
{this._enableDebuggerSidebar(this._toggleDebuggerSidebarButton.state==="left");}},_enableDebuggerSidebar:function(show)
{this._toggleDebuggerSidebarButton.state=show?"right":"left";this._toggleDebuggerSidebarButton.title=show?WebInspector.UIString("Hide debugger"):WebInspector.UIString("Show debugger");if(show)
this.splitView.showSidebarElement();else
this.splitView.hideSidebarElement();this._debugSidebarResizeWidgetElement.enableStyleClass("hidden",!show);WebInspector.settings.debuggerSidebarHidden.set(!show);},_itemCreationRequested:function(event)
{var project=event.data.project;var path=event.data.path;var uiSourceCodeToCopy=event.data.uiSourceCode;var filePath;var shouldHideNavigator;var uiSourceCode;if(uiSourceCodeToCopy){function contentLoaded(content)
{createFile.call(this,content||"");}
uiSourceCodeToCopy.requestContent(contentLoaded.bind(this));}else{createFile.call(this);}
function createFile(content)
{project.createFile(path,null,content||"",fileCreated.bind(this));}
function fileCreated(path)
{if(!path)
return;filePath=path;uiSourceCode=project.uiSourceCode(filePath);this._showSourceLocation(uiSourceCode);shouldHideNavigator=!this._navigatorController.isNavigatorPinned();if(this._navigatorController.isNavigatorHidden())
this._navigatorController.showNavigatorOverlay();this._navigator.rename(uiSourceCode,callback.bind(this));}
function callback(committed)
{if(shouldHideNavigator)
this._navigatorController.hideNavigatorOverlay();if(!committed){project.deleteFile(uiSourceCode);return;}
this._recreateSourceFrameIfNeeded(uiSourceCode);this._navigator.updateIcon(uiSourceCode);this._showSourceLocation(uiSourceCode);}},_itemRenamingRequested:function(event)
{var uiSourceCode=(event.data);var shouldHideNavigator=!this._navigatorController.isNavigatorPinned();if(this._navigatorController.isNavigatorHidden())
this._navigatorController.showNavigatorOverlay();this._navigator.rename(uiSourceCode,callback.bind(this));function callback(committed)
{if(shouldHideNavigator&&committed)
this._navigatorController.hideNavigatorOverlay();this._recreateSourceFrameIfNeeded(uiSourceCode);this._navigator.updateIcon(uiSourceCode);this._showSourceLocation(uiSourceCode);}},_showLocalHistory:function(uiSourceCode)
{WebInspector.RevisionHistoryView.showHistory(uiSourceCode);},appendApplicableItems:function(event,contextMenu,target)
{this._appendUISourceCodeItems(contextMenu,target);this._appendFunctionItems(contextMenu,target);},_mapFileSystemToNetwork:function(uiSourceCode)
{WebInspector.SelectUISourceCodeForProjectTypeDialog.show(uiSourceCode.name(),WebInspector.projectTypes.Network,mapFileSystemToNetwork.bind(this),this.editorView.mainElement)
function mapFileSystemToNetwork(networkUISourceCode)
{this._workspace.addMapping(networkUISourceCode,uiSourceCode,WebInspector.fileSystemWorkspaceProvider);}},_removeNetworkMapping:function(uiSourceCode)
{if(confirm(WebInspector.UIString("Are you sure you want to remove network mapping?")))
this._workspace.removeMapping(uiSourceCode);},_mapNetworkToFileSystem:function(networkUISourceCode)
{WebInspector.SelectUISourceCodeForProjectTypeDialog.show(networkUISourceCode.name(),WebInspector.projectTypes.FileSystem,mapNetworkToFileSystem.bind(this),this.editorView.mainElement)
function mapNetworkToFileSystem(uiSourceCode)
{this._workspace.addMapping(networkUISourceCode,uiSourceCode,WebInspector.fileSystemWorkspaceProvider);}},_appendUISourceCodeMappingItems:function(contextMenu,uiSourceCode)
{if(uiSourceCode.project().type()===WebInspector.projectTypes.FileSystem){var hasMappings=!!uiSourceCode.url;if(!hasMappings)
contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Map to network resource\u2026":"Map to Network Resource\u2026"),this._mapFileSystemToNetwork.bind(this,uiSourceCode));else
contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Remove network mapping":"Remove Network Mapping"),this._removeNetworkMapping.bind(this,uiSourceCode));}
if(uiSourceCode.project().type()===WebInspector.projectTypes.Network){function filterProject(project)
{return project.type()===WebInspector.projectTypes.FileSystem;}
if(!this._workspace.projects().filter(filterProject).length)
return;if(this._workspace.uiSourceCodeForURL(uiSourceCode.url)===uiSourceCode)
contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Map to file system resource\u2026":"Map to File System Resource\u2026"),this._mapNetworkToFileSystem.bind(this,uiSourceCode));}},_appendUISourceCodeItems:function(contextMenu,target)
{if(!(target instanceof WebInspector.UISourceCode))
return;var uiSourceCode=(target);contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Local modifications\u2026":"Local Modifications\u2026"),this._showLocalHistory.bind(this,uiSourceCode));if(WebInspector.isolatedFileSystemManager.supportsFileSystems())
this._appendUISourceCodeMappingItems(contextMenu,uiSourceCode);},_appendFunctionItems:function(contextMenu,target)
{if(!(target instanceof WebInspector.RemoteObject))
return;var remoteObject=(target);if(remoteObject.type!=="function")
return;function didGetDetails(error,response)
{if(error){console.error(error);return;}
WebInspector.inspectorView.setCurrentPanel(this);var uiLocation=WebInspector.debuggerModel.rawLocationToUILocation(response.location);this._showSourceLocation(uiLocation.uiSourceCode,uiLocation.lineNumber,uiLocation.columnNumber);}
function revealFunction()
{DebuggerAgent.getFunctionDetails(remoteObject.objectId,didGetDetails.bind(this));}
contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Show function definition":"Show Function Definition"),revealFunction.bind(this));},showGoToSourceDialog:function()
{var uiSourceCodes=this._editorContainer.historyUISourceCodes();var defaultScores=new Map();for(var i=1;i<uiSourceCodes.length;++i)
defaultScores.put(uiSourceCodes[i],uiSourceCodes.length-i);WebInspector.OpenResourceDialog.show(this,this.editorView.mainElement,undefined,defaultScores);},_dockSideChanged:function()
{var dockSide=WebInspector.dockController.dockSide();var vertically=dockSide===WebInspector.DockController.State.DockedToRight&&WebInspector.settings.splitVerticallyWhenDockedToRight.get();this._splitVertically(vertically);},_splitVertically:function(vertically)
{if(this.sidebarPaneView&&vertically===!this.splitView.isVertical())
return;if(this.sidebarPaneView)
this.sidebarPaneView.detach();this.splitView.setVertical(!vertically);if(!vertically){this.sidebarPaneView=new WebInspector.SidebarPaneStack();for(var pane in this.sidebarPanes)
this.sidebarPaneView.addPane(this.sidebarPanes[pane]);this._extensionSidebarPanesContainer=this.sidebarPaneView;this.sidebarElement.appendChild(this.debugToolbar);}else{this._enableDebuggerSidebar(true);this.sidebarPaneView=new WebInspector.SplitView(true,this.name+"PanelSplitSidebarRatio",0.5);var group1=new WebInspector.SidebarPaneStack();group1.show(this.sidebarPaneView.firstElement());group1.element.id="scripts-sidebar-stack-pane";group1.addPane(this.sidebarPanes.callstack);group1.addPane(this.sidebarPanes.jsBreakpoints);group1.addPane(this.sidebarPanes.domBreakpoints);group1.addPane(this.sidebarPanes.xhrBreakpoints);group1.addPane(this.sidebarPanes.eventListenerBreakpoints);if(this.sidebarPanes.workerList)
group1.addPane(this.sidebarPanes.workerList);var group2=new WebInspector.SidebarTabbedPane();group2.show(this.sidebarPaneView.secondElement());group2.addPane(this.sidebarPanes.scopechain);group2.addPane(this.sidebarPanes.watchExpressions);this._extensionSidebarPanesContainer=group2;this.sidebarPaneView.firstElement().appendChild(this.debugToolbar);}
for(var i=0;i<this._extensionSidebarPanes.length;++i)
this._extensionSidebarPanesContainer.addPane(this._extensionSidebarPanes[i]);this.sidebarPaneView.element.id="scripts-debug-sidebar-contents";this.sidebarPaneView.show(this.splitView.sidebarElement);this.sidebarPanes.scopechain.expand();this.sidebarPanes.jsBreakpoints.expand();this.sidebarPanes.callstack.expand();if(WebInspector.settings.watchExpressions.get().length>0)
this.sidebarPanes.watchExpressions.expand();},canSetFooterElement:function()
{return true;},setFooterElement:function(element)
{if(element){this._editorFooterElement.removeStyleClass("hidden");this._editorFooterElement.appendChild(element);}else{this._editorFooterElement.addStyleClass("hidden");this._editorFooterElement.removeChildren();}
this.doResize();},addExtensionSidebarPane:function(id,pane)
{this._extensionSidebarPanes.push(pane);this._extensionSidebarPanesContainer.addPane(pane);this.setHideOnDetach();},get tabbedEditorContainer()
{return this._editorContainer;},__proto__:WebInspector.Panel.prototype}
WebInspector.SourcesView=function()
{WebInspector.View.call(this);this.registerRequiredCSS("sourcesView.css");this.element.id="sources-panel-sources-view";this.element.addStyleClass("vbox");}
WebInspector.SourcesView.prototype={__proto__:WebInspector.View.prototype}
WebInspector.DrawerEditorView=function()
{WebInspector.View.call(this);this.element.id="drawer-editor-view";this.element.addStyleClass("vbox");}
WebInspector.DrawerEditorView.prototype={__proto__:WebInspector.View.prototype}