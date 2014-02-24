WebInspector.ProfileType=function(id,name)
{this._id=id;this._name=name;this._profiles=[];this._profilesIdMap={};this.treeElement=null;}
WebInspector.ProfileType.Events={AddProfileHeader:"add-profile-header",RemoveProfileHeader:"remove-profile-header",ProgressUpdated:"progress-updated",ViewUpdated:"view-updated"}
WebInspector.ProfileType.prototype={hasTemporaryView:function()
{return false;},fileExtension:function()
{return null;},get statusBarItems()
{return[];},get buttonTooltip()
{return"";},get id()
{return this._id;},get treeItemTitle()
{return this._name;},get name()
{return this._name;},buttonClicked:function()
{return false;},get description()
{return"";},isInstantProfile:function()
{return false;},isEnabled:function()
{return true;},getProfiles:function()
{return this._profiles.filter(function(profile){return!profile.isTemporary;});},decorationElement:function()
{return null;},getProfile:function(uid)
{return this._profilesIdMap[this._makeKey(uid)];},createTemporaryProfile:function(title)
{throw new Error("Needs implemented.");},createProfile:function(profile)
{throw new Error("Not supported for "+this._name+" profiles.");},_makeKey:function(id)
{return id+'/'+escape(this.id);},addProfile:function(profile)
{this._profiles.push(profile);this._profilesIdMap[this._makeKey(profile.uid)]=profile;this.dispatchEventToListeners(WebInspector.ProfileType.Events.AddProfileHeader,profile);},removeProfile:function(profile)
{for(var i=0;i<this._profiles.length;++i){if(this._profiles[i].uid===profile.uid){this._profiles.splice(i,1);break;}}
delete this._profilesIdMap[this._makeKey(profile.uid)];},findTemporaryProfile:function()
{for(var i=0;i<this._profiles.length;++i){if(this._profiles[i].isTemporary)
return this._profiles[i];}
return null;},_reset:function()
{var profiles=this._profiles.slice(0);for(var i=0;i<profiles.length;++i){var profile=profiles[i];var view=profile.existingView();if(view){view.detach();if("dispose"in view)
view.dispose();}
this.dispatchEventToListeners(WebInspector.ProfileType.Events.RemoveProfileHeader,profile);}
this.treeElement.removeChildren();this._profiles=[];this._profilesIdMap={};},__proto__:WebInspector.Object.prototype}
WebInspector.ProfileHeader=function(profileType,title,uid)
{this._profileType=profileType;this.title=title;this.isTemporary=uid===undefined;this.uid=this.isTemporary?-1:uid;this._fromFile=false;}
WebInspector.ProfileHeader._nextProfileFromFileUid=1;WebInspector.ProfileHeader.prototype={profileType:function()
{return this._profileType;},createSidebarTreeElement:function()
{throw new Error("Needs implemented.");},existingView:function()
{return this._view;},view:function(panel)
{if(!this._view)
this._view=this.createView(panel);return this._view;},createView:function(panel)
{throw new Error("Not implemented.");},dispose:function()
{},load:function(callback)
{},canSaveToFile:function()
{return false;},saveToFile:function()
{throw new Error("Needs implemented");},loadFromFile:function(file)
{throw new Error("Needs implemented");},fromFile:function()
{return this._fromFile;},setFromFile:function()
{this._fromFile=true;this.uid="From file #"+WebInspector.ProfileHeader._nextProfileFromFileUid++;}}
WebInspector.ProfilesPanel=function(name,type)
{var singleProfileMode=typeof name!=="undefined";name=name||"profiles";WebInspector.Panel.call(this,name);this.registerRequiredCSS("panelEnablerView.css");this.registerRequiredCSS("heapProfiler.css");this.registerRequiredCSS("profilesPanel.css");this.createSidebarViewWithTree();this.splitView.mainElement.addStyleClass("vbox");this.splitView.sidebarElement.addStyleClass("vbox");this.profilesItemTreeElement=new WebInspector.ProfilesSidebarTreeElement(this);this.sidebarTree.appendChild(this.profilesItemTreeElement);this._singleProfileMode=singleProfileMode;this._profileTypesByIdMap={};this.profileViews=document.createElement("div");this.profileViews.id="profile-views";this.profileViews.addStyleClass("vbox");this.splitView.mainElement.appendChild(this.profileViews);var statusBarContainer=this.splitView.mainElement.createChild("div","profiles-status-bar");this._statusBarElement=statusBarContainer.createChild("div","status-bar");var sidebarTreeBox=this.sidebarElement.createChild("div","profiles-sidebar-tree-box");sidebarTreeBox.appendChild(this.sidebarTreeElement);var statusBarContainerLeft=this.sidebarElement.createChild("div","profiles-status-bar");this._statusBarButtons=statusBarContainerLeft.createChild("div","status-bar");this.recordButton=new WebInspector.StatusBarButton("","record-profile-status-bar-item");this.recordButton.addEventListener("click",this.toggleRecordButton,this);this._statusBarButtons.appendChild(this.recordButton.element);this.clearResultsButton=new WebInspector.StatusBarButton(WebInspector.UIString("Clear all profiles."),"clear-status-bar-item");this.clearResultsButton.addEventListener("click",this._clearProfiles,this);this._statusBarButtons.appendChild(this.clearResultsButton.element);this._profileTypeStatusBarItemsContainer=this._statusBarElement.createChild("div");this._profileViewStatusBarItemsContainer=this._statusBarElement.createChild("div");if(singleProfileMode){this._launcherView=this._createLauncherView();this._registerProfileType((type));this._selectedProfileType=type;this._updateProfileTypeSpecificUI();}else{this._launcherView=new WebInspector.MultiProfileLauncherView(this);this._launcherView.addEventListener(WebInspector.MultiProfileLauncherView.EventTypes.ProfileTypeSelected,this._onProfileTypeSelected,this);this._registerProfileType(new WebInspector.CPUProfileType());this._registerProfileType(new WebInspector.HeapSnapshotProfileType());this._registerProfileType(new WebInspector.TrackingHeapSnapshotProfileType(this));if(!WebInspector.WorkerManager.isWorkerFrontend()&&WebInspector.experimentsSettings.canvasInspection.isEnabled())
this._registerProfileType(new WebInspector.CanvasProfileType());}
this._reset();this._createFileSelectorElement();this.element.addEventListener("contextmenu",this._handleContextMenuEvent.bind(this),true);this._registerShortcuts();WebInspector.ContextMenu.registerProvider(this);this._configureCpuProfilerSamplingInterval();WebInspector.settings.highResolutionCpuProfiling.addChangeListener(this._configureCpuProfilerSamplingInterval,this);}
WebInspector.ProfilesPanel.prototype={_createFileSelectorElement:function()
{if(this._fileSelectorElement)
this.element.removeChild(this._fileSelectorElement);this._fileSelectorElement=WebInspector.createFileSelectorElement(this._loadFromFile.bind(this));this.element.appendChild(this._fileSelectorElement);},_createLauncherView:function()
{return new WebInspector.ProfileLauncherView(this);},_findProfileTypeByExtension:function(fileName)
{for(var id in this._profileTypesByIdMap){var type=this._profileTypesByIdMap[id];var extension=type.fileExtension();if(!extension)
continue;if(fileName.endsWith(type.fileExtension()))
return type;}
return null;},_registerShortcuts:function()
{this.registerShortcuts(WebInspector.ProfilesPanelDescriptor.ShortcutKeys.StartStopRecording,this.toggleRecordButton.bind(this));},_configureCpuProfilerSamplingInterval:function()
{var intervalUs=WebInspector.settings.highResolutionCpuProfiling.get()?100:1000;ProfilerAgent.setSamplingInterval(intervalUs,didChangeInterval.bind(this));function didChangeInterval(error)
{if(error)
WebInspector.showErrorMessage(error)}},_loadFromFile:function(file)
{this._createFileSelectorElement();var profileType=this._findProfileTypeByExtension(file.name);if(!profileType){var extensions=[];for(var id in this._profileTypesByIdMap){var extension=this._profileTypesByIdMap[id].fileExtension();if(!extension)
continue;extensions.push(extension);}
WebInspector.log(WebInspector.UIString("Can't load file. Only files with extensions '%s' can be loaded.",extensions.join("', '")));return;}
if(!!profileType.findTemporaryProfile()){WebInspector.log(WebInspector.UIString("Can't load profile when other profile is recording."));return;}
var name=file.name;if(name.endsWith(profileType.fileExtension()))
name=name.substr(0,name.length-profileType.fileExtension().length);var temporaryProfile=profileType.createTemporaryProfile(name);temporaryProfile.setFromFile();profileType.addProfile(temporaryProfile);temporaryProfile.loadFromFile(file);},toggleRecordButton:function(event)
{var isProfiling=this._selectedProfileType.buttonClicked();this.setRecordingProfile(this._selectedProfileType.id,isProfiling);return true;},_onProfileTypeSelected:function(event)
{this._selectedProfileType=(event.data);this._updateProfileTypeSpecificUI();},_updateProfileTypeSpecificUI:function()
{this.recordButton.title=this._selectedProfileType.buttonTooltip;this._launcherView.updateProfileType(this._selectedProfileType);this._profileTypeStatusBarItemsContainer.removeChildren();var statusBarItems=this._selectedProfileType.statusBarItems;if(statusBarItems){for(var i=0;i<statusBarItems.length;++i)
this._profileTypeStatusBarItemsContainer.appendChild(statusBarItems[i]);}},_reset:function()
{WebInspector.Panel.prototype.reset.call(this);for(var typeId in this._profileTypesByIdMap)
this._profileTypesByIdMap[typeId]._reset();delete this.visibleView;delete this.currentQuery;this.searchCanceled();this._profileGroups={};this.recordButton.toggled=false;if(this._selectedProfileType)
this.recordButton.title=this._selectedProfileType.buttonTooltip;this._launcherView.profileFinished();this.sidebarTreeElement.removeStyleClass("some-expandable");this._launcherView.detach();this.profileViews.removeChildren();this._profileViewStatusBarItemsContainer.removeChildren();this.removeAllListeners();this.recordButton.visible=true;this._profileViewStatusBarItemsContainer.removeStyleClass("hidden");this.clearResultsButton.element.removeStyleClass("hidden");this.profilesItemTreeElement.select();this._showLauncherView();},_showLauncherView:function()
{this.closeVisibleView();this._profileViewStatusBarItemsContainer.removeChildren();this._launcherView.show(this.profileViews);this.visibleView=this._launcherView;},_clearProfiles:function()
{ProfilerAgent.clearProfiles();HeapProfilerAgent.clearProfiles();this._reset();},_garbageCollectButtonClicked:function()
{HeapProfilerAgent.collectGarbage();},_registerProfileType:function(profileType)
{this._profileTypesByIdMap[profileType.id]=profileType;this._launcherView.addProfileType(profileType);profileType.treeElement=new WebInspector.SidebarSectionTreeElement(profileType.treeItemTitle,null,true);profileType.treeElement.hidden=!this._singleProfileMode;this.sidebarTree.appendChild(profileType.treeElement);profileType.treeElement.childrenListElement.addEventListener("contextmenu",this._handleContextMenuEvent.bind(this),true);function onAddProfileHeader(event)
{this._addProfileHeader(event.data);}
function onRemoveProfileHeader(event)
{this._removeProfileHeader(event.data);}
function onProgressUpdated(event)
{this._reportProfileProgress(event.data.profile,event.data.done,event.data.total);}
profileType.addEventListener(WebInspector.ProfileType.Events.ViewUpdated,this._updateProfileTypeSpecificUI,this);profileType.addEventListener(WebInspector.ProfileType.Events.AddProfileHeader,onAddProfileHeader,this);profileType.addEventListener(WebInspector.ProfileType.Events.RemoveProfileHeader,onRemoveProfileHeader,this);profileType.addEventListener(WebInspector.ProfileType.Events.ProgressUpdated,onProgressUpdated,this);},_handleContextMenuEvent:function(event)
{var element=event.srcElement;while(element&&!element.treeElement&&element!==this.element)
element=element.parentElement;if(!element)
return;if(element.treeElement&&element.treeElement.handleContextMenuEvent){element.treeElement.handleContextMenuEvent(event,this);return;}
var contextMenu=new WebInspector.ContextMenu(event);if(this.visibleView instanceof WebInspector.HeapSnapshotView){this.visibleView.populateContextMenu(contextMenu,event);}
if(element!==this.element||event.srcElement===this.sidebarElement){contextMenu.appendItem(WebInspector.UIString("Load\u2026"),this._fileSelectorElement.click.bind(this._fileSelectorElement));}
contextMenu.show();},_makeTitleKey:function(text,profileTypeId)
{return escape(text)+'/'+escape(profileTypeId);},_addProfileHeader:function(profile)
{var profileType=profile.profileType();var typeId=profileType.id;var sidebarParent=profileType.treeElement;sidebarParent.hidden=false;var small=false;var alternateTitle;if(!profile.fromFile()&&!profile.isTemporary){var profileTitleKey=this._makeTitleKey(profile.title,typeId);if(!(profileTitleKey in this._profileGroups))
this._profileGroups[profileTitleKey]=[];var group=this._profileGroups[profileTitleKey];group.push(profile);if(group.length===2){group._profilesTreeElement=new WebInspector.ProfileGroupSidebarTreeElement(this,profile.title);var index=sidebarParent.children.indexOf(group[0]._profilesTreeElement);sidebarParent.insertChild(group._profilesTreeElement,index);var selected=group[0]._profilesTreeElement.selected;sidebarParent.removeChild(group[0]._profilesTreeElement);group._profilesTreeElement.appendChild(group[0]._profilesTreeElement);if(selected)
group[0]._profilesTreeElement.revealAndSelect();group[0]._profilesTreeElement.small=true;group[0]._profilesTreeElement.mainTitle=WebInspector.UIString("Run %d",1);this.sidebarTreeElement.addStyleClass("some-expandable");}
if(group.length>=2){sidebarParent=group._profilesTreeElement;alternateTitle=WebInspector.UIString("Run %d",group.length);small=true;}}
var profileTreeElement=profile.createSidebarTreeElement();profile.sidebarElement=profileTreeElement;profileTreeElement.small=small;if(alternateTitle)
profileTreeElement.mainTitle=alternateTitle;profile._profilesTreeElement=profileTreeElement;var temporaryProfile=profileType.findTemporaryProfile();if(profile.isTemporary||!temporaryProfile)
sidebarParent.appendChild(profileTreeElement);else{if(temporaryProfile){sidebarParent.insertBeforeChild(profileTreeElement,temporaryProfile._profilesTreeElement);this._removeTemporaryProfile(profile.profileType().id);}
if(!this.visibleView||this.visibleView===this._launcherView)
this._showProfile(profile);this.dispatchEventToListeners("profile added",{type:typeId});}},_removeProfileHeader:function(profile)
{profile.dispose();profile.profileType().removeProfile(profile);var sidebarParent=profile.profileType().treeElement;var profileTitleKey=this._makeTitleKey(profile.title,profile.profileType().id);var group=this._profileGroups[profileTitleKey];if(group){group.splice(group.indexOf(profile),1);if(group.length===1){var index=sidebarParent.children.indexOf(group._profilesTreeElement);sidebarParent.insertChild(group[0]._profilesTreeElement,index);group[0]._profilesTreeElement.small=false;group[0]._profilesTreeElement.mainTitle=group[0].title;sidebarParent.removeChild(group._profilesTreeElement);}
if(group.length!==0)
sidebarParent=group._profilesTreeElement;else
delete this._profileGroups[profileTitleKey];}
sidebarParent.removeChild(profile._profilesTreeElement);if(!sidebarParent.children.length){this.profilesItemTreeElement.select();this._showLauncherView();sidebarParent.hidden=!this._singleProfileMode;}},_showProfile:function(profile)
{if(!profile||(profile.isTemporary&&!profile.profileType().hasTemporaryView()))
return null;var view=profile.view(this);if(view===this.visibleView)
return view;this.closeVisibleView();view.show(this.profileViews);profile._profilesTreeElement._suppressOnSelect=true;profile._profilesTreeElement.revealAndSelect();delete profile._profilesTreeElement._suppressOnSelect;this.visibleView=view;this._profileViewStatusBarItemsContainer.removeChildren();var statusBarItems=view.statusBarItems;if(statusBarItems)
for(var i=0;i<statusBarItems.length;++i)
this._profileViewStatusBarItemsContainer.appendChild(statusBarItems[i]);return view;},showObject:function(snapshotObjectId,viewName)
{var heapProfiles=this.getProfileType(WebInspector.HeapSnapshotProfileType.TypeId).getProfiles();for(var i=0;i<heapProfiles.length;i++){var profile=heapProfiles[i];if(profile.maxJSObjectId>=snapshotObjectId){this._showProfile(profile);var view=profile.view(this);view.changeView(viewName,function(){view.dataGrid.highlightObjectByHeapSnapshotId(snapshotObjectId);});break;}}},_createTemporaryProfile:function(typeId)
{var type=this.getProfileType(typeId);if(!type.findTemporaryProfile())
type.addProfile(type.createTemporaryProfile());},_removeTemporaryProfile:function(typeId)
{var temporaryProfile=this.getProfileType(typeId).findTemporaryProfile();if(!!temporaryProfile)
this._removeProfileHeader(temporaryProfile);},getProfile:function(typeId,uid)
{return this.getProfileType(typeId).getProfile(uid);},showView:function(view)
{this._showProfile(view.profile);},getProfileType:function(typeId)
{return this._profileTypesByIdMap[typeId];},showProfile:function(typeId,uid)
{return this._showProfile(this.getProfile(typeId,Number(uid)));},closeVisibleView:function()
{if(this.visibleView)
this.visibleView.detach();delete this.visibleView;},performSearch:function(query,shouldJump)
{this.searchCanceled();var searchableViews=this._searchableViews();if(!searchableViews||!searchableViews.length)
return;var visibleView=this.visibleView;var matchesCountUpdateTimeout=null;function updateMatchesCount()
{WebInspector.searchController.updateSearchMatchesCount(this._totalSearchMatches,this);WebInspector.searchController.updateCurrentMatchIndex(this._currentSearchResultIndex,this);matchesCountUpdateTimeout=null;}
function updateMatchesCountSoon()
{if(matchesCountUpdateTimeout)
return;matchesCountUpdateTimeout=setTimeout(updateMatchesCount.bind(this),500);}
function finishedCallback(view,searchMatches)
{if(!searchMatches)
return;this._totalSearchMatches+=searchMatches;this._searchResults.push(view);this.searchMatchFound(view,searchMatches);updateMatchesCountSoon.call(this);if(shouldJump&&view===visibleView)
view.jumpToFirstSearchResult();}
var i=0;var panel=this;var boundFinishedCallback=finishedCallback.bind(this);var chunkIntervalIdentifier=null;function processChunk()
{var view=searchableViews[i];if(++i>=searchableViews.length){if(panel._currentSearchChunkIntervalIdentifier===chunkIntervalIdentifier)
delete panel._currentSearchChunkIntervalIdentifier;clearInterval(chunkIntervalIdentifier);}
if(!view)
return;view.currentQuery=query;view.performSearch(query,boundFinishedCallback);}
processChunk();chunkIntervalIdentifier=setInterval(processChunk,25);this._currentSearchChunkIntervalIdentifier=chunkIntervalIdentifier;},jumpToNextSearchResult:function()
{if(!this.showView||!this._searchResults||!this._searchResults.length)
return;var showFirstResult=false;this._currentSearchResultIndex=this._searchResults.indexOf(this.visibleView);if(this._currentSearchResultIndex===-1){this._currentSearchResultIndex=0;showFirstResult=true;}
var currentView=this._searchResults[this._currentSearchResultIndex];if(currentView.showingLastSearchResult()){if(++this._currentSearchResultIndex>=this._searchResults.length)
this._currentSearchResultIndex=0;currentView=this._searchResults[this._currentSearchResultIndex];showFirstResult=true;}
WebInspector.searchController.updateCurrentMatchIndex(this._currentSearchResultIndex,this);if(currentView!==this.visibleView){this.showView(currentView);WebInspector.searchController.showSearchField();}
if(showFirstResult)
currentView.jumpToFirstSearchResult();else
currentView.jumpToNextSearchResult();},jumpToPreviousSearchResult:function()
{if(!this.showView||!this._searchResults||!this._searchResults.length)
return;var showLastResult=false;this._currentSearchResultIndex=this._searchResults.indexOf(this.visibleView);if(this._currentSearchResultIndex===-1){this._currentSearchResultIndex=0;showLastResult=true;}
var currentView=this._searchResults[this._currentSearchResultIndex];if(currentView.showingFirstSearchResult()){if(--this._currentSearchResultIndex<0)
this._currentSearchResultIndex=(this._searchResults.length-1);currentView=this._searchResults[this._currentSearchResultIndex];showLastResult=true;}
WebInspector.searchController.updateCurrentMatchIndex(this._currentSearchResultIndex,this);if(currentView!==this.visibleView){this.showView(currentView);WebInspector.searchController.showSearchField();}
if(showLastResult)
currentView.jumpToLastSearchResult();else
currentView.jumpToPreviousSearchResult();},_getAllProfiles:function()
{var profiles=[];for(var typeId in this._profileTypesByIdMap)
profiles=profiles.concat(this._profileTypesByIdMap[typeId].getProfiles());return profiles;},_searchableViews:function()
{var profiles=this._getAllProfiles();var searchableViews=[];for(var i=0;i<profiles.length;++i){var view=profiles[i].view(this);if(view.performSearch)
searchableViews.push(view)}
var index=searchableViews.indexOf(this.visibleView);if(index>0){searchableViews[index]=searchableViews[0];searchableViews[0]=this.visibleView;}
return searchableViews;},searchMatchFound:function(view,matches)
{view.profile._profilesTreeElement.searchMatches=matches;},searchCanceled:function()
{if(this._searchResults){for(var i=0;i<this._searchResults.length;++i){var view=this._searchResults[i];if(view.searchCanceled)
view.searchCanceled();delete view.currentQuery;}}
WebInspector.Panel.prototype.searchCanceled.call(this);if(this._currentSearchChunkIntervalIdentifier){clearInterval(this._currentSearchChunkIntervalIdentifier);delete this._currentSearchChunkIntervalIdentifier;}
this._totalSearchMatches=0;this._currentSearchResultIndex=0;this._searchResults=[];var profiles=this._getAllProfiles();for(var i=0;i<profiles.length;++i)
profiles[i]._profilesTreeElement.searchMatches=0;},setRecordingProfile:function(profileType,isProfiling)
{var profileTypeObject=this.getProfileType(profileType);this.recordButton.toggled=isProfiling;this.recordButton.title=profileTypeObject.buttonTooltip;if(isProfiling){this._launcherView.profileStarted();this._createTemporaryProfile(profileType);if(profileTypeObject.hasTemporaryView())
this._showProfile(profileTypeObject.findTemporaryProfile());}else
this._launcherView.profileFinished();},_reportProfileProgress:function(profile,done,total)
{profile.sidebarElement.subtitle=WebInspector.UIString("%.0f%",(done/total)*100);profile.sidebarElement.wait=true;},appendApplicableItems:function(event,contextMenu,target)
{if(WebInspector.inspectorView.currentPanel()!==this)
return;var object=(target);var objectId=object.objectId;if(!objectId)
return;var heapProfiles=this.getProfileType(WebInspector.HeapSnapshotProfileType.TypeId).getProfiles();if(!heapProfiles.length)
return;function revealInView(viewName)
{HeapProfilerAgent.getHeapObjectId(objectId,didReceiveHeapObjectId.bind(this,viewName));}
function didReceiveHeapObjectId(viewName,error,result)
{if(WebInspector.inspectorView.currentPanel()!==this)
return;if(!error)
this.showObject(result,viewName);}
contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Reveal in Dominators view":"Reveal in Dominators View"),revealInView.bind(this,"Dominators"));contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Reveal in Summary view":"Reveal in Summary View"),revealInView.bind(this,"Summary"));},__proto__:WebInspector.Panel.prototype}
WebInspector.ProfileSidebarTreeElement=function(profile,titleFormat,className)
{this.profile=profile;this._titleFormat=titleFormat;WebInspector.SidebarTreeElement.call(this,className,"","",profile,false);this.refreshTitles();}
WebInspector.ProfileSidebarTreeElement.prototype={onselect:function()
{if(!this._suppressOnSelect)
this.treeOutline.panel._showProfile(this.profile);},ondelete:function()
{this.treeOutline.panel._removeProfileHeader(this.profile);return true;},get mainTitle()
{if(this._mainTitle)
return this._mainTitle;return this.profile.title;},set mainTitle(x)
{this._mainTitle=x;this.refreshTitles();},set searchMatches(matches)
{if(!matches){if(!this.bubbleElement)
return;this.bubbleElement.removeStyleClass("search-matches");this.bubbleText="";return;}
this.bubbleText=matches;this.bubbleElement.addStyleClass("search-matches");},handleContextMenuEvent:function(event,panel)
{var profile=this.profile;var contextMenu=new WebInspector.ContextMenu(event);contextMenu.appendItem(WebInspector.UIString("Load\u2026"),panel._fileSelectorElement.click.bind(panel._fileSelectorElement));if(profile.canSaveToFile())
contextMenu.appendItem(WebInspector.UIString("Save\u2026"),profile.saveToFile.bind(profile));contextMenu.appendItem(WebInspector.UIString("Delete"),this.ondelete.bind(this));contextMenu.show();},__proto__:WebInspector.SidebarTreeElement.prototype}
WebInspector.ProfileGroupSidebarTreeElement=function(panel,title,subtitle)
{WebInspector.SidebarTreeElement.call(this,"profile-group-sidebar-tree-item",title,subtitle,null,true);this._panel=panel;}
WebInspector.ProfileGroupSidebarTreeElement.prototype={onselect:function()
{if(this.children.length>0)
this._panel._showProfile(this.children[this.children.length-1].profile);},__proto__:WebInspector.SidebarTreeElement.prototype}
WebInspector.ProfilesSidebarTreeElement=function(panel)
{this._panel=panel;this.small=false;WebInspector.SidebarTreeElement.call(this,"profile-launcher-view-tree-item",WebInspector.UIString("Profiles"),"",null,false);}
WebInspector.ProfilesSidebarTreeElement.prototype={onselect:function()
{this._panel._showLauncherView();},get selectable()
{return true;},__proto__:WebInspector.SidebarTreeElement.prototype}
WebInspector.CPUProfilerPanel=function()
{WebInspector.ProfilesPanel.call(this,"cpu-profiler",new WebInspector.CPUProfileType());}
WebInspector.CPUProfilerPanel.prototype={__proto__:WebInspector.ProfilesPanel.prototype}
WebInspector.HeapProfilerPanel=function()
{var heapSnapshotProfileType=new WebInspector.HeapSnapshotProfileType();WebInspector.ProfilesPanel.call(this,"heap-profiler",heapSnapshotProfileType);this._singleProfileMode=false;this._registerProfileType(new WebInspector.TrackingHeapSnapshotProfileType(this));this._launcherView.addEventListener(WebInspector.MultiProfileLauncherView.EventTypes.ProfileTypeSelected,this._onProfileTypeSelected,this);this._launcherView._profileTypeChanged(heapSnapshotProfileType);}
WebInspector.HeapProfilerPanel.prototype={_createLauncherView:function()
{return new WebInspector.MultiProfileLauncherView(this);},__proto__:WebInspector.ProfilesPanel.prototype}
WebInspector.CanvasProfilerPanel=function()
{WebInspector.ProfilesPanel.call(this,"canvas-profiler",new WebInspector.CanvasProfileType());}
WebInspector.CanvasProfilerPanel.prototype={__proto__:WebInspector.ProfilesPanel.prototype}
WebInspector.ProfileDataGridNode=function(profileNode,owningTree,hasChildren)
{this.profileNode=profileNode;WebInspector.DataGridNode.call(this,null,hasChildren);this.tree=owningTree;this.childrenByCallUID={};this.lastComparator=null;this.callUID=profileNode.callUID;this.selfTime=profileNode.selfTime;this.totalTime=profileNode.totalTime;this.functionName=profileNode.functionName;this._deoptReason=(!profileNode.deoptReason||profileNode.deoptReason==="no reason")?"":profileNode.deoptReason;this.url=profileNode.url;}
WebInspector.ProfileDataGridNode.prototype={get data()
{function formatMilliseconds(time)
{return WebInspector.UIString("%.1f\u2009ms",time);}
var data={};if(this._deoptReason){var div=document.createElement("div");var marker=div.createChild("span");marker.className="profile-warn-marker";marker.title=WebInspector.UIString("Not optimized: %s",this._deoptReason);var functionName=div.createChild("span");functionName.textContent=this.functionName;data["function"]=div;}else
data["function"]=this.functionName;if(this.tree.profileView.showSelfTimeAsPercent.get())
data["self"]=WebInspector.UIString("%.2f%",this.selfPercent);else
data["self"]=formatMilliseconds(this.selfTime);if(this.tree.profileView.showTotalTimeAsPercent.get())
data["total"]=WebInspector.UIString("%.2f%",this.totalPercent);else
data["total"]=formatMilliseconds(this.totalTime);return data;},createCell:function(columnIdentifier)
{var cell=WebInspector.DataGridNode.prototype.createCell.call(this,columnIdentifier);if(columnIdentifier==="self"&&this._searchMatchedSelfColumn)
cell.addStyleClass("highlight");else if(columnIdentifier==="total"&&this._searchMatchedTotalColumn)
cell.addStyleClass("highlight");if(columnIdentifier!=="function")
return cell;if(this._deoptReason)
cell.addStyleClass("not-optimized");if(this.profileNode._searchMatchedFunctionColumn)
cell.addStyleClass("highlight");if(this.profileNode.scriptId!=="0"){var lineNumber=this.profileNode.lineNumber?this.profileNode.lineNumber-1:0;var columnNumber=this.profileNode.columnNumber?this.profileNode.columnNumber-1:0;var location=new WebInspector.DebuggerModel.Location(this.profileNode.scriptId,lineNumber,columnNumber);var urlElement=this.tree.profileView._linkifier.linkifyRawLocation(location,"profile-node-file");if(!urlElement)
urlElement=this.tree.profileView._linkifier.linkifyLocation(this.profileNode.url,lineNumber,columnNumber,"profile-node-file");urlElement.style.maxWidth="75%";cell.insertBefore(urlElement,cell.firstChild);}
return cell;},select:function(supressSelectedEvent)
{WebInspector.DataGridNode.prototype.select.call(this,supressSelectedEvent);this.tree.profileView._dataGridNodeSelected(this);},deselect:function(supressDeselectedEvent)
{WebInspector.DataGridNode.prototype.deselect.call(this,supressDeselectedEvent);this.tree.profileView._dataGridNodeDeselected(this);},sort:function(comparator,force)
{var gridNodeGroups=[[this]];for(var gridNodeGroupIndex=0;gridNodeGroupIndex<gridNodeGroups.length;++gridNodeGroupIndex){var gridNodes=gridNodeGroups[gridNodeGroupIndex];var count=gridNodes.length;for(var index=0;index<count;++index){var gridNode=gridNodes[index];if(!force&&(!gridNode.expanded||gridNode.lastComparator===comparator)){if(gridNode.children.length)
gridNode.shouldRefreshChildren=true;continue;}
gridNode.lastComparator=comparator;var children=gridNode.children;var childCount=children.length;if(childCount){children.sort(comparator);for(var childIndex=0;childIndex<childCount;++childIndex)
children[childIndex]._recalculateSiblings(childIndex);gridNodeGroups.push(children);}}}},insertChild:function(profileDataGridNode,index)
{WebInspector.DataGridNode.prototype.insertChild.call(this,profileDataGridNode,index);this.childrenByCallUID[profileDataGridNode.callUID]=profileDataGridNode;},removeChild:function(profileDataGridNode)
{WebInspector.DataGridNode.prototype.removeChild.call(this,profileDataGridNode);delete this.childrenByCallUID[profileDataGridNode.callUID];},removeChildren:function()
{WebInspector.DataGridNode.prototype.removeChildren.call(this);this.childrenByCallUID={};},findChild:function(node)
{if(!node)
return null;return this.childrenByCallUID[node.callUID];},get selfPercent()
{return this.selfTime/this.tree.totalTime*100.0;},get totalPercent()
{return this.totalTime/this.tree.totalTime*100.0;},get _parent()
{return this.parent!==this.dataGrid?this.parent:this.tree;},populate:function()
{if(this._populated)
return;this._populated=true;this._sharedPopulate();var currentComparator=this.tree.lastComparator;if(currentComparator)
this.sort(currentComparator,true);},_save:function()
{if(this._savedChildren)
return;this._savedSelfTime=this.selfTime;this._savedTotalTime=this.totalTime;this._savedChildren=this.children.slice();},_restore:function()
{if(!this._savedChildren)
return;this.selfTime=this._savedSelfTime;this.totalTime=this._savedTotalTime;this.removeChildren();var children=this._savedChildren;var count=children.length;for(var index=0;index<count;++index){children[index]._restore();this.appendChild(children[index]);}},_merge:function(child,shouldAbsorb)
{this.selfTime+=child.selfTime;if(!shouldAbsorb)
this.totalTime+=child.totalTime;var children=this.children.slice();this.removeChildren();var count=children.length;for(var index=0;index<count;++index){if(!shouldAbsorb||children[index]!==child)
this.appendChild(children[index]);}
children=child.children.slice();count=children.length;for(var index=0;index<count;++index){var orphanedChild=children[index],existingChild=this.childrenByCallUID[orphanedChild.callUID];if(existingChild)
existingChild._merge(orphanedChild,false);else
this.appendChild(orphanedChild);}},__proto__:WebInspector.DataGridNode.prototype}
WebInspector.ProfileDataGridTree=function(profileView,rootProfileNode)
{this.tree=this;this.children=[];this.profileView=profileView;this.totalTime=rootProfileNode.totalTime;this.lastComparator=null;this.childrenByCallUID={};}
WebInspector.ProfileDataGridTree.prototype={get expanded()
{return true;},appendChild:function(child)
{this.insertChild(child,this.children.length);},insertChild:function(child,index)
{this.children.splice(index,0,child);this.childrenByCallUID[child.callUID]=child;},removeChildren:function()
{this.children=[];this.childrenByCallUID={};},findChild:WebInspector.ProfileDataGridNode.prototype.findChild,sort:WebInspector.ProfileDataGridNode.prototype.sort,_save:function()
{if(this._savedChildren)
return;this._savedTotalTime=this.totalTime;this._savedChildren=this.children.slice();},restore:function()
{if(!this._savedChildren)
return;this.children=this._savedChildren;this.totalTime=this._savedTotalTime;var children=this.children;var count=children.length;for(var index=0;index<count;++index)
children[index]._restore();this._savedChildren=null;}}
WebInspector.ProfileDataGridTree.propertyComparators=[{},{}];WebInspector.ProfileDataGridTree.propertyComparator=function(property,isAscending)
{var comparator=WebInspector.ProfileDataGridTree.propertyComparators[(isAscending?1:0)][property];if(!comparator){if(isAscending){comparator=function(lhs,rhs)
{if(lhs[property]<rhs[property])
return-1;if(lhs[property]>rhs[property])
return 1;return 0;}}else{comparator=function(lhs,rhs)
{if(lhs[property]>rhs[property])
return-1;if(lhs[property]<rhs[property])
return 1;return 0;}}
WebInspector.ProfileDataGridTree.propertyComparators[(isAscending?1:0)][property]=comparator;}
return comparator;};WebInspector.AllocationProfile=function(profile)
{this._strings=profile.strings;this._nextNodeId=1;this._idToFunctionInfo={};this._idToNode={};this._collapsedTopNodeIdToFunctionInfo={};this._traceTops=null;this._buildAllocationFunctionInfos(profile);this._traceTree=this._buildInvertedAllocationTree(profile);}
WebInspector.AllocationProfile.prototype={_buildAllocationFunctionInfos:function(profile)
{var strings=this._strings;var functionInfoFields=profile.snapshot.meta.trace_function_info_fields;var functionIdOffset=functionInfoFields.indexOf("function_id");var functionNameOffset=functionInfoFields.indexOf("name");var scriptNameOffset=functionInfoFields.indexOf("script_name");var scriptIdOffset=functionInfoFields.indexOf("script_id");var lineOffset=functionInfoFields.indexOf("line");var columnOffset=functionInfoFields.indexOf("column");var functionInfoFieldCount=functionInfoFields.length;var map=this._idToFunctionInfo;map[0]=new WebInspector.FunctionAllocationInfo("(root)","<unknown>",0,-1,-1);var rawInfos=profile.trace_function_infos;var infoLength=rawInfos.length;for(var i=0;i<infoLength;i+=functionInfoFieldCount){map[rawInfos[i+functionIdOffset]]=new WebInspector.FunctionAllocationInfo(strings[rawInfos[i+functionNameOffset]],strings[rawInfos[i+scriptNameOffset]],rawInfos[i+scriptIdOffset],rawInfos[i+lineOffset],rawInfos[i+columnOffset]);}},_buildInvertedAllocationTree:function(profile)
{var traceTreeRaw=profile.trace_tree;var idToFunctionInfo=this._idToFunctionInfo;var traceNodeFields=profile.snapshot.meta.trace_node_fields;var nodeIdOffset=traceNodeFields.indexOf("id");var functionIdOffset=traceNodeFields.indexOf("function_id");var allocationCountOffset=traceNodeFields.indexOf("count");var allocationSizeOffset=traceNodeFields.indexOf("size");var childrenOffset=traceNodeFields.indexOf("children");var nodeFieldCount=traceNodeFields.length;function traverseNode(rawNodeArray,nodeOffset,parent)
{var functionInfo=idToFunctionInfo[rawNodeArray[nodeOffset+functionIdOffset]];var result=new WebInspector.AllocationTraceNode(rawNodeArray[nodeOffset+nodeIdOffset],functionInfo,rawNodeArray[nodeOffset+allocationCountOffset],rawNodeArray[nodeOffset+allocationSizeOffset],parent);functionInfo.addTraceTopNode(result);var rawChildren=rawNodeArray[nodeOffset+childrenOffset];for(var i=0;i<rawChildren.length;i+=nodeFieldCount){result.children.push(traverseNode(rawChildren,i,result));}
return result;}
return traverseNode(traceTreeRaw,0,null);},serializeTraceTops:function()
{if(this._traceTops)
return this._traceTops;var result=this._traceTops=[];var idToFunctionInfo=this._idToFunctionInfo;for(var id in idToFunctionInfo){var info=idToFunctionInfo[id];if(info.totalCount===0)
continue;var nodeId=this._nextNodeId++;result.push(this._serializeNode(nodeId,info,info.totalCount,info.totalSize,true));this._collapsedTopNodeIdToFunctionInfo[nodeId]=info;}
result.sort(function(a,b){return b.size-a.size;});return result;},serializeCallers:function(nodeId)
{var node=this._idToNode[nodeId];if(!node){var functionInfo=this._collapsedTopNodeIdToFunctionInfo[nodeId];node=functionInfo.tracesWithThisTop();delete this._collapsedTopNodeIdToFunctionInfo[nodeId];this._idToNode[nodeId]=node;}
var nodesWithSingleCaller=[];while(node.callers().length===1){node=node.callers()[0];nodesWithSingleCaller.push(this._serializeCaller(node));}
var branchingCallers=[];var callers=node.callers();for(var i=0;i<callers.length;i++){branchingCallers.push(this._serializeCaller(callers[i]));}
return{nodesWithSingleCaller:nodesWithSingleCaller,branchingCallers:branchingCallers};},_serializeCaller:function(node)
{var callerId=this._nextNodeId++;this._idToNode[callerId]=node;return this._serializeNode(callerId,node.functionInfo,node.allocationCount,node.allocationSize,node.hasCallers());},_serializeNode:function(nodeId,functionInfo,count,size,hasChildren)
{return{id:nodeId,name:functionInfo.functionName,scriptName:functionInfo.scriptName,line:functionInfo.line,column:functionInfo.column,count:count,size:size,hasChildren:hasChildren};}}
WebInspector.AllocationTraceNode=function(id,functionInfo,count,size,parent)
{this.id=id;this.functionInfo=functionInfo;this.allocationCount=count;this.allocationSize=size;this.parent=parent;this.children=[];}
WebInspector.AllocationBackTraceNode=function(functionInfo)
{this.functionInfo=functionInfo;this.allocationCount=0;this.allocationSize=0;this._callers=[];}
WebInspector.AllocationBackTraceNode.prototype={addCaller:function(traceNode)
{var functionInfo=traceNode.functionInfo;var result;for(var i=0;i<this._callers.length;i++){var caller=this._callers[i];if(caller.functionInfo===functionInfo){result=caller;break;}}
if(!result){result=new WebInspector.AllocationBackTraceNode(functionInfo);this._callers.push(result);}
return result;},callers:function()
{return this._callers;},hasCallers:function()
{return this._callers.length>0;}}
WebInspector.FunctionAllocationInfo=function(functionName,scriptName,scriptId,line,column)
{this.functionName=functionName;this.scriptName=scriptName;this.scriptId=scriptId;this.line=line;this.column=column;this.totalCount=0;this.totalSize=0;this._traceTops=[];}
WebInspector.FunctionAllocationInfo.prototype={addTraceTopNode:function(node)
{if(node.allocationCount===0)
return;this._traceTops.push(node);this.totalCount+=node.allocationCount;this.totalSize+=node.allocationSize;},tracesWithThisTop:function()
{if(!this._traceTops.length)
return null;if(!this._backTraceTree)
this._buildAllocationTraceTree();return this._backTraceTree;},_buildAllocationTraceTree:function()
{this._backTraceTree=new WebInspector.AllocationBackTraceNode(this._traceTops[0].functionInfo);for(var i=0;i<this._traceTops.length;i++){var node=this._traceTops[i];var backTraceNode=this._backTraceTree;var count=node.allocationCount;var size=node.allocationSize;while(true){backTraceNode.allocationCount+=count;backTraceNode.allocationSize+=size;node=node.parent;if(node===null){break;}
backTraceNode=backTraceNode.addCaller(node);}}}};WebInspector.BottomUpProfileDataGridNode=function(profileNode,owningTree)
{WebInspector.ProfileDataGridNode.call(this,profileNode,owningTree,this._willHaveChildren(profileNode));this._remainingNodeInfos=[];}
WebInspector.BottomUpProfileDataGridNode.prototype={_takePropertiesFromProfileDataGridNode:function(profileDataGridNode)
{this._save();this.selfTime=profileDataGridNode.selfTime;this.totalTime=profileDataGridNode.totalTime;},_keepOnlyChild:function(child)
{this._save();this.removeChildren();this.appendChild(child);},_exclude:function(aCallUID)
{if(this._remainingNodeInfos)
this.populate();this._save();var children=this.children;var index=this.children.length;while(index--)
children[index]._exclude(aCallUID);var child=this.childrenByCallUID[aCallUID];if(child)
this._merge(child,true);},_restore:function()
{WebInspector.ProfileDataGridNode.prototype._restore();if(!this.children.length)
this.hasChildren=this._willHaveChildren(this.profileNode);},_merge:function(child,shouldAbsorb)
{this.selfTime-=child.selfTime;WebInspector.ProfileDataGridNode.prototype._merge.call(this,child,shouldAbsorb);},_sharedPopulate:function()
{var remainingNodeInfos=this._remainingNodeInfos;var count=remainingNodeInfos.length;for(var index=0;index<count;++index){var nodeInfo=remainingNodeInfos[index];var ancestor=nodeInfo.ancestor;var focusNode=nodeInfo.focusNode;var child=this.findChild(ancestor);if(child){var totalTimeAccountedFor=nodeInfo.totalTimeAccountedFor;child.selfTime+=focusNode.selfTime;if(!totalTimeAccountedFor)
child.totalTime+=focusNode.totalTime;}else{child=new WebInspector.BottomUpProfileDataGridNode(ancestor,this.tree);if(ancestor!==focusNode){child.selfTime=focusNode.selfTime;child.totalTime=focusNode.totalTime;}
this.appendChild(child);}
var parent=ancestor.parent;if(parent&&parent.parent){nodeInfo.ancestor=parent;child._remainingNodeInfos.push(nodeInfo);}}
delete this._remainingNodeInfos;},_willHaveChildren:function(profileNode)
{return!!(profileNode.parent&&profileNode.parent.parent);},__proto__:WebInspector.ProfileDataGridNode.prototype}
WebInspector.BottomUpProfileDataGridTree=function(profileView,rootProfileNode)
{WebInspector.ProfileDataGridTree.call(this,profileView,rootProfileNode);var profileNodeUIDs=0;var profileNodeGroups=[[],[rootProfileNode]];var visitedProfileNodesForCallUID={};this._remainingNodeInfos=[];for(var profileNodeGroupIndex=0;profileNodeGroupIndex<profileNodeGroups.length;++profileNodeGroupIndex){var parentProfileNodes=profileNodeGroups[profileNodeGroupIndex];var profileNodes=profileNodeGroups[++profileNodeGroupIndex];var count=profileNodes.length;for(var index=0;index<count;++index){var profileNode=profileNodes[index];if(!profileNode.UID)
profileNode.UID=++profileNodeUIDs;if(profileNode.head&&profileNode!==profileNode.head){var visitedNodes=visitedProfileNodesForCallUID[profileNode.callUID];var totalTimeAccountedFor=false;if(!visitedNodes){visitedNodes={}
visitedProfileNodesForCallUID[profileNode.callUID]=visitedNodes;}else{var parentCount=parentProfileNodes.length;for(var parentIndex=0;parentIndex<parentCount;++parentIndex){if(visitedNodes[parentProfileNodes[parentIndex].UID]){totalTimeAccountedFor=true;break;}}}
visitedNodes[profileNode.UID]=true;this._remainingNodeInfos.push({ancestor:profileNode,focusNode:profileNode,totalTimeAccountedFor:totalTimeAccountedFor});}
var children=profileNode.children;if(children.length){profileNodeGroups.push(parentProfileNodes.concat([profileNode]))
profileNodeGroups.push(children);}}}
var any=(this);var node=(any);WebInspector.BottomUpProfileDataGridNode.prototype.populate.call(node);return this;}
WebInspector.BottomUpProfileDataGridTree.prototype={focus:function(profileDataGridNode)
{if(!profileDataGridNode)
return;this._save();var currentNode=profileDataGridNode;var focusNode=profileDataGridNode;while(currentNode.parent&&(currentNode instanceof WebInspector.ProfileDataGridNode)){currentNode._takePropertiesFromProfileDataGridNode(profileDataGridNode);focusNode=currentNode;currentNode=currentNode.parent;if(currentNode instanceof WebInspector.ProfileDataGridNode)
currentNode._keepOnlyChild(focusNode);}
this.children=[focusNode];this.totalTime=profileDataGridNode.totalTime;},exclude:function(profileDataGridNode)
{if(!profileDataGridNode)
return;this._save();var excludedCallUID=profileDataGridNode.callUID;var excludedTopLevelChild=this.childrenByCallUID[excludedCallUID];if(excludedTopLevelChild)
this.children.remove(excludedTopLevelChild);var children=this.children;var count=children.length;for(var index=0;index<count;++index)
children[index]._exclude(excludedCallUID);if(this.lastComparator)
this.sort(this.lastComparator,true);},_sharedPopulate:WebInspector.BottomUpProfileDataGridNode.prototype._sharedPopulate,__proto__:WebInspector.ProfileDataGridTree.prototype};WebInspector.CPUProfileView=function(profileHeader)
{WebInspector.View.call(this);this.element.addStyleClass("profile-view");this.showSelfTimeAsPercent=WebInspector.settings.createSetting("cpuProfilerShowSelfTimeAsPercent",true);this.showTotalTimeAsPercent=WebInspector.settings.createSetting("cpuProfilerShowTotalTimeAsPercent",true);this.showAverageTimeAsPercent=WebInspector.settings.createSetting("cpuProfilerShowAverageTimeAsPercent",true);this._viewType=WebInspector.settings.createSetting("cpuProfilerView",WebInspector.CPUProfileView._TypeHeavy);var columns=[];columns.push({id:"self",title:WebInspector.UIString("Self"),width:"72px",sort:WebInspector.DataGrid.Order.Descending,sortable:true});columns.push({id:"total",title:WebInspector.UIString("Total"),width:"72px",sortable:true});columns.push({id:"function",title:WebInspector.UIString("Function"),disclosure:true,sortable:true});this.dataGrid=new WebInspector.DataGrid(columns);this.dataGrid.addEventListener(WebInspector.DataGrid.Events.SortingChanged,this._sortProfile,this);this.dataGrid.element.addEventListener("mousedown",this._mouseDownInDataGrid.bind(this),true);this.dataGrid.show(this.element);this.viewSelectComboBox=new WebInspector.StatusBarComboBox(this._changeView.bind(this));var options={};options[WebInspector.CPUProfileView._TypeFlame]=this.viewSelectComboBox.createOption(WebInspector.UIString("Flame Chart"),"",WebInspector.CPUProfileView._TypeFlame);options[WebInspector.CPUProfileView._TypeHeavy]=this.viewSelectComboBox.createOption(WebInspector.UIString("Heavy (Bottom Up)"),"",WebInspector.CPUProfileView._TypeHeavy);options[WebInspector.CPUProfileView._TypeTree]=this.viewSelectComboBox.createOption(WebInspector.UIString("Tree (Top Down)"),"",WebInspector.CPUProfileView._TypeTree);var optionName=this._viewType.get()||WebInspector.CPUProfileView._TypeFlame;var option=options[optionName]||options[WebInspector.CPUProfileView._TypeFlame];this.viewSelectComboBox.select(option);this._statusBarButtonsElement=document.createElement("span");this.percentButton=new WebInspector.StatusBarButton("","percent-time-status-bar-item");this.percentButton.addEventListener("click",this._percentClicked,this);this._statusBarButtonsElement.appendChild(this.percentButton.element);this.focusButton=new WebInspector.StatusBarButton(WebInspector.UIString("Focus selected function."),"focus-profile-node-status-bar-item");this.focusButton.setEnabled(false);this.focusButton.addEventListener("click",this._focusClicked,this);this._statusBarButtonsElement.appendChild(this.focusButton.element);this.excludeButton=new WebInspector.StatusBarButton(WebInspector.UIString("Exclude selected function."),"exclude-profile-node-status-bar-item");this.excludeButton.setEnabled(false);this.excludeButton.addEventListener("click",this._excludeClicked,this);this._statusBarButtonsElement.appendChild(this.excludeButton.element);this.resetButton=new WebInspector.StatusBarButton(WebInspector.UIString("Restore all functions."),"reset-profile-status-bar-item");this.resetButton.visible=false;this.resetButton.addEventListener("click",this._resetClicked,this);this._statusBarButtonsElement.appendChild(this.resetButton.element);this.profileHead=(null);this.profile=profileHeader;this._linkifier=new WebInspector.Linkifier(new WebInspector.Linkifier.DefaultFormatter(30));if(this.profile._profile)
this._processProfileData(this.profile._profile);else
ProfilerAgent.getCPUProfile(this.profile.uid,this._getCPUProfileCallback.bind(this));}
WebInspector.CPUProfileView._TypeFlame="Flame";WebInspector.CPUProfileView._TypeTree="Tree";WebInspector.CPUProfileView._TypeHeavy="Heavy";WebInspector.CPUProfileView.prototype={selectRange:function(timeLeft,timeRight)
{if(!this._flameChart)
return;this._flameChart.selectRange(timeLeft,timeRight);},_revealProfilerNode:function(event)
{var current=this.profileDataGridTree.children[0];while(current&&current.profileNode!==event.data)
current=current.traverseNextNode(false,null,false);if(current)
current.revealAndSelect();},_getCPUProfileCallback:function(error,profile)
{if(error)
return;if(!profile.head){return;}
this._processProfileData(profile);},_processProfileData:function(profile)
{this.profileHead=profile.head;this.samples=profile.samples;this._calculateTimes(profile);this._assignParentsInProfile();if(this.samples)
this._buildIdToNodeMap();this._changeView();this._updatePercentButton();if(this._flameChart)
this._flameChart.update();},get statusBarItems()
{return[this.viewSelectComboBox.element,this._statusBarButtonsElement];},_getBottomUpProfileDataGridTree:function()
{if(!this._bottomUpProfileDataGridTree)
this._bottomUpProfileDataGridTree=new WebInspector.BottomUpProfileDataGridTree(this,this.profileHead);return this._bottomUpProfileDataGridTree;},_getTopDownProfileDataGridTree:function()
{if(!this._topDownProfileDataGridTree)
this._topDownProfileDataGridTree=new WebInspector.TopDownProfileDataGridTree(this,this.profileHead);return this._topDownProfileDataGridTree;},willHide:function()
{this._currentSearchResultIndex=-1;},refresh:function()
{var selectedProfileNode=this.dataGrid.selectedNode?this.dataGrid.selectedNode.profileNode:null;this.dataGrid.rootNode().removeChildren();var children=this.profileDataGridTree.children;var count=children.length;for(var index=0;index<count;++index)
this.dataGrid.rootNode().appendChild(children[index]);if(selectedProfileNode)
selectedProfileNode.selected=true;},refreshVisibleData:function()
{var child=this.dataGrid.rootNode().children[0];while(child){child.refresh();child=child.traverseNextNode(false,null,true);}},refreshShowAsPercents:function()
{this._updatePercentButton();this.refreshVisibleData();},searchCanceled:function()
{if(this._searchResults){for(var i=0;i<this._searchResults.length;++i){var profileNode=this._searchResults[i].profileNode;delete profileNode._searchMatchedSelfColumn;delete profileNode._searchMatchedTotalColumn;delete profileNode._searchMatchedFunctionColumn;profileNode.refresh();}}
delete this._searchFinishedCallback;this._currentSearchResultIndex=-1;this._searchResults=[];},performSearch:function(query,finishedCallback)
{this.searchCanceled();query=query.trim();if(!query.length)
return;this._searchFinishedCallback=finishedCallback;var greaterThan=(query.startsWith(">"));var lessThan=(query.startsWith("<"));var equalTo=(query.startsWith("=")||((greaterThan||lessThan)&&query.indexOf("=")===1));var percentUnits=(query.lastIndexOf("%")===(query.length-1));var millisecondsUnits=(query.length>2&&query.lastIndexOf("ms")===(query.length-2));var secondsUnits=(!millisecondsUnits&&query.lastIndexOf("s")===(query.length-1));var queryNumber=parseFloat(query);if(greaterThan||lessThan||equalTo){if(equalTo&&(greaterThan||lessThan))
queryNumber=parseFloat(query.substring(2));else
queryNumber=parseFloat(query.substring(1));}
var queryNumberMilliseconds=(secondsUnits?(queryNumber*1000):queryNumber);if(!isNaN(queryNumber)&&!(greaterThan||lessThan))
equalTo=true;var matcher=createPlainTextSearchRegex(query,"i");function matchesQuery(profileDataGridNode)
{delete profileDataGridNode._searchMatchedSelfColumn;delete profileDataGridNode._searchMatchedTotalColumn;delete profileDataGridNode._searchMatchedFunctionColumn;if(percentUnits){if(lessThan){if(profileDataGridNode.selfPercent<queryNumber)
profileDataGridNode._searchMatchedSelfColumn=true;if(profileDataGridNode.totalPercent<queryNumber)
profileDataGridNode._searchMatchedTotalColumn=true;}else if(greaterThan){if(profileDataGridNode.selfPercent>queryNumber)
profileDataGridNode._searchMatchedSelfColumn=true;if(profileDataGridNode.totalPercent>queryNumber)
profileDataGridNode._searchMatchedTotalColumn=true;}
if(equalTo){if(profileDataGridNode.selfPercent==queryNumber)
profileDataGridNode._searchMatchedSelfColumn=true;if(profileDataGridNode.totalPercent==queryNumber)
profileDataGridNode._searchMatchedTotalColumn=true;}}else if(millisecondsUnits||secondsUnits){if(lessThan){if(profileDataGridNode.selfTime<queryNumberMilliseconds)
profileDataGridNode._searchMatchedSelfColumn=true;if(profileDataGridNode.totalTime<queryNumberMilliseconds)
profileDataGridNode._searchMatchedTotalColumn=true;}else if(greaterThan){if(profileDataGridNode.selfTime>queryNumberMilliseconds)
profileDataGridNode._searchMatchedSelfColumn=true;if(profileDataGridNode.totalTime>queryNumberMilliseconds)
profileDataGridNode._searchMatchedTotalColumn=true;}
if(equalTo){if(profileDataGridNode.selfTime==queryNumberMilliseconds)
profileDataGridNode._searchMatchedSelfColumn=true;if(profileDataGridNode.totalTime==queryNumberMilliseconds)
profileDataGridNode._searchMatchedTotalColumn=true;}}
if(profileDataGridNode.functionName.match(matcher)||(profileDataGridNode.url&&profileDataGridNode.url.match(matcher)))
profileDataGridNode._searchMatchedFunctionColumn=true;if(profileDataGridNode._searchMatchedSelfColumn||profileDataGridNode._searchMatchedTotalColumn||profileDataGridNode._searchMatchedFunctionColumn)
{profileDataGridNode.refresh();return true;}
return false;}
var current=this.profileDataGridTree.children[0];while(current){if(matchesQuery(current)){this._searchResults.push({profileNode:current});}
current=current.traverseNextNode(false,null,false);}
finishedCallback(this,this._searchResults.length);},jumpToFirstSearchResult:function()
{if(!this._searchResults||!this._searchResults.length)
return;this._currentSearchResultIndex=0;this._jumpToSearchResult(this._currentSearchResultIndex);},jumpToLastSearchResult:function()
{if(!this._searchResults||!this._searchResults.length)
return;this._currentSearchResultIndex=(this._searchResults.length-1);this._jumpToSearchResult(this._currentSearchResultIndex);},jumpToNextSearchResult:function()
{if(!this._searchResults||!this._searchResults.length)
return;if(++this._currentSearchResultIndex>=this._searchResults.length)
this._currentSearchResultIndex=0;this._jumpToSearchResult(this._currentSearchResultIndex);},jumpToPreviousSearchResult:function()
{if(!this._searchResults||!this._searchResults.length)
return;if(--this._currentSearchResultIndex<0)
this._currentSearchResultIndex=(this._searchResults.length-1);this._jumpToSearchResult(this._currentSearchResultIndex);},showingFirstSearchResult:function()
{return(this._currentSearchResultIndex===0);},showingLastSearchResult:function()
{return(this._searchResults&&this._currentSearchResultIndex===(this._searchResults.length-1));},_jumpToSearchResult:function(index)
{var searchResult=this._searchResults[index];if(!searchResult)
return;var profileNode=searchResult.profileNode;profileNode.revealAndSelect();},_ensureFlameChartCreated:function()
{if(this._flameChart)
return;this._flameChart=new WebInspector.FlameChart(this);this._flameChart.addEventListener(WebInspector.FlameChart.Events.SelectedNode,this._onSelectedNode.bind(this));},_onSelectedNode:function(event)
{var node=event.data;if(!node||!node.scriptId)
return;var script=WebInspector.debuggerModel.scriptForId(node.scriptId)
if(!script)
return;var uiLocation=script.rawLocationToUILocation(node.lineNumber);if(!uiLocation)
return;WebInspector.panel("sources").showUILocation(uiLocation);},_changeView:function()
{if(!this.profile)
return;switch(this.viewSelectComboBox.selectedOption().value){case WebInspector.CPUProfileView._TypeFlame:this._ensureFlameChartCreated();this.dataGrid.detach();this._flameChart.show(this.element);this._viewType.set(WebInspector.CPUProfileView._TypeFlame);this._statusBarButtonsElement.enableStyleClass("hidden",true);return;case WebInspector.CPUProfileView._TypeTree:this.profileDataGridTree=this._getTopDownProfileDataGridTree();this._sortProfile();this._viewType.set(WebInspector.CPUProfileView._TypeTree);break;case WebInspector.CPUProfileView._TypeHeavy:this.profileDataGridTree=this._getBottomUpProfileDataGridTree();this._sortProfile();this._viewType.set(WebInspector.CPUProfileView._TypeHeavy);break;}
this._statusBarButtonsElement.enableStyleClass("hidden",false);if(this._flameChart)
this._flameChart.detach();this.dataGrid.show(this.element);if(!this.currentQuery||!this._searchFinishedCallback||!this._searchResults)
return;this._searchFinishedCallback(this,-this._searchResults.length);this.performSearch(this.currentQuery,this._searchFinishedCallback);},_percentClicked:function(event)
{var currentState=this.showSelfTimeAsPercent.get()&&this.showTotalTimeAsPercent.get()&&this.showAverageTimeAsPercent.get();this.showSelfTimeAsPercent.set(!currentState);this.showTotalTimeAsPercent.set(!currentState);this.showAverageTimeAsPercent.set(!currentState);this.refreshShowAsPercents();},_updatePercentButton:function()
{if(this.showSelfTimeAsPercent.get()&&this.showTotalTimeAsPercent.get()&&this.showAverageTimeAsPercent.get()){this.percentButton.title=WebInspector.UIString("Show absolute total and self times.");this.percentButton.toggled=true;}else{this.percentButton.title=WebInspector.UIString("Show total and self times as percentages.");this.percentButton.toggled=false;}},_focusClicked:function(event)
{if(!this.dataGrid.selectedNode)
return;this.resetButton.visible=true;this.profileDataGridTree.focus(this.dataGrid.selectedNode);this.refresh();this.refreshVisibleData();},_excludeClicked:function(event)
{var selectedNode=this.dataGrid.selectedNode
if(!selectedNode)
return;selectedNode.deselect();this.resetButton.visible=true;this.profileDataGridTree.exclude(selectedNode);this.refresh();this.refreshVisibleData();},_resetClicked:function(event)
{this.resetButton.visible=false;this.profileDataGridTree.restore();this._linkifier.reset();this.refresh();this.refreshVisibleData();},_dataGridNodeSelected:function(node)
{this.focusButton.setEnabled(true);this.excludeButton.setEnabled(true);},_dataGridNodeDeselected:function(node)
{this.focusButton.setEnabled(false);this.excludeButton.setEnabled(false);},_sortProfile:function()
{var sortAscending=this.dataGrid.isSortOrderAscending();var sortColumnIdentifier=this.dataGrid.sortColumnIdentifier();var sortProperty={"self":"selfTime","total":"totalTime","function":"functionName"}[sortColumnIdentifier];this.profileDataGridTree.sort(WebInspector.ProfileDataGridTree.propertyComparator(sortProperty,sortAscending));this.refresh();},_mouseDownInDataGrid:function(event)
{if(event.detail<2)
return;var cell=event.target.enclosingNodeOrSelfWithNodeName("td");if(!cell||(!cell.hasStyleClass("total-column")&&!cell.hasStyleClass("self-column")&&!cell.hasStyleClass("average-column")))
return;if(cell.hasStyleClass("total-column"))
this.showTotalTimeAsPercent.set(!this.showTotalTimeAsPercent.get());else if(cell.hasStyleClass("self-column"))
this.showSelfTimeAsPercent.set(!this.showSelfTimeAsPercent.get());else if(cell.hasStyleClass("average-column"))
this.showAverageTimeAsPercent.set(!this.showAverageTimeAsPercent.get());this.refreshShowAsPercents();event.consume(true);},_calculateTimes:function(profile)
{function totalHitCount(node){var result=node.hitCount;for(var i=0;i<node.children.length;i++)
result+=totalHitCount(node.children[i]);return result;}
profile.totalHitCount=totalHitCount(profile.head);var durationMs=1000*(profile.endTime-profile.startTime);var samplingInterval=durationMs/profile.totalHitCount;this.samplingIntervalMs=samplingInterval;function calculateTimesForNode(node){node.selfTime=node.hitCount*samplingInterval;var totalHitCount=node.hitCount;for(var i=0;i<node.children.length;i++)
totalHitCount+=calculateTimesForNode(node.children[i]);node.totalTime=totalHitCount*samplingInterval;return totalHitCount;}
calculateTimesForNode(profile.head);},_assignParentsInProfile:function()
{var head=this.profileHead;head.parent=null;head.head=null;var nodesToTraverse=[{parent:head,children:head.children}];while(nodesToTraverse.length>0){var pair=nodesToTraverse.pop();var parent=pair.parent;var children=pair.children;var length=children.length;for(var i=0;i<length;++i){children[i].head=head;children[i].parent=parent;if(children[i].children.length>0)
nodesToTraverse.push({parent:children[i],children:children[i].children});}}},_buildIdToNodeMap:function()
{var idToNode=this._idToNode={};var stack=[this.profileHead];while(stack.length){var node=stack.pop();idToNode[node.id]=node;for(var i=0;i<node.children.length;i++)
stack.push(node.children[i]);}
var topLevelNodes=this.profileHead.children;for(var i=0;i<topLevelNodes.length;i++){var node=topLevelNodes[i];if(node.functionName=="(garbage collector)"){this._gcNode=node;break;}}},__proto__:WebInspector.View.prototype}
WebInspector.CPUProfileType=function()
{WebInspector.ProfileType.call(this,WebInspector.CPUProfileType.TypeId,WebInspector.UIString("Collect JavaScript CPU Profile"));InspectorBackend.registerProfilerDispatcher(this);this._recording=false;WebInspector.CPUProfileType.instance=this;}
WebInspector.CPUProfileType.TypeId="CPU";WebInspector.CPUProfileType.prototype={fileExtension:function()
{return".cpuprofile";},get buttonTooltip()
{return this._recording?WebInspector.UIString("Stop CPU profiling."):WebInspector.UIString("Start CPU profiling.");},buttonClicked:function()
{if(this._recording){this.stopRecordingProfile();return false;}else{this.startRecordingProfile();return true;}},get treeItemTitle()
{return WebInspector.UIString("CPU PROFILES");},get description()
{return WebInspector.UIString("CPU profiles show where the execution time is spent in your page's JavaScript functions.");},addProfileHeader:function(profileHeader)
{this.addProfile(this.createProfile(profileHeader));},isRecordingProfile:function()
{return this._recording;},startRecordingProfile:function()
{this._recording=true;WebInspector.userMetrics.ProfilesCPUProfileTaken.record();ProfilerAgent.start();},stopRecordingProfile:function()
{this._recording=false;ProfilerAgent.stop();},setRecordingProfile:function(isProfiling)
{this._recording=isProfiling;},createTemporaryProfile:function(title)
{title=title||WebInspector.UIString("Recording\u2026");return new WebInspector.CPUProfileHeader(this,title);},createProfile:function(profile)
{return new WebInspector.CPUProfileHeader(this,profile.title,profile.uid);},removeProfile:function(profile)
{WebInspector.ProfileType.prototype.removeProfile.call(this,profile);if(!profile.isTemporary&&!profile.fromFile())
ProfilerAgent.removeProfile(this.id,profile.uid);},resetProfiles:function()
{this._reset();},addHeapSnapshotChunk:function(uid,chunk)
{throw new Error("Never called");},reportHeapSnapshotProgress:function(done,total)
{throw new Error("Never called");},__proto__:WebInspector.ProfileType.prototype}
WebInspector.CPUProfileHeader=function(type,title,uid)
{WebInspector.ProfileHeader.call(this,type,title,uid);}
WebInspector.CPUProfileHeader.prototype={onTransferStarted:function()
{this._jsonifiedProfile="";this.sidebarElement.subtitle=WebInspector.UIString("Loading\u2026 %s",Number.bytesToString(this._jsonifiedProfile.length));},onChunkTransferred:function(reader)
{this.sidebarElement.subtitle=WebInspector.UIString("Loading\u2026 %d\%",Number.bytesToString(this._jsonifiedProfile.length));},onTransferFinished:function()
{this.sidebarElement.subtitle=WebInspector.UIString("Parsing\u2026");this._profile=JSON.parse(this._jsonifiedProfile);this._jsonifiedProfile=null;this.sidebarElement.subtitle=WebInspector.UIString("Loaded");this.isTemporary=false;},onError:function(reader,e)
{switch(e.target.error.code){case e.target.error.NOT_FOUND_ERR:this.sidebarElement.subtitle=WebInspector.UIString("'%s' not found.",reader.fileName());break;case e.target.error.NOT_READABLE_ERR:this.sidebarElement.subtitle=WebInspector.UIString("'%s' is not readable",reader.fileName());break;case e.target.error.ABORT_ERR:break;default:this.sidebarElement.subtitle=WebInspector.UIString("'%s' error %d",reader.fileName(),e.target.error.code);}},write:function(text)
{this._jsonifiedProfile+=text;},close:function(){},createSidebarTreeElement:function()
{return new WebInspector.ProfileSidebarTreeElement(this,WebInspector.UIString("Profile %d"),"profile-sidebar-tree-item");},createView:function(profilesPanel)
{return new WebInspector.CPUProfileView(this);},canSaveToFile:function()
{return true;},saveToFile:function()
{var fileOutputStream=new WebInspector.FileOutputStream();function getCPUProfileCallback(error,profile)
{if(error){fileOutputStream.close();return;}
if(!profile.head){fileOutputStream.close();return;}
fileOutputStream.write(JSON.stringify(profile),fileOutputStream.close.bind(fileOutputStream));}
function onOpen()
{ProfilerAgent.getCPUProfile(this.uid,getCPUProfileCallback.bind(this));}
this._fileName=this._fileName||"CPU-"+new Date().toISO8601Compact()+this._profileType.fileExtension();fileOutputStream.open(this._fileName,onOpen.bind(this));},loadFromFile:function(file)
{this.sidebarElement.subtitle=WebInspector.UIString("Loading\u2026");this.sidebarElement.wait=true;var fileReader=new WebInspector.ChunkedFileReader(file,10000000,this);fileReader.start(this);},__proto__:WebInspector.ProfileHeader.prototype};WebInspector.FlameChart=function(cpuProfileView)
{WebInspector.View.call(this);this.registerRequiredCSS("flameChart.css");this.element.className="fill";this.element.id="cpu-flame-chart";this._overviewContainer=this.element.createChild("div","overview-container");this._overviewGrid=new WebInspector.OverviewGrid("flame-chart");this._overviewCanvas=this._overviewContainer.createChild("canvas","flame-chart-overview-canvas");this._overviewContainer.appendChild(this._overviewGrid.element);this._overviewCalculator=new WebInspector.FlameChart.OverviewCalculator();this._overviewGrid.addEventListener(WebInspector.OverviewGrid.Events.WindowChanged,this._onWindowChanged,this);this._chartContainer=this.element.createChild("div","chart-container");this._timelineGrid=new WebInspector.TimelineGrid();this._chartContainer.appendChild(this._timelineGrid.element);this._calculator=new WebInspector.FlameChart.Calculator();this._canvas=this._chartContainer.createChild("canvas");this._canvas.addEventListener("mousemove",this._onMouseMove.bind(this));WebInspector.installDragHandle(this._canvas,this._startCanvasDragging.bind(this),this._canvasDragging.bind(this),this._endCanvasDragging.bind(this),"col-resize");this._entryInfo=this._chartContainer.createChild("div","entry-info");this._cpuProfileView=cpuProfileView;this._windowLeft=0.0;this._windowRight=1.0;this._barHeight=15;this._minWidth=1;this._paddingLeft=15;this._canvas.addEventListener("mousewheel",this._onMouseWheel.bind(this),false);this._canvas.addEventListener("click",this._onClick.bind(this),false);this._linkifier=new WebInspector.Linkifier();this._highlightedEntryIndex=-1;if(!WebInspector.FlameChart._colorGenerator)
WebInspector.FlameChart._colorGenerator=new WebInspector.FlameChart.ColorGenerator();}
WebInspector.FlameChart.Calculator=function()
{}
WebInspector.FlameChart.Calculator.prototype={_updateBoundaries:function(flameChart)
{function log10(x)
{return Math.log(x)/Math.LN10;}
this._decimalDigits=Math.max(0,-Math.floor(log10(flameChart._timelineGrid.gridSliceTime*1.01)));this._minimumBoundaries=flameChart._windowLeft*flameChart._timelineData.totalTime;this._maximumBoundaries=flameChart._windowRight*flameChart._timelineData.totalTime;this.paddingLeft=flameChart._paddingLeft;this._width=flameChart._canvas.width-this.paddingLeft;this._timeToPixel=this._width/this.boundarySpan();},computePosition:function(time)
{return(time-this._minimumBoundaries)*this._timeToPixel+this.paddingLeft;},formatTime:function(value)
{var format="%."+this._decimalDigits+"f\u2009ms";return WebInspector.UIString(format,value+this._minimumBoundaries);},maximumBoundary:function()
{return this._maximumBoundaries;},minimumBoundary:function()
{return this._minimumBoundaries;},zeroTime:function()
{return 0;},boundarySpan:function()
{return this._maximumBoundaries-this._minimumBoundaries;}}
WebInspector.FlameChart.OverviewCalculator=function()
{}
WebInspector.FlameChart.OverviewCalculator.prototype={_updateBoundaries:function(flameChart)
{this._minimumBoundaries=0;this._maximumBoundaries=flameChart._timelineData.totalTime;this._xScaleFactor=flameChart._canvas.width/flameChart._timelineData.totalTime;},computePosition:function(time)
{return(time-this._minimumBoundaries)*this._xScaleFactor;},formatTime:function(value)
{return Number.secondsToString((value+this._minimumBoundaries)/1000);},maximumBoundary:function()
{return this._maximumBoundaries;},minimumBoundary:function()
{return this._minimumBoundaries;},zeroTime:function()
{return this._minimumBoundaries;},boundarySpan:function()
{return this._maximumBoundaries-this._minimumBoundaries;}}
WebInspector.FlameChart.Events={SelectedNode:"SelectedNode"}
WebInspector.FlameChart.ColorGenerator=function()
{this._colorPairs={};this._colorIndexes=[];this._currentColorIndex=0;this._colorPairForID("(idle)::0",50);this._colorPairForID("(program)::0",50);this._colorPairForID("(garbage collector)::0",50);}
WebInspector.FlameChart.ColorGenerator.prototype={_colorPairForID:function(id,sat)
{if(typeof sat!=="number")
sat=100;var colorPairs=this._colorPairs;var colorPair=colorPairs[id];if(!colorPair){colorPairs[id]=colorPair=this._createPair(this._currentColorIndex++,sat);this._colorIndexes[colorPair.index]=colorPair;}
return colorPair;},_colorPairForIndex:function(index)
{return this._colorIndexes[index];},_createPair:function(index,sat)
{var hue=(index*7+12*(index%2))%360;return{index:index,highlighted:"hsla("+hue+", "+sat+"%, 33%, 0.7)",normal:"hsla("+hue+", "+sat+"%, 66%, 0.7)"}}}
WebInspector.FlameChart.Entry=function(colorPair,depth,duration,startTime,node)
{this.colorPair=colorPair;this.depth=depth;this.duration=duration;this.startTime=startTime;this.node=node;this.selfTime=0;}
WebInspector.FlameChart.prototype={selectRange:function(timeLeft,timeRight)
{this._overviewGrid.setWindow(timeLeft/this._totalTime,timeRight/this._totalTime);},_onWindowChanged:function(event)
{this._scheduleUpdate();},_startCanvasDragging:function(event)
{if(!this._timelineData)
return false;this._isDragging=true;this._wasDragged=false;this._dragStartPoint=event.pageX;this._dragStartWindowLeft=this._windowLeft;this._dragStartWindowRight=this._windowRight;return true;},_canvasDragging:function(event)
{var pixelShift=this._dragStartPoint-event.pageX;var windowShift=pixelShift/this._totalPixels;var windowLeft=Math.max(0,this._dragStartWindowLeft+windowShift);if(windowLeft===this._windowLeft)
return;windowShift=windowLeft-this._dragStartWindowLeft;var windowRight=Math.min(1,this._dragStartWindowRight+windowShift);if(windowRight===this._windowRight)
return;windowShift=windowRight-this._dragStartWindowRight;this._overviewGrid.setWindow(this._dragStartWindowLeft+windowShift,this._dragStartWindowRight+windowShift);this._wasDragged=true;},_endCanvasDragging:function()
{this._isDragging=false;},_calculateTimelineData:function()
{if(this._timelineData)
return this._timelineData;if(!this._cpuProfileView.profileHead)
return null;var samples=this._cpuProfileView.samples;var idToNode=this._cpuProfileView._idToNode;var gcNode=this._cpuProfileView._gcNode;var samplesCount=samples.length;var samplingInterval=this._cpuProfileView.samplingIntervalMs;var index=0;var entries=([]);var openIntervals=[];var stackTrace=[];var colorGenerator=WebInspector.FlameChart._colorGenerator;var colorEntryIndexes=[];var maxDepth=5;var depth=0;for(var sampleIndex=0;sampleIndex<samplesCount;sampleIndex++){var node=idToNode[samples[sampleIndex]];stackTrace.length=0;while(node){stackTrace.push(node);node=node.parent;}
stackTrace.pop();maxDepth=Math.max(maxDepth,depth);depth=0;node=stackTrace.pop();var intervalIndex;if(node===gcNode){while(depth<openIntervals.length){intervalIndex=openIntervals[depth].index;entries[intervalIndex].duration+=samplingInterval;++depth;}
if(openIntervals.length>0&&openIntervals.peekLast().node===node){entries[intervalIndex].selfTime+=samplingInterval;continue;}}
while(node&&depth<openIntervals.length&&node===openIntervals[depth].node){intervalIndex=openIntervals[depth].index;entries[intervalIndex].duration+=samplingInterval;node=stackTrace.pop();++depth;}
if(depth<openIntervals.length)
openIntervals.length=depth;if(!node){entries[intervalIndex].selfTime+=samplingInterval;continue;}
while(node){var colorPair=colorGenerator._colorPairForID(node.functionName+":"+node.url+":"+node.lineNumber);var indexesForColor=colorEntryIndexes[colorPair.index];if(!indexesForColor)
indexesForColor=colorEntryIndexes[colorPair.index]=[];var entry=new WebInspector.FlameChart.Entry(colorPair,depth,samplingInterval,sampleIndex*samplingInterval,node);indexesForColor.push(entries.length);entries.push(entry);openIntervals.push({node:node,index:index});++index;node=stackTrace.pop();++depth;}
entries[entries.length-1].selfTime+=samplingInterval;}
this._maxStackDepth=Math.max(maxDepth,depth);var entryColorIndexes=new Uint16Array(entries.length);var entryLevels=new Uint8Array(entries.length);var entryTotalTimes=new Float32Array(entries.length);var entryOffsets=new Float32Array(entries.length);var entryTitles=new Array(entries.length);var entryDeoptFlags=new Uint8Array(entries.length);for(var i=0;i<entries.length;++i){var entry=entries[i];entryColorIndexes[i]=colorPair.index;entryLevels[i]=entry.depth;entryTotalTimes[i]=entry.duration;entryOffsets[i]=entry.startTime;entryTitles[i]=entry.node.functionName;var reason=entry.node.deoptReason;entryDeoptFlags[i]=(reason&&reason!=="no reason");}
this._timelineData={entries:entries,totalTime:this._cpuProfileView.profileHead.totalTime,entryColorIndexes:entryColorIndexes,entryLevels:entryLevels,entryTotalTimes:entryTotalTimes,entryOffsets:entryOffsets,colorEntryIndexes:colorEntryIndexes,entryTitles:entryTitles,entryDeoptFlags:entryDeoptFlags};return this._timelineData;},_onMouseMove:function(event)
{if(this._isDragging)
return;var entryIndex=this._coordinatesToEntryIndex(event.offsetX,event.offsetY);if(this._highlightedEntryIndex===entryIndex)
return;if(entryIndex===-1||this._timelineData.entries[entryIndex].node.scriptId==="0")
this._canvas.style.cursor="default";else
this._canvas.style.cursor="pointer";this._highlightedEntryIndex=entryIndex;this._scheduleUpdate();},_millisecondsToString:function(ms)
{if(ms===0)
return"0";if(ms<1000)
return WebInspector.UIString("%.1f\u2009ms",ms);return Number.secondsToString(ms/1000,true);},_prepareHighlightedEntryInfo:function()
{if(this._isDragging)
return null;var entry=this._timelineData.entries[this._highlightedEntryIndex];if(!entry)
return null;var node=entry.node;if(!node)
return null;var entryInfo=[];function pushEntryInfoRow(title,text)
{var row={};row.title=title;row.text=text;entryInfo.push(row);}
pushEntryInfoRow(WebInspector.UIString("Name"),node.functionName);if(this._cpuProfileView.samples){var selfTime=this._millisecondsToString(entry.selfTime);var totalTime=this._millisecondsToString(entry.duration);pushEntryInfoRow(WebInspector.UIString("Self time"),selfTime);pushEntryInfoRow(WebInspector.UIString("Total time"),totalTime);}
if(node.url)
pushEntryInfoRow(WebInspector.UIString("URL"),node.url+":"+node.lineNumber);pushEntryInfoRow(WebInspector.UIString("Aggregated self time"),Number.secondsToString(node.selfTime/1000,true));pushEntryInfoRow(WebInspector.UIString("Aggregated total time"),Number.secondsToString(node.totalTime/1000,true));if(node.deoptReason&&node.deoptReason!=="no reason")
pushEntryInfoRow(WebInspector.UIString("Not optimized"),node.deoptReason);return entryInfo;},_onClick:function(e)
{if(this._wasDragged)
return;if(this._highlightedEntryIndex===-1)
return;var node=this._timelineData.entries[this._highlightedEntryIndex].node;this.dispatchEventToListeners(WebInspector.FlameChart.Events.SelectedNode,node);},_onMouseWheel:function(e)
{if(e.wheelDeltaY){const zoomFactor=1.1;const mouseWheelZoomSpeed=1/120;var zoom=Math.pow(zoomFactor,-e.wheelDeltaY*mouseWheelZoomSpeed);var overviewReference=(this._pixelWindowLeft+e.offsetX-this._paddingLeft)/this._totalPixels;this._overviewGrid.zoom(zoom,overviewReference);}else{var shift=Number.constrain(-1*this._windowWidth/4*e.wheelDeltaX/120,-this._windowLeft,1-this._windowRight);this._overviewGrid.setWindow(this._windowLeft+shift,this._windowRight+shift);}},_coordinatesToEntryIndex:function(x,y)
{var timelineData=this._timelineData;if(!timelineData)
return-1;var timelineEntries=timelineData.entries;var cursorTime=(x+this._pixelWindowLeft-this._paddingLeft)*this._pixelToTime;var cursorLevel=Math.floor((this._canvas.height/window.devicePixelRatio-y)/this._barHeight);for(var i=0;i<timelineEntries.length;++i){if(cursorTime<timelineEntries[i].startTime)
return-1;if(cursorTime<(timelineEntries[i].startTime+timelineEntries[i].duration)&&cursorLevel===timelineEntries[i].depth)
return i;}
return-1;},onResize:function()
{this._updateOverviewCanvas=true;this._scheduleUpdate();},_drawOverviewCanvas:function(width,height)
{if(!this._timelineData)
return;var timelineEntries=this._timelineData.entries;var drawData=new Uint8Array(width);var scaleFactor=width/this._totalTime;for(var entryIndex=0;entryIndex<timelineEntries.length;++entryIndex){var entry=timelineEntries[entryIndex];var start=Math.floor(entry.startTime*scaleFactor);var finish=Math.floor((entry.startTime+entry.duration)*scaleFactor);for(var x=start;x<finish;++x)
drawData[x]=Math.max(drawData[x],entry.depth+1);}
var ratio=window.devicePixelRatio;var canvasWidth=width*ratio;var canvasHeight=height*ratio;this._overviewCanvas.width=canvasWidth;this._overviewCanvas.height=canvasHeight;this._overviewCanvas.style.width=width+"px";this._overviewCanvas.style.height=height+"px";var context=this._overviewCanvas.getContext("2d");var yScaleFactor=canvasHeight/(this._maxStackDepth*1.1);context.lineWidth=1;context.translate(0.5,0.5);context.strokeStyle="rgba(20,0,0,0.4)";context.fillStyle="rgba(214,225,254,0.8)";context.moveTo(-1,canvasHeight-1);if(drawData)
context.lineTo(-1,Math.round(height-drawData[0]*yScaleFactor-1));var value;for(var x=0;x<width;++x){value=Math.round(canvasHeight-drawData[x]*yScaleFactor-1);context.lineTo(x*ratio,value);}
context.lineTo(canvasWidth+1,value);context.lineTo(canvasWidth+1,canvasHeight-1);context.fill();context.stroke();context.closePath();},draw:function(width,height)
{var timelineData=this._calculateTimelineData();if(!timelineData)
return;var ratio=window.devicePixelRatio;this._canvas.width=width*ratio;this._canvas.height=height*ratio;this._canvas.style.width=width+"px";this._canvas.style.height=height+"px";var context=this._canvas.getContext("2d");context.scale(ratio,ratio);var timeWindowRight=this._timeWindowRight;var timeToPixel=this._timeToPixel;var pixelWindowLeft=this._pixelWindowLeft;var paddingLeft=this._paddingLeft;var minWidth=this._minWidth;var entryTotalTimes=this._timelineData.entryTotalTimes;var entryOffsets=this._timelineData.entryOffsets;var entryLevels=this._timelineData.entryLevels;var colorEntryIndexes=this._timelineData.colorEntryIndexes;var entryTitles=this._timelineData.entryTitles;var entryDeoptFlags=this._timelineData.entryDeoptFlags;var colorGenerator=WebInspector.FlameChart._colorGenerator;var titleIndexes=new Uint32Array(this._timelineData.entryTotalTimes);var lastTitleIndex=0;var dotsWidth=context.measureText("\u2026").width;var textPaddingLeft=2;this._minTextWidth=context.measureText("\u2026").width+textPaddingLeft;var minTextWidth=this._minTextWidth;var marksField=[];for(var i=0;i<this._maxStackDepth;++i)
marksField.push(new Uint16Array(width));var barHeight=this._barHeight;var barX=0;var barWidth=0;var barRight=0;var barLevel=0;var bHeight=height-barHeight;context.strokeStyle="black";var colorPair;var entryIndex=0;var entryOffset=0;for(var colorIndex=0;colorIndex<colorEntryIndexes.length;++colorIndex){colorPair=colorGenerator._colorPairForIndex(colorIndex);context.fillStyle=colorPair.normal;var indexes=colorEntryIndexes[colorIndex];if(!indexes)
continue;context.beginPath();for(var i=0;i<indexes.length;++i){entryIndex=indexes[i];entryOffset=entryOffsets[entryIndex];if(entryOffset>timeWindowRight)
break;barX=Math.ceil(entryOffset*timeToPixel)-pixelWindowLeft+paddingLeft;barRight=Math.floor((entryOffset+entryTotalTimes[entryIndex])*timeToPixel)-pixelWindowLeft+paddingLeft;if(barRight<0)
continue;barWidth=(barRight-barX)||minWidth;barLevel=entryLevels[entryIndex];var marksRow=marksField[barLevel];if(barWidth<=marksRow[barX])
continue;marksRow[barX]=barWidth;if(entryIndex===this._highlightedEntryIndex){context.fill();context.beginPath();context.fillStyle=colorPair.highlighted;}
context.rect(barX,bHeight-barLevel*barHeight,barWidth,barHeight);if(entryIndex===this._highlightedEntryIndex){context.fill();context.beginPath();context.fillStyle=colorPair.normal;}
if(barWidth>minTextWidth)
titleIndexes[lastTitleIndex++]=entryIndex;}
context.fill();}
var font=(barHeight-4)+"px "+window.getComputedStyle(this.element,null).getPropertyValue("font-family");var boldFont="bold "+font;var isBoldFontSelected=false;context.font=font;context.textBaseline="alphabetic";context.fillStyle="#333";this._dotsWidth=context.measureText("\u2026").width;var textBaseHeight=bHeight+barHeight-4;for(var i=0;i<lastTitleIndex;++i){entryIndex=titleIndexes[i];if(isBoldFontSelected){if(!entryDeoptFlags[entryIndex]){context.font=font;isBoldFontSelected=false;}}else{if(entryDeoptFlags[entryIndex]){context.font=boldFont;isBoldFontSelected=true;}}
entryOffset=entryOffsets[entryIndex];barX=Math.floor(entryOffset*timeToPixel)-pixelWindowLeft+paddingLeft;barRight=Math.ceil((entryOffset+entryTotalTimes[entryIndex])*timeToPixel)-pixelWindowLeft+paddingLeft;barWidth=(barRight-barX)||minWidth;var xText=Math.max(0,barX);var widthText=barWidth-textPaddingLeft+barX-xText;var title=this._prepareText(context,entryTitles[entryIndex],widthText);if(title)
context.fillText(title,xText+textPaddingLeft,textBaseHeight-entryLevels[entryIndex]*barHeight);}
var entryInfo=this._prepareHighlightedEntryInfo();this._entryInfo.removeChildren();if(entryInfo)
this._entryInfo.appendChild(this._buildEntryInfo(entryInfo));},_buildEntryInfo:function(entryInfo)
{var infoTable=document.createElement("table");infoTable.className="info-table";for(var i=0;i<entryInfo.length;++i){var row=infoTable.createChild("tr");var titleCell=row.createChild("td");titleCell.textContent=entryInfo[i].title;titleCell.className="title";var textCell=row.createChild("td");textCell.textContent=entryInfo[i].text;}
return infoTable;},_prepareText:function(context,title,maxSize)
{if(maxSize<this._dotsWidth)
return null;var titleWidth=context.measureText(title).width;if(maxSize>titleWidth)
return title;maxSize-=this._dotsWidth;var dotRegExp=/[\.\$]/g;var match=dotRegExp.exec(title);if(!match){var visiblePartSize=maxSize/titleWidth;var newTextLength=Math.floor(title.length*visiblePartSize)+1;var minTextLength=4;if(newTextLength<minTextLength)
return null;var substring;do{--newTextLength;substring=title.substring(0,newTextLength);}while(context.measureText(substring).width>maxSize);return title.substring(0,newTextLength)+"\u2026";}
while(match){var substring=title.substring(match.index+1);var width=context.measureText(substring).width;if(maxSize>width)
return"\u2026"+substring;match=dotRegExp.exec(title);}
var i=0;do{++i;}while(context.measureText(title.substring(0,i)).width<maxSize);return title.substring(0,i-1)+"\u2026";},_scheduleUpdate:function()
{if(this._updateTimerId)
return;this._updateTimerId=setTimeout(this.update.bind(this),10);},_updateBoundaries:function()
{this._windowLeft=this._overviewGrid.windowLeft();this._windowRight=this._overviewGrid.windowRight();this._windowWidth=this._windowRight-this._windowLeft;this._totalTime=this._timelineData.totalTime;this._timeWindowLeft=this._windowLeft*this._totalTime;this._timeWindowRight=this._windowRight*this._totalTime;this._pixelWindowWidth=this._chartContainer.clientWidth;this._totalPixels=Math.floor(this._pixelWindowWidth/this._windowWidth);this._pixelWindowLeft=Math.floor(this._totalPixels*this._windowLeft);this._pixelWindowRight=Math.floor(this._totalPixels*this._windowRight);this._timeToPixel=this._totalPixels/this._totalTime;this._pixelToTime=this._totalTime/this._totalPixels;this._paddingLeftTime=this._paddingLeft/this._timeToPixel;},update:function()
{this._updateTimerId=0;if(!this._timelineData)
this._calculateTimelineData();if(!this._timelineData)
return;this._updateBoundaries();this.draw(this._chartContainer.clientWidth,this._chartContainer.clientHeight);this._calculator._updateBoundaries(this);this._overviewCalculator._updateBoundaries(this);this._timelineGrid.element.style.width=this.element.clientWidth;this._timelineGrid.updateDividers(this._calculator);this._overviewGrid.updateDividers(this._overviewCalculator);if(this._updateOverviewCanvas){this._drawOverviewCanvas(this._overviewContainer.clientWidth,this._overviewContainer.clientHeight-20);this._updateOverviewCanvas=false;}},__proto__:WebInspector.View.prototype};;WebInspector.HeapSnapshotArraySlice=function(array,start,end)
{this._array=array;this._start=start;this.length=end-start;}
WebInspector.HeapSnapshotArraySlice.prototype={item:function(index)
{return this._array[this._start+index];},slice:function(start,end)
{if(typeof end==="undefined")
end=this.length;return this._array.subarray(this._start+start,this._start+end);}}
WebInspector.HeapSnapshotEdge=function(snapshot,edges,edgeIndex)
{this._snapshot=snapshot;this._edges=edges;this.edgeIndex=edgeIndex||0;}
WebInspector.HeapSnapshotEdge.prototype={clone:function()
{return new WebInspector.HeapSnapshotEdge(this._snapshot,this._edges,this.edgeIndex);},hasStringName:function()
{throw new Error("Not implemented");},name:function()
{throw new Error("Not implemented");},node:function()
{return this._snapshot.createNode(this.nodeIndex());},nodeIndex:function()
{return this._edges.item(this.edgeIndex+this._snapshot._edgeToNodeOffset);},rawEdges:function()
{return this._edges;},toString:function()
{return"HeapSnapshotEdge: "+this.name();},type:function()
{return this._snapshot._edgeTypes[this._type()];},serialize:function()
{var node=this.node();return{name:this.name(),node:node.serialize(),nodeIndex:this.nodeIndex(),type:this.type(),distance:node.distance()};},_type:function()
{return this._edges.item(this.edgeIndex+this._snapshot._edgeTypeOffset);}};WebInspector.HeapSnapshotEdgeIterator=function(edge)
{this.edge=edge;}
WebInspector.HeapSnapshotEdgeIterator.prototype={rewind:function()
{this.edge.edgeIndex=0;},hasNext:function()
{return this.edge.edgeIndex<this.edge._edges.length;},index:function()
{return this.edge.edgeIndex;},setIndex:function(newIndex)
{this.edge.edgeIndex=newIndex;},item:function()
{return this.edge;},next:function()
{this.edge.edgeIndex+=this.edge._snapshot._edgeFieldsCount;}};WebInspector.HeapSnapshotRetainerEdge=function(snapshot,retainedNodeIndex,retainerIndex)
{this._snapshot=snapshot;this._retainedNodeIndex=retainedNodeIndex;var retainedNodeOrdinal=retainedNodeIndex/snapshot._nodeFieldCount;this._firstRetainer=snapshot._firstRetainerIndex[retainedNodeOrdinal];this._retainersCount=snapshot._firstRetainerIndex[retainedNodeOrdinal+1]-this._firstRetainer;this.setRetainerIndex(retainerIndex);}
WebInspector.HeapSnapshotRetainerEdge.prototype={clone:function()
{return new WebInspector.HeapSnapshotRetainerEdge(this._snapshot,this._retainedNodeIndex,this.retainerIndex());},hasStringName:function()
{return this._edge().hasStringName();},name:function()
{return this._edge().name();},node:function()
{return this._node();},nodeIndex:function()
{return this._nodeIndex;},retainerIndex:function()
{return this._retainerIndex;},setRetainerIndex:function(newIndex)
{if(newIndex!==this._retainerIndex){this._retainerIndex=newIndex;this.edgeIndex=newIndex;}},set edgeIndex(edgeIndex)
{var retainerIndex=this._firstRetainer+edgeIndex;this._globalEdgeIndex=this._snapshot._retainingEdges[retainerIndex];this._nodeIndex=this._snapshot._retainingNodes[retainerIndex];delete this._edgeInstance;delete this._nodeInstance;},_node:function()
{if(!this._nodeInstance)
this._nodeInstance=this._snapshot.createNode(this._nodeIndex);return this._nodeInstance;},_edge:function()
{if(!this._edgeInstance){var edgeIndex=this._globalEdgeIndex-this._node()._edgeIndexesStart();this._edgeInstance=this._snapshot.createEdge(this._node().rawEdges(),edgeIndex);}
return this._edgeInstance;},toString:function()
{return this._edge().toString();},serialize:function()
{var node=this.node();return{name:this.name(),node:node.serialize(),nodeIndex:this.nodeIndex(),type:this.type(),distance:node.distance()};},type:function()
{return this._edge().type();}}
WebInspector.HeapSnapshotRetainerEdgeIterator=function(retainer)
{this.retainer=retainer;}
WebInspector.HeapSnapshotRetainerEdgeIterator.prototype={rewind:function()
{this.retainer.setRetainerIndex(0);},hasNext:function()
{return this.retainer.retainerIndex()<this.retainer._retainersCount;},index:function()
{return this.retainer.retainerIndex();},setIndex:function(newIndex)
{this.retainer.setRetainerIndex(newIndex);},item:function()
{return this.retainer;},next:function()
{this.retainer.setRetainerIndex(this.retainer.retainerIndex()+1);}};WebInspector.HeapSnapshotNode=function(snapshot,nodeIndex)
{this._snapshot=snapshot;this._firstNodeIndex=nodeIndex;this.nodeIndex=nodeIndex;}
WebInspector.HeapSnapshotNode.prototype={distance:function()
{return this._snapshot._nodeDistances[this.nodeIndex/this._snapshot._nodeFieldCount];},className:function()
{throw new Error("Not implemented");},classIndex:function()
{throw new Error("Not implemented");},dominatorIndex:function()
{var nodeFieldCount=this._snapshot._nodeFieldCount;return this._snapshot._dominatorsTree[this.nodeIndex/this._snapshot._nodeFieldCount]*nodeFieldCount;},edges:function()
{return new WebInspector.HeapSnapshotEdgeIterator(this._snapshot.createEdge(this.rawEdges(),0));},edgesCount:function()
{return(this._edgeIndexesEnd()-this._edgeIndexesStart())/this._snapshot._edgeFieldsCount;},id:function()
{throw new Error("Not implemented");},isRoot:function()
{return this.nodeIndex===this._snapshot._rootNodeIndex;},name:function()
{return this._snapshot._strings[this._name()];},rawEdges:function()
{return new WebInspector.HeapSnapshotArraySlice(this._snapshot._containmentEdges,this._edgeIndexesStart(),this._edgeIndexesEnd());},retainedSize:function()
{var snapshot=this._snapshot;return snapshot._nodes[this.nodeIndex+snapshot._nodeRetainedSizeOffset];},retainers:function()
{return new WebInspector.HeapSnapshotRetainerEdgeIterator(this._snapshot.createRetainingEdge(this.nodeIndex,0));},selfSize:function()
{var snapshot=this._snapshot;return snapshot._nodes[this.nodeIndex+snapshot._nodeSelfSizeOffset];},type:function()
{return this._snapshot._nodeTypes[this._type()];},serialize:function()
{return{id:this.id(),name:this.name(),distance:this.distance(),nodeIndex:this.nodeIndex,retainedSize:this.retainedSize(),selfSize:this.selfSize(),type:this.type(),};},_name:function()
{var snapshot=this._snapshot;return snapshot._nodes[this.nodeIndex+snapshot._nodeNameOffset];},_edgeIndexesStart:function()
{return this._snapshot._firstEdgeIndexes[this._ordinal()];},_edgeIndexesEnd:function()
{return this._snapshot._firstEdgeIndexes[this._ordinal()+1];},_ordinal:function()
{return this.nodeIndex/this._snapshot._nodeFieldCount;},_nextNodeIndex:function()
{return this.nodeIndex+this._snapshot._nodeFieldCount;},_type:function()
{var snapshot=this._snapshot;return snapshot._nodes[this.nodeIndex+snapshot._nodeTypeOffset];}};WebInspector.HeapSnapshotNodeIterator=function(node)
{this.node=node;this._nodesLength=node._snapshot._nodes.length;}
WebInspector.HeapSnapshotNodeIterator.prototype={rewind:function()
{this.node.nodeIndex=this.node._firstNodeIndex;},hasNext:function()
{return this.node.nodeIndex<this._nodesLength;},index:function()
{return this.node.nodeIndex;},setIndex:function(newIndex)
{this.node.nodeIndex=newIndex;},item:function()
{return this.node;},next:function()
{this.node.nodeIndex=this.node._nextNodeIndex();}}
WebInspector.HeapSnapshotProgress=function(dispatcher)
{this._dispatcher=dispatcher;}
WebInspector.HeapSnapshotProgress.Event={Update:"ProgressUpdate"};WebInspector.HeapSnapshotProgress.prototype={updateStatus:function(status)
{this._sendUpdateEvent(WebInspector.UIString(status));},updateProgress:function(title,value,total)
{var percentValue=((total?(value/total):0)*100).toFixed(0);this._sendUpdateEvent(WebInspector.UIString(title,percentValue));},_sendUpdateEvent:function(text)
{if(this._dispatcher)
this._dispatcher.sendEvent(WebInspector.HeapSnapshotProgress.Event.Update,text);}}
WebInspector.HeapSnapshot=function(profile,progress)
{this.uid=profile.snapshot.uid;this._nodes=profile.nodes;this._containmentEdges=profile.edges;this._metaNode=profile.snapshot.meta;this._strings=profile.strings;this._progress=progress;this._noDistance=-5;this._rootNodeIndex=0;if(profile.snapshot.root_index)
this._rootNodeIndex=profile.snapshot.root_index;this._snapshotDiffs={};this._aggregatesForDiff=null;this._init();if(WebInspector.HeapSnapshot.enableAllocationProfiler){this._progress.updateStatus("Buiding allocation statistics\u2026");this._allocationProfile=new WebInspector.AllocationProfile(profile);this._progress.updateStatus("Done");}}
WebInspector.HeapSnapshot.enableAllocationProfiler=false;function HeapSnapshotMetainfo()
{this.node_fields=[];this.node_types=[];this.edge_fields=[];this.edge_types=[];this.trace_function_info_fields=[];this.trace_node_fields=[];this.type_strings={};}
function HeapSnapshotHeader()
{this.title="";this.uid=0;this.meta=new HeapSnapshotMetainfo();this.node_count=0;this.edge_count=0;}
WebInspector.HeapSnapshot.prototype={_init:function()
{var meta=this._metaNode;this._nodeTypeOffset=meta.node_fields.indexOf("type");this._nodeNameOffset=meta.node_fields.indexOf("name");this._nodeIdOffset=meta.node_fields.indexOf("id");this._nodeSelfSizeOffset=meta.node_fields.indexOf("self_size");this._nodeEdgeCountOffset=meta.node_fields.indexOf("edge_count");this._nodeFieldCount=meta.node_fields.length;this._nodeTypes=meta.node_types[this._nodeTypeOffset];this._nodeHiddenType=this._nodeTypes.indexOf("hidden");this._nodeObjectType=this._nodeTypes.indexOf("object");this._nodeNativeType=this._nodeTypes.indexOf("native");this._nodeConsStringType=this._nodeTypes.indexOf("concatenated string");this._nodeSlicedStringType=this._nodeTypes.indexOf("sliced string");this._nodeCodeType=this._nodeTypes.indexOf("code");this._nodeSyntheticType=this._nodeTypes.indexOf("synthetic");this._edgeFieldsCount=meta.edge_fields.length;this._edgeTypeOffset=meta.edge_fields.indexOf("type");this._edgeNameOffset=meta.edge_fields.indexOf("name_or_index");this._edgeToNodeOffset=meta.edge_fields.indexOf("to_node");this._edgeTypes=meta.edge_types[this._edgeTypeOffset];this._edgeTypes.push("invisible");this._edgeElementType=this._edgeTypes.indexOf("element");this._edgeHiddenType=this._edgeTypes.indexOf("hidden");this._edgeInternalType=this._edgeTypes.indexOf("internal");this._edgeShortcutType=this._edgeTypes.indexOf("shortcut");this._edgeWeakType=this._edgeTypes.indexOf("weak");this._edgeInvisibleType=this._edgeTypes.indexOf("invisible");this.nodeCount=this._nodes.length/this._nodeFieldCount;this._edgeCount=this._containmentEdges.length/this._edgeFieldsCount;this._progress.updateStatus("Building edge indexes\u2026");this._buildEdgeIndexes();this._progress.updateStatus("Marking invisible edges\u2026");this._markInvisibleEdges();this._progress.updateStatus("Building retainers\u2026");this._buildRetainers();this._progress.updateStatus("Calculating node flags\u2026");this._calculateFlags();this._progress.updateStatus("Calculating distances\u2026");this._calculateDistances();this._progress.updateStatus("Building postorder index\u2026");var result=this._buildPostOrderIndex();this._progress.updateStatus("Building dominator tree\u2026");this._dominatorsTree=this._buildDominatorTree(result.postOrderIndex2NodeOrdinal,result.nodeOrdinal2PostOrderIndex);this._progress.updateStatus("Calculating retained sizes\u2026");this._calculateRetainedSizes(result.postOrderIndex2NodeOrdinal);this._progress.updateStatus("Buiding dominated nodes\u2026");this._buildDominatedNodes();this._progress.updateStatus("Finished processing.");},_buildEdgeIndexes:function()
{var nodes=this._nodes;var nodeCount=this.nodeCount;var firstEdgeIndexes=this._firstEdgeIndexes=new Uint32Array(nodeCount+1);var nodeFieldCount=this._nodeFieldCount;var edgeFieldsCount=this._edgeFieldsCount;var nodeEdgeCountOffset=this._nodeEdgeCountOffset;firstEdgeIndexes[nodeCount]=this._containmentEdges.length;for(var nodeOrdinal=0,edgeIndex=0;nodeOrdinal<nodeCount;++nodeOrdinal){firstEdgeIndexes[nodeOrdinal]=edgeIndex;edgeIndex+=nodes[nodeOrdinal*nodeFieldCount+nodeEdgeCountOffset]*edgeFieldsCount;}},_buildRetainers:function()
{var retainingNodes=this._retainingNodes=new Uint32Array(this._edgeCount);var retainingEdges=this._retainingEdges=new Uint32Array(this._edgeCount);var firstRetainerIndex=this._firstRetainerIndex=new Uint32Array(this.nodeCount+1);var containmentEdges=this._containmentEdges;var edgeFieldsCount=this._edgeFieldsCount;var nodeFieldCount=this._nodeFieldCount;var edgeToNodeOffset=this._edgeToNodeOffset;var firstEdgeIndexes=this._firstEdgeIndexes;var nodeCount=this.nodeCount;for(var toNodeFieldIndex=edgeToNodeOffset,l=containmentEdges.length;toNodeFieldIndex<l;toNodeFieldIndex+=edgeFieldsCount){var toNodeIndex=containmentEdges[toNodeFieldIndex];if(toNodeIndex%nodeFieldCount)
throw new Error("Invalid toNodeIndex "+toNodeIndex);++firstRetainerIndex[toNodeIndex/nodeFieldCount];}
for(var i=0,firstUnusedRetainerSlot=0;i<nodeCount;i++){var retainersCount=firstRetainerIndex[i];firstRetainerIndex[i]=firstUnusedRetainerSlot;retainingNodes[firstUnusedRetainerSlot]=retainersCount;firstUnusedRetainerSlot+=retainersCount;}
firstRetainerIndex[nodeCount]=retainingNodes.length;var nextNodeFirstEdgeIndex=firstEdgeIndexes[0];for(var srcNodeOrdinal=0;srcNodeOrdinal<nodeCount;++srcNodeOrdinal){var firstEdgeIndex=nextNodeFirstEdgeIndex;nextNodeFirstEdgeIndex=firstEdgeIndexes[srcNodeOrdinal+1];var srcNodeIndex=srcNodeOrdinal*nodeFieldCount;for(var edgeIndex=firstEdgeIndex;edgeIndex<nextNodeFirstEdgeIndex;edgeIndex+=edgeFieldsCount){var toNodeIndex=containmentEdges[edgeIndex+edgeToNodeOffset];if(toNodeIndex%nodeFieldCount)
throw new Error("Invalid toNodeIndex "+toNodeIndex);var firstRetainerSlotIndex=firstRetainerIndex[toNodeIndex/nodeFieldCount];var nextUnusedRetainerSlotIndex=firstRetainerSlotIndex+(--retainingNodes[firstRetainerSlotIndex]);retainingNodes[nextUnusedRetainerSlotIndex]=srcNodeIndex;retainingEdges[nextUnusedRetainerSlotIndex]=edgeIndex;}}},createNode:function(nodeIndex)
{throw new Error("Not implemented");},createEdge:function(edges,edgeIndex)
{throw new Error("Not implemented");},createRetainingEdge:function(retainedNodeIndex,retainerIndex)
{throw new Error("Not implemented");},dispose:function()
{delete this._nodes;delete this._strings;delete this._retainingEdges;delete this._retainingNodes;delete this._firstRetainerIndex;if(this._aggregates){delete this._aggregates;delete this._aggregatesSortedFlags;}
delete this._dominatedNodes;delete this._firstDominatedNodeIndex;delete this._nodeDistances;delete this._dominatorsTree;},_allNodes:function()
{return new WebInspector.HeapSnapshotNodeIterator(this.rootNode());},rootNode:function()
{return this.createNode(this._rootNodeIndex);},get rootNodeIndex()
{return this._rootNodeIndex;},get totalSize()
{return this.rootNode().retainedSize();},_getDominatedIndex:function(nodeIndex)
{if(nodeIndex%this._nodeFieldCount)
throw new Error("Invalid nodeIndex: "+nodeIndex);return this._firstDominatedNodeIndex[nodeIndex/this._nodeFieldCount];},_dominatedNodesOfNode:function(node)
{var dominatedIndexFrom=this._getDominatedIndex(node.nodeIndex);var dominatedIndexTo=this._getDominatedIndex(node._nextNodeIndex());return new WebInspector.HeapSnapshotArraySlice(this._dominatedNodes,dominatedIndexFrom,dominatedIndexTo);},aggregates:function(sortedIndexes,key,filterString)
{if(!this._aggregates){this._aggregates={};this._aggregatesSortedFlags={};}
var aggregatesByClassName=this._aggregates[key];if(aggregatesByClassName){if(sortedIndexes&&!this._aggregatesSortedFlags[key]){this._sortAggregateIndexes(aggregatesByClassName);this._aggregatesSortedFlags[key]=sortedIndexes;}
return aggregatesByClassName;}
var filter;if(filterString)
filter=this._parseFilter(filterString);var aggregates=this._buildAggregates(filter);this._calculateClassesRetainedSize(aggregates.aggregatesByClassIndex,filter);aggregatesByClassName=aggregates.aggregatesByClassName;if(sortedIndexes)
this._sortAggregateIndexes(aggregatesByClassName);this._aggregatesSortedFlags[key]=sortedIndexes;this._aggregates[key]=aggregatesByClassName;return aggregatesByClassName;},allocationTracesTops:function()
{return this._allocationProfile.serializeTraceTops();},allocationNodeCallers:function(nodeId)
{return this._allocationProfile.serializeCallers(nodeId);},aggregatesForDiff:function()
{if(this._aggregatesForDiff)
return this._aggregatesForDiff;var aggregatesByClassName=this.aggregates(true,"allObjects");this._aggregatesForDiff={};var node=this.createNode();for(var className in aggregatesByClassName){var aggregate=aggregatesByClassName[className];var indexes=aggregate.idxs;var ids=new Array(indexes.length);var selfSizes=new Array(indexes.length);for(var i=0;i<indexes.length;i++){node.nodeIndex=indexes[i];ids[i]=node.id();selfSizes[i]=node.selfSize();}
this._aggregatesForDiff[className]={indexes:indexes,ids:ids,selfSizes:selfSizes};}
return this._aggregatesForDiff;},_isUserRoot:function(node)
{return true;},forEachRoot:function(action,userRootsOnly)
{for(var iter=this.rootNode().edges();iter.hasNext();iter.next()){var node=iter.edge.node();if(!userRootsOnly||this._isUserRoot(node))
action(node);}},_calculateDistances:function()
{var nodeFieldCount=this._nodeFieldCount;var nodeCount=this.nodeCount;var distances=new Int32Array(nodeCount);var noDistance=this._noDistance;for(var i=0;i<nodeCount;++i)
distances[i]=noDistance;var nodesToVisit=new Uint32Array(this.nodeCount);var nodesToVisitLength=0;function enqueueNode(node)
{var ordinal=node._ordinal();if(distances[ordinal]!==noDistance)
return;distances[ordinal]=0;nodesToVisit[nodesToVisitLength++]=node.nodeIndex;}
this.forEachRoot(enqueueNode,true);this._bfs(nodesToVisit,nodesToVisitLength,distances);nodesToVisitLength=0;this.forEachRoot(enqueueNode);this._bfs(nodesToVisit,nodesToVisitLength,distances);this._nodeDistances=distances;},_bfs:function(nodesToVisit,nodesToVisitLength,distances)
{var edgeFieldsCount=this._edgeFieldsCount;var nodeFieldCount=this._nodeFieldCount;var containmentEdges=this._containmentEdges;var firstEdgeIndexes=this._firstEdgeIndexes;var edgeToNodeOffset=this._edgeToNodeOffset;var edgeTypeOffset=this._edgeTypeOffset;var nodeCount=this.nodeCount;var containmentEdgesLength=containmentEdges.length;var edgeWeakType=this._edgeWeakType;var noDistance=this._noDistance;var index=0;while(index<nodesToVisitLength){var nodeIndex=nodesToVisit[index++];var nodeOrdinal=nodeIndex/nodeFieldCount;var distance=distances[nodeOrdinal]+1;var firstEdgeIndex=firstEdgeIndexes[nodeOrdinal];var edgesEnd=firstEdgeIndexes[nodeOrdinal+1];for(var edgeIndex=firstEdgeIndex;edgeIndex<edgesEnd;edgeIndex+=edgeFieldsCount){var edgeType=containmentEdges[edgeIndex+edgeTypeOffset];if(edgeType==edgeWeakType)
continue;var childNodeIndex=containmentEdges[edgeIndex+edgeToNodeOffset];var childNodeOrdinal=childNodeIndex/nodeFieldCount;if(distances[childNodeOrdinal]!==noDistance)
continue;distances[childNodeOrdinal]=distance;nodesToVisit[nodesToVisitLength++]=childNodeIndex;}}
if(nodesToVisitLength>nodeCount)
throw new Error("BFS failed. Nodes to visit ("+nodesToVisitLength+") is more than nodes count ("+nodeCount+")");},_buildAggregates:function(filter)
{var aggregates={};var aggregatesByClassName={};var classIndexes=[];var nodes=this._nodes;var mapAndFlag=this.userObjectsMapAndFlag();var flags=mapAndFlag?mapAndFlag.map:null;var flag=mapAndFlag?mapAndFlag.flag:0;var nodesLength=nodes.length;var nodeNativeType=this._nodeNativeType;var nodeFieldCount=this._nodeFieldCount;var selfSizeOffset=this._nodeSelfSizeOffset;var nodeTypeOffset=this._nodeTypeOffset;var node=this.rootNode();var nodeDistances=this._nodeDistances;for(var nodeIndex=0;nodeIndex<nodesLength;nodeIndex+=nodeFieldCount){var nodeOrdinal=nodeIndex/nodeFieldCount;if(flags&&!(flags[nodeOrdinal]&flag))
continue;node.nodeIndex=nodeIndex;if(filter&&!filter(node))
continue;var selfSize=nodes[nodeIndex+selfSizeOffset];if(!selfSize&&nodes[nodeIndex+nodeTypeOffset]!==nodeNativeType)
continue;var classIndex=node.classIndex();if(!(classIndex in aggregates)){var nodeType=node.type();var nameMatters=nodeType==="object"||nodeType==="native";var value={count:1,distance:nodeDistances[nodeOrdinal],self:selfSize,maxRet:0,type:nodeType,name:nameMatters?node.name():null,idxs:[nodeIndex]};aggregates[classIndex]=value;classIndexes.push(classIndex);aggregatesByClassName[node.className()]=value;}else{var clss=aggregates[classIndex];clss.distance=Math.min(clss.distance,nodeDistances[nodeOrdinal]);++clss.count;clss.self+=selfSize;clss.idxs.push(nodeIndex);}}
for(var i=0,l=classIndexes.length;i<l;++i){var classIndex=classIndexes[i];aggregates[classIndex].idxs=aggregates[classIndex].idxs.slice();}
return{aggregatesByClassName:aggregatesByClassName,aggregatesByClassIndex:aggregates};},_calculateClassesRetainedSize:function(aggregates,filter)
{var rootNodeIndex=this._rootNodeIndex;var node=this.createNode(rootNodeIndex);var list=[rootNodeIndex];var sizes=[-1];var classes=[];var seenClassNameIndexes={};var nodeFieldCount=this._nodeFieldCount;var nodeTypeOffset=this._nodeTypeOffset;var nodeNativeType=this._nodeNativeType;var dominatedNodes=this._dominatedNodes;var nodes=this._nodes;var mapAndFlag=this.userObjectsMapAndFlag();var flags=mapAndFlag?mapAndFlag.map:null;var flag=mapAndFlag?mapAndFlag.flag:0;var firstDominatedNodeIndex=this._firstDominatedNodeIndex;while(list.length){var nodeIndex=list.pop();node.nodeIndex=nodeIndex;var classIndex=node.classIndex();var seen=!!seenClassNameIndexes[classIndex];var nodeOrdinal=nodeIndex/nodeFieldCount;var dominatedIndexFrom=firstDominatedNodeIndex[nodeOrdinal];var dominatedIndexTo=firstDominatedNodeIndex[nodeOrdinal+1];if(!seen&&(!flags||(flags[nodeOrdinal]&flag))&&(!filter||filter(node))&&(node.selfSize()||nodes[nodeIndex+nodeTypeOffset]===nodeNativeType)){aggregates[classIndex].maxRet+=node.retainedSize();if(dominatedIndexFrom!==dominatedIndexTo){seenClassNameIndexes[classIndex]=true;sizes.push(list.length);classes.push(classIndex);}}
for(var i=dominatedIndexFrom;i<dominatedIndexTo;i++)
list.push(dominatedNodes[i]);var l=list.length;while(sizes[sizes.length-1]===l){sizes.pop();classIndex=classes.pop();seenClassNameIndexes[classIndex]=false;}}},_sortAggregateIndexes:function(aggregates)
{var nodeA=this.createNode();var nodeB=this.createNode();for(var clss in aggregates)
aggregates[clss].idxs.sort(function(idxA,idxB){nodeA.nodeIndex=idxA;nodeB.nodeIndex=idxB;return nodeA.id()<nodeB.id()?-1:1;});},_buildPostOrderIndex:function()
{var nodeFieldCount=this._nodeFieldCount;var nodes=this._nodes;var nodeCount=this.nodeCount;var rootNodeOrdinal=this._rootNodeIndex/nodeFieldCount;var edgeFieldsCount=this._edgeFieldsCount;var edgeTypeOffset=this._edgeTypeOffset;var edgeToNodeOffset=this._edgeToNodeOffset;var edgeShortcutType=this._edgeShortcutType;var firstEdgeIndexes=this._firstEdgeIndexes;var containmentEdges=this._containmentEdges;var containmentEdgesLength=this._containmentEdges.length;var mapAndFlag=this.userObjectsMapAndFlag();var flags=mapAndFlag?mapAndFlag.map:null;var flag=mapAndFlag?mapAndFlag.flag:0;var nodesToVisit=new Uint32Array(nodeCount);var postOrderIndex2NodeOrdinal=new Uint32Array(nodeCount);var nodeOrdinal2PostOrderIndex=new Uint32Array(nodeCount);var painted=new Uint8Array(nodeCount);var nodesToVisitLength=0;var postOrderIndex=0;var grey=1;var black=2;nodesToVisit[nodesToVisitLength++]=rootNodeOrdinal;painted[rootNodeOrdinal]=grey;while(nodesToVisitLength){var nodeOrdinal=nodesToVisit[nodesToVisitLength-1];if(painted[nodeOrdinal]===grey){painted[nodeOrdinal]=black;var nodeFlag=!flags||(flags[nodeOrdinal]&flag);var beginEdgeIndex=firstEdgeIndexes[nodeOrdinal];var endEdgeIndex=firstEdgeIndexes[nodeOrdinal+1];for(var edgeIndex=beginEdgeIndex;edgeIndex<endEdgeIndex;edgeIndex+=edgeFieldsCount){if(nodeOrdinal!==rootNodeOrdinal&&containmentEdges[edgeIndex+edgeTypeOffset]===edgeShortcutType)
continue;var childNodeIndex=containmentEdges[edgeIndex+edgeToNodeOffset];var childNodeOrdinal=childNodeIndex/nodeFieldCount;var childNodeFlag=!flags||(flags[childNodeOrdinal]&flag);if(nodeOrdinal!==rootNodeOrdinal&&childNodeFlag&&!nodeFlag)
continue;if(!painted[childNodeOrdinal]){painted[childNodeOrdinal]=grey;nodesToVisit[nodesToVisitLength++]=childNodeOrdinal;}}}else{nodeOrdinal2PostOrderIndex[nodeOrdinal]=postOrderIndex;postOrderIndex2NodeOrdinal[postOrderIndex++]=nodeOrdinal;--nodesToVisitLength;}}
if(postOrderIndex!==nodeCount){console.log("Error: Corrupted snapshot. "+(nodeCount-postOrderIndex)+" nodes are unreachable from the root:");var dumpNode=this.rootNode();for(var i=0;i<nodeCount;++i){if(painted[i]!==black){nodeOrdinal2PostOrderIndex[i]=postOrderIndex;postOrderIndex2NodeOrdinal[postOrderIndex++]=i;dumpNode.nodeIndex=i*nodeFieldCount;console.log(JSON.stringify(dumpNode.serialize()));for(var retainers=dumpNode.retainers();retainers.hasNext();retainers=retainers.item().node().retainers())
console.log("  edgeName: "+retainers.item().name()+" nodeClassName: "+retainers.item().node().className());}}}
return{postOrderIndex2NodeOrdinal:postOrderIndex2NodeOrdinal,nodeOrdinal2PostOrderIndex:nodeOrdinal2PostOrderIndex};},_buildDominatorTree:function(postOrderIndex2NodeOrdinal,nodeOrdinal2PostOrderIndex)
{var nodeFieldCount=this._nodeFieldCount;var nodes=this._nodes;var firstRetainerIndex=this._firstRetainerIndex;var retainingNodes=this._retainingNodes;var retainingEdges=this._retainingEdges;var edgeFieldsCount=this._edgeFieldsCount;var edgeTypeOffset=this._edgeTypeOffset;var edgeToNodeOffset=this._edgeToNodeOffset;var edgeShortcutType=this._edgeShortcutType;var firstEdgeIndexes=this._firstEdgeIndexes;var containmentEdges=this._containmentEdges;var containmentEdgesLength=this._containmentEdges.length;var rootNodeIndex=this._rootNodeIndex;var mapAndFlag=this.userObjectsMapAndFlag();var flags=mapAndFlag?mapAndFlag.map:null;var flag=mapAndFlag?mapAndFlag.flag:0;var nodesCount=postOrderIndex2NodeOrdinal.length;var rootPostOrderedIndex=nodesCount-1;var noEntry=nodesCount;var dominators=new Uint32Array(nodesCount);for(var i=0;i<rootPostOrderedIndex;++i)
dominators[i]=noEntry;dominators[rootPostOrderedIndex]=rootPostOrderedIndex;var affected=new Uint8Array(nodesCount);var nodeOrdinal;{nodeOrdinal=this._rootNodeIndex/nodeFieldCount;var beginEdgeToNodeFieldIndex=firstEdgeIndexes[nodeOrdinal]+edgeToNodeOffset;var endEdgeToNodeFieldIndex=firstEdgeIndexes[nodeOrdinal+1];for(var toNodeFieldIndex=beginEdgeToNodeFieldIndex;toNodeFieldIndex<endEdgeToNodeFieldIndex;toNodeFieldIndex+=edgeFieldsCount){var childNodeOrdinal=containmentEdges[toNodeFieldIndex]/nodeFieldCount;affected[nodeOrdinal2PostOrderIndex[childNodeOrdinal]]=1;}}
var changed=true;while(changed){changed=false;for(var postOrderIndex=rootPostOrderedIndex-1;postOrderIndex>=0;--postOrderIndex){if(affected[postOrderIndex]===0)
continue;affected[postOrderIndex]=0;if(dominators[postOrderIndex]===rootPostOrderedIndex)
continue;nodeOrdinal=postOrderIndex2NodeOrdinal[postOrderIndex];var nodeFlag=!flags||(flags[nodeOrdinal]&flag);var newDominatorIndex=noEntry;var beginRetainerIndex=firstRetainerIndex[nodeOrdinal];var endRetainerIndex=firstRetainerIndex[nodeOrdinal+1];for(var retainerIndex=beginRetainerIndex;retainerIndex<endRetainerIndex;++retainerIndex){var retainerEdgeIndex=retainingEdges[retainerIndex];var retainerEdgeType=containmentEdges[retainerEdgeIndex+edgeTypeOffset];var retainerNodeIndex=retainingNodes[retainerIndex];if(retainerNodeIndex!==rootNodeIndex&&retainerEdgeType===edgeShortcutType)
continue;var retainerNodeOrdinal=retainerNodeIndex/nodeFieldCount;var retainerNodeFlag=!flags||(flags[retainerNodeOrdinal]&flag);if(retainerNodeIndex!==rootNodeIndex&&nodeFlag&&!retainerNodeFlag)
continue;var retanerPostOrderIndex=nodeOrdinal2PostOrderIndex[retainerNodeOrdinal];if(dominators[retanerPostOrderIndex]!==noEntry){if(newDominatorIndex===noEntry)
newDominatorIndex=retanerPostOrderIndex;else{while(retanerPostOrderIndex!==newDominatorIndex){while(retanerPostOrderIndex<newDominatorIndex)
retanerPostOrderIndex=dominators[retanerPostOrderIndex];while(newDominatorIndex<retanerPostOrderIndex)
newDominatorIndex=dominators[newDominatorIndex];}}
if(newDominatorIndex===rootPostOrderedIndex)
break;}}
if(newDominatorIndex!==noEntry&&dominators[postOrderIndex]!==newDominatorIndex){dominators[postOrderIndex]=newDominatorIndex;changed=true;nodeOrdinal=postOrderIndex2NodeOrdinal[postOrderIndex];beginEdgeToNodeFieldIndex=firstEdgeIndexes[nodeOrdinal]+edgeToNodeOffset;endEdgeToNodeFieldIndex=firstEdgeIndexes[nodeOrdinal+1];for(var toNodeFieldIndex=beginEdgeToNodeFieldIndex;toNodeFieldIndex<endEdgeToNodeFieldIndex;toNodeFieldIndex+=edgeFieldsCount){var childNodeOrdinal=containmentEdges[toNodeFieldIndex]/nodeFieldCount;affected[nodeOrdinal2PostOrderIndex[childNodeOrdinal]]=1;}}}}
var dominatorsTree=new Uint32Array(nodesCount);for(var postOrderIndex=0,l=dominators.length;postOrderIndex<l;++postOrderIndex){nodeOrdinal=postOrderIndex2NodeOrdinal[postOrderIndex];dominatorsTree[nodeOrdinal]=postOrderIndex2NodeOrdinal[dominators[postOrderIndex]];}
return dominatorsTree;},_calculateRetainedSizes:function(postOrderIndex2NodeOrdinal)
{var nodeCount=this.nodeCount;var nodes=this._nodes;var nodeSelfSizeOffset=this._nodeSelfSizeOffset;var nodeFieldCount=this._nodeFieldCount;var dominatorsTree=this._dominatorsTree;var nodeRetainedSizeOffset=this._nodeRetainedSizeOffset=this._nodeEdgeCountOffset;delete this._nodeEdgeCountOffset;for(var nodeIndex=0,l=nodes.length;nodeIndex<l;nodeIndex+=nodeFieldCount)
nodes[nodeIndex+nodeRetainedSizeOffset]=nodes[nodeIndex+nodeSelfSizeOffset];for(var postOrderIndex=0;postOrderIndex<nodeCount-1;++postOrderIndex){var nodeOrdinal=postOrderIndex2NodeOrdinal[postOrderIndex];var nodeIndex=nodeOrdinal*nodeFieldCount;var dominatorIndex=dominatorsTree[nodeOrdinal]*nodeFieldCount;nodes[dominatorIndex+nodeRetainedSizeOffset]+=nodes[nodeIndex+nodeRetainedSizeOffset];}},_buildDominatedNodes:function()
{var indexArray=this._firstDominatedNodeIndex=new Uint32Array(this.nodeCount+1);var dominatedNodes=this._dominatedNodes=new Uint32Array(this.nodeCount-1);var nodeFieldCount=this._nodeFieldCount;var dominatorsTree=this._dominatorsTree;var fromNodeOrdinal=0;var toNodeOrdinal=this.nodeCount;var rootNodeOrdinal=this._rootNodeIndex/nodeFieldCount;if(rootNodeOrdinal===fromNodeOrdinal)
fromNodeOrdinal=1;else if(rootNodeOrdinal===toNodeOrdinal-1)
toNodeOrdinal=toNodeOrdinal-1;else
throw new Error("Root node is expected to be either first or last");for(var nodeOrdinal=fromNodeOrdinal;nodeOrdinal<toNodeOrdinal;++nodeOrdinal)
++indexArray[dominatorsTree[nodeOrdinal]];var firstDominatedNodeIndex=0;for(var i=0,l=this.nodeCount;i<l;++i){var dominatedCount=dominatedNodes[firstDominatedNodeIndex]=indexArray[i];indexArray[i]=firstDominatedNodeIndex;firstDominatedNodeIndex+=dominatedCount;}
indexArray[this.nodeCount]=dominatedNodes.length;for(var nodeOrdinal=fromNodeOrdinal;nodeOrdinal<toNodeOrdinal;++nodeOrdinal){var dominatorOrdinal=dominatorsTree[nodeOrdinal];var dominatedRefIndex=indexArray[dominatorOrdinal];dominatedRefIndex+=(--dominatedNodes[dominatedRefIndex]);dominatedNodes[dominatedRefIndex]=nodeOrdinal*nodeFieldCount;}},_markInvisibleEdges:function()
{throw new Error("Not implemented");},_calculateFlags:function()
{throw new Error("Not implemented");},userObjectsMapAndFlag:function()
{throw new Error("Not implemented");},calculateSnapshotDiff:function(baseSnapshotId,baseSnapshotAggregates)
{var snapshotDiff=this._snapshotDiffs[baseSnapshotId];if(snapshotDiff)
return snapshotDiff;snapshotDiff={};var aggregates=this.aggregates(true,"allObjects");for(var className in baseSnapshotAggregates){var baseAggregate=baseSnapshotAggregates[className];var diff=this._calculateDiffForClass(baseAggregate,aggregates[className]);if(diff)
snapshotDiff[className]=diff;}
var emptyBaseAggregate={ids:[],indexes:[],selfSizes:[]};for(var className in aggregates){if(className in baseSnapshotAggregates)
continue;snapshotDiff[className]=this._calculateDiffForClass(emptyBaseAggregate,aggregates[className]);}
this._snapshotDiffs[baseSnapshotId]=snapshotDiff;return snapshotDiff;},_calculateDiffForClass:function(baseAggregate,aggregate)
{var baseIds=baseAggregate.ids;var baseIndexes=baseAggregate.indexes;var baseSelfSizes=baseAggregate.selfSizes;var indexes=aggregate?aggregate.idxs:[];var i=0,l=baseIds.length;var j=0,m=indexes.length;var diff={addedCount:0,removedCount:0,addedSize:0,removedSize:0,deletedIndexes:[],addedIndexes:[]};var nodeB=this.createNode(indexes[j]);while(i<l&&j<m){var nodeAId=baseIds[i];if(nodeAId<nodeB.id()){diff.deletedIndexes.push(baseIndexes[i]);diff.removedCount++;diff.removedSize+=baseSelfSizes[i];++i;}else if(nodeAId>nodeB.id()){diff.addedIndexes.push(indexes[j]);diff.addedCount++;diff.addedSize+=nodeB.selfSize();nodeB.nodeIndex=indexes[++j];}else{++i;nodeB.nodeIndex=indexes[++j];}}
while(i<l){diff.deletedIndexes.push(baseIndexes[i]);diff.removedCount++;diff.removedSize+=baseSelfSizes[i];++i;}
while(j<m){diff.addedIndexes.push(indexes[j]);diff.addedCount++;diff.addedSize+=nodeB.selfSize();nodeB.nodeIndex=indexes[++j];}
diff.countDelta=diff.addedCount-diff.removedCount;diff.sizeDelta=diff.addedSize-diff.removedSize;if(!diff.addedCount&&!diff.removedCount)
return null;return diff;},_nodeForSnapshotObjectId:function(snapshotObjectId)
{for(var it=this._allNodes();it.hasNext();it.next()){if(it.node.id()===snapshotObjectId)
return it.node;}
return null;},nodeClassName:function(snapshotObjectId)
{var node=this._nodeForSnapshotObjectId(snapshotObjectId);if(node)
return node.className();return null;},dominatorIdsForNode:function(snapshotObjectId)
{var node=this._nodeForSnapshotObjectId(snapshotObjectId);if(!node)
return null;var result=[];while(!node.isRoot()){result.push(node.id());node.nodeIndex=node.dominatorIndex();}
return result;},_parseFilter:function(filter)
{if(!filter)
return null;var parsedFilter=eval("(function(){return "+filter+"})()");return parsedFilter.bind(this);},createEdgesProvider:function(nodeIndex,showHiddenData)
{var node=this.createNode(nodeIndex);var filter=this.containmentEdgesFilter(showHiddenData);return new WebInspector.HeapSnapshotEdgesProvider(this,filter,node.edges());},createEdgesProviderForTest:function(nodeIndex,filter)
{var node=this.createNode(nodeIndex);return new WebInspector.HeapSnapshotEdgesProvider(this,filter,node.edges());},retainingEdgesFilter:function(showHiddenData)
{return null;},containmentEdgesFilter:function(showHiddenData)
{return null;},createRetainingEdgesProvider:function(nodeIndex,showHiddenData)
{var node=this.createNode(nodeIndex);var filter=this.retainingEdgesFilter(showHiddenData);return new WebInspector.HeapSnapshotEdgesProvider(this,filter,node.retainers());},createAddedNodesProvider:function(baseSnapshotId,className)
{var snapshotDiff=this._snapshotDiffs[baseSnapshotId];var diffForClass=snapshotDiff[className];return new WebInspector.HeapSnapshotNodesProvider(this,null,diffForClass.addedIndexes);},createDeletedNodesProvider:function(nodeIndexes)
{return new WebInspector.HeapSnapshotNodesProvider(this,null,nodeIndexes);},classNodesFilter:function()
{return null;},createNodesProviderForClass:function(className,aggregatesKey)
{return new WebInspector.HeapSnapshotNodesProvider(this,this.classNodesFilter(),this.aggregates(false,aggregatesKey)[className].idxs);},createNodesProviderForDominator:function(nodeIndex)
{var node=this.createNode(nodeIndex);return new WebInspector.HeapSnapshotNodesProvider(this,null,this._dominatedNodesOfNode(node));},updateStaticData:function()
{return{nodeCount:this.nodeCount,rootNodeIndex:this._rootNodeIndex,totalSize:this.totalSize,uid:this.uid};}};WebInspector.HeapSnapshotFilteredOrderedIterator=function(iterator,filter,unfilteredIterationOrder)
{this._filter=filter;this._iterator=iterator;this._unfilteredIterationOrder=unfilteredIterationOrder;this._iterationOrder=null;this._position=0;this._currentComparator=null;this._sortedPrefixLength=0;this._sortedSuffixLength=0;}
WebInspector.HeapSnapshotFilteredOrderedIterator.prototype={_createIterationOrder:function()
{if(this._iterationOrder)
return;if(this._unfilteredIterationOrder&&!this._filter){this._iterationOrder=this._unfilteredIterationOrder.slice(0);this._unfilteredIterationOrder=null;return;}
this._iterationOrder=[];var iterator=this._iterator;if(!this._unfilteredIterationOrder&&!this._filter){for(iterator.rewind();iterator.hasNext();iterator.next())
this._iterationOrder.push(iterator.index());}else if(!this._unfilteredIterationOrder){for(iterator.rewind();iterator.hasNext();iterator.next()){if(this._filter(iterator.item()))
this._iterationOrder.push(iterator.index());}}else{var order=this._unfilteredIterationOrder.constructor===Array?this._unfilteredIterationOrder:this._unfilteredIterationOrder.slice(0);for(var i=0,l=order.length;i<l;++i){iterator.setIndex(order[i]);if(this._filter(iterator.item()))
this._iterationOrder.push(iterator.index());}
this._unfilteredIterationOrder=null;}},rewind:function()
{this._position=0;},hasNext:function()
{return this._position<this._iterationOrder.length;},isEmpty:function()
{if(this._iterationOrder)
return!this._iterationOrder.length;if(this._unfilteredIterationOrder&&!this._filter)
return!this._unfilteredIterationOrder.length;var iterator=this._iterator;if(!this._unfilteredIterationOrder&&!this._filter){iterator.rewind();return!iterator.hasNext();}else if(!this._unfilteredIterationOrder){for(iterator.rewind();iterator.hasNext();iterator.next())
if(this._filter(iterator.item()))
return false;}else{var order=this._unfilteredIterationOrder.constructor===Array?this._unfilteredIterationOrder:this._unfilteredIterationOrder.slice(0);for(var i=0,l=order.length;i<l;++i){iterator.setIndex(order[i]);if(this._filter(iterator.item()))
return false;}}
return true;},item:function()
{this._iterator.setIndex(this._iterationOrder[this._position]);return this._iterator.item();},get length()
{this._createIterationOrder();return this._iterationOrder.length;},next:function()
{++this._position;},serializeItemsRange:function(begin,end)
{this._createIterationOrder();if(begin>end)
throw new Error("Start position > end position: "+begin+" > "+end);if(end>this._iterationOrder.length)
end=this._iterationOrder.length;if(this._sortedPrefixLength<end&&begin<this._iterationOrder.length-this._sortedSuffixLength){this.sort(this._currentComparator,this._sortedPrefixLength,this._iterationOrder.length-1-this._sortedSuffixLength,begin,end-1);if(begin<=this._sortedPrefixLength)
this._sortedPrefixLength=end;if(end>=this._iterationOrder.length-this._sortedSuffixLength)
this._sortedSuffixLength=this._iterationOrder.length-begin;}
this._position=begin;var startPosition=this._position;var count=end-begin;var result=new Array(count);for(var i=0;i<count&&this.hasNext();++i,this.next())
result[i]=this.item().serialize();result.length=i;result.totalLength=this._iterationOrder.length;result.startPosition=startPosition;result.endPosition=this._position;return result;},sortAll:function()
{this._createIterationOrder();if(this._sortedPrefixLength+this._sortedSuffixLength>=this._iterationOrder.length)
return;this.sort(this._currentComparator,this._sortedPrefixLength,this._iterationOrder.length-1-this._sortedSuffixLength,this._sortedPrefixLength,this._iterationOrder.length-1-this._sortedSuffixLength);this._sortedPrefixLength=this._iterationOrder.length;this._sortedSuffixLength=0;},sortAndRewind:function(comparator)
{this._currentComparator=comparator;this._sortedPrefixLength=0;this._sortedSuffixLength=0;this.rewind();}}
WebInspector.HeapSnapshotFilteredOrderedIterator.prototype.createComparator=function(fieldNames)
{return{fieldName1:fieldNames[0],ascending1:fieldNames[1],fieldName2:fieldNames[2],ascending2:fieldNames[3]};}
WebInspector.HeapSnapshotEdgesProvider=function(snapshot,filter,edgesIter)
{this.snapshot=snapshot;WebInspector.HeapSnapshotFilteredOrderedIterator.call(this,edgesIter,filter);}
WebInspector.HeapSnapshotEdgesProvider.prototype={sort:function(comparator,leftBound,rightBound,windowLeft,windowRight)
{var fieldName1=comparator.fieldName1;var fieldName2=comparator.fieldName2;var ascending1=comparator.ascending1;var ascending2=comparator.ascending2;var edgeA=this._iterator.item().clone();var edgeB=edgeA.clone();var nodeA=this.snapshot.createNode();var nodeB=this.snapshot.createNode();function compareEdgeFieldName(ascending,indexA,indexB)
{edgeA.edgeIndex=indexA;edgeB.edgeIndex=indexB;if(edgeB.name()==="__proto__")return-1;if(edgeA.name()==="__proto__")return 1;var result=edgeA.hasStringName()===edgeB.hasStringName()?(edgeA.name()<edgeB.name()?-1:(edgeA.name()>edgeB.name()?1:0)):(edgeA.hasStringName()?-1:1);return ascending?result:-result;}
function compareNodeField(fieldName,ascending,indexA,indexB)
{edgeA.edgeIndex=indexA;nodeA.nodeIndex=edgeA.nodeIndex();var valueA=nodeA[fieldName]();edgeB.edgeIndex=indexB;nodeB.nodeIndex=edgeB.nodeIndex();var valueB=nodeB[fieldName]();var result=valueA<valueB?-1:(valueA>valueB?1:0);return ascending?result:-result;}
function compareEdgeAndNode(indexA,indexB){var result=compareEdgeFieldName(ascending1,indexA,indexB);if(result===0)
result=compareNodeField(fieldName2,ascending2,indexA,indexB);if(result===0)
return indexA-indexB;return result;}
function compareNodeAndEdge(indexA,indexB){var result=compareNodeField(fieldName1,ascending1,indexA,indexB);if(result===0)
result=compareEdgeFieldName(ascending2,indexA,indexB);if(result===0)
return indexA-indexB;return result;}
function compareNodeAndNode(indexA,indexB){var result=compareNodeField(fieldName1,ascending1,indexA,indexB);if(result===0)
result=compareNodeField(fieldName2,ascending2,indexA,indexB);if(result===0)
return indexA-indexB;return result;}
if(fieldName1==="!edgeName")
this._iterationOrder.sortRange(compareEdgeAndNode,leftBound,rightBound,windowLeft,windowRight);else if(fieldName2==="!edgeName")
this._iterationOrder.sortRange(compareNodeAndEdge,leftBound,rightBound,windowLeft,windowRight);else
this._iterationOrder.sortRange(compareNodeAndNode,leftBound,rightBound,windowLeft,windowRight);},__proto__:WebInspector.HeapSnapshotFilteredOrderedIterator.prototype}
WebInspector.HeapSnapshotNodesProvider=function(snapshot,filter,nodeIndexes)
{this.snapshot=snapshot;WebInspector.HeapSnapshotFilteredOrderedIterator.call(this,snapshot._allNodes(),filter,nodeIndexes);}
WebInspector.HeapSnapshotNodesProvider.prototype={nodePosition:function(snapshotObjectId)
{this._createIterationOrder();if(this.isEmpty())
return-1;this.sortAll();var node=this.snapshot.createNode();for(var i=0;i<this._iterationOrder.length;i++){node.nodeIndex=this._iterationOrder[i];if(node.id()===snapshotObjectId)
return i;}
return-1;},sort:function(comparator,leftBound,rightBound,windowLeft,windowRight)
{var fieldName1=comparator.fieldName1;var fieldName2=comparator.fieldName2;var ascending1=comparator.ascending1;var ascending2=comparator.ascending2;var nodeA=this.snapshot.createNode();var nodeB=this.snapshot.createNode();function sortByNodeField(fieldName,ascending)
{var valueOrFunctionA=nodeA[fieldName];var valueA=typeof valueOrFunctionA!=="function"?valueOrFunctionA:valueOrFunctionA.call(nodeA);var valueOrFunctionB=nodeB[fieldName];var valueB=typeof valueOrFunctionB!=="function"?valueOrFunctionB:valueOrFunctionB.call(nodeB);var result=valueA<valueB?-1:(valueA>valueB?1:0);return ascending?result:-result;}
function sortByComparator(indexA,indexB){nodeA.nodeIndex=indexA;nodeB.nodeIndex=indexB;var result=sortByNodeField(fieldName1,ascending1);if(result===0)
result=sortByNodeField(fieldName2,ascending2);if(result===0)
return indexA-indexB;return result;}
this._iterationOrder.sortRange(sortByComparator,leftBound,rightBound,windowLeft,windowRight);},__proto__:WebInspector.HeapSnapshotFilteredOrderedIterator.prototype};WebInspector.HeapSnapshotSortableDataGrid=function(columns)
{WebInspector.DataGrid.call(this,columns);this._recursiveSortingDepth=0;this._highlightedNode=null;this._populatedAndSorted=false;this.addEventListener("sorting complete",this._sortingComplete,this);this.addEventListener(WebInspector.DataGrid.Events.SortingChanged,this.sortingChanged,this);}
WebInspector.HeapSnapshotSortableDataGrid.Events={ContentShown:"ContentShown"}
WebInspector.HeapSnapshotSortableDataGrid.prototype={defaultPopulateCount:function()
{return 100;},dispose:function()
{var children=this.topLevelNodes();for(var i=0,l=children.length;i<l;++i)
children[i].dispose();},wasShown:function()
{if(this._populatedAndSorted)
this.dispatchEventToListeners(WebInspector.HeapSnapshotSortableDataGrid.Events.ContentShown,this);},_sortingComplete:function()
{this.removeEventListener("sorting complete",this._sortingComplete,this);this._populatedAndSorted=true;this.dispatchEventToListeners(WebInspector.HeapSnapshotSortableDataGrid.Events.ContentShown,this);},willHide:function()
{this._clearCurrentHighlight();},populateContextMenu:function(profilesPanel,contextMenu,event)
{var td=event.target.enclosingNodeOrSelfWithNodeName("td");if(!td)
return;var node=td.heapSnapshotNode;function revealInDominatorsView()
{profilesPanel.showObject(node.snapshotNodeId,"Dominators");}
function revealInSummaryView()
{profilesPanel.showObject(node.snapshotNodeId,"Summary");}
if(node&&node.showRetainingEdges){contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Reveal in Summary view":"Reveal in Summary View"),revealInSummaryView.bind(this));contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Reveal in Dominators view":"Reveal in Dominators View"),revealInDominatorsView.bind(this));}
else if(node instanceof WebInspector.HeapSnapshotInstanceNode||node instanceof WebInspector.HeapSnapshotObjectNode){contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Reveal in Dominators view":"Reveal in Dominators View"),revealInDominatorsView.bind(this));}else if(node instanceof WebInspector.HeapSnapshotDominatorObjectNode){contextMenu.appendItem(WebInspector.UIString(WebInspector.useLowerCaseMenuTitles()?"Reveal in Summary view":"Reveal in Summary View"),revealInSummaryView.bind(this));}},resetSortingCache:function()
{delete this._lastSortColumnIdentifier;delete this._lastSortAscending;},topLevelNodes:function()
{return this.rootNode().children;},highlightObjectByHeapSnapshotId:function(heapSnapshotObjectId)
{},highlightNode:function(node)
{var prevNode=this._highlightedNode;this._clearCurrentHighlight();this._highlightedNode=node;this._highlightedNode.element.addStyleClass("highlighted-row");if(node===prevNode){var element=node.element;var parent=element.parentElement;var nextSibling=element.nextSibling;parent.removeChild(element);parent.insertBefore(element,nextSibling);}},nodeWasDetached:function(node)
{if(this._highlightedNode===node)
this._clearCurrentHighlight();},_clearCurrentHighlight:function()
{if(!this._highlightedNode)
return
this._highlightedNode.element.removeStyleClass("highlighted-row");this._highlightedNode=null;},changeNameFilter:function(filter)
{filter=filter.toLowerCase();var children=this.topLevelNodes();for(var i=0,l=children.length;i<l;++i){var node=children[i];if(node.depth===0)
node.revealed=node._name.toLowerCase().indexOf(filter)!==-1;}
this.updateVisibleNodes();},sortingChanged:function()
{var sortAscending=this.isSortOrderAscending();var sortColumnIdentifier=this.sortColumnIdentifier();if(this._lastSortColumnIdentifier===sortColumnIdentifier&&this._lastSortAscending===sortAscending)
return;this._lastSortColumnIdentifier=sortColumnIdentifier;this._lastSortAscending=sortAscending;var sortFields=this._sortFields(sortColumnIdentifier,sortAscending);function SortByTwoFields(nodeA,nodeB)
{var field1=nodeA[sortFields[0]];var field2=nodeB[sortFields[0]];var result=field1<field2?-1:(field1>field2?1:0);if(!sortFields[1])
result=-result;if(result!==0)
return result;field1=nodeA[sortFields[2]];field2=nodeB[sortFields[2]];result=field1<field2?-1:(field1>field2?1:0);if(!sortFields[3])
result=-result;return result;}
this._performSorting(SortByTwoFields);},_performSorting:function(sortFunction)
{this.recursiveSortingEnter();var children=this._topLevelNodes;this.rootNode().removeChildren();children.sort(sortFunction);for(var i=0,l=children.length;i<l;++i){var child=children[i];this.appendChildAfterSorting(child);if(child.expanded)
child.sort();}
this.updateVisibleNodes();this.recursiveSortingLeave();},appendChildAfterSorting:function(child)
{var revealed=child.revealed;this.rootNode().appendChild(child);child.revealed=revealed;},updateVisibleNodes:function()
{},recursiveSortingEnter:function()
{++this._recursiveSortingDepth;},recursiveSortingLeave:function()
{if(!this._recursiveSortingDepth)
return;if(!--this._recursiveSortingDepth)
this.dispatchEventToListeners("sorting complete");},__proto__:WebInspector.DataGrid.prototype}
WebInspector.HeapSnapshotViewportDataGrid=function(columns)
{WebInspector.HeapSnapshotSortableDataGrid.call(this,columns);this.scrollContainer.addEventListener("scroll",this._onScroll.bind(this),true);this._topLevelNodes=[];this._topPadding=new WebInspector.HeapSnapshotPaddingNode();this._bottomPadding=new WebInspector.HeapSnapshotPaddingNode();this._nodeToHighlightAfterScroll=null;}
WebInspector.HeapSnapshotViewportDataGrid.prototype={topLevelNodes:function()
{return this._topLevelNodes;},appendChildAfterSorting:function(child)
{},updateVisibleNodes:function()
{var scrollTop=this.scrollContainer.scrollTop;var viewPortHeight=this.scrollContainer.offsetHeight;this._removePaddingRows();var children=this._topLevelNodes;var i=0;var topPadding=0;while(i<children.length){if(children[i].revealed){var newTop=topPadding+children[i].nodeHeight();if(newTop>scrollTop)
break;topPadding=newTop;}
++i;}
var selectedNode=this.selectedNode;this.rootNode().removeChildren();var heightToFill=viewPortHeight+(scrollTop-topPadding);var filledHeight=0;while(i<children.length&&filledHeight<heightToFill){if(children[i].revealed){this.rootNode().appendChild(children[i]);filledHeight+=children[i].nodeHeight();}
++i;}
var bottomPadding=0;while(i<children.length){bottomPadding+=children[i].nodeHeight();++i;}
this._addPaddingRows(topPadding,bottomPadding);if(selectedNode){if(selectedNode.parent){selectedNode.select(true);}else{this.selectedNode=selectedNode;}}},appendTopLevelNode:function(node)
{this._topLevelNodes.push(node);},removeTopLevelNodes:function()
{this.rootNode().removeChildren();this._topLevelNodes=[];},highlightNode:function(node)
{if(this._isScrolledIntoView(node.element))
WebInspector.HeapSnapshotSortableDataGrid.prototype.highlightNode.call(this,node);else{node.element.scrollIntoViewIfNeeded(true);this._nodeToHighlightAfterScroll=node;}},_isScrolledIntoView:function(element)
{var viewportTop=this.scrollContainer.scrollTop;var viewportBottom=viewportTop+this.scrollContainer.clientHeight;var elemTop=element.offsetTop
var elemBottom=elemTop+element.offsetHeight;return elemBottom<=viewportBottom&&elemTop>=viewportTop;},_addPaddingRows:function(top,bottom)
{if(this._topPadding.element.parentNode!==this.dataTableBody)
this.dataTableBody.insertBefore(this._topPadding.element,this.dataTableBody.firstChild);if(this._bottomPadding.element.parentNode!==this.dataTableBody)
this.dataTableBody.insertBefore(this._bottomPadding.element,this.dataTableBody.lastChild);this._topPadding.setHeight(top);this._bottomPadding.setHeight(bottom);},_removePaddingRows:function()
{this._bottomPadding.removeFromTable();this._topPadding.removeFromTable();},onResize:function()
{WebInspector.HeapSnapshotSortableDataGrid.prototype.onResize.call(this);this.updateVisibleNodes();},_onScroll:function(event)
{this.updateVisibleNodes();if(this._nodeToHighlightAfterScroll){WebInspector.HeapSnapshotSortableDataGrid.prototype.highlightNode.call(this,this._nodeToHighlightAfterScroll);this._nodeToHighlightAfterScroll=null;}},__proto__:WebInspector.HeapSnapshotSortableDataGrid.prototype}
WebInspector.HeapSnapshotPaddingNode=function()
{this.element=document.createElement("tr");this.element.addStyleClass("revealed");}
WebInspector.HeapSnapshotPaddingNode.prototype={setHeight:function(height)
{this.element.style.height=height+"px";},removeFromTable:function()
{var parent=this.element.parentNode;if(parent)
parent.removeChild(this.element);}}
WebInspector.HeapSnapshotContainmentDataGrid=function(columns)
{columns=columns||[{id:"object",title:WebInspector.UIString("Object"),disclosure:true,sortable:true},{id:"distance",title:WebInspector.UIString("Distance"),width:"80px",sortable:true},{id:"shallowSize",title:WebInspector.UIString("Shallow Size"),width:"120px",sortable:true},{id:"retainedSize",title:WebInspector.UIString("Retained Size"),width:"120px",sortable:true,sort:WebInspector.DataGrid.Order.Descending}];WebInspector.HeapSnapshotSortableDataGrid.call(this,columns);}
WebInspector.HeapSnapshotContainmentDataGrid.prototype={setDataSource:function(snapshot,nodeIndex)
{this.snapshot=snapshot;var node=new WebInspector.HeapSnapshotNode(snapshot,nodeIndex||snapshot.rootNodeIndex);var fakeEdge={node:node};this.setRootNode(new WebInspector.HeapSnapshotObjectNode(this,false,fakeEdge,null));this.rootNode().sort();},sortingChanged:function()
{this.rootNode().sort();},__proto__:WebInspector.HeapSnapshotSortableDataGrid.prototype}
WebInspector.HeapSnapshotRetainmentDataGrid=function()
{this.showRetainingEdges=true;var columns=[{id:"object",title:WebInspector.UIString("Object"),disclosure:true,sortable:true},{id:"distance",title:WebInspector.UIString("Distance"),width:"80px",sortable:true,sort:WebInspector.DataGrid.Order.Ascending},{id:"shallowSize",title:WebInspector.UIString("Shallow Size"),width:"120px",sortable:true},{id:"retainedSize",title:WebInspector.UIString("Retained Size"),width:"120px",sortable:true}];WebInspector.HeapSnapshotContainmentDataGrid.call(this,columns);}
WebInspector.HeapSnapshotRetainmentDataGrid.Events={ExpandRetainersComplete:"ExpandRetainersComplete"}
WebInspector.HeapSnapshotRetainmentDataGrid.prototype={_sortFields:function(sortColumn,sortAscending)
{return{object:["_name",sortAscending,"_count",false],count:["_count",sortAscending,"_name",true],shallowSize:["_shallowSize",sortAscending,"_name",true],retainedSize:["_retainedSize",sortAscending,"_name",true],distance:["_distance",sortAscending,"_name",true]}[sortColumn];},reset:function()
{this.rootNode().removeChildren();this.resetSortingCache();},setDataSource:function(snapshot,nodeIndex)
{WebInspector.HeapSnapshotContainmentDataGrid.prototype.setDataSource.call(this,snapshot,nodeIndex);var dataGrid=this;var maxExpandLevels=20;function populateComplete()
{this.removeEventListener(WebInspector.HeapSnapshotGridNode.Events.PopulateComplete,populateComplete,this);this.expand();if(--maxExpandLevels>0&&this.children.length>0){var retainer=this.children[0];if(retainer._distance>1){retainer.addEventListener(WebInspector.HeapSnapshotGridNode.Events.PopulateComplete,populateComplete,retainer);retainer.populate();return;}}
dataGrid.dispatchEventToListeners(WebInspector.HeapSnapshotRetainmentDataGrid.Events.ExpandRetainersComplete);}
this.rootNode().addEventListener(WebInspector.HeapSnapshotGridNode.Events.PopulateComplete,populateComplete,this.rootNode());},__proto__:WebInspector.HeapSnapshotContainmentDataGrid.prototype}
WebInspector.HeapSnapshotConstructorsDataGrid=function()
{var columns=[{id:"object",title:WebInspector.UIString("Constructor"),disclosure:true,sortable:true},{id:"distance",title:WebInspector.UIString("Distance"),width:"90px",sortable:true},{id:"count",title:WebInspector.UIString("Objects Count"),width:"90px",sortable:true},{id:"shallowSize",title:WebInspector.UIString("Shallow Size"),width:"120px",sortable:true},{id:"retainedSize",title:WebInspector.UIString("Retained Size"),width:"120px",sort:WebInspector.DataGrid.Order.Descending,sortable:true}];WebInspector.HeapSnapshotViewportDataGrid.call(this,columns);this._profileIndex=-1;this._topLevelNodes=[];this._objectIdToSelect=null;}
WebInspector.HeapSnapshotConstructorsDataGrid.Request=function(minNodeId,maxNodeId)
{if(typeof minNodeId==="number"){this.key=minNodeId+".."+maxNodeId;this.filter="function(node) { var id = node.id(); return id > "+minNodeId+" && id <= "+maxNodeId+"; }";}else{this.key="allObjects";this.filter=null;}}
WebInspector.HeapSnapshotConstructorsDataGrid.prototype={_sortFields:function(sortColumn,sortAscending)
{return{object:["_name",sortAscending,"_count",false],distance:["_distance",sortAscending,"_retainedSize",true],count:["_count",sortAscending,"_name",true],shallowSize:["_shallowSize",sortAscending,"_name",true],retainedSize:["_retainedSize",sortAscending,"_name",true]}[sortColumn];},highlightObjectByHeapSnapshotId:function(id)
{if(!this.snapshot){this._objectIdToSelect=id;return;}
function didGetClassName(className)
{var constructorNodes=this.topLevelNodes();for(var i=0;i<constructorNodes.length;i++){var parent=constructorNodes[i];if(parent._name===className){parent.revealNodeBySnapshotObjectId(parseInt(id,10));return;}}}
this.snapshot.nodeClassName(parseInt(id,10),didGetClassName.bind(this));},setDataSource:function(snapshot)
{this.snapshot=snapshot;if(this._profileIndex===-1)
this._populateChildren();if(this._objectIdToSelect){this.highlightObjectByHeapSnapshotId(this._objectIdToSelect);this._objectIdToSelect=null;}},setSelectionRange:function(minNodeId,maxNodeId)
{this._populateChildren(new WebInspector.HeapSnapshotConstructorsDataGrid.Request(minNodeId,maxNodeId));},_aggregatesReceived:function(key,aggregates)
{this._requestInProgress=null;if(this._nextRequest){this.snapshot.aggregates(false,this._nextRequest.key,this._nextRequest.filter,this._aggregatesReceived.bind(this,this._nextRequest.key));this._requestInProgress=this._nextRequest;this._nextRequest=null;}
this.dispose();this.removeTopLevelNodes();this.resetSortingCache();for(var constructor in aggregates)
this.appendTopLevelNode(new WebInspector.HeapSnapshotConstructorNode(this,constructor,aggregates[constructor],key));this.sortingChanged();this._lastKey=key;},_populateChildren:function(request)
{request=request||new WebInspector.HeapSnapshotConstructorsDataGrid.Request();if(this._requestInProgress){this._nextRequest=this._requestInProgress.key===request.key?null:request;return;}
if(this._lastKey===request.key)
return;this._requestInProgress=request;this.snapshot.aggregates(false,request.key,request.filter,this._aggregatesReceived.bind(this,request.key));},filterSelectIndexChanged:function(profiles,profileIndex)
{this._profileIndex=profileIndex;var request=null;if(profileIndex!==-1){var minNodeId=profileIndex>0?profiles[profileIndex-1].maxJSObjectId:0;var maxNodeId=profiles[profileIndex].maxJSObjectId;request=new WebInspector.HeapSnapshotConstructorsDataGrid.Request(minNodeId,maxNodeId)}
this._populateChildren(request);},__proto__:WebInspector.HeapSnapshotViewportDataGrid.prototype}
WebInspector.HeapSnapshotDiffDataGrid=function()
{var columns=[{id:"object",title:WebInspector.UIString("Constructor"),disclosure:true,sortable:true},{id:"addedCount",title:WebInspector.UIString("# New"),width:"72px",sortable:true},{id:"removedCount",title:WebInspector.UIString("# Deleted"),width:"72px",sortable:true},{id:"countDelta",title:"# Delta",width:"64px",sortable:true},{id:"addedSize",title:WebInspector.UIString("Alloc. Size"),width:"72px",sortable:true,sort:WebInspector.DataGrid.Order.Descending},{id:"removedSize",title:WebInspector.UIString("Freed Size"),width:"72px",sortable:true},{id:"sizeDelta",title:"Size Delta",width:"72px",sortable:true}];WebInspector.HeapSnapshotViewportDataGrid.call(this,columns);}
WebInspector.HeapSnapshotDiffDataGrid.prototype={defaultPopulateCount:function()
{return 50;},_sortFields:function(sortColumn,sortAscending)
{return{object:["_name",sortAscending,"_count",false],addedCount:["_addedCount",sortAscending,"_name",true],removedCount:["_removedCount",sortAscending,"_name",true],countDelta:["_countDelta",sortAscending,"_name",true],addedSize:["_addedSize",sortAscending,"_name",true],removedSize:["_removedSize",sortAscending,"_name",true],sizeDelta:["_sizeDelta",sortAscending,"_name",true]}[sortColumn];},setDataSource:function(snapshot)
{this.snapshot=snapshot;},setBaseDataSource:function(baseSnapshot)
{this.baseSnapshot=baseSnapshot;this.dispose();this.removeTopLevelNodes();this.resetSortingCache();if(this.baseSnapshot===this.snapshot){this.dispatchEventToListeners("sorting complete");return;}
this._populateChildren();},_populateChildren:function()
{function aggregatesForDiffReceived(aggregatesForDiff)
{this.snapshot.calculateSnapshotDiff(this.baseSnapshot.uid,aggregatesForDiff,didCalculateSnapshotDiff.bind(this));function didCalculateSnapshotDiff(diffByClassName)
{for(var className in diffByClassName){var diff=diffByClassName[className];this.appendTopLevelNode(new WebInspector.HeapSnapshotDiffNode(this,className,diff));}
this.sortingChanged();}}
this.baseSnapshot.aggregatesForDiff(aggregatesForDiffReceived.bind(this));},__proto__:WebInspector.HeapSnapshotViewportDataGrid.prototype}
WebInspector.HeapSnapshotDominatorsDataGrid=function()
{var columns=[{id:"object",title:WebInspector.UIString("Object"),disclosure:true,sortable:true},{id:"shallowSize",title:WebInspector.UIString("Shallow Size"),width:"120px",sortable:true},{id:"retainedSize",title:WebInspector.UIString("Retained Size"),width:"120px",sort:WebInspector.DataGrid.Order.Descending,sortable:true}];WebInspector.HeapSnapshotSortableDataGrid.call(this,columns);this._objectIdToSelect=null;}
WebInspector.HeapSnapshotDominatorsDataGrid.prototype={defaultPopulateCount:function()
{return 25;},setDataSource:function(snapshot)
{this.snapshot=snapshot;var fakeNode={nodeIndex:this.snapshot.rootNodeIndex};this.setRootNode(new WebInspector.HeapSnapshotDominatorObjectNode(this,fakeNode));this.rootNode().sort();if(this._objectIdToSelect){this.highlightObjectByHeapSnapshotId(this._objectIdToSelect);this._objectIdToSelect=null;}},sortingChanged:function()
{this.rootNode().sort();},highlightObjectByHeapSnapshotId:function(id)
{if(!this.snapshot){this._objectIdToSelect=id;return;}
function didGetDominators(dominatorIds)
{if(!dominatorIds){WebInspector.log(WebInspector.UIString("Cannot find corresponding heap snapshot node"));return;}
var dominatorNode=this.rootNode();expandNextDominator.call(this,dominatorIds,dominatorNode);}
function expandNextDominator(dominatorIds,dominatorNode)
{if(!dominatorNode){console.error("Cannot find dominator node");return;}
if(!dominatorIds.length){this.highlightNode(dominatorNode);dominatorNode.element.scrollIntoViewIfNeeded(true);return;}
var snapshotObjectId=dominatorIds.pop();dominatorNode.retrieveChildBySnapshotObjectId(snapshotObjectId,expandNextDominator.bind(this,dominatorIds));}
this.snapshot.dominatorIdsForNode(parseInt(id,10),didGetDominators.bind(this));},__proto__:WebInspector.HeapSnapshotSortableDataGrid.prototype}
WebInspector.AllocationDataGrid=function()
{var columns=[{id:"count",title:WebInspector.UIString("Count"),width:"72px",sortable:true,sort:WebInspector.DataGrid.Order.Descending},{id:"size",title:WebInspector.UIString("Size"),width:"72px",sortable:true,sort:WebInspector.DataGrid.Order.Descending},{id:"name",title:WebInspector.UIString("Function"),disclosure:true,sortable:true},];WebInspector.DataGrid.call(this,columns);this._linkifier=new WebInspector.Linkifier();}
WebInspector.AllocationDataGrid.prototype={setDataSource:function(snapshot)
{this._snapshot=snapshot;this._snapshot.allocationTracesTops(didReceiveAllocationTracesTops.bind(this));function didReceiveAllocationTracesTops(tops)
{var root=this.rootNode();for(var i=0;i<tops.length;i++)
root.appendChild(new WebInspector.AllocationGridNode(this,tops[i]));}},__proto__:WebInspector.DataGrid.prototype}
WebInspector.AllocationGridNode=function(dataGrid,data)
{WebInspector.DataGridNode.call(this,data,data.hasChildren);this._dataGrid=dataGrid;this._populated=false;}
WebInspector.AllocationGridNode.prototype={populate:function()
{if(this._populated)
return;this._populated=true;this._dataGrid._snapshot.allocationNodeCallers(this.data.id,didReceiveCallers.bind(this));function didReceiveCallers(callers)
{var callersChain=callers.nodesWithSingleCaller;var parentNode=this;for(var i=0;i<callersChain.length;i++){var child=new WebInspector.AllocationGridNode(this._dataGrid,callersChain[i]);parentNode.appendChild(child);parentNode=child;parentNode._populated=true;if(this.expanded)
parentNode.expand();}
var callersBranch=callers.branchingCallers;for(var i=0;i<callersBranch.length;i++)
parentNode.appendChild(new WebInspector.AllocationGridNode(this._dataGrid,callersBranch[i]));}},expand:function()
{WebInspector.DataGridNode.prototype.expand.call(this);if(this.children.length===1)
this.children[0].expand();},createCell:function(columnIdentifier)
{var cell=WebInspector.DataGridNode.prototype.createCell.call(this,columnIdentifier);if(columnIdentifier!=="name")
return cell;var functionInfo=this.data;if(functionInfo.scriptName){var urlElement=this._dataGrid._linkifier.linkifyLocation(functionInfo.scriptName,functionInfo.line-1,functionInfo.column-1,"profile-node-file");urlElement.style.maxWidth="75%";cell.insertBefore(urlElement,cell.firstChild);}
return cell;},__proto__:WebInspector.DataGridNode.prototype};WebInspector.HeapSnapshotGridNode=function(tree,hasChildren)
{WebInspector.DataGridNode.call(this,null,hasChildren);this._dataGrid=tree;this._instanceCount=0;this._savedChildren=null;this._retrievedChildrenRanges=[];}
WebInspector.HeapSnapshotGridNode.Events={PopulateComplete:"PopulateComplete"}
WebInspector.HeapSnapshotGridNode.prototype={createProvider:function()
{throw new Error("Needs implemented.");},_provider:function()
{if(!this._providerObject)
this._providerObject=this.createProvider();return this._providerObject;},createCell:function(columnIdentifier)
{var cell=WebInspector.DataGridNode.prototype.createCell.call(this,columnIdentifier);if(this._searchMatched)
cell.addStyleClass("highlight");return cell;},collapse:function()
{WebInspector.DataGridNode.prototype.collapse.call(this);this._dataGrid.updateVisibleNodes();},dispose:function()
{if(this._provider())
this._provider().dispose();for(var node=this.children[0];node;node=node.traverseNextNode(true,this,true))
if(node.dispose)
node.dispose();},_reachableFromWindow:false,queryObjectContent:function(callback)
{},wasDetached:function()
{this._dataGrid.nodeWasDetached(this);},_toPercentString:function(num)
{return num.toFixed(0)+"\u2009%";},childForPosition:function(nodePosition)
{var indexOfFirsChildInRange=0;for(var i=0;i<this._retrievedChildrenRanges.length;i++){var range=this._retrievedChildrenRanges[i];if(range.from<=nodePosition&&nodePosition<range.to){var childIndex=indexOfFirsChildInRange+nodePosition-range.from;return this.children[childIndex];}
indexOfFirsChildInRange+=range.to-range.from+1;}
return null;},_createValueCell:function(columnIdentifier)
{var cell=document.createElement("td");cell.className=columnIdentifier+"-column";if(this.dataGrid.snapshot.totalSize!==0){var div=document.createElement("div");var valueSpan=document.createElement("span");valueSpan.textContent=this.data[columnIdentifier];div.appendChild(valueSpan);var percentColumn=columnIdentifier+"-percent";if(percentColumn in this.data){var percentSpan=document.createElement("span");percentSpan.className="percent-column";percentSpan.textContent=this.data[percentColumn];div.appendChild(percentSpan);div.addStyleClass("heap-snapshot-multiple-values");}
cell.appendChild(div);}
return cell;},populate:function(event)
{if(this._populated)
return;this._populated=true;function sorted()
{this._populateChildren();}
this._provider().sortAndRewind(this.comparator(),sorted.bind(this));},expandWithoutPopulate:function(callback)
{this._populated=true;this.expand();this._provider().sortAndRewind(this.comparator(),callback);},_populateChildren:function(fromPosition,toPosition,afterPopulate)
{fromPosition=fromPosition||0;toPosition=toPosition||fromPosition+this._dataGrid.defaultPopulateCount();var firstNotSerializedPosition=fromPosition;function serializeNextChunk()
{if(firstNotSerializedPosition>=toPosition)
return;var end=Math.min(firstNotSerializedPosition+this._dataGrid.defaultPopulateCount(),toPosition);this._provider().serializeItemsRange(firstNotSerializedPosition,end,childrenRetrieved.bind(this));firstNotSerializedPosition=end;}
function insertRetrievedChild(item,insertionIndex)
{if(this._savedChildren){var hash=this._childHashForEntity(item);if(hash in this._savedChildren){this.insertChild(this._savedChildren[hash],insertionIndex);return;}}
this.insertChild(this._createChildNode(item),insertionIndex);}
function insertShowMoreButton(from,to,insertionIndex)
{var button=new WebInspector.ShowMoreDataGridNode(this._populateChildren.bind(this),from,to,this._dataGrid.defaultPopulateCount());this.insertChild(button,insertionIndex);}
function childrenRetrieved(items)
{var itemIndex=0;var itemPosition=items.startPosition;var insertionIndex=0;if(!this._retrievedChildrenRanges.length){if(items.startPosition>0){this._retrievedChildrenRanges.push({from:0,to:0});insertShowMoreButton.call(this,0,items.startPosition,insertionIndex++);}
this._retrievedChildrenRanges.push({from:items.startPosition,to:items.endPosition});for(var i=0,l=items.length;i<l;++i)
insertRetrievedChild.call(this,items[i],insertionIndex++);if(items.endPosition<items.totalLength)
insertShowMoreButton.call(this,items.endPosition,items.totalLength,insertionIndex++);}else{var rangeIndex=0;var found=false;var range;while(rangeIndex<this._retrievedChildrenRanges.length){range=this._retrievedChildrenRanges[rangeIndex];if(range.to>=itemPosition){found=true;break;}
insertionIndex+=range.to-range.from;if(range.to<items.totalLength)
insertionIndex+=1;++rangeIndex;}
if(!found||items.startPosition<range.from){this.children[insertionIndex-1].setEndPosition(items.startPosition);insertShowMoreButton.call(this,items.startPosition,found?range.from:items.totalLength,insertionIndex);range={from:items.startPosition,to:items.startPosition};if(!found)
rangeIndex=this._retrievedChildrenRanges.length;this._retrievedChildrenRanges.splice(rangeIndex,0,range);}else{insertionIndex+=itemPosition-range.from;}
while(range.to<items.endPosition){var skipCount=range.to-itemPosition;insertionIndex+=skipCount;itemIndex+=skipCount;itemPosition=range.to;var nextRange=this._retrievedChildrenRanges[rangeIndex+1];var newEndOfRange=nextRange?nextRange.from:items.totalLength;if(newEndOfRange>items.endPosition)
newEndOfRange=items.endPosition;while(itemPosition<newEndOfRange){insertRetrievedChild.call(this,items[itemIndex++],insertionIndex++);++itemPosition;}
if(nextRange&&newEndOfRange===nextRange.from){range.to=nextRange.to;this.removeChild(this.children[insertionIndex]);this._retrievedChildrenRanges.splice(rangeIndex+1,1);}else{range.to=newEndOfRange;if(newEndOfRange===items.totalLength)
this.removeChild(this.children[insertionIndex]);else
this.children[insertionIndex].setStartPosition(items.endPosition);}}}
this._instanceCount+=items.length;if(firstNotSerializedPosition<toPosition){serializeNextChunk.call(this);return;}
if(afterPopulate)
afterPopulate();this.dispatchEventToListeners(WebInspector.HeapSnapshotGridNode.Events.PopulateComplete);}
serializeNextChunk.call(this);},_saveChildren:function()
{this._savedChildren=null;for(var i=0,childrenCount=this.children.length;i<childrenCount;++i){var child=this.children[i];if(!child.expanded)
continue;if(!this._savedChildren)
this._savedChildren={};this._savedChildren[this._childHashForNode(child)]=child;}},sort:function()
{this._dataGrid.recursiveSortingEnter();function afterSort()
{this._saveChildren();this.removeChildren();this._retrievedChildrenRanges=[];function afterPopulate()
{for(var i=0,l=this.children.length;i<l;++i){var child=this.children[i];if(child.expanded)
child.sort();}
this._dataGrid.recursiveSortingLeave();}
var instanceCount=this._instanceCount;this._instanceCount=0;this._populateChildren(0,instanceCount,afterPopulate.bind(this));}
this._provider().sortAndRewind(this.comparator(),afterSort.bind(this));},__proto__:WebInspector.DataGridNode.prototype}
WebInspector.HeapSnapshotGenericObjectNode=function(tree,node)
{this.snapshotNodeIndex=0;WebInspector.HeapSnapshotGridNode.call(this,tree,false);if(!node)
return;this._name=node.name;this._displayName=node.displayName;this._type=node.type;this._distance=node.distance;this._shallowSize=node.selfSize;this._retainedSize=node.retainedSize;this.snapshotNodeId=node.id;this.snapshotNodeIndex=node.nodeIndex;if(this._type==="string")
this._reachableFromWindow=true;else if(this._type==="object"&&this._name.startsWith("Window")){this._name=this.shortenWindowURL(this._name,false);this._reachableFromWindow=true;}else if(node.canBeQueried)
this._reachableFromWindow=true;if(node.detachedDOMTreeNode)
this.detachedDOMTreeNode=true;};WebInspector.HeapSnapshotGenericObjectNode.prototype={createCell:function(columnIdentifier)
{var cell=columnIdentifier!=="object"?this._createValueCell(columnIdentifier):this._createObjectCell();if(this._searchMatched)
cell.addStyleClass("highlight");return cell;},_createObjectCell:function()
{var cell=document.createElement("td");cell.className="object-column";var div=document.createElement("div");div.className="source-code event-properties";div.style.overflow="visible";var data=this.data["object"];if(this._prefixObjectCell)
this._prefixObjectCell(div,data);var valueSpan=document.createElement("span");valueSpan.className="value console-formatted-"+data.valueStyle;valueSpan.textContent=data.value;div.appendChild(valueSpan);if(this.data.displayName){var nameSpan=document.createElement("span");nameSpan.className="name console-formatted-name";nameSpan.textContent=" "+this.data.displayName;div.appendChild(nameSpan);}
var idSpan=document.createElement("span");idSpan.className="console-formatted-id";idSpan.textContent=" @"+data["nodeId"];div.appendChild(idSpan);if(this._postfixObjectCell)
this._postfixObjectCell(div,data);cell.appendChild(div);cell.addStyleClass("disclosure");if(this.depth)
cell.style.setProperty("padding-left",(this.depth*this.dataGrid.indentWidth)+"px");cell.heapSnapshotNode=this;return cell;},get data()
{var data=this._emptyData();var value=this._name;var valueStyle="object";switch(this._type){case"concatenated string":case"string":value="\""+value+"\"";valueStyle="string";break;case"regexp":value="/"+value+"/";valueStyle="string";break;case"closure":value="function"+(value?" ":"")+value+"()";valueStyle="function";break;case"number":valueStyle="number";break;case"hidden":valueStyle="null";break;case"array":if(!value)
value="[]";else
value+="[]";break;};if(this._reachableFromWindow)
valueStyle+=" highlight";if(value==="Object")
value="";if(this.detachedDOMTreeNode)
valueStyle+=" detached-dom-tree-node";data["object"]={valueStyle:valueStyle,value:value,nodeId:this.snapshotNodeId};data["displayName"]=this._displayName;data["distance"]=this._distance;data["shallowSize"]=Number.withThousandsSeparator(this._shallowSize);data["retainedSize"]=Number.withThousandsSeparator(this._retainedSize);data["shallowSize-percent"]=this._toPercentString(this._shallowSizePercent);data["retainedSize-percent"]=this._toPercentString(this._retainedSizePercent);return this._enhanceData?this._enhanceData(data):data;},queryObjectContent:function(callback,objectGroupName)
{if(this._type==="string")
callback(WebInspector.RemoteObject.fromPrimitiveValue(this._name));else{function formatResult(error,object)
{if(!error&&object.type)
callback(WebInspector.RemoteObject.fromPayload(object),!!error);else
callback(WebInspector.RemoteObject.fromPrimitiveValue(WebInspector.UIString("Not available")));}
HeapProfilerAgent.getObjectByHeapObjectId(String(this.snapshotNodeId),objectGroupName,formatResult);}},get _retainedSizePercent()
{return this._retainedSize/this.dataGrid.snapshot.totalSize*100.0;},get _shallowSizePercent()
{return this._shallowSize/this.dataGrid.snapshot.totalSize*100.0;},updateHasChildren:function()
{function isEmptyCallback(isEmpty)
{this.hasChildren=!isEmpty;}
this._provider().isEmpty(isEmptyCallback.bind(this));},shortenWindowURL:function(fullName,hasObjectId)
{var startPos=fullName.indexOf("/");var endPos=hasObjectId?fullName.indexOf("@"):fullName.length;if(startPos!==-1&&endPos!==-1){var fullURL=fullName.substring(startPos+1,endPos).trimLeft();var url=fullURL.trimURL();if(url.length>40)
url=url.trimMiddle(40);return fullName.substr(0,startPos+2)+url+fullName.substr(endPos);}else
return fullName;},__proto__:WebInspector.HeapSnapshotGridNode.prototype}
WebInspector.HeapSnapshotObjectNode=function(tree,isFromBaseSnapshot,edge,parentGridNode)
{WebInspector.HeapSnapshotGenericObjectNode.call(this,tree,edge.node);this._referenceName=edge.name;this._referenceType=edge.type;this._distance=edge.distance;this.showRetainingEdges=tree.showRetainingEdges;this._isFromBaseSnapshot=isFromBaseSnapshot;this._parentGridNode=parentGridNode;this._cycledWithAncestorGridNode=this._findAncestorWithSameSnapshotNodeId();if(!this._cycledWithAncestorGridNode)
this.updateHasChildren();}
WebInspector.HeapSnapshotObjectNode.prototype={createProvider:function()
{var tree=this._dataGrid;var showHiddenData=WebInspector.settings.showAdvancedHeapSnapshotProperties.get();var snapshot=this._isFromBaseSnapshot?tree.baseSnapshot:tree.snapshot;if(this.showRetainingEdges)
return snapshot.createRetainingEdgesProvider(this.snapshotNodeIndex,showHiddenData);else
return snapshot.createEdgesProvider(this.snapshotNodeIndex,showHiddenData);},_findAncestorWithSameSnapshotNodeId:function()
{var ancestor=this._parentGridNode;while(ancestor){if(ancestor.snapshotNodeId===this.snapshotNodeId)
return ancestor;ancestor=ancestor._parentGridNode;}
return null;},_createChildNode:function(item)
{return new WebInspector.HeapSnapshotObjectNode(this._dataGrid,this._isFromBaseSnapshot,item,this);},_childHashForEntity:function(edge)
{var prefix=this.showRetainingEdges?edge.node.id+"#":"";return prefix+edge.type+"#"+edge.name;},_childHashForNode:function(childNode)
{var prefix=this.showRetainingEdges?childNode.snapshotNodeId+"#":"";return prefix+childNode._referenceType+"#"+childNode._referenceName;},comparator:function()
{var sortAscending=this._dataGrid.isSortOrderAscending();var sortColumnIdentifier=this._dataGrid.sortColumnIdentifier();var sortFields={object:["!edgeName",sortAscending,"retainedSize",false],count:["!edgeName",true,"retainedSize",false],shallowSize:["selfSize",sortAscending,"!edgeName",true],retainedSize:["retainedSize",sortAscending,"!edgeName",true],distance:["distance",sortAscending,"_name",true]}[sortColumnIdentifier]||["!edgeName",true,"retainedSize",false];return WebInspector.HeapSnapshotFilteredOrderedIterator.prototype.createComparator(sortFields);},_emptyData:function()
{return{count:"",addedCount:"",removedCount:"",countDelta:"",addedSize:"",removedSize:"",sizeDelta:""};},_enhanceData:function(data)
{var name=this._referenceName;if(name==="")name="(empty)";var nameClass="name";switch(this._referenceType){case"context":nameClass="console-formatted-number";break;case"internal":case"hidden":nameClass="console-formatted-null";break;case"element":name="["+name+"]";break;}
data["object"].nameClass=nameClass;data["object"].name=name;data["distance"]=this._distance;return data;},_prefixObjectCell:function(div,data)
{if(this._cycledWithAncestorGridNode)
div.className+=" cycled-ancessor-node";var nameSpan=document.createElement("span");nameSpan.className=data.nameClass;nameSpan.textContent=data.name;div.appendChild(nameSpan);var separatorSpan=document.createElement("span");separatorSpan.className="grayed";separatorSpan.textContent=this.showRetainingEdges?" in ":" :: ";div.appendChild(separatorSpan);},__proto__:WebInspector.HeapSnapshotGenericObjectNode.prototype}
WebInspector.HeapSnapshotInstanceNode=function(tree,baseSnapshot,snapshot,node)
{WebInspector.HeapSnapshotGenericObjectNode.call(this,tree,node);this._baseSnapshotOrSnapshot=baseSnapshot||snapshot;this._isDeletedNode=!!baseSnapshot;this.updateHasChildren();};WebInspector.HeapSnapshotInstanceNode.prototype={createProvider:function()
{var showHiddenData=WebInspector.settings.showAdvancedHeapSnapshotProperties.get();return this._baseSnapshotOrSnapshot.createEdgesProvider(this.snapshotNodeIndex,showHiddenData);},_createChildNode:function(item)
{return new WebInspector.HeapSnapshotObjectNode(this._dataGrid,this._isDeletedNode,item,null);},_childHashForEntity:function(edge)
{return edge.type+"#"+edge.name;},_childHashForNode:function(childNode)
{return childNode._referenceType+"#"+childNode._referenceName;},comparator:function()
{var sortAscending=this._dataGrid.isSortOrderAscending();var sortColumnIdentifier=this._dataGrid.sortColumnIdentifier();var sortFields={object:["!edgeName",sortAscending,"retainedSize",false],distance:["distance",sortAscending,"retainedSize",false],count:["!edgeName",true,"retainedSize",false],addedSize:["selfSize",sortAscending,"!edgeName",true],removedSize:["selfSize",sortAscending,"!edgeName",true],shallowSize:["selfSize",sortAscending,"!edgeName",true],retainedSize:["retainedSize",sortAscending,"!edgeName",true]}[sortColumnIdentifier]||["!edgeName",true,"retainedSize",false];return WebInspector.HeapSnapshotFilteredOrderedIterator.prototype.createComparator(sortFields);},_emptyData:function()
{return{count:"",countDelta:"",sizeDelta:""};},_enhanceData:function(data)
{if(this._isDeletedNode){data["addedCount"]="";data["addedSize"]="";data["removedCount"]="\u2022";data["removedSize"]=Number.withThousandsSeparator(this._shallowSize);}else{data["addedCount"]="\u2022";data["addedSize"]=Number.withThousandsSeparator(this._shallowSize);data["removedCount"]="";data["removedSize"]="";}
return data;},get isDeletedNode()
{return this._isDeletedNode;},__proto__:WebInspector.HeapSnapshotGenericObjectNode.prototype}
WebInspector.HeapSnapshotConstructorNode=function(tree,className,aggregate,aggregatesKey)
{WebInspector.HeapSnapshotGridNode.call(this,tree,aggregate.count>0);this._name=className;this._aggregatesKey=aggregatesKey;this._distance=aggregate.distance;this._count=aggregate.count;this._shallowSize=aggregate.self;this._retainedSize=aggregate.maxRet;}
WebInspector.HeapSnapshotConstructorNode.prototype={createProvider:function()
{return this._dataGrid.snapshot.createNodesProviderForClass(this._name,this._aggregatesKey)},revealNodeBySnapshotObjectId:function(snapshotObjectId)
{function didExpand()
{this._provider().nodePosition(snapshotObjectId,didGetNodePosition.bind(this));}
function didGetNodePosition(nodePosition)
{if(nodePosition===-1)
this.collapse();else
this._populateChildren(nodePosition,null,didPopulateChildren.bind(this,nodePosition));}
function didPopulateChildren(nodePosition)
{var indexOfFirsChildInRange=0;for(var i=0;i<this._retrievedChildrenRanges.length;i++){var range=this._retrievedChildrenRanges[i];if(range.from<=nodePosition&&nodePosition<range.to){var childIndex=indexOfFirsChildInRange+nodePosition-range.from;var instanceNode=this.children[childIndex];this._dataGrid.highlightNode(instanceNode);return;}
indexOfFirsChildInRange+=range.to-range.from+1;}}
this.expandWithoutPopulate(didExpand.bind(this));},createCell:function(columnIdentifier)
{var cell=columnIdentifier!=="object"?this._createValueCell(columnIdentifier):WebInspector.HeapSnapshotGridNode.prototype.createCell.call(this,columnIdentifier);if(this._searchMatched)
cell.addStyleClass("highlight");return cell;},_createChildNode:function(item)
{return new WebInspector.HeapSnapshotInstanceNode(this._dataGrid,null,this._dataGrid.snapshot,item);},comparator:function()
{var sortAscending=this._dataGrid.isSortOrderAscending();var sortColumnIdentifier=this._dataGrid.sortColumnIdentifier();var sortFields={object:["id",sortAscending,"retainedSize",false],distance:["distance",sortAscending,"retainedSize",false],count:["id",true,"retainedSize",false],shallowSize:["selfSize",sortAscending,"id",true],retainedSize:["retainedSize",sortAscending,"id",true]}[sortColumnIdentifier];return WebInspector.HeapSnapshotFilteredOrderedIterator.prototype.createComparator(sortFields);},_childHashForEntity:function(node)
{return node.id;},_childHashForNode:function(childNode)
{return childNode.snapshotNodeId;},get data()
{var data={object:this._name};data["count"]=Number.withThousandsSeparator(this._count);data["distance"]=this._distance;data["shallowSize"]=Number.withThousandsSeparator(this._shallowSize);data["retainedSize"]=Number.withThousandsSeparator(this._retainedSize);data["count-percent"]=this._toPercentString(this._countPercent);data["shallowSize-percent"]=this._toPercentString(this._shallowSizePercent);data["retainedSize-percent"]=this._toPercentString(this._retainedSizePercent);return data;},get _countPercent()
{return this._count/this.dataGrid.snapshot.nodeCount*100.0;},get _retainedSizePercent()
{return this._retainedSize/this.dataGrid.snapshot.totalSize*100.0;},get _shallowSizePercent()
{return this._shallowSize/this.dataGrid.snapshot.totalSize*100.0;},__proto__:WebInspector.HeapSnapshotGridNode.prototype}
WebInspector.HeapSnapshotDiffNodesProvider=function(addedNodesProvider,deletedNodesProvider,addedCount,removedCount)
{this._addedNodesProvider=addedNodesProvider;this._deletedNodesProvider=deletedNodesProvider;this._addedCount=addedCount;this._removedCount=removedCount;}
WebInspector.HeapSnapshotDiffNodesProvider.prototype={dispose:function()
{this._addedNodesProvider.dispose();this._deletedNodesProvider.dispose();},isEmpty:function(callback)
{callback(false);},serializeItemsRange:function(beginPosition,endPosition,callback)
{function didReceiveAllItems(items)
{items.totalLength=this._addedCount+this._removedCount;callback(items);}
function didReceiveDeletedItems(addedItems,items)
{if(!addedItems.length)
addedItems.startPosition=this._addedCount+items.startPosition;for(var i=0;i<items.length;i++){items[i].isAddedNotRemoved=false;addedItems.push(items[i]);}
addedItems.endPosition=this._addedCount+items.endPosition;didReceiveAllItems.call(this,addedItems);}
function didReceiveAddedItems(items)
{for(var i=0;i<items.length;i++)
items[i].isAddedNotRemoved=true;if(items.endPosition<endPosition)
return this._deletedNodesProvider.serializeItemsRange(0,endPosition-items.endPosition,didReceiveDeletedItems.bind(this,items));items.totalLength=this._addedCount+this._removedCount;didReceiveAllItems.call(this,items);}
if(beginPosition<this._addedCount)
this._addedNodesProvider.serializeItemsRange(beginPosition,endPosition,didReceiveAddedItems.bind(this));else
this._deletedNodesProvider.serializeItemsRange(beginPosition-this._addedCount,endPosition-this._addedCount,didReceiveDeletedItems.bind(this,[]));},sortAndRewind:function(comparator,callback)
{function afterSort()
{this._deletedNodesProvider.sortAndRewind(comparator,callback);}
this._addedNodesProvider.sortAndRewind(comparator,afterSort.bind(this));}};WebInspector.HeapSnapshotDiffNode=function(tree,className,diffForClass)
{WebInspector.HeapSnapshotGridNode.call(this,tree,true);this._name=className;this._addedCount=diffForClass.addedCount;this._removedCount=diffForClass.removedCount;this._countDelta=diffForClass.countDelta;this._addedSize=diffForClass.addedSize;this._removedSize=diffForClass.removedSize;this._sizeDelta=diffForClass.sizeDelta;this._deletedIndexes=diffForClass.deletedIndexes;}
WebInspector.HeapSnapshotDiffNode.prototype={createProvider:function()
{var tree=this._dataGrid;return new WebInspector.HeapSnapshotDiffNodesProvider(tree.snapshot.createAddedNodesProvider(tree.baseSnapshot.uid,this._name),tree.baseSnapshot.createDeletedNodesProvider(this._deletedIndexes),this._addedCount,this._removedCount);},_createChildNode:function(item)
{if(item.isAddedNotRemoved)
return new WebInspector.HeapSnapshotInstanceNode(this._dataGrid,null,this._dataGrid.snapshot,item);else
return new WebInspector.HeapSnapshotInstanceNode(this._dataGrid,this._dataGrid.baseSnapshot,null,item);},_childHashForEntity:function(node)
{return node.id;},_childHashForNode:function(childNode)
{return childNode.snapshotNodeId;},comparator:function()
{var sortAscending=this._dataGrid.isSortOrderAscending();var sortColumnIdentifier=this._dataGrid.sortColumnIdentifier();var sortFields={object:["id",sortAscending,"selfSize",false],addedCount:["selfSize",sortAscending,"id",true],removedCount:["selfSize",sortAscending,"id",true],countDelta:["selfSize",sortAscending,"id",true],addedSize:["selfSize",sortAscending,"id",true],removedSize:["selfSize",sortAscending,"id",true],sizeDelta:["selfSize",sortAscending,"id",true]}[sortColumnIdentifier];return WebInspector.HeapSnapshotFilteredOrderedIterator.prototype.createComparator(sortFields);},_signForDelta:function(delta)
{if(delta===0)
return"";if(delta>0)
return"+";else
return"\u2212";},get data()
{var data={object:this._name};data["addedCount"]=Number.withThousandsSeparator(this._addedCount);data["removedCount"]=Number.withThousandsSeparator(this._removedCount);data["countDelta"]=this._signForDelta(this._countDelta)+Number.withThousandsSeparator(Math.abs(this._countDelta));data["addedSize"]=Number.withThousandsSeparator(this._addedSize);data["removedSize"]=Number.withThousandsSeparator(this._removedSize);data["sizeDelta"]=this._signForDelta(this._sizeDelta)+Number.withThousandsSeparator(Math.abs(this._sizeDelta));return data;},__proto__:WebInspector.HeapSnapshotGridNode.prototype}
WebInspector.HeapSnapshotDominatorObjectNode=function(tree,node)
{WebInspector.HeapSnapshotGenericObjectNode.call(this,tree,node);this.updateHasChildren();};WebInspector.HeapSnapshotDominatorObjectNode.prototype={createProvider:function()
{return this._dataGrid.snapshot.createNodesProviderForDominator(this.snapshotNodeIndex);},retrieveChildBySnapshotObjectId:function(snapshotObjectId,callback)
{function didExpand()
{this._provider().nodePosition(snapshotObjectId,didGetNodePosition.bind(this));}
function didGetNodePosition(nodePosition)
{if(nodePosition===-1){this.collapse();callback(null);}else
this._populateChildren(nodePosition,null,didPopulateChildren.bind(this,nodePosition));}
function didPopulateChildren(nodePosition)
{var child=this.childForPosition(nodePosition);callback(child);}
this.hasChildren=true;this.expandWithoutPopulate(didExpand.bind(this));},_createChildNode:function(item)
{return new WebInspector.HeapSnapshotDominatorObjectNode(this._dataGrid,item);},_childHashForEntity:function(node)
{return node.id;},_childHashForNode:function(childNode)
{return childNode.snapshotNodeId;},comparator:function()
{var sortAscending=this._dataGrid.isSortOrderAscending();var sortColumnIdentifier=this._dataGrid.sortColumnIdentifier();var sortFields={object:["id",sortAscending,"retainedSize",false],shallowSize:["selfSize",sortAscending,"id",true],retainedSize:["retainedSize",sortAscending,"id",true]}[sortColumnIdentifier];return WebInspector.HeapSnapshotFilteredOrderedIterator.prototype.createComparator(sortFields);},_emptyData:function()
{return{};},__proto__:WebInspector.HeapSnapshotGenericObjectNode.prototype};WebInspector.HeapSnapshotLoader=function(dispatcher)
{this._reset();this._progress=new WebInspector.HeapSnapshotProgress(dispatcher);}
WebInspector.HeapSnapshotLoader.prototype={dispose:function()
{this._reset();},_reset:function()
{this._json="";this._state="find-snapshot-info";this._snapshot={};},close:function()
{if(this._json)
this._parseStringsArray();},buildSnapshot:function(constructorName)
{this._progress.updateStatus("Processing snapshot\u2026");var constructor=WebInspector[constructorName];var result=new constructor(this._snapshot,this._progress);this._reset();return result;},_parseUintArray:function()
{var index=0;var char0="0".charCodeAt(0),char9="9".charCodeAt(0),closingBracket="]".charCodeAt(0);var length=this._json.length;while(true){while(index<length){var code=this._json.charCodeAt(index);if(char0<=code&&code<=char9)
break;else if(code===closingBracket){this._json=this._json.slice(index+1);return false;}
++index;}
if(index===length){this._json="";return true;}
var nextNumber=0;var startIndex=index;while(index<length){var code=this._json.charCodeAt(index);if(char0>code||code>char9)
break;nextNumber*=10;nextNumber+=(code-char0);++index;}
if(index===length){this._json=this._json.slice(startIndex);return true;}
this._array[this._arrayIndex++]=nextNumber;}},_parseStringsArray:function()
{this._progress.updateStatus("Parsing strings\u2026");var closingBracketIndex=this._json.lastIndexOf("]");if(closingBracketIndex===-1)
throw new Error("Incomplete JSON");this._json=this._json.slice(0,closingBracketIndex+1);this._snapshot.strings=JSON.parse(this._json);},write:function(chunk)
{this._json+=chunk;while(true){switch(this._state){case"find-snapshot-info":{var snapshotToken="\"snapshot\"";var snapshotTokenIndex=this._json.indexOf(snapshotToken);if(snapshotTokenIndex===-1)
throw new Error("Snapshot token not found");this._json=this._json.slice(snapshotTokenIndex+snapshotToken.length+1);this._state="parse-snapshot-info";this._progress.updateStatus("Loading snapshot info\u2026");break;}
case"parse-snapshot-info":{var closingBracketIndex=WebInspector.findBalancedCurlyBrackets(this._json);if(closingBracketIndex===-1)
return;this._snapshot.snapshot=(JSON.parse(this._json.slice(0,closingBracketIndex)));this._json=this._json.slice(closingBracketIndex);this._state="find-nodes";break;}
case"find-nodes":{var nodesToken="\"nodes\"";var nodesTokenIndex=this._json.indexOf(nodesToken);if(nodesTokenIndex===-1)
return;var bracketIndex=this._json.indexOf("[",nodesTokenIndex);if(bracketIndex===-1)
return;this._json=this._json.slice(bracketIndex+1);var node_fields_count=this._snapshot.snapshot.meta.node_fields.length;var nodes_length=this._snapshot.snapshot.node_count*node_fields_count;this._array=new Uint32Array(nodes_length);this._arrayIndex=0;this._state="parse-nodes";break;}
case"parse-nodes":{var hasMoreData=this._parseUintArray();this._progress.updateProgress("Loading nodes\u2026 %d\%",this._arrayIndex,this._array.length);if(hasMoreData)
return;this._snapshot.nodes=this._array;this._state="find-edges";this._array=null;break;}
case"find-edges":{var edgesToken="\"edges\"";var edgesTokenIndex=this._json.indexOf(edgesToken);if(edgesTokenIndex===-1)
return;var bracketIndex=this._json.indexOf("[",edgesTokenIndex);if(bracketIndex===-1)
return;this._json=this._json.slice(bracketIndex+1);var edge_fields_count=this._snapshot.snapshot.meta.edge_fields.length;var edges_length=this._snapshot.snapshot.edge_count*edge_fields_count;this._array=new Uint32Array(edges_length);this._arrayIndex=0;this._state="parse-edges";break;}
case"parse-edges":{var hasMoreData=this._parseUintArray();this._progress.updateProgress("Loading edges\u2026 %d\%",this._arrayIndex,this._array.length);if(hasMoreData)
return;this._snapshot.edges=this._array;this._array=null;if(WebInspector.HeapSnapshot.enableAllocationProfiler)
this._state="find-trace-function-infos";else
this._state="find-strings";break;}
case"find-trace-function-infos":{var tracesToken="\"trace_function_infos\"";var tracesTokenIndex=this._json.indexOf(tracesToken);if(tracesTokenIndex===-1)
return;var bracketIndex=this._json.indexOf("[",tracesTokenIndex);if(bracketIndex===-1)
return;this._json=this._json.slice(bracketIndex+1);var trace_function_info_field_count=this._snapshot.snapshot.meta.trace_function_info_fields.length;var trace_function_info_length=this._snapshot.snapshot.trace_function_count*trace_function_info_field_count;this._array=new Uint32Array(trace_function_info_length);this._arrayIndex=0;this._state="parse-trace-function-infos";break;}
case"parse-trace-function-infos":{if(this._parseUintArray())
return;this._snapshot.trace_function_infos=this._array;this._array=null;this._state="find-trace-tree";break;}
case"find-trace-tree":{var tracesToken="\"trace_tree\"";var tracesTokenIndex=this._json.indexOf(tracesToken);if(tracesTokenIndex===-1)
return;var bracketIndex=this._json.indexOf("[",tracesTokenIndex);if(bracketIndex===-1)
return;this._json=this._json.slice(bracketIndex);this._state="parse-trace-tree";break;}
case"parse-trace-tree":{var stringsToken="\"strings\"";var stringsTokenIndex=this._json.indexOf(stringsToken);if(stringsTokenIndex===-1)
return;var bracketIndex=this._json.lastIndexOf("]",stringsTokenIndex);this._snapshot.trace_tree=JSON.parse(this._json.substring(0,bracketIndex+1));this._json=this._json.slice(bracketIndex);this._state="find-strings";this._progress.updateStatus("Loading strings\u2026");break;}
case"find-strings":{var stringsToken="\"strings\"";var stringsTokenIndex=this._json.indexOf(stringsToken);if(stringsTokenIndex===-1)
return;var bracketIndex=this._json.indexOf("[",stringsTokenIndex);if(bracketIndex===-1)
return;this._json=this._json.slice(bracketIndex);this._state="accumulate-strings";break;}
case"accumulate-strings":return;}}}};;WebInspector.HeapSnapshotWorkerWrapper=function()
{}
WebInspector.HeapSnapshotWorkerWrapper.prototype={postMessage:function(message)
{},terminate:function()
{},__proto__:WebInspector.Object.prototype}
WebInspector.HeapSnapshotRealWorker=function()
{this._worker=new Worker("HeapSnapshotWorker.js");this._worker.addEventListener("message",this._messageReceived.bind(this),false);}
WebInspector.HeapSnapshotRealWorker.prototype={_messageReceived:function(event)
{var message=event.data;this.dispatchEventToListeners("message",message);},postMessage:function(message)
{this._worker.postMessage(message);},terminate:function()
{this._worker.terminate();},__proto__:WebInspector.HeapSnapshotWorkerWrapper.prototype}
WebInspector.AsyncTaskQueue=function()
{this._queue=[];this._isTimerSheduled=false;}
WebInspector.AsyncTaskQueue.prototype={addTask:function(task)
{this._queue.push(task);this._scheduleTimer();},_onTimeout:function()
{this._isTimerSheduled=false;var queue=this._queue;this._queue=[];for(var i=0;i<queue.length;i++){try{queue[i]();}catch(e){console.error("Exception while running task: "+e.stack);}}
this._scheduleTimer();},_scheduleTimer:function()
{if(this._queue.length&&!this._isTimerSheduled){setTimeout(this._onTimeout.bind(this),0);this._isTimerSheduled=true;}}}
WebInspector.HeapSnapshotFakeWorker=function()
{this._dispatcher=new WebInspector.HeapSnapshotWorkerDispatcher(window,this._postMessageFromWorker.bind(this));this._asyncTaskQueue=new WebInspector.AsyncTaskQueue();}
WebInspector.HeapSnapshotFakeWorker.prototype={postMessage:function(message)
{function dispatch()
{if(this._dispatcher)
this._dispatcher.dispatchMessage({data:message});}
this._asyncTaskQueue.addTask(dispatch.bind(this));},terminate:function()
{this._dispatcher=null;},_postMessageFromWorker:function(message)
{function send()
{this.dispatchEventToListeners("message",message);}
this._asyncTaskQueue.addTask(send.bind(this));},__proto__:WebInspector.HeapSnapshotWorkerWrapper.prototype}
WebInspector.HeapSnapshotWorkerProxy=function(eventHandler)
{this._eventHandler=eventHandler;this._nextObjectId=1;this._nextCallId=1;this._callbacks=[];this._previousCallbacks=[];this._worker=typeof InspectorTest==="undefined"?new WebInspector.HeapSnapshotRealWorker():new WebInspector.HeapSnapshotFakeWorker();this._worker.addEventListener("message",this._messageReceived,this);}
WebInspector.HeapSnapshotWorkerProxy.prototype={createLoader:function(snapshotConstructorName,proxyConstructor)
{var objectId=this._nextObjectId++;var proxy=new WebInspector.HeapSnapshotLoaderProxy(this,objectId,snapshotConstructorName,proxyConstructor);this._postMessage({callId:this._nextCallId++,disposition:"create",objectId:objectId,methodName:"WebInspector.HeapSnapshotLoader"});return proxy;},dispose:function()
{this._worker.terminate();if(this._interval)
clearInterval(this._interval);},disposeObject:function(objectId)
{this._postMessage({callId:this._nextCallId++,disposition:"dispose",objectId:objectId});},callGetter:function(callback,objectId,getterName)
{var callId=this._nextCallId++;this._callbacks[callId]=callback;this._postMessage({callId:callId,disposition:"getter",objectId:objectId,methodName:getterName});},callFactoryMethod:function(callback,objectId,methodName,proxyConstructor)
{var callId=this._nextCallId++;var methodArguments=Array.prototype.slice.call(arguments,4);var newObjectId=this._nextObjectId++;if(callback){function wrapCallback(remoteResult)
{callback(remoteResult?new proxyConstructor(this,newObjectId):null);}
this._callbacks[callId]=wrapCallback.bind(this);this._postMessage({callId:callId,disposition:"factory",objectId:objectId,methodName:methodName,methodArguments:methodArguments,newObjectId:newObjectId});return null;}else{this._postMessage({callId:callId,disposition:"factory",objectId:objectId,methodName:methodName,methodArguments:methodArguments,newObjectId:newObjectId});return new proxyConstructor(this,newObjectId);}},callMethod:function(callback,objectId,methodName)
{var callId=this._nextCallId++;var methodArguments=Array.prototype.slice.call(arguments,3);if(callback)
this._callbacks[callId]=callback;this._postMessage({callId:callId,disposition:"method",objectId:objectId,methodName:methodName,methodArguments:methodArguments});},startCheckingForLongRunningCalls:function()
{if(this._interval)
return;this._checkLongRunningCalls();this._interval=setInterval(this._checkLongRunningCalls.bind(this),300);},_checkLongRunningCalls:function()
{for(var callId in this._previousCallbacks)
if(!(callId in this._callbacks))
delete this._previousCallbacks[callId];var hasLongRunningCalls=false;for(callId in this._previousCallbacks){hasLongRunningCalls=true;break;}
this.dispatchEventToListeners("wait",hasLongRunningCalls);for(callId in this._callbacks)
this._previousCallbacks[callId]=true;},_findFunction:function(name)
{var path=name.split(".");var result=window;for(var i=0;i<path.length;++i)
result=result[path[i]];return result;},_messageReceived:function(event)
{var data=event.data;if(data.eventName){if(this._eventHandler)
this._eventHandler(data.eventName,data.data);return;}
if(data.error){if(data.errorMethodName)
WebInspector.log(WebInspector.UIString("An error happened when a call for method '%s' was requested",data.errorMethodName));WebInspector.log(data.errorCallStack);delete this._callbacks[data.callId];return;}
if(!this._callbacks[data.callId])
return;var callback=this._callbacks[data.callId];delete this._callbacks[data.callId];callback(data.result);},_postMessage:function(message)
{this._worker.postMessage(message);},__proto__:WebInspector.Object.prototype}
WebInspector.HeapSnapshotProxyObject=function(worker,objectId)
{this._worker=worker;this._objectId=objectId;}
WebInspector.HeapSnapshotProxyObject.prototype={_callWorker:function(workerMethodName,args)
{args.splice(1,0,this._objectId);return this._worker[workerMethodName].apply(this._worker,args);},dispose:function()
{this._worker.disposeObject(this._objectId);},disposeWorker:function()
{this._worker.dispose();},callFactoryMethod:function(callback,methodName,proxyConstructor,var_args)
{return this._callWorker("callFactoryMethod",Array.prototype.slice.call(arguments,0));},callGetter:function(callback,getterName)
{return this._callWorker("callGetter",Array.prototype.slice.call(arguments,0));},callMethod:function(callback,methodName,var_args)
{return this._callWorker("callMethod",Array.prototype.slice.call(arguments,0));},get worker(){return this._worker;}};WebInspector.HeapSnapshotLoaderProxy=function(worker,objectId,snapshotConstructorName,proxyConstructor)
{WebInspector.HeapSnapshotProxyObject.call(this,worker,objectId);this._snapshotConstructorName=snapshotConstructorName;this._proxyConstructor=proxyConstructor;this._pendingSnapshotConsumers=[];}
WebInspector.HeapSnapshotLoaderProxy.prototype={addConsumer:function(callback)
{this._pendingSnapshotConsumers.push(callback);},write:function(chunk,callback)
{this.callMethod(callback,"write",chunk);},close:function(callback)
{function buildSnapshot()
{if(callback)
callback();this.callFactoryMethod(updateStaticData.bind(this),"buildSnapshot",this._proxyConstructor,this._snapshotConstructorName);}
function updateStaticData(snapshotProxy)
{this.dispose();snapshotProxy.updateStaticData(notifyPendingConsumers.bind(this));}
function notifyPendingConsumers(snapshotProxy)
{for(var i=0;i<this._pendingSnapshotConsumers.length;++i)
this._pendingSnapshotConsumers[i](snapshotProxy);this._pendingSnapshotConsumers=[];}
this.callMethod(buildSnapshot.bind(this),"close");},__proto__:WebInspector.HeapSnapshotProxyObject.prototype}
WebInspector.HeapSnapshotProxy=function(worker,objectId)
{WebInspector.HeapSnapshotProxyObject.call(this,worker,objectId);}
WebInspector.HeapSnapshotProxy.prototype={aggregates:function(sortedIndexes,key,filter,callback)
{this.callMethod(callback,"aggregates",sortedIndexes,key,filter);},aggregatesForDiff:function(callback)
{this.callMethod(callback,"aggregatesForDiff");},calculateSnapshotDiff:function(baseSnapshotId,baseSnapshotAggregates,callback)
{this.callMethod(callback,"calculateSnapshotDiff",baseSnapshotId,baseSnapshotAggregates);},nodeClassName:function(snapshotObjectId,callback)
{this.callMethod(callback,"nodeClassName",snapshotObjectId);},dominatorIdsForNode:function(nodeIndex,callback)
{this.callMethod(callback,"dominatorIdsForNode",nodeIndex);},createEdgesProvider:function(nodeIndex,showHiddenData)
{return this.callFactoryMethod(null,"createEdgesProvider",WebInspector.HeapSnapshotProviderProxy,nodeIndex,showHiddenData);},createRetainingEdgesProvider:function(nodeIndex,showHiddenData)
{return this.callFactoryMethod(null,"createRetainingEdgesProvider",WebInspector.HeapSnapshotProviderProxy,nodeIndex,showHiddenData);},createAddedNodesProvider:function(baseSnapshotId,className)
{return this.callFactoryMethod(null,"createAddedNodesProvider",WebInspector.HeapSnapshotProviderProxy,baseSnapshotId,className);},createDeletedNodesProvider:function(nodeIndexes)
{return this.callFactoryMethod(null,"createDeletedNodesProvider",WebInspector.HeapSnapshotProviderProxy,nodeIndexes);},createNodesProvider:function(filter)
{return this.callFactoryMethod(null,"createNodesProvider",WebInspector.HeapSnapshotProviderProxy,filter);},createNodesProviderForClass:function(className,aggregatesKey)
{return this.callFactoryMethod(null,"createNodesProviderForClass",WebInspector.HeapSnapshotProviderProxy,className,aggregatesKey);},createNodesProviderForDominator:function(nodeIndex)
{return this.callFactoryMethod(null,"createNodesProviderForDominator",WebInspector.HeapSnapshotProviderProxy,nodeIndex);},maxJsNodeId:function(callback)
{this.callMethod(callback,"maxJsNodeId");},allocationTracesTops:function(callback)
{this.callMethod(callback,"allocationTracesTops");},allocationNodeCallers:function(nodeId,callback)
{this.callMethod(callback,"allocationNodeCallers",nodeId);},dispose:function()
{this.disposeWorker();},get nodeCount()
{return this._staticData.nodeCount;},get rootNodeIndex()
{return this._staticData.rootNodeIndex;},updateStaticData:function(callback)
{function dataReceived(staticData)
{this._staticData=staticData;callback(this);}
this.callMethod(dataReceived.bind(this),"updateStaticData");},get totalSize()
{return this._staticData.totalSize;},get uid()
{return this._staticData.uid;},__proto__:WebInspector.HeapSnapshotProxyObject.prototype}
WebInspector.HeapSnapshotProviderProxy=function(worker,objectId)
{WebInspector.HeapSnapshotProxyObject.call(this,worker,objectId);}
WebInspector.HeapSnapshotProviderProxy.prototype={nodePosition:function(snapshotObjectId,callback)
{this.callMethod(callback,"nodePosition",snapshotObjectId);},isEmpty:function(callback)
{this.callMethod(callback,"isEmpty");},serializeItemsRange:function(startPosition,endPosition,callback)
{this.callMethod(callback,"serializeItemsRange",startPosition,endPosition);},sortAndRewind:function(comparator,callback)
{this.callMethod(callback,"sortAndRewind",comparator);},__proto__:WebInspector.HeapSnapshotProxyObject.prototype};WebInspector.HeapSnapshotView=function(parent,profile)
{WebInspector.View.call(this);this.element.addStyleClass("heap-snapshot-view");this.parent=parent;this.parent.addEventListener("profile added",this._onProfileHeaderAdded,this);if(profile._profileType.id===WebInspector.TrackingHeapSnapshotProfileType.TypeId){this._trackingOverviewGrid=new WebInspector.HeapTrackingOverviewGrid(profile);this._trackingOverviewGrid.addEventListener(WebInspector.HeapTrackingOverviewGrid.IdsRangeChanged,this._onIdsRangeChanged.bind(this));this._trackingOverviewGrid.show(this.element);}
this.viewsContainer=document.createElement("div");this.viewsContainer.addStyleClass("views-container");this.element.appendChild(this.viewsContainer);this.containmentView=new WebInspector.View();this.containmentView.element.addStyleClass("view");this.containmentDataGrid=new WebInspector.HeapSnapshotContainmentDataGrid();this.containmentDataGrid.element.addEventListener("mousedown",this._mouseDownInContentsGrid.bind(this),true);this.containmentDataGrid.show(this.containmentView.element);this.containmentDataGrid.addEventListener(WebInspector.DataGrid.Events.SelectedNode,this._selectionChanged,this);this.constructorsView=new WebInspector.View();this.constructorsView.element.addStyleClass("view");this.constructorsView.element.appendChild(this._createToolbarWithClassNameFilter());this.constructorsDataGrid=new WebInspector.HeapSnapshotConstructorsDataGrid();this.constructorsDataGrid.element.addStyleClass("class-view-grid");this.constructorsDataGrid.element.addEventListener("mousedown",this._mouseDownInContentsGrid.bind(this),true);this.constructorsDataGrid.show(this.constructorsView.element);this.constructorsDataGrid.addEventListener(WebInspector.DataGrid.Events.SelectedNode,this._selectionChanged,this);this.dataGrid=(this.constructorsDataGrid);this.currentView=this.constructorsView;this.currentView.show(this.viewsContainer);this.diffView=new WebInspector.View();this.diffView.element.addStyleClass("view");this.diffView.element.appendChild(this._createToolbarWithClassNameFilter());this.diffDataGrid=new WebInspector.HeapSnapshotDiffDataGrid();this.diffDataGrid.element.addStyleClass("class-view-grid");this.diffDataGrid.show(this.diffView.element);this.diffDataGrid.addEventListener(WebInspector.DataGrid.Events.SelectedNode,this._selectionChanged,this);this.dominatorView=new WebInspector.View();this.dominatorView.element.addStyleClass("view");this.dominatorDataGrid=new WebInspector.HeapSnapshotDominatorsDataGrid();this.dominatorDataGrid.element.addEventListener("mousedown",this._mouseDownInContentsGrid.bind(this),true);this.dominatorDataGrid.show(this.dominatorView.element);this.dominatorDataGrid.addEventListener(WebInspector.DataGrid.Events.SelectedNode,this._selectionChanged,this);if(WebInspector.HeapSnapshot.enableAllocationProfiler){this.allocationView=new WebInspector.View();this.allocationView.element.addStyleClass("view");this.allocationDataGrid=new WebInspector.AllocationDataGrid();this.allocationDataGrid.element.addEventListener("mousedown",this._mouseDownInContentsGrid.bind(this),true);this.allocationDataGrid.show(this.allocationView.element);this.allocationDataGrid.addEventListener(WebInspector.DataGrid.Events.SelectedNode,this._selectionChanged,this);}
this.retainmentViewHeader=document.createElement("div");this.retainmentViewHeader.addStyleClass("retainers-view-header");WebInspector.installDragHandle(this.retainmentViewHeader,this._startRetainersHeaderDragging.bind(this),this._retainersHeaderDragging.bind(this),this._endRetainersHeaderDragging.bind(this),"row-resize");var retainingPathsTitleDiv=document.createElement("div");retainingPathsTitleDiv.className="title";var retainingPathsTitle=document.createElement("span");retainingPathsTitle.textContent=WebInspector.UIString("Object's retaining tree");retainingPathsTitleDiv.appendChild(retainingPathsTitle);this.retainmentViewHeader.appendChild(retainingPathsTitleDiv);this.element.appendChild(this.retainmentViewHeader);this.retainmentView=new WebInspector.View();this.retainmentView.element.addStyleClass("view");this.retainmentView.element.addStyleClass("retaining-paths-view");this.retainmentDataGrid=new WebInspector.HeapSnapshotRetainmentDataGrid();this.retainmentDataGrid.show(this.retainmentView.element);this.retainmentDataGrid.addEventListener(WebInspector.DataGrid.Events.SelectedNode,this._inspectedObjectChanged,this);this.retainmentView.show(this.element);this.retainmentDataGrid.reset();this.viewSelect=new WebInspector.StatusBarComboBox(this._onSelectedViewChanged.bind(this));this.views=[{title:"Summary",view:this.constructorsView,grid:this.constructorsDataGrid},{title:"Comparison",view:this.diffView,grid:this.diffDataGrid},{title:"Containment",view:this.containmentView,grid:this.containmentDataGrid}];if(WebInspector.settings.showAdvancedHeapSnapshotProperties.get())
this.views.push({title:"Dominators",view:this.dominatorView,grid:this.dominatorDataGrid});if(WebInspector.HeapSnapshot.enableAllocationProfiler)
this.views.push({title:"Allocation",view:this.allocationView,grid:this.allocationDataGrid});this.views.current=0;for(var i=0;i<this.views.length;++i)
this.viewSelect.createOption(WebInspector.UIString(this.views[i].title));this._profileUid=profile.uid;this._profileTypeId=profile.profileType().id;this.baseSelect=new WebInspector.StatusBarComboBox(this._changeBase.bind(this));this.baseSelect.element.addStyleClass("hidden");this._updateBaseOptions();this.filterSelect=new WebInspector.StatusBarComboBox(this._changeFilter.bind(this));this._updateFilterOptions();this.selectedSizeText=new WebInspector.StatusBarText("");this._popoverHelper=new WebInspector.ObjectPopoverHelper(this.element,this._getHoverAnchor.bind(this),this._resolveObjectForPopover.bind(this),undefined,true);this.profile.load(profileCallback.bind(this));function profileCallback(heapSnapshotProxy)
{var list=this._profiles();var profileIndex;for(var i=0;i<list.length;++i){if(list[i].uid===this._profileUid){profileIndex=i;break;}}
if(profileIndex>0)
this.baseSelect.setSelectedIndex(profileIndex-1);else
this.baseSelect.setSelectedIndex(profileIndex);this.dataGrid.setDataSource(heapSnapshotProxy);}}
WebInspector.HeapSnapshotView.prototype={_onIdsRangeChanged:function(event)
{var minId=event.data.minId;var maxId=event.data.maxId;this.selectedSizeText.setText(WebInspector.UIString("Selected size: %s",Number.bytesToString(event.data.size)));if(this.constructorsDataGrid.snapshot)
this.constructorsDataGrid.setSelectionRange(minId,maxId);},dispose:function()
{this.parent.removeEventListener("profile added",this._onProfileHeaderAdded,this);this.profile.dispose();if(this.baseProfile)
this.baseProfile.dispose();this.containmentDataGrid.dispose();this.constructorsDataGrid.dispose();this.diffDataGrid.dispose();this.dominatorDataGrid.dispose();this.retainmentDataGrid.dispose();},get statusBarItems()
{return[this.viewSelect.element,this.baseSelect.element,this.filterSelect.element,this.selectedSizeText.element];},get profile()
{return this.parent.getProfile(this._profileTypeId,this._profileUid);},get baseProfile()
{return this.parent.getProfile(this._profileTypeId,this._baseProfileUid);},wasShown:function()
{this.profile.load(profileCallback.bind(this));function profileCallback(){this.profile._wasShown();if(this.baseProfile)
this.baseProfile.load(function(){});}},willHide:function()
{this._currentSearchResultIndex=-1;this._popoverHelper.hidePopover();if(this.helpPopover&&this.helpPopover.isShowing())
this.helpPopover.hide();},onResize:function()
{var height=this.retainmentView.element.clientHeight;this._updateRetainmentViewHeight(height);},searchCanceled:function()
{if(this._searchResults){for(var i=0;i<this._searchResults.length;++i){var node=this._searchResults[i].node;delete node._searchMatched;node.refresh();}}
delete this._searchFinishedCallback;this._currentSearchResultIndex=-1;this._searchResults=[];},performSearch:function(query,finishedCallback)
{this.searchCanceled();query=query.trim();if(!query)
return;if(this.currentView!==this.constructorsView&&this.currentView!==this.diffView)
return;this._searchFinishedCallback=finishedCallback;var nameRegExp=createPlainTextSearchRegex(query,"i");var snapshotNodeId=null;function matchesByName(gridNode){return("_name"in gridNode)&&nameRegExp.test(gridNode._name);}
function matchesById(gridNode){return("snapshotNodeId"in gridNode)&&gridNode.snapshotNodeId===snapshotNodeId;}
var matchPredicate;if(query.charAt(0)!=="@")
matchPredicate=matchesByName;else{snapshotNodeId=parseInt(query.substring(1),10);matchPredicate=matchesById;}
function matchesQuery(gridNode)
{delete gridNode._searchMatched;if(matchPredicate(gridNode)){gridNode._searchMatched=true;gridNode.refresh();return true;}
return false;}
var current=this.dataGrid.rootNode().children[0];var depth=0;var info={};const maxDepth=1;while(current){if(matchesQuery(current))
this._searchResults.push({node:current});current=current.traverseNextNode(false,null,(depth>=maxDepth),info);depth+=info.depthChange;}
finishedCallback(this,this._searchResults.length);},jumpToFirstSearchResult:function()
{if(!this._searchResults||!this._searchResults.length)
return;this._currentSearchResultIndex=0;this._jumpToSearchResult(this._currentSearchResultIndex);},jumpToLastSearchResult:function()
{if(!this._searchResults||!this._searchResults.length)
return;this._currentSearchResultIndex=(this._searchResults.length-1);this._jumpToSearchResult(this._currentSearchResultIndex);},jumpToNextSearchResult:function()
{if(!this._searchResults||!this._searchResults.length)
return;if(++this._currentSearchResultIndex>=this._searchResults.length)
this._currentSearchResultIndex=0;this._jumpToSearchResult(this._currentSearchResultIndex);},jumpToPreviousSearchResult:function()
{if(!this._searchResults||!this._searchResults.length)
return;if(--this._currentSearchResultIndex<0)
this._currentSearchResultIndex=(this._searchResults.length-1);this._jumpToSearchResult(this._currentSearchResultIndex);},showingFirstSearchResult:function()
{return(this._currentSearchResultIndex===0);},showingLastSearchResult:function()
{return(this._searchResults&&this._currentSearchResultIndex===(this._searchResults.length-1));},_jumpToSearchResult:function(index)
{var searchResult=this._searchResults[index];if(!searchResult)
return;var node=searchResult.node;node.revealAndSelect();},refreshVisibleData:function()
{var child=this.dataGrid.rootNode().children[0];while(child){child.refresh();child=child.traverseNextNode(false,null,true);}},_changeBase:function()
{if(this._baseProfileUid===this._profiles()[this.baseSelect.selectedIndex()].uid)
return;this._baseProfileUid=this._profiles()[this.baseSelect.selectedIndex()].uid;var dataGrid=(this.dataGrid);if(dataGrid.snapshot)
this.baseProfile.load(dataGrid.setBaseDataSource.bind(dataGrid));if(!this.currentQuery||!this._searchFinishedCallback||!this._searchResults)
return;this._searchFinishedCallback(this,-this._searchResults.length);this.performSearch(this.currentQuery,this._searchFinishedCallback);},_changeFilter:function()
{var profileIndex=this.filterSelect.selectedIndex()-1;this.dataGrid.filterSelectIndexChanged(this._profiles(),profileIndex);WebInspector.notifications.dispatchEventToListeners(WebInspector.UserMetrics.UserAction,{action:WebInspector.UserMetrics.UserActionNames.HeapSnapshotFilterChanged,label:this.filterSelect.selectedOption().label});if(!this.currentQuery||!this._searchFinishedCallback||!this._searchResults)
return;this._searchFinishedCallback(this,-this._searchResults.length);this.performSearch(this.currentQuery,this._searchFinishedCallback);},_createToolbarWithClassNameFilter:function()
{var toolbar=document.createElement("div");toolbar.addStyleClass("class-view-toolbar");var classNameFilter=document.createElement("input");classNameFilter.addStyleClass("class-name-filter");classNameFilter.setAttribute("placeholder",WebInspector.UIString("Class filter"));classNameFilter.addEventListener("keyup",this._changeNameFilter.bind(this,classNameFilter),false);toolbar.appendChild(classNameFilter);return toolbar;},_changeNameFilter:function(classNameInputElement)
{var filter=classNameInputElement.value;this.dataGrid.changeNameFilter(filter);},_profiles:function()
{return this.parent.getProfileType(this._profileTypeId).getProfiles();},populateContextMenu:function(contextMenu,event)
{this.dataGrid.populateContextMenu(this.parent,contextMenu,event);},_selectionChanged:function(event)
{var selectedNode=event.target.selectedNode;this._setRetainmentDataGridSource(selectedNode);this._inspectedObjectChanged(event);},_inspectedObjectChanged:function(event)
{var selectedNode=event.target.selectedNode;if(!this.profile.fromFile()&&selectedNode instanceof WebInspector.HeapSnapshotGenericObjectNode)
ConsoleAgent.addInspectedHeapObject(selectedNode.snapshotNodeId);},_setRetainmentDataGridSource:function(nodeItem)
{if(nodeItem&&nodeItem.snapshotNodeIndex)
this.retainmentDataGrid.setDataSource(nodeItem.isDeletedNode?nodeItem.dataGrid.baseSnapshot:nodeItem.dataGrid.snapshot,nodeItem.snapshotNodeIndex);else
this.retainmentDataGrid.reset();},_mouseDownInContentsGrid:function(event)
{if(event.detail<2)
return;var cell=event.target.enclosingNodeOrSelfWithNodeName("td");if(!cell||(!cell.hasStyleClass("count-column")&&!cell.hasStyleClass("shallowSize-column")&&!cell.hasStyleClass("retainedSize-column")))
return;event.consume(true);},changeView:function(viewTitle,callback)
{var viewIndex=null;for(var i=0;i<this.views.length;++i){if(this.views[i].title===viewTitle){viewIndex=i;break;}}
if(this.views.current===viewIndex||viewIndex==null){setTimeout(callback,0);return;}
function dataGridContentShown(event)
{var dataGrid=event.data;dataGrid.removeEventListener(WebInspector.HeapSnapshotSortableDataGrid.Events.ContentShown,dataGridContentShown,this);if(dataGrid===this.dataGrid)
callback();}
this.views[viewIndex].grid.addEventListener(WebInspector.HeapSnapshotSortableDataGrid.Events.ContentShown,dataGridContentShown,this);this.viewSelect.setSelectedIndex(viewIndex);this._changeView(viewIndex);},_updateDataSourceAndView:function()
{var dataGrid=this.dataGrid;if(dataGrid.snapshot)
return;this.profile.load(didLoadSnapshot.bind(this));function didLoadSnapshot(snapshotProxy)
{if(this.dataGrid!==dataGrid)
return;if(dataGrid.snapshot!==snapshotProxy)
dataGrid.setDataSource(snapshotProxy);if(dataGrid===this.diffDataGrid){if(!this._baseProfileUid)
this._baseProfileUid=this._profiles()[this.baseSelect.selectedIndex()].uid;this.baseProfile.load(didLoadBaseSnaphot.bind(this));}}
function didLoadBaseSnaphot(baseSnapshotProxy)
{if(this.diffDataGrid.baseSnapshot!==baseSnapshotProxy)
this.diffDataGrid.setBaseDataSource(baseSnapshotProxy);}},_onSelectedViewChanged:function(event)
{this._changeView(event.target.selectedIndex);},_updateSelectorsVisibility:function()
{if(this.currentView===this.diffView)
this.baseSelect.element.removeStyleClass("hidden");else
this.baseSelect.element.addStyleClass("hidden");if(this.currentView===this.constructorsView){if(this._trackingOverviewGrid){this._trackingOverviewGrid.element.removeStyleClass("hidden");this._trackingOverviewGrid.update();this.viewsContainer.addStyleClass("reserve-80px-at-top");}
this.filterSelect.element.removeStyleClass("hidden");}else{this.filterSelect.element.addStyleClass("hidden");if(this._trackingOverviewGrid){this._trackingOverviewGrid.element.addStyleClass("hidden");this.viewsContainer.removeStyleClass("reserve-80px-at-top");}}},_changeView:function(selectedIndex)
{if(selectedIndex===this.views.current)
return;this.views.current=selectedIndex;this.currentView.detach();var view=this.views[this.views.current];this.currentView=view.view;this.dataGrid=view.grid;this.currentView.show(this.viewsContainer);this.refreshVisibleData();this.dataGrid.updateWidths();this._updateSelectorsVisibility();this._updateDataSourceAndView();if(!this.currentQuery||!this._searchFinishedCallback||!this._searchResults)
return;this._searchFinishedCallback(this,-this._searchResults.length);this.performSearch(this.currentQuery,this._searchFinishedCallback);},_getHoverAnchor:function(target)
{var span=target.enclosingNodeOrSelfWithNodeName("span");if(!span)
return;var row=target.enclosingNodeOrSelfWithNodeName("tr");if(!row)
return;span.node=row._dataGridNode;return span;},_resolveObjectForPopover:function(element,showCallback,objectGroupName)
{if(this.profile.fromFile())
return;element.node.queryObjectContent(showCallback,objectGroupName);},_startRetainersHeaderDragging:function(event)
{if(!this.isShowing())
return false;this._previousDragPosition=event.pageY;return true;},_retainersHeaderDragging:function(event)
{var height=this.retainmentView.element.clientHeight;height+=this._previousDragPosition-event.pageY;this._previousDragPosition=event.pageY;this._updateRetainmentViewHeight(height);event.consume(true);},_endRetainersHeaderDragging:function(event)
{delete this._previousDragPosition;event.consume();},_updateRetainmentViewHeight:function(height)
{height=Number.constrain(height,Preferences.minConsoleHeight,this.element.clientHeight-Preferences.minConsoleHeight);this.viewsContainer.style.bottom=(height+this.retainmentViewHeader.clientHeight)+"px";if(this._trackingOverviewGrid&&this.currentView===this.constructorsView)
this.viewsContainer.addStyleClass("reserve-80px-at-top");this.retainmentView.element.style.height=height+"px";this.retainmentViewHeader.style.bottom=height+"px";this.currentView.doResize();},_updateBaseOptions:function()
{var list=this._profiles();if(this.baseSelect.size()===list.length)
return;for(var i=this.baseSelect.size(),n=list.length;i<n;++i){var title=list[i].title;this.baseSelect.createOption(title);}},_updateFilterOptions:function()
{var list=this._profiles();if(this.filterSelect.size()-1===list.length)
return;if(!this.filterSelect.size())
this.filterSelect.createOption(WebInspector.UIString("All objects"));for(var i=this.filterSelect.size()-1,n=list.length;i<n;++i){var title=list[i].title;if(!i)
title=WebInspector.UIString("Objects allocated before %s",title);else
title=WebInspector.UIString("Objects allocated between %s and %s",list[i-1].title,title);this.filterSelect.createOption(title);}},_onProfileHeaderAdded:function(event)
{if(!event.data||event.data.type!==this._profileTypeId)
return;this._updateBaseOptions();this._updateFilterOptions();},__proto__:WebInspector.View.prototype}
WebInspector.HeapProfilerDispatcher=function()
{this._dispatchers=[];InspectorBackend.registerHeapProfilerDispatcher(this);}
WebInspector.HeapProfilerDispatcher.prototype={register:function(dispatcher)
{this._dispatchers.push(dispatcher);},_genericCaller:function(eventName)
{var args=Array.prototype.slice.call(arguments.callee.caller.arguments);for(var i=0;i<this._dispatchers.length;++i)
this._dispatchers[i][eventName].apply(this._dispatchers[i],args);},heapStatsUpdate:function(samples)
{this._genericCaller("heapStatsUpdate");},lastSeenObjectId:function(lastSeenObjectId,timestamp)
{this._genericCaller("lastSeenObjectId");},addProfileHeader:function(profileHeader)
{this._genericCaller("addProfileHeader");},addHeapSnapshotChunk:function(uid,chunk)
{this._genericCaller("addHeapSnapshotChunk");},reportHeapSnapshotProgress:function(done,total)
{this._genericCaller("reportHeapSnapshotProgress");},resetProfiles:function()
{this._genericCaller("resetProfiles");}}
WebInspector.HeapProfilerDispatcher._dispatcher=new WebInspector.HeapProfilerDispatcher();WebInspector.HeapSnapshotProfileType=function()
{WebInspector.ProfileType.call(this,WebInspector.HeapSnapshotProfileType.TypeId,WebInspector.UIString("Take Heap Snapshot"));WebInspector.HeapProfilerDispatcher._dispatcher.register(this);}
WebInspector.HeapSnapshotProfileType.TypeId="HEAP";WebInspector.HeapSnapshotProfileType.SnapshotReceived="SnapshotReceived";WebInspector.HeapSnapshotProfileType.prototype={fileExtension:function()
{return".heapsnapshot";},get buttonTooltip()
{return WebInspector.UIString("Take heap snapshot.");},isInstantProfile:function()
{return true;},buttonClicked:function()
{this._takeHeapSnapshot(function(){});WebInspector.userMetrics.ProfilesHeapProfileTaken.record();return false;},heapStatsUpdate:function(samples)
{},lastSeenObjectId:function(lastSeenObjectId,timestamp)
{},get treeItemTitle()
{return WebInspector.UIString("HEAP SNAPSHOTS");},get description()
{return WebInspector.UIString("Heap snapshot profiles show memory distribution among your page's JavaScript objects and related DOM nodes.");},createTemporaryProfile:function(title)
{title=title||WebInspector.UIString("Snapshotting\u2026");return new WebInspector.HeapProfileHeader(this,title);},createProfile:function(profile)
{return new WebInspector.HeapProfileHeader(this,profile.title,profile.uid,profile.maxJSObjectId||0);},_takeHeapSnapshot:function(callback)
{var temporaryProfile=this.findTemporaryProfile();if(!temporaryProfile)
this.addProfile(this.createTemporaryProfile());HeapProfilerAgent.takeHeapSnapshot(true,callback);},addProfileHeader:function(profileHeader)
{if(!this.findTemporaryProfile())
return;var profile=this.createProfile(profileHeader);profile._profileSamples=this._profileSamples;this._profileSamples=null;this.addProfile(profile);},addHeapSnapshotChunk:function(uid,chunk)
{var profile=this._profilesIdMap[this._makeKey(uid)];if(profile)
profile.transferChunk(chunk);},reportHeapSnapshotProgress:function(done,total)
{var profile=this.findTemporaryProfile();if(profile)
this.dispatchEventToListeners(WebInspector.ProfileType.Events.ProgressUpdated,{"profile":profile,"done":done,"total":total});},resetProfiles:function()
{this._reset();},removeProfile:function(profile)
{WebInspector.ProfileType.prototype.removeProfile.call(this,profile);if(!profile.isTemporary&&!profile.fromFile())
HeapProfilerAgent.removeProfile(profile.uid);},_snapshotReceived:function(profile)
{this.dispatchEventToListeners(WebInspector.HeapSnapshotProfileType.SnapshotReceived,profile);},__proto__:WebInspector.ProfileType.prototype}
WebInspector.TrackingHeapSnapshotProfileType=function(profilesPanel)
{WebInspector.ProfileType.call(this,WebInspector.TrackingHeapSnapshotProfileType.TypeId,WebInspector.UIString("Record Heap Allocations"));this._profilesPanel=profilesPanel;WebInspector.HeapProfilerDispatcher._dispatcher.register(this);}
WebInspector.TrackingHeapSnapshotProfileType.TypeId="HEAP-RECORD";WebInspector.TrackingHeapSnapshotProfileType.HeapStatsUpdate="HeapStatsUpdate";WebInspector.TrackingHeapSnapshotProfileType.TrackingStarted="TrackingStarted";WebInspector.TrackingHeapSnapshotProfileType.TrackingStopped="TrackingStopped";WebInspector.TrackingHeapSnapshotProfileType.prototype={heapStatsUpdate:function(samples)
{if(!this._profileSamples)
return;var index;for(var i=0;i<samples.length;i+=3){index=samples[i];var count=samples[i+1];var size=samples[i+2];this._profileSamples.sizes[index]=size;if(!this._profileSamples.max[index]||size>this._profileSamples.max[index])
this._profileSamples.max[index]=size;}
this._lastUpdatedIndex=index;},lastSeenObjectId:function(lastSeenObjectId,timestamp)
{var profileSamples=this._profileSamples;if(!profileSamples)
return;var currentIndex=Math.max(profileSamples.ids.length,profileSamples.max.length-1);profileSamples.ids[currentIndex]=lastSeenObjectId;if(!profileSamples.max[currentIndex]){profileSamples.max[currentIndex]=0;profileSamples.sizes[currentIndex]=0;}
profileSamples.timestamps[currentIndex]=timestamp;if(profileSamples.totalTime<timestamp-profileSamples.timestamps[0])
profileSamples.totalTime*=2;this.dispatchEventToListeners(WebInspector.TrackingHeapSnapshotProfileType.HeapStatsUpdate,this._profileSamples);var profile=this.findTemporaryProfile();profile.sidebarElement.wait=true;if(profile.sidebarElement&&!profile.sidebarElement.wait)
profile.sidebarElement.wait=true;},hasTemporaryView:function()
{return true;},get buttonTooltip()
{return this._recording?WebInspector.UIString("Stop recording heap profile."):WebInspector.UIString("Start recording heap profile.");},isInstantProfile:function()
{return false;},buttonClicked:function()
{return this._toggleRecording();},_startRecordingProfile:function()
{this._lastSeenIndex=-1;this._profileSamples={'sizes':[],'ids':[],'timestamps':[],'max':[],'totalTime':30000};this._recording=true;HeapProfilerAgent.startTrackingHeapObjects();this.dispatchEventToListeners(WebInspector.TrackingHeapSnapshotProfileType.TrackingStarted);},_stopRecordingProfile:function()
{HeapProfilerAgent.stopTrackingHeapObjects();HeapProfilerAgent.takeHeapSnapshot(true);this._recording=false;this.dispatchEventToListeners(WebInspector.TrackingHeapSnapshotProfileType.TrackingStopped);},_toggleRecording:function()
{if(this._recording)
this._stopRecordingProfile();else
this._startRecordingProfile();return this._recording;},get treeItemTitle()
{return WebInspector.UIString("HEAP TIMELINES");},get description()
{return WebInspector.UIString("Record JavaScript object allocations over time. Use this profile type to isolate memory leaks.");},_reset:function()
{WebInspector.HeapSnapshotProfileType.prototype._reset.call(this);if(this._recording)
this._stopRecordingProfile();this._profileSamples=null;this._lastSeenIndex=-1;},createTemporaryProfile:function(title)
{title=title||WebInspector.UIString("Recording\u2026");return new WebInspector.HeapProfileHeader(this,title);},__proto__:WebInspector.HeapSnapshotProfileType.prototype}
WebInspector.HeapProfileHeader=function(type,title,uid,maxJSObjectId)
{WebInspector.ProfileHeader.call(this,type,title,uid);this.maxJSObjectId=maxJSObjectId;this._receiver=null;this._snapshotProxy=null;this._totalNumberOfChunks=0;this._transferHandler=null;}
WebInspector.HeapProfileHeader.prototype={createSidebarTreeElement:function()
{return new WebInspector.ProfileSidebarTreeElement(this,WebInspector.UIString("Snapshot %d"),"heap-snapshot-sidebar-tree-item");},createView:function(profilesPanel)
{return new WebInspector.HeapSnapshotView(profilesPanel,this);},load:function(callback)
{if(this.uid===-1)
return;if(this._snapshotProxy){callback(this._snapshotProxy);return;}
this._numberOfChunks=0;if(!this._receiver){this._setupWorker();this._transferHandler=new WebInspector.BackendSnapshotLoader(this);this.sidebarElement.subtitle=WebInspector.UIString("Loading\u2026");this.sidebarElement.wait=true;this._transferSnapshot();}
var loaderProxy=(this._receiver);loaderProxy.addConsumer(callback);},_transferSnapshot:function()
{function finishTransfer()
{if(this._transferHandler){this._transferHandler.finishTransfer();this._totalNumberOfChunks=this._transferHandler._totalNumberOfChunks;}}
HeapProfilerAgent.getHeapSnapshot(this.uid,finishTransfer.bind(this));},snapshotConstructorName:function()
{return"JSHeapSnapshot";},snapshotProxyConstructor:function()
{return WebInspector.HeapSnapshotProxy;},_setupWorker:function()
{function setProfileWait(event)
{this.sidebarElement.wait=event.data;}
var worker=new WebInspector.HeapSnapshotWorkerProxy(this._handleWorkerEvent.bind(this));worker.addEventListener("wait",setProfileWait,this);var loaderProxy=worker.createLoader(this.snapshotConstructorName(),this.snapshotProxyConstructor());loaderProxy.addConsumer(this._snapshotReceived.bind(this));this._receiver=loaderProxy;},_handleWorkerEvent:function(eventName,data)
{if(WebInspector.HeapSnapshotProgress.Event.Update!==eventName)
return;this._updateSubtitle(data);},dispose:function()
{if(this._receiver)
this._receiver.close();else if(this._snapshotProxy)
this._snapshotProxy.dispose();if(this._view){var view=this._view;this._view=null;view.dispose();}},_updateSubtitle:function(value)
{this.sidebarElement.subtitle=value;},_didCompleteSnapshotTransfer:function()
{this.sidebarElement.subtitle=Number.bytesToString(this._snapshotProxy.totalSize);this.sidebarElement.wait=false;},transferChunk:function(chunk)
{this._transferHandler.transferChunk(chunk);},_snapshotReceived:function(snapshotProxy)
{this._receiver=null;if(snapshotProxy)
this._snapshotProxy=snapshotProxy;this._didCompleteSnapshotTransfer();var worker=(this._snapshotProxy.worker);this.isTemporary=false;worker.startCheckingForLongRunningCalls();this.notifySnapshotReceived();if(this.fromFile()){function didGetMaxNodeId(id)
{this.maxJSObjectId=id;}
snapshotProxy.maxJsNodeId(didGetMaxNodeId.bind(this));}},notifySnapshotReceived:function()
{this._profileType._snapshotReceived(this);},_wasShown:function()
{},canSaveToFile:function()
{return!this.fromFile()&&!!this._snapshotProxy&&!this._receiver;},saveToFile:function()
{var fileOutputStream=new WebInspector.FileOutputStream();function onOpen()
{this._receiver=fileOutputStream;this._transferHandler=new WebInspector.SaveSnapshotHandler(this);this._transferSnapshot();}
this._fileName=this._fileName||"Heap-"+new Date().toISO8601Compact()+this._profileType.fileExtension();fileOutputStream.open(this._fileName,onOpen.bind(this));},loadFromFile:function(file)
{this.sidebarElement.subtitle=WebInspector.UIString("Loading\u2026");this.sidebarElement.wait=true;this._setupWorker();var delegate=new WebInspector.HeapSnapshotLoadFromFileDelegate(this);var fileReader=this._createFileReader(file,delegate);fileReader.start(this._receiver);},_createFileReader:function(file,delegate)
{return new WebInspector.ChunkedFileReader(file,10000000,delegate);},__proto__:WebInspector.ProfileHeader.prototype}
WebInspector.SnapshotTransferHandler=function(header,title)
{this._numberOfChunks=0;this._savedChunks=0;this._header=header;this._totalNumberOfChunks=0;this._title=title;}
WebInspector.SnapshotTransferHandler.prototype={transferChunk:function(chunk)
{++this._numberOfChunks;this._header._receiver.write(chunk,this._didTransferChunk.bind(this));},finishTransfer:function()
{},_didTransferChunk:function()
{this._updateProgress(++this._savedChunks,this._totalNumberOfChunks);},_updateProgress:function(value,total)
{}}
WebInspector.SaveSnapshotHandler=function(header)
{WebInspector.SnapshotTransferHandler.call(this,header,"Saving\u2026 %d\%");this._totalNumberOfChunks=header._totalNumberOfChunks;this._updateProgress(0,this._totalNumberOfChunks);}
WebInspector.SaveSnapshotHandler.prototype={_updateProgress:function(value,total)
{var percentValue=((total?(value/total):0)*100).toFixed(0);this._header._updateSubtitle(WebInspector.UIString(this._title,percentValue));if(value===total){this._header._receiver.close();this._header._didCompleteSnapshotTransfer();}},__proto__:WebInspector.SnapshotTransferHandler.prototype}
WebInspector.BackendSnapshotLoader=function(header)
{WebInspector.SnapshotTransferHandler.call(this,header,"Loading\u2026 %d\%");}
WebInspector.BackendSnapshotLoader.prototype={finishTransfer:function()
{this._header._receiver.close(this._didFinishTransfer.bind(this));this._totalNumberOfChunks=this._numberOfChunks;},_didFinishTransfer:function()
{console.assert(this._totalNumberOfChunks===this._savedChunks,"Not all chunks were transfered.");},__proto__:WebInspector.SnapshotTransferHandler.prototype}
WebInspector.HeapSnapshotLoadFromFileDelegate=function(snapshotHeader)
{this._snapshotHeader=snapshotHeader;}
WebInspector.HeapSnapshotLoadFromFileDelegate.prototype={onTransferStarted:function()
{},onChunkTransferred:function(reader)
{},onTransferFinished:function()
{},onError:function(reader,e)
{switch(e.target.error.code){case e.target.error.NOT_FOUND_ERR:this._snapshotHeader._updateSubtitle(WebInspector.UIString("'%s' not found.",reader.fileName()));break;case e.target.error.NOT_READABLE_ERR:this._snapshotHeader._updateSubtitle(WebInspector.UIString("'%s' is not readable",reader.fileName()));break;case e.target.error.ABORT_ERR:break;default:this._snapshotHeader._updateSubtitle(WebInspector.UIString("'%s' error %d",reader.fileName(),e.target.error.code));}}}
WebInspector.HeapTrackingOverviewGrid=function(heapProfileHeader)
{WebInspector.View.call(this);this.registerRequiredCSS("flameChart.css");this.element.id="heap-recording-view";this._overviewContainer=this.element.createChild("div","overview-container");this._overviewGrid=new WebInspector.OverviewGrid("heap-recording");this._overviewCanvas=this._overviewContainer.createChild("canvas","heap-recording-overview-canvas");this._overviewContainer.appendChild(this._overviewGrid.element);this._overviewCalculator=new WebInspector.HeapTrackingOverviewGrid.OverviewCalculator();this._overviewGrid.addEventListener(WebInspector.OverviewGrid.Events.WindowChanged,this._onWindowChanged,this);this._profileSamples=heapProfileHeader._profileSamples||heapProfileHeader._profileType._profileSamples;if(heapProfileHeader.isTemporary){this._profileType=heapProfileHeader._profileType;this._profileType.addEventListener(WebInspector.TrackingHeapSnapshotProfileType.HeapStatsUpdate,this._onHeapStatsUpdate,this);this._profileType.addEventListener(WebInspector.TrackingHeapSnapshotProfileType.TrackingStopped,this._onStopTracking,this);}
var timestamps=this._profileSamples.timestamps;var totalTime=this._profileSamples.totalTime;this._windowLeft=0.0;this._windowRight=totalTime&&timestamps.length?(timestamps[timestamps.length-1]-timestamps[0])/totalTime:1.0;this._overviewGrid.setWindow(this._windowLeft,this._windowRight);this._yScale=new WebInspector.HeapTrackingOverviewGrid.SmoothScale();this._xScale=new WebInspector.HeapTrackingOverviewGrid.SmoothScale();}
WebInspector.HeapTrackingOverviewGrid.IdsRangeChanged="IdsRangeChanged";WebInspector.HeapTrackingOverviewGrid.prototype={_onStopTracking:function(event)
{this._profileType.removeEventListener(WebInspector.TrackingHeapSnapshotProfileType.HeapStatsUpdate,this._onHeapStatsUpdate,this);this._profileType.removeEventListener(WebInspector.TrackingHeapSnapshotProfileType.TrackingStopped,this._onStopTracking,this);},_onHeapStatsUpdate:function(event)
{this._profileSamples=event.data;this._scheduleUpdate();},_drawOverviewCanvas:function(width,height)
{if(!this._profileSamples)
return;var profileSamples=this._profileSamples;var sizes=profileSamples.sizes;var topSizes=profileSamples.max;var timestamps=profileSamples.timestamps;var startTime=timestamps[0];var endTime=timestamps[timestamps.length-1];var scaleFactor=this._xScale.nextScale(width/profileSamples.totalTime);var maxSize=0;function aggregateAndCall(sizes,callback)
{var size=0;var currentX=0;for(var i=1;i<timestamps.length;++i){var x=Math.floor((timestamps[i]-startTime)*scaleFactor);if(x!==currentX){if(size)
callback(currentX,size);size=0;currentX=x;}
size+=sizes[i];}
callback(currentX,size);}
function maxSizeCallback(x,size)
{maxSize=Math.max(maxSize,size);}
aggregateAndCall(sizes,maxSizeCallback);var yScaleFactor=this._yScale.nextScale(maxSize?height/(maxSize*1.1):0.0);this._overviewCanvas.width=width*window.devicePixelRatio;this._overviewCanvas.height=height*window.devicePixelRatio;this._overviewCanvas.style.width=width+"px";this._overviewCanvas.style.height=height+"px";var context=this._overviewCanvas.getContext("2d");context.scale(window.devicePixelRatio,window.devicePixelRatio);context.beginPath();context.lineWidth=2;context.strokeStyle="rgba(192, 192, 192, 0.6)";var currentX=(endTime-startTime)*scaleFactor;context.moveTo(currentX,height-1);context.lineTo(currentX,0);context.stroke();context.closePath();var gridY;var gridValue;var gridLabelHeight=14;if(yScaleFactor){const maxGridValue=(height-gridLabelHeight)/yScaleFactor;gridValue=Math.pow(1024,Math.floor(Math.log(maxGridValue)/Math.log(1024)));gridValue*=Math.pow(10,Math.floor(Math.log(maxGridValue/gridValue)/Math.LN10));if(gridValue*5<=maxGridValue)
gridValue*=5;gridY=Math.round(height-gridValue*yScaleFactor-0.5)+0.5;context.beginPath();context.lineWidth=1;context.strokeStyle="rgba(0, 0, 0, 0.2)";context.moveTo(0,gridY);context.lineTo(width,gridY);context.stroke();context.closePath();}
function drawBarCallback(x,size)
{context.moveTo(x,height-1);context.lineTo(x,Math.round(height-size*yScaleFactor-1));}
context.beginPath();context.lineWidth=2;context.strokeStyle="rgba(192, 192, 192, 0.6)";aggregateAndCall(topSizes,drawBarCallback);context.stroke();context.closePath();context.beginPath();context.lineWidth=2;context.strokeStyle="rgba(0, 0, 192, 0.8)";aggregateAndCall(sizes,drawBarCallback);context.stroke();context.closePath();if(gridValue){var label=Number.bytesToString(gridValue);var labelPadding=4;var labelX=0;var labelY=gridY-0.5;var labelWidth=2*labelPadding+context.measureText(label).width;context.beginPath();context.textBaseline="bottom";context.font="10px "+window.getComputedStyle(this.element,null).getPropertyValue("font-family");context.fillStyle="rgba(255, 255, 255, 0.75)";context.fillRect(labelX,labelY-gridLabelHeight,labelWidth,gridLabelHeight);context.fillStyle="rgb(64, 64, 64)";context.fillText(label,labelX+labelPadding,labelY);context.fill();context.closePath();}},onResize:function()
{this._updateOverviewCanvas=true;this._scheduleUpdate();},_onWindowChanged:function()
{if(!this._updateGridTimerId)
this._updateGridTimerId=setTimeout(this._updateGrid.bind(this),10);},_scheduleUpdate:function()
{if(this._updateTimerId)
return;this._updateTimerId=setTimeout(this.update.bind(this),10);},_updateBoundaries:function()
{this._windowLeft=this._overviewGrid.windowLeft();this._windowRight=this._overviewGrid.windowRight();this._windowWidth=this._windowRight-this._windowLeft;},update:function()
{this._updateTimerId=null;if(!this.isShowing())
return;this._updateBoundaries();this._overviewCalculator._updateBoundaries(this);this._overviewGrid.updateDividers(this._overviewCalculator);this._drawOverviewCanvas(this._overviewContainer.clientWidth,this._overviewContainer.clientHeight-20);},_updateGrid:function()
{this._updateGridTimerId=0;this._updateBoundaries();var ids=this._profileSamples.ids;var timestamps=this._profileSamples.timestamps;var sizes=this._profileSamples.sizes;var startTime=timestamps[0];var totalTime=this._profileSamples.totalTime;var timeLeft=startTime+totalTime*this._windowLeft;var timeRight=startTime+totalTime*this._windowRight;var minId=0;var maxId=ids[ids.length-1]+1;var size=0;for(var i=0;i<timestamps.length;++i){if(!timestamps[i])
continue;if(timestamps[i]>timeRight)
break;maxId=ids[i];if(timestamps[i]<timeLeft){minId=ids[i];continue;}
size+=sizes[i];}
this.dispatchEventToListeners(WebInspector.HeapTrackingOverviewGrid.IdsRangeChanged,{minId:minId,maxId:maxId,size:size});},__proto__:WebInspector.View.prototype}
WebInspector.HeapTrackingOverviewGrid.SmoothScale=function()
{this._lastUpdate=0;this._currentScale=0.0;}
WebInspector.HeapTrackingOverviewGrid.SmoothScale.prototype={nextScale:function(target){target=target||this._currentScale;if(this._currentScale){var now=Date.now();var timeDeltaMs=now-this._lastUpdate;this._lastUpdate=now;var maxChangePerSec=20;var maxChangePerDelta=Math.pow(maxChangePerSec,timeDeltaMs/1000);var scaleChange=target/this._currentScale;this._currentScale*=Number.constrain(scaleChange,1/maxChangePerDelta,maxChangePerDelta);}else
this._currentScale=target;return this._currentScale;}}
WebInspector.HeapTrackingOverviewGrid.OverviewCalculator=function()
{}
WebInspector.HeapTrackingOverviewGrid.OverviewCalculator.prototype={_updateBoundaries:function(chart)
{this._minimumBoundaries=0;this._maximumBoundaries=chart._profileSamples.totalTime;this._xScaleFactor=chart._overviewContainer.clientWidth/this._maximumBoundaries;},computePosition:function(time)
{return(time-this._minimumBoundaries)*this._xScaleFactor;},formatTime:function(value)
{return Number.secondsToString((value+this._minimumBoundaries)/1000);},maximumBoundary:function()
{return this._maximumBoundaries;},minimumBoundary:function()
{return this._minimumBoundaries;},zeroTime:function()
{return this._minimumBoundaries;},boundarySpan:function()
{return this._maximumBoundaries-this._minimumBoundaries;}};WebInspector.HeapSnapshotWorkerDispatcher=function(globalObject,postMessage)
{this._objects=[];this._global=globalObject;this._postMessage=postMessage;}
WebInspector.HeapSnapshotWorkerDispatcher.prototype={_findFunction:function(name)
{var path=name.split(".");var result=this._global;for(var i=0;i<path.length;++i)
result=result[path[i]];return result;},sendEvent:function(name,data)
{this._postMessage({eventName:name,data:data});},dispatchMessage:function(event)
{var data=event.data;var response={callId:data.callId};try{switch(data.disposition){case"create":{var constructorFunction=this._findFunction(data.methodName);this._objects[data.objectId]=new constructorFunction(this);break;}
case"dispose":{delete this._objects[data.objectId];break;}
case"getter":{var object=this._objects[data.objectId];var result=object[data.methodName];response.result=result;break;}
case"factory":{var object=this._objects[data.objectId];var result=object[data.methodName].apply(object,data.methodArguments);if(result)
this._objects[data.newObjectId]=result;response.result=!!result;break;}
case"method":{var object=this._objects[data.objectId];response.result=object[data.methodName].apply(object,data.methodArguments);break;}}}catch(e){response.error=e.toString();response.errorCallStack=e.stack;if(data.methodName)
response.errorMethodName=data.methodName;}
this._postMessage(response);}};;WebInspector.JSHeapSnapshot=function(profile,progress)
{this._nodeFlags={canBeQueried:1,detachedDOMTreeNode:2,pageObject:4,visitedMarkerMask:0x0ffff,visitedMarker:0x10000};this._lazyStringCache={};WebInspector.HeapSnapshot.call(this,profile,progress);}
WebInspector.JSHeapSnapshot.prototype={maxJsNodeId:function()
{var nodeFieldCount=this._nodeFieldCount;var nodes=this._nodes;var nodesLength=nodes.length;var id=0;for(var nodeIndex=this._nodeIdOffset;nodeIndex<nodesLength;nodeIndex+=nodeFieldCount){var nextId=nodes[nodeIndex];if(nextId%2===0)
continue;if(id<nodes[nodeIndex])
id=nodes[nodeIndex];}
return id;},createNode:function(nodeIndex)
{return new WebInspector.JSHeapSnapshotNode(this,nodeIndex);},createEdge:function(edges,edgeIndex)
{return new WebInspector.JSHeapSnapshotEdge(this,edges,edgeIndex);},createRetainingEdge:function(retainedNodeIndex,retainerIndex)
{return new WebInspector.JSHeapSnapshotRetainerEdge(this,retainedNodeIndex,retainerIndex);},classNodesFilter:function()
{function filter(node)
{return node.isUserObject();}
return filter;},containmentEdgesFilter:function(showHiddenData)
{function filter(edge){if(edge.isInvisible())
return false;if(showHiddenData)
return true;return!edge.isHidden()&&!edge.node().isHidden();}
return filter;},retainingEdgesFilter:function(showHiddenData)
{var containmentEdgesFilter=this.containmentEdgesFilter(showHiddenData);function filter(edge)
{return containmentEdgesFilter(edge)&&!edge.node().isRoot()&&!edge.isWeak();}
return filter;},dispose:function()
{WebInspector.HeapSnapshot.prototype.dispose.call(this);delete this._flags;},_markInvisibleEdges:function()
{for(var iter=this.rootNode().edges();iter.hasNext();iter.next()){var edge=iter.edge;if(!edge.isShortcut())
continue;var node=edge.node();var propNames={};for(var innerIter=node.edges();innerIter.hasNext();innerIter.next()){var globalObjEdge=innerIter.edge;if(globalObjEdge.isShortcut())
propNames[globalObjEdge._nameOrIndex()]=true;}
for(innerIter.rewind();innerIter.hasNext();innerIter.next()){var globalObjEdge=innerIter.edge;if(!globalObjEdge.isShortcut()&&globalObjEdge.node().isHidden()&&globalObjEdge._hasStringName()&&(globalObjEdge._nameOrIndex()in propNames))
this._containmentEdges[globalObjEdge._edges._start+globalObjEdge.edgeIndex+this._edgeTypeOffset]=this._edgeInvisibleType;}}},_calculateFlags:function()
{this._flags=new Uint32Array(this.nodeCount);this._markDetachedDOMTreeNodes();this._markQueriableHeapObjects();this._markPageOwnedNodes();},_isUserRoot:function(node)
{return node.isUserRoot()||node.isDocumentDOMTreesRoot();},forEachRoot:function(action,userRootsOnly)
{function getChildNodeByName(node,name)
{for(var iter=node.edges();iter.hasNext();iter.next()){var child=iter.edge.node();if(child.name()===name)
return child;}
return null;}
function getChildNodeByLinkName(node,name)
{for(var iter=node.edges();iter.hasNext();iter.next()){var edge=iter.edge;if(edge.name()===name)
return edge.node();}
return null;}
var visitedNodes={};function doAction(node)
{var ordinal=node._ordinal();if(!visitedNodes[ordinal]){action(node);visitedNodes[ordinal]=true;}}
var gcRoots=getChildNodeByName(this.rootNode(),"(GC roots)");if(!gcRoots)
return;if(userRootsOnly){for(var iter=this.rootNode().edges();iter.hasNext();iter.next()){var node=iter.edge.node();if(node.isDocumentDOMTreesRoot())
doAction(node);else if(node.isUserRoot()){var nativeContextNode=getChildNodeByLinkName(node,"native_context");if(nativeContextNode)
doAction(nativeContextNode);else
doAction(node);}}}else{for(var iter=gcRoots.edges();iter.hasNext();iter.next()){var subRoot=iter.edge.node();for(var iter2=subRoot.edges();iter2.hasNext();iter2.next())
doAction(iter2.edge.node());doAction(subRoot);}
for(var iter=this.rootNode().edges();iter.hasNext();iter.next())
doAction(iter.edge.node())}},userObjectsMapAndFlag:function()
{return{map:this._flags,flag:this._nodeFlags.pageObject};},_flagsOfNode:function(node)
{return this._flags[node.nodeIndex/this._nodeFieldCount];},_markDetachedDOMTreeNodes:function()
{var flag=this._nodeFlags.detachedDOMTreeNode;var detachedDOMTreesRoot;for(var iter=this.rootNode().edges();iter.hasNext();iter.next()){var node=iter.edge.node();if(node.name()==="(Detached DOM trees)"){detachedDOMTreesRoot=node;break;}}
if(!detachedDOMTreesRoot)
return;var detachedDOMTreeRE=/^Detached DOM tree/;for(var iter=detachedDOMTreesRoot.edges();iter.hasNext();iter.next()){var node=iter.edge.node();if(detachedDOMTreeRE.test(node.className())){for(var edgesIter=node.edges();edgesIter.hasNext();edgesIter.next())
this._flags[edgesIter.edge.node().nodeIndex/this._nodeFieldCount]|=flag;}}},_markQueriableHeapObjects:function()
{var flag=this._nodeFlags.canBeQueried;var hiddenEdgeType=this._edgeHiddenType;var internalEdgeType=this._edgeInternalType;var invisibleEdgeType=this._edgeInvisibleType;var weakEdgeType=this._edgeWeakType;var edgeToNodeOffset=this._edgeToNodeOffset;var edgeTypeOffset=this._edgeTypeOffset;var edgeFieldsCount=this._edgeFieldsCount;var containmentEdges=this._containmentEdges;var nodes=this._nodes;var nodeCount=this.nodeCount;var nodeFieldCount=this._nodeFieldCount;var firstEdgeIndexes=this._firstEdgeIndexes;var flags=this._flags;var list=[];for(var iter=this.rootNode().edges();iter.hasNext();iter.next()){if(iter.edge.node().isUserRoot())
list.push(iter.edge.node().nodeIndex/nodeFieldCount);}
while(list.length){var nodeOrdinal=list.pop();if(flags[nodeOrdinal]&flag)
continue;flags[nodeOrdinal]|=flag;var beginEdgeIndex=firstEdgeIndexes[nodeOrdinal];var endEdgeIndex=firstEdgeIndexes[nodeOrdinal+1];for(var edgeIndex=beginEdgeIndex;edgeIndex<endEdgeIndex;edgeIndex+=edgeFieldsCount){var childNodeIndex=containmentEdges[edgeIndex+edgeToNodeOffset];var childNodeOrdinal=childNodeIndex/nodeFieldCount;if(flags[childNodeOrdinal]&flag)
continue;var type=containmentEdges[edgeIndex+edgeTypeOffset];if(type===hiddenEdgeType||type===invisibleEdgeType||type===internalEdgeType||type===weakEdgeType)
continue;list.push(childNodeOrdinal);}}},_markPageOwnedNodes:function()
{var edgeShortcutType=this._edgeShortcutType;var edgeElementType=this._edgeElementType;var edgeToNodeOffset=this._edgeToNodeOffset;var edgeTypeOffset=this._edgeTypeOffset;var edgeFieldsCount=this._edgeFieldsCount;var edgeWeakType=this._edgeWeakType;var firstEdgeIndexes=this._firstEdgeIndexes;var containmentEdges=this._containmentEdges;var containmentEdgesLength=containmentEdges.length;var nodes=this._nodes;var nodeFieldCount=this._nodeFieldCount;var nodesCount=this.nodeCount;var flags=this._flags;var flag=this._nodeFlags.pageObject;var visitedMarker=this._nodeFlags.visitedMarker;var visitedMarkerMask=this._nodeFlags.visitedMarkerMask;var markerAndFlag=visitedMarker|flag;var nodesToVisit=new Uint32Array(nodesCount);var nodesToVisitLength=0;var rootNodeOrdinal=this._rootNodeIndex/nodeFieldCount;var node=this.rootNode();for(var edgeIndex=firstEdgeIndexes[rootNodeOrdinal],endEdgeIndex=firstEdgeIndexes[rootNodeOrdinal+1];edgeIndex<endEdgeIndex;edgeIndex+=edgeFieldsCount){var edgeType=containmentEdges[edgeIndex+edgeTypeOffset];var nodeIndex=containmentEdges[edgeIndex+edgeToNodeOffset];if(edgeType===edgeElementType){node.nodeIndex=nodeIndex;if(!node.isDocumentDOMTreesRoot())
continue;}else if(edgeType!==edgeShortcutType)
continue;var nodeOrdinal=nodeIndex/nodeFieldCount;nodesToVisit[nodesToVisitLength++]=nodeOrdinal;flags[nodeOrdinal]|=visitedMarker;}
while(nodesToVisitLength){var nodeOrdinal=nodesToVisit[--nodesToVisitLength];flags[nodeOrdinal]|=flag;flags[nodeOrdinal]&=visitedMarkerMask;var beginEdgeIndex=firstEdgeIndexes[nodeOrdinal];var endEdgeIndex=firstEdgeIndexes[nodeOrdinal+1];for(var edgeIndex=beginEdgeIndex;edgeIndex<endEdgeIndex;edgeIndex+=edgeFieldsCount){var childNodeIndex=containmentEdges[edgeIndex+edgeToNodeOffset];var childNodeOrdinal=childNodeIndex/nodeFieldCount;if(flags[childNodeOrdinal]&markerAndFlag)
continue;var type=containmentEdges[edgeIndex+edgeTypeOffset];if(type===edgeWeakType)
continue;nodesToVisit[nodesToVisitLength++]=childNodeOrdinal;flags[childNodeOrdinal]|=visitedMarker;}}},__proto__:WebInspector.HeapSnapshot.prototype};WebInspector.JSHeapSnapshotNode=function(snapshot,nodeIndex)
{WebInspector.HeapSnapshotNode.call(this,snapshot,nodeIndex)}
WebInspector.JSHeapSnapshotNode.prototype={canBeQueried:function()
{var flags=this._snapshot._flagsOfNode(this);return!!(flags&this._snapshot._nodeFlags.canBeQueried);},isUserObject:function()
{var flags=this._snapshot._flagsOfNode(this);return!!(flags&this._snapshot._nodeFlags.pageObject);},name:function(){var snapshot=this._snapshot;if(this._type()===snapshot._nodeConsStringType){var string=snapshot._lazyStringCache[this.nodeIndex];if(typeof string==="undefined"){string=this._consStringName();snapshot._lazyStringCache[this.nodeIndex]=string;}
return string;}
return WebInspector.HeapSnapshotNode.prototype.name.call(this);},_consStringName:function()
{var snapshot=this._snapshot;var consStringType=snapshot._nodeConsStringType;var edgeInternalType=snapshot._edgeInternalType;var edgeFieldsCount=snapshot._edgeFieldsCount;var edgeToNodeOffset=snapshot._edgeToNodeOffset;var edgeTypeOffset=snapshot._edgeTypeOffset;var edgeNameOffset=snapshot._edgeNameOffset;var strings=snapshot._strings;var edges=snapshot._containmentEdges;var firstEdgeIndexes=snapshot._firstEdgeIndexes;var nodeFieldCount=snapshot._nodeFieldCount;var nodeTypeOffset=snapshot._nodeTypeOffset;var nodeNameOffset=snapshot._nodeNameOffset;var nodes=snapshot._nodes;var nodesStack=[];nodesStack.push(this.nodeIndex);var name="";while(nodesStack.length&&name.length<1024){var nodeIndex=nodesStack.pop();if(nodes[nodeIndex+nodeTypeOffset]!==consStringType){name+=strings[nodes[nodeIndex+nodeNameOffset]];continue;}
var nodeOrdinal=nodeIndex/nodeFieldCount;var beginEdgeIndex=firstEdgeIndexes[nodeOrdinal];var endEdgeIndex=firstEdgeIndexes[nodeOrdinal+1];var firstNodeIndex=0;var secondNodeIndex=0;for(var edgeIndex=beginEdgeIndex;edgeIndex<endEdgeIndex&&(!firstNodeIndex||!secondNodeIndex);edgeIndex+=edgeFieldsCount){var edgeType=edges[edgeIndex+edgeTypeOffset];if(edgeType===edgeInternalType){var edgeName=strings[edges[edgeIndex+edgeNameOffset]];if(edgeName==="first")
firstNodeIndex=edges[edgeIndex+edgeToNodeOffset];else if(edgeName==="second")
secondNodeIndex=edges[edgeIndex+edgeToNodeOffset];}}
nodesStack.push(secondNodeIndex);nodesStack.push(firstNodeIndex);}
return name;},className:function()
{var type=this.type();switch(type){case"hidden":return"(system)";case"object":case"native":return this.name();case"code":return"(compiled code)";default:return"("+type+")";}},classIndex:function()
{var snapshot=this._snapshot;var nodes=snapshot._nodes;var type=nodes[this.nodeIndex+snapshot._nodeTypeOffset];;if(type===snapshot._nodeObjectType||type===snapshot._nodeNativeType)
return nodes[this.nodeIndex+snapshot._nodeNameOffset];return-1-type;},id:function()
{var snapshot=this._snapshot;return snapshot._nodes[this.nodeIndex+snapshot._nodeIdOffset];},isHidden:function()
{return this._type()===this._snapshot._nodeHiddenType;},isSynthetic:function()
{return this._type()===this._snapshot._nodeSyntheticType;},isUserRoot:function()
{return!this.isSynthetic();},isDocumentDOMTreesRoot:function()
{return this.isSynthetic()&&this.name()==="(Document DOM trees)";},serialize:function()
{var result=WebInspector.HeapSnapshotNode.prototype.serialize.call(this);var flags=this._snapshot._flagsOfNode(this);if(flags&this._snapshot._nodeFlags.canBeQueried)
result.canBeQueried=true;if(flags&this._snapshot._nodeFlags.detachedDOMTreeNode)
result.detachedDOMTreeNode=true;return result;},__proto__:WebInspector.HeapSnapshotNode.prototype};WebInspector.JSHeapSnapshotEdge=function(snapshot,edges,edgeIndex)
{WebInspector.HeapSnapshotEdge.call(this,snapshot,edges,edgeIndex);}
WebInspector.JSHeapSnapshotEdge.prototype={clone:function()
{return new WebInspector.JSHeapSnapshotEdge(this._snapshot,this._edges,this.edgeIndex);},hasStringName:function()
{if(!this.isShortcut())
return this._hasStringName();return isNaN(parseInt(this._name(),10));},isElement:function()
{return this._type()===this._snapshot._edgeElementType;},isHidden:function()
{return this._type()===this._snapshot._edgeHiddenType;},isWeak:function()
{return this._type()===this._snapshot._edgeWeakType;},isInternal:function()
{return this._type()===this._snapshot._edgeInternalType;},isInvisible:function()
{return this._type()===this._snapshot._edgeInvisibleType;},isShortcut:function()
{return this._type()===this._snapshot._edgeShortcutType;},name:function()
{if(!this.isShortcut())
return this._name();var numName=parseInt(this._name(),10);return isNaN(numName)?this._name():numName;},toString:function()
{var name=this.name();switch(this.type()){case"context":return"->"+name;case"element":return"["+name+"]";case"weak":return"[["+name+"]]";case"property":return name.indexOf(" ")===-1?"."+name:"[\""+name+"\"]";case"shortcut":if(typeof name==="string")
return name.indexOf(" ")===-1?"."+name:"[\""+name+"\"]";else
return"["+name+"]";case"internal":case"hidden":case"invisible":return"{"+name+"}";};return"?"+name+"?";},_hasStringName:function()
{return!this.isElement()&&!this.isHidden()&&!this.isWeak();},_name:function()
{return this._hasStringName()?this._snapshot._strings[this._nameOrIndex()]:this._nameOrIndex();},_nameOrIndex:function()
{return this._edges.item(this.edgeIndex+this._snapshot._edgeNameOffset);},_type:function()
{return this._edges.item(this.edgeIndex+this._snapshot._edgeTypeOffset);},__proto__:WebInspector.HeapSnapshotEdge.prototype};WebInspector.JSHeapSnapshotRetainerEdge=function(snapshot,retainedNodeIndex,retainerIndex)
{WebInspector.HeapSnapshotRetainerEdge.call(this,snapshot,retainedNodeIndex,retainerIndex);}
WebInspector.JSHeapSnapshotRetainerEdge.prototype={clone:function()
{return new WebInspector.JSHeapSnapshotRetainerEdge(this._snapshot,this._retainedNodeIndex,this.retainerIndex());},isHidden:function()
{return this._edge().isHidden();},isInternal:function()
{return this._edge().isInternal();},isInvisible:function()
{return this._edge().isInvisible();},isShortcut:function()
{return this._edge().isShortcut();},isWeak:function()
{return this._edge().isWeak();},__proto__:WebInspector.HeapSnapshotRetainerEdge.prototype};WebInspector.ProfileLauncherView=function(profilesPanel)
{WebInspector.View.call(this);this._panel=profilesPanel;this.element.addStyleClass("profile-launcher-view");this.element.addStyleClass("panel-enabler-view");this._contentElement=this.element.createChild("div","profile-launcher-view-content");this._innerContentElement=this._contentElement.createChild("div");this._controlButton=this._contentElement.createChild("button","control-profiling");this._controlButton.addEventListener("click",this._controlButtonClicked.bind(this),false);}
WebInspector.ProfileLauncherView.prototype={addProfileType:function(profileType)
{var descriptionElement=this._innerContentElement.createChild("h1");descriptionElement.textContent=profileType.description;var decorationElement=profileType.decorationElement();if(decorationElement)
this._innerContentElement.appendChild(decorationElement);this._isInstantProfile=profileType.isInstantProfile();this._isEnabled=profileType.isEnabled();this._profileTypeId=profileType.id;},_controlButtonClicked:function()
{this._panel.toggleRecordButton();},_updateControls:function()
{if(this._isEnabled)
this._controlButton.removeAttribute("disabled");else
this._controlButton.setAttribute("disabled","");if(this._isInstantProfile){this._controlButton.removeStyleClass("running");this._controlButton.textContent=WebInspector.UIString("Take Snapshot");}else if(this._isProfiling){this._controlButton.addStyleClass("running");this._controlButton.textContent=WebInspector.UIString("Stop");}else{this._controlButton.removeStyleClass("running");this._controlButton.textContent=WebInspector.UIString("Start");}},profileStarted:function()
{this._isProfiling=true;WebInspector.profileManager.notifyStarted(this._profileTypeId);this._updateControls();},profileFinished:function()
{this._isProfiling=false;WebInspector.profileManager.notifyStoped(this._profileTypeId);this._updateControls();},updateProfileType:function(profileType)
{this._isInstantProfile=profileType.isInstantProfile();this._isEnabled=profileType.isEnabled();this._profileTypeId=profileType.id;this._updateControls();},__proto__:WebInspector.View.prototype}
WebInspector.MultiProfileLauncherView=function(profilesPanel)
{WebInspector.ProfileLauncherView.call(this,profilesPanel);var header=this._innerContentElement.createChild("h1");header.textContent=WebInspector.UIString("Select profiling type");this._profileTypeSelectorForm=this._innerContentElement.createChild("form");this._innerContentElement.createChild("div","flexible-space");}
WebInspector.MultiProfileLauncherView.EventTypes={ProfileTypeSelected:"profile-type-selected"}
WebInspector.MultiProfileLauncherView.prototype={addProfileType:function(profileType)
{var checked=!this._profileTypeSelectorForm.children.length;var labelElement=this._profileTypeSelectorForm.createChild("label");labelElement.textContent=profileType.name;var optionElement=document.createElement("input");labelElement.insertBefore(optionElement,labelElement.firstChild);optionElement.type="radio";optionElement.name="profile-type";optionElement.style.hidden=true;if(checked){optionElement.checked=checked;this.dispatchEventToListeners(WebInspector.MultiProfileLauncherView.EventTypes.ProfileTypeSelected,profileType);}
optionElement.addEventListener("change",this._profileTypeChanged.bind(this,profileType),false);var descriptionElement=labelElement.createChild("p");descriptionElement.textContent=profileType.description;var decorationElement=profileType.decorationElement();if(decorationElement)
labelElement.appendChild(decorationElement);},_controlButtonClicked:function()
{this._panel.toggleRecordButton();},_updateControls:function()
{WebInspector.ProfileLauncherView.prototype._updateControls.call(this);var items=this._profileTypeSelectorForm.elements;for(var i=0;i<items.length;++i){if(items[i].type==="radio")
items[i].disabled=this._isProfiling;}},_profileTypeChanged:function(profileType,event)
{this.dispatchEventToListeners(WebInspector.MultiProfileLauncherView.EventTypes.ProfileTypeSelected,profileType);this._isInstantProfile=profileType.isInstantProfile();this._isEnabled=profileType.isEnabled();this._profileTypeId=profileType.id;this._updateControls();},profileStarted:function()
{this._isProfiling=true;WebInspector.profileManager.notifyStarted(this._profileTypeId);this._updateControls();},profileFinished:function()
{this._isProfiling=false;WebInspector.profileManager.notifyStoped(this._profileTypeId);this._updateControls();},__proto__:WebInspector.ProfileLauncherView.prototype};WebInspector.TopDownProfileDataGridNode=function(profileNode,owningTree)
{var hasChildren=!!(profileNode.children&&profileNode.children.length);WebInspector.ProfileDataGridNode.call(this,profileNode,owningTree,hasChildren);this._remainingChildren=profileNode.children;}
WebInspector.TopDownProfileDataGridNode.prototype={_sharedPopulate:function()
{var children=this._remainingChildren;var childrenLength=children.length;for(var i=0;i<childrenLength;++i)
this.appendChild(new WebInspector.TopDownProfileDataGridNode(children[i],this.tree));this._remainingChildren=null;},_exclude:function(aCallUID)
{if(this._remainingChildren)
this.populate();this._save();var children=this.children;var index=this.children.length;while(index--)
children[index]._exclude(aCallUID);var child=this.childrenByCallUID[aCallUID];if(child)
this._merge(child,true);},__proto__:WebInspector.ProfileDataGridNode.prototype}
WebInspector.TopDownProfileDataGridTree=function(profileView,rootProfileNode)
{WebInspector.ProfileDataGridTree.call(this,profileView,rootProfileNode);this._remainingChildren=rootProfileNode.children;var any=(this);var node=(any);WebInspector.TopDownProfileDataGridNode.prototype.populate.call(node);}
WebInspector.TopDownProfileDataGridTree.prototype={focus:function(profileDataGridNode)
{if(!profileDataGridNode)
return;this._save();profileDataGridNode.savePosition();this.children=[profileDataGridNode];this.totalTime=profileDataGridNode.totalTime;},exclude:function(profileDataGridNode)
{if(!profileDataGridNode)
return;this._save();var excludedCallUID=profileDataGridNode.callUID;var any=(this);var node=(any);WebInspector.TopDownProfileDataGridNode.prototype._exclude.call(node,excludedCallUID);if(this.lastComparator)
this.sort(this.lastComparator,true);},restore:function()
{if(!this._savedChildren)
return;this.children[0].restorePosition();WebInspector.ProfileDataGridTree.prototype.restore.call(this);},_merge:WebInspector.TopDownProfileDataGridNode.prototype._merge,_sharedPopulate:WebInspector.TopDownProfileDataGridNode.prototype._sharedPopulate,__proto__:WebInspector.ProfileDataGridTree.prototype};WebInspector.CanvasProfileView=function(profile)
{WebInspector.View.call(this);this.registerRequiredCSS("canvasProfiler.css");this.element.addStyleClass("canvas-profile-view");this._profile=profile;this._traceLogId=profile.traceLogId();this._traceLogPlayer=profile.traceLogPlayer();this._linkifier=new WebInspector.Linkifier();const defaultReplayLogWidthPercent=0.34;this._replayInfoSplitView=new WebInspector.SplitView(true,"canvasProfileViewReplaySplitLocation",defaultReplayLogWidthPercent);this._replayInfoSplitView.setMainElementConstraints(defaultReplayLogWidthPercent,defaultReplayLogWidthPercent);this._replayInfoSplitView.show(this.element);this._imageSplitView=new WebInspector.SplitView(false,"canvasProfileViewSplitLocation",300);this._imageSplitView.show(this._replayInfoSplitView.firstElement());var replayImageContainer=this._imageSplitView.firstElement();replayImageContainer.id="canvas-replay-image-container";this._replayImageElement=replayImageContainer.createChild("img","canvas-replay-image");this._debugInfoElement=replayImageContainer.createChild("div","canvas-debug-info hidden");this._spinnerIcon=replayImageContainer.createChild("img","canvas-spinner-icon hidden");var replayLogContainer=this._imageSplitView.secondElement();var controlsContainer=replayLogContainer.createChild("div","status-bar");var logGridContainer=replayLogContainer.createChild("div","canvas-replay-log");this._createControlButton(controlsContainer,"canvas-replay-first-step",WebInspector.UIString("First call."),this._onReplayFirstStepClick.bind(this));this._createControlButton(controlsContainer,"canvas-replay-prev-step",WebInspector.UIString("Previous call."),this._onReplayStepClick.bind(this,false));this._createControlButton(controlsContainer,"canvas-replay-next-step",WebInspector.UIString("Next call."),this._onReplayStepClick.bind(this,true));this._createControlButton(controlsContainer,"canvas-replay-prev-draw",WebInspector.UIString("Previous drawing call."),this._onReplayDrawingCallClick.bind(this,false));this._createControlButton(controlsContainer,"canvas-replay-next-draw",WebInspector.UIString("Next drawing call."),this._onReplayDrawingCallClick.bind(this,true));this._createControlButton(controlsContainer,"canvas-replay-last-step",WebInspector.UIString("Last call."),this._onReplayLastStepClick.bind(this));this._replayContextSelector=new WebInspector.StatusBarComboBox(this._onReplayContextChanged.bind(this));this._replayContextSelector.createOption(WebInspector.UIString("<screenshot auto>"),WebInspector.UIString("Show screenshot of the last replayed resource."),"");controlsContainer.appendChild(this._replayContextSelector.element);this._installReplayInfoSidebarWidgets(controlsContainer);this._replayStateView=new WebInspector.CanvasReplayStateView(this._traceLogPlayer);this._replayStateView.show(this._replayInfoSplitView.secondElement());this._replayContexts={};var columns=[{title:"#",sortable:false,width:"5%"},{title:WebInspector.UIString("Call"),sortable:false,width:"75%",disclosure:true},{title:WebInspector.UIString("Location"),sortable:false,width:"20%"}];this._logGrid=new WebInspector.DataGrid(columns);this._logGrid.element.addStyleClass("fill");this._logGrid.show(logGridContainer);this._logGrid.addEventListener(WebInspector.DataGrid.Events.SelectedNode,this._replayTraceLog,this);this.element.addEventListener("mousedown",this._onMouseClick.bind(this),true);this._popoverHelper=new WebInspector.ObjectPopoverHelper(this.element,this._popoverAnchor.bind(this),this._resolveObjectForPopover.bind(this),this._onHidePopover.bind(this),true);this._popoverHelper.setRemoteObjectFormatter(this._hexNumbersFormatter.bind(this));this._requestTraceLog(0);}
WebInspector.CanvasProfileView.TraceLogPollingInterval=500;WebInspector.CanvasProfileView.prototype={dispose:function()
{this._linkifier.reset();},get statusBarItems()
{return[];},get profile()
{return this._profile;},elementsToRestoreScrollPositionsFor:function()
{return[this._logGrid.scrollContainer];},_installReplayInfoSidebarWidgets:function(controlsContainer)
{this._replayInfoResizeWidgetElement=controlsContainer.createChild("div","resizer-widget");this._replayInfoSplitView.installResizer(this._replayInfoResizeWidgetElement);this._toggleReplayStateSidebarButton=new WebInspector.StatusBarButton("","right-sidebar-show-hide-button canvas-sidebar-show-hide-button",3);this._toggleReplayStateSidebarButton.addEventListener("click",clickHandler,this);controlsContainer.appendChild(this._toggleReplayStateSidebarButton.element);this._enableReplayInfoSidebar(false);function clickHandler()
{this._enableReplayInfoSidebar(this._toggleReplayStateSidebarButton.state==="left");}},_enableReplayInfoSidebar:function(show)
{if(show){this._toggleReplayStateSidebarButton.state="right";this._toggleReplayStateSidebarButton.title=WebInspector.UIString("Hide sidebar.");this._replayInfoSplitView.showBoth();}else{this._toggleReplayStateSidebarButton.state="left";this._toggleReplayStateSidebarButton.title=WebInspector.UIString("Show sidebar.");this._replayInfoSplitView.showOnlyFirst();}
this._replayInfoResizeWidgetElement.enableStyleClass("hidden",!show);},_onMouseClick:function(event)
{var resourceLinkElement=event.target.enclosingNodeOrSelfWithClass("canvas-formatted-resource");if(resourceLinkElement){this._enableReplayInfoSidebar(true);this._replayStateView.selectResource(resourceLinkElement.__resourceId);event.consume(true);return;}
if(event.target.enclosingNodeOrSelfWithClass("webkit-html-resource-link"))
event.consume(false);},_createControlButton:function(parent,className,title,clickCallback)
{var button=new WebInspector.StatusBarButton(title,className+" canvas-replay-button");parent.appendChild(button.element);button.makeLongClickEnabled();button.addEventListener("click",clickCallback,this);button.addEventListener("longClickDown",clickCallback,this);button.addEventListener("longClickPress",clickCallback,this);},_onReplayContextChanged:function()
{var selectedContextId=this._replayContextSelector.selectedOption().value;function didReceiveResourceState(resourceState)
{this._enableWaitIcon(false);if(selectedContextId!==this._replayContextSelector.selectedOption().value)
return;var imageURL=(resourceState&&resourceState.imageURL)||"";this._replayImageElement.src=imageURL;this._replayImageElement.style.visibility=imageURL?"":"hidden";}
this._enableWaitIcon(true);this._traceLogPlayer.getResourceState(selectedContextId,didReceiveResourceState.bind(this));},_onReplayStepClick:function(forward)
{var selectedNode=this._logGrid.selectedNode;if(!selectedNode)
return;var nextNode=selectedNode;do{nextNode=forward?nextNode.traverseNextNode(false):nextNode.traversePreviousNode(false);}while(nextNode&&typeof nextNode.index!=="number");(nextNode||selectedNode).revealAndSelect();},_onReplayDrawingCallClick:function(forward)
{var selectedNode=this._logGrid.selectedNode;if(!selectedNode)
return;var nextNode=selectedNode;while(nextNode){var sibling=forward?nextNode.nextSibling:nextNode.previousSibling;if(sibling){nextNode=sibling;if(nextNode.hasChildren||nextNode.call.isDrawingCall)
break;}else{nextNode=nextNode.parent;if(!forward)
break;}}
if(!nextNode&&forward)
this._onReplayLastStepClick();else
(nextNode||selectedNode).revealAndSelect();},_onReplayFirstStepClick:function()
{var firstNode=this._logGrid.rootNode().children[0];if(firstNode)
firstNode.revealAndSelect();},_onReplayLastStepClick:function()
{var lastNode=this._logGrid.rootNode().children.peekLast();if(!lastNode)
return;while(lastNode.expanded){var lastChild=lastNode.children.peekLast();if(!lastChild)
break;lastNode=lastChild;}
lastNode.revealAndSelect();},_enableWaitIcon:function(enable)
{this._spinnerIcon.enableStyleClass("hidden",!enable);this._debugInfoElement.enableStyleClass("hidden",enable);},_replayTraceLog:function()
{if(this._pendingReplayTraceLogEvent)
return;var index=this._selectedCallIndex();if(index===-1||index===this._lastReplayCallIndex)
return;this._lastReplayCallIndex=index;this._pendingReplayTraceLogEvent=true;function didReplayTraceLog(resourceState,replayTime)
{delete this._pendingReplayTraceLogEvent;this._enableWaitIcon(false);this._debugInfoElement.textContent="Replay time: "+Number.secondsToString(replayTime/1000,true);this._onReplayContextChanged();if(index!==this._selectedCallIndex())
this._replayTraceLog();}
this._enableWaitIcon(true);this._traceLogPlayer.replayTraceLog(index,didReplayTraceLog.bind(this));},_requestTraceLog:function(offset)
{function didReceiveTraceLog(traceLog)
{this._enableWaitIcon(false);if(!traceLog)
return;var callNodes=[];var calls=traceLog.calls;var index=traceLog.startOffset;for(var i=0,n=calls.length;i<n;++i)
callNodes.push(this._createCallNode(index++,calls[i]));var contexts=traceLog.contexts;for(var i=0,n=contexts.length;i<n;++i){var contextId=contexts[i].resourceId||"";var description=contexts[i].description||"";if(this._replayContexts[contextId])
continue;this._replayContexts[contextId]=true;this._replayContextSelector.createOption(description,WebInspector.UIString("Show screenshot of this context's canvas."),contextId);}
this._appendCallNodes(callNodes);if(traceLog.alive)
setTimeout(this._requestTraceLog.bind(this,index),WebInspector.CanvasProfileView.TraceLogPollingInterval);else
this._flattenSingleFrameNode();this._profile._updateCapturingStatus(traceLog);this._onReplayLastStepClick();}
this._enableWaitIcon(true);this._traceLogPlayer.getTraceLog(offset,undefined,didReceiveTraceLog.bind(this));},_selectedCallIndex:function()
{var node=this._logGrid.selectedNode;return node?this._peekLastRecursively(node).index:-1;},_peekLastRecursively:function(node)
{var lastChild;while((lastChild=node.children.peekLast()))
node=lastChild;return node;},_appendCallNodes:function(callNodes)
{var rootNode=this._logGrid.rootNode();var frameNode=rootNode.children.peekLast();if(frameNode&&this._peekLastRecursively(frameNode).call.isFrameEndCall)
frameNode=null;for(var i=0,n=callNodes.length;i<n;++i){if(!frameNode){var index=rootNode.children.length;var data={};data[0]="";data[1]="Frame #"+(index+1);data[2]="";frameNode=new WebInspector.DataGridNode(data);frameNode.selectable=true;rootNode.appendChild(frameNode);}
var nextFrameCallIndex=i+1;while(nextFrameCallIndex<n&&!callNodes[nextFrameCallIndex-1].call.isFrameEndCall)
++nextFrameCallIndex;this._appendCallNodesToFrameNode(frameNode,callNodes,i,nextFrameCallIndex);i=nextFrameCallIndex-1;frameNode=null;}},_appendCallNodesToFrameNode:function(frameNode,callNodes,fromIndex,toIndex)
{var self=this;function appendDrawCallGroup()
{var index=self._drawCallGroupsCount||0;var data={};data[0]="";data[1]="Draw call group #"+(index+1);data[2]="";var node=new WebInspector.DataGridNode(data);node.selectable=true;self._drawCallGroupsCount=index+1;frameNode.appendChild(node);return node;}
function splitDrawCallGroup(drawCallGroup)
{var splitIndex=0;var splitNode;while((splitNode=drawCallGroup.children[splitIndex])){if(splitNode.call.isDrawingCall)
break;++splitIndex;}
var newDrawCallGroup=appendDrawCallGroup();var lastNode;while((lastNode=drawCallGroup.children[splitIndex+1]))
newDrawCallGroup.appendChild(lastNode);return newDrawCallGroup;}
var drawCallGroup=frameNode.children.peekLast();var groupHasDrawCall=false;if(drawCallGroup){for(var i=0,n=drawCallGroup.children.length;i<n;++i){if(drawCallGroup.children[i].call.isDrawingCall){groupHasDrawCall=true;break;}}}else
drawCallGroup=appendDrawCallGroup();for(var i=fromIndex;i<toIndex;++i){var node=callNodes[i];drawCallGroup.appendChild(node);if(node.call.isDrawingCall){if(groupHasDrawCall)
drawCallGroup=splitDrawCallGroup(drawCallGroup);else
groupHasDrawCall=true;}}},_createCallNode:function(index,call)
{var callViewElement=document.createElement("div");var data={};data[0]=index+1;data[1]=callViewElement;data[2]="";if(call.sourceURL){var lineNumber=Math.max(0,call.lineNumber-1)||0;var columnNumber=Math.max(0,call.columnNumber-1)||0;data[2]=this._linkifier.linkifyLocation(call.sourceURL,lineNumber,columnNumber);}
callViewElement.createChild("span","canvas-function-name").textContent=call.functionName||"context."+call.property;if(call.arguments){callViewElement.createTextChild("(");for(var i=0,n=call.arguments.length;i<n;++i){var argument=(call.arguments[i]);if(i)
callViewElement.createTextChild(", ");var element=WebInspector.CanvasProfileDataGridHelper.createCallArgumentElement(argument);element.__argumentIndex=i;callViewElement.appendChild(element);}
callViewElement.createTextChild(")");}else if(call.value){callViewElement.createTextChild(" = ");callViewElement.appendChild(WebInspector.CanvasProfileDataGridHelper.createCallArgumentElement(call.value));}
if(call.result){callViewElement.createTextChild(" => ");callViewElement.appendChild(WebInspector.CanvasProfileDataGridHelper.createCallArgumentElement(call.result));}
var node=new WebInspector.DataGridNode(data);node.index=index;node.selectable=true;node.call=call;return node;},_popoverAnchor:function(element,event)
{var argumentElement=element.enclosingNodeOrSelfWithClass("canvas-call-argument");if(!argumentElement||argumentElement.__suppressPopover)
return null;return argumentElement;},_resolveObjectForPopover:function(argumentElement,showCallback,objectGroupName)
{function showObjectPopover(error,result,resourceState)
{if(error)
return;if(!result)
return;this._popoverAnchorElement=argumentElement.cloneNode(true);this._popoverAnchorElement.addStyleClass("canvas-popover-anchor");this._popoverAnchorElement.addStyleClass("source-frame-eval-expression");argumentElement.parentElement.appendChild(this._popoverAnchorElement);var diffLeft=this._popoverAnchorElement.boxInWindow().x-argumentElement.boxInWindow().x;this._popoverAnchorElement.style.left=this._popoverAnchorElement.offsetLeft-diffLeft+"px";showCallback(WebInspector.RemoteObject.fromPayload(result),false,this._popoverAnchorElement);}
var evalResult=argumentElement.__evalResult;if(evalResult)
showObjectPopover.call(this,null,evalResult);else{var dataGridNode=this._logGrid.dataGridNodeFromNode(argumentElement);if(!dataGridNode||typeof dataGridNode.index!=="number"){this._popoverHelper.hidePopover();return;}
var callIndex=dataGridNode.index;var argumentIndex=argumentElement.__argumentIndex;if(typeof argumentIndex!=="number")
argumentIndex=-1;CanvasAgent.evaluateTraceLogCallArgument(this._traceLogId,callIndex,argumentIndex,objectGroupName,showObjectPopover.bind(this));}},_hexNumbersFormatter:function(object)
{if(object.type==="number"){var str="0000"+Number(object.description).toString(16).toUpperCase();str=str.replace(/^0+(.{4,})$/,"$1");return"0x"+str;}
return object.description||"";},_onHidePopover:function()
{if(this._popoverAnchorElement){this._popoverAnchorElement.remove()
delete this._popoverAnchorElement;}},_flattenSingleFrameNode:function()
{var rootNode=this._logGrid.rootNode();if(rootNode.children.length!==1)
return;var frameNode=rootNode.children[0];while(frameNode.children[0])
rootNode.appendChild(frameNode.children[0]);rootNode.removeChild(frameNode);},__proto__:WebInspector.View.prototype}
WebInspector.CanvasProfileType=function()
{WebInspector.ProfileType.call(this,WebInspector.CanvasProfileType.TypeId,WebInspector.UIString("Capture Canvas Frame"));this._nextProfileUid=1;this._recording=false;this._lastProfileHeader=null;this._capturingModeSelector=new WebInspector.StatusBarComboBox(this._dispatchViewUpdatedEvent.bind(this));this._capturingModeSelector.element.title=WebInspector.UIString("Canvas capture mode.");this._capturingModeSelector.createOption(WebInspector.UIString("Single Frame"),WebInspector.UIString("Capture a single canvas frame."),"");this._capturingModeSelector.createOption(WebInspector.UIString("Consecutive Frames"),WebInspector.UIString("Capture consecutive canvas frames."),"1");this._frameOptions={};this._framesWithCanvases={};this._frameSelector=new WebInspector.StatusBarComboBox(this._dispatchViewUpdatedEvent.bind(this));this._frameSelector.element.title=WebInspector.UIString("Frame containing the canvases to capture.");this._frameSelector.element.addStyleClass("hidden");WebInspector.runtimeModel.contextLists().forEach(this._addFrame,this);WebInspector.runtimeModel.addEventListener(WebInspector.RuntimeModel.Events.FrameExecutionContextListAdded,this._frameAdded,this);WebInspector.runtimeModel.addEventListener(WebInspector.RuntimeModel.Events.FrameExecutionContextListRemoved,this._frameRemoved,this);this._dispatcher=new WebInspector.CanvasDispatcher(this);this._canvasAgentEnabled=false;this._decorationElement=document.createElement("div");this._decorationElement.className="profile-canvas-decoration";this._updateDecorationElement();}
WebInspector.CanvasProfileType.TypeId="CANVAS_PROFILE";WebInspector.CanvasProfileType.prototype={get statusBarItems()
{return[this._capturingModeSelector.element,this._frameSelector.element];},get buttonTooltip()
{if(this._isSingleFrameMode())
return WebInspector.UIString("Capture next canvas frame.");else
return this._recording?WebInspector.UIString("Stop capturing canvas frames."):WebInspector.UIString("Start capturing canvas frames.");},buttonClicked:function()
{if(!this._canvasAgentEnabled)
return false;if(this._recording){this._recording=false;this._stopFrameCapturing();}else if(this._isSingleFrameMode()){this._recording=false;this._runSingleFrameCapturing();}else{this._recording=true;this._startFrameCapturing();}
return this._recording;},_runSingleFrameCapturing:function()
{var frameId=this._selectedFrameId();CanvasAgent.captureFrame(frameId,this._didStartCapturingFrame.bind(this,frameId));},_startFrameCapturing:function()
{var frameId=this._selectedFrameId();CanvasAgent.startCapturing(frameId,this._didStartCapturingFrame.bind(this,frameId));},_stopFrameCapturing:function()
{if(!this._lastProfileHeader)
return;var profileHeader=this._lastProfileHeader;var traceLogId=profileHeader.traceLogId();this._lastProfileHeader=null;function didStopCapturing()
{profileHeader._updateCapturingStatus();}
CanvasAgent.stopCapturing(traceLogId,didStopCapturing.bind(this));},_didStartCapturingFrame:function(frameId,error,traceLogId)
{if(error||this._lastProfileHeader&&this._lastProfileHeader.traceLogId()===traceLogId)
return;var profileHeader=new WebInspector.CanvasProfileHeader(this,WebInspector.UIString("Trace Log %d",this._nextProfileUid),this._nextProfileUid,traceLogId,frameId);++this._nextProfileUid;this._lastProfileHeader=profileHeader;this.addProfile(profileHeader);profileHeader._updateCapturingStatus();},get treeItemTitle()
{return WebInspector.UIString("CANVAS PROFILE");},get description()
{return WebInspector.UIString("Canvas calls instrumentation");},decorationElement:function()
{return this._decorationElement;},_reset:function()
{WebInspector.ProfileType.prototype._reset.call(this);this._nextProfileUid=1;},removeProfile:function(profile)
{WebInspector.ProfileType.prototype.removeProfile.call(this,profile);if(this._recording&&profile===this._lastProfileHeader)
this._recording=false;},setRecordingProfile:function(isProfiling)
{this._recording=isProfiling;},createTemporaryProfile:function(title)
{title=title||WebInspector.UIString("Capturing\u2026");return new WebInspector.CanvasProfileHeader(this,title);},createProfile:function(profile)
{return new WebInspector.CanvasProfileHeader(this,profile.title,-1);},_updateDecorationElement:function(forcePageReload)
{this._decorationElement.removeChildren();this._decorationElement.createChild("div","warning-icon-small");this._decorationElement.appendChild(document.createTextNode(this._canvasAgentEnabled?WebInspector.UIString("Canvas Profiler is enabled."):WebInspector.UIString("Canvas Profiler is disabled.")));var button=this._decorationElement.createChild("button");button.type="button";button.textContent=this._canvasAgentEnabled?WebInspector.UIString("Disable"):WebInspector.UIString("Enable");button.addEventListener("click",this._onProfilerEnableButtonClick.bind(this,!this._canvasAgentEnabled),false);if(forcePageReload){if(this._canvasAgentEnabled){function hasUninstrumentedCanvasesCallback(error,result)
{if(error||result)
PageAgent.reload();}
CanvasAgent.hasUninstrumentedCanvases(hasUninstrumentedCanvasesCallback.bind(this));}else{for(var frameId in this._framesWithCanvases){if(this._framesWithCanvases.hasOwnProperty(frameId)){PageAgent.reload();break;}}}}},_onProfilerEnableButtonClick:function(enable)
{if(this._canvasAgentEnabled===enable)
return;function callback(error)
{if(error)
return;this._canvasAgentEnabled=enable;this._updateDecorationElement(true);this._dispatchViewUpdatedEvent();}
if(enable)
CanvasAgent.enable(callback.bind(this));else
CanvasAgent.disable(callback.bind(this));},_isSingleFrameMode:function()
{return!this._capturingModeSelector.selectedOption().value;},_frameAdded:function(event)
{var contextList=(event.data);this._addFrame(contextList);},_addFrame:function(contextList)
{var frameId=contextList.frameId;var option=document.createElement("option");option.text=contextList.displayName;option.title=contextList.url;option.value=frameId;this._frameOptions[frameId]=option;if(this._framesWithCanvases[frameId]){this._frameSelector.addOption(option);this._dispatchViewUpdatedEvent();}},_frameRemoved:function(event)
{var contextList=(event.data);var frameId=contextList.frameId;var option=this._frameOptions[frameId];if(option&&this._framesWithCanvases[frameId]){this._frameSelector.removeOption(option);this._dispatchViewUpdatedEvent();}
delete this._frameOptions[frameId];delete this._framesWithCanvases[frameId];},_contextCreated:function(frameId)
{if(this._framesWithCanvases[frameId])
return;this._framesWithCanvases[frameId]=true;var option=this._frameOptions[frameId];if(option){this._frameSelector.addOption(option);this._dispatchViewUpdatedEvent();}},_traceLogsRemoved:function(frameId,traceLogId)
{var sidebarElementsToDelete=[];var sidebarElements=((this.treeElement&&this.treeElement.children)||[]);for(var i=0,n=sidebarElements.length;i<n;++i){var header=(sidebarElements[i].profile);if(!header)
continue;if(frameId&&frameId!==header.frameId())
continue;if(traceLogId&&traceLogId!==header.traceLogId())
continue;sidebarElementsToDelete.push(sidebarElements[i]);}
for(var i=0,n=sidebarElementsToDelete.length;i<n;++i)
sidebarElementsToDelete[i].ondelete();},_selectedFrameId:function()
{var option=this._frameSelector.selectedOption();return option?option.value:undefined;},_dispatchViewUpdatedEvent:function()
{this._frameSelector.element.enableStyleClass("hidden",this._frameSelector.size()<=1);this.dispatchEventToListeners(WebInspector.ProfileType.Events.ViewUpdated);},isInstantProfile:function()
{return this._isSingleFrameMode();},isEnabled:function()
{return this._canvasAgentEnabled;},__proto__:WebInspector.ProfileType.prototype}
WebInspector.CanvasDispatcher=function(profileType)
{this._profileType=profileType;InspectorBackend.registerCanvasDispatcher(this);}
WebInspector.CanvasDispatcher.prototype={contextCreated:function(frameId)
{this._profileType._contextCreated(frameId);},traceLogsRemoved:function(frameId,traceLogId)
{this._profileType._traceLogsRemoved(frameId,traceLogId);}}
WebInspector.CanvasProfileHeader=function(type,title,uid,traceLogId,frameId)
{WebInspector.ProfileHeader.call(this,type,title,uid);this._traceLogId=traceLogId||"";this._frameId=frameId;this._alive=true;this._traceLogSize=0;this._traceLogPlayer=traceLogId?new WebInspector.CanvasTraceLogPlayerProxy(traceLogId):null;}
WebInspector.CanvasProfileHeader.prototype={traceLogId:function()
{return this._traceLogId;},traceLogPlayer:function()
{return this._traceLogPlayer;},frameId:function()
{return this._frameId;},createSidebarTreeElement:function()
{return new WebInspector.ProfileSidebarTreeElement(this,WebInspector.UIString("Trace Log %d"),"profile-sidebar-tree-item");},createView:function(profilesPanel)
{return new WebInspector.CanvasProfileView(this);},dispose:function()
{if(this._traceLogPlayer)
this._traceLogPlayer.dispose();clearTimeout(this._requestStatusTimer);this._alive=false;},_updateCapturingStatus:function(traceLog)
{if(!this.sidebarElement||!this._traceLogId)
return;if(traceLog){this._alive=traceLog.alive;this._traceLogSize=traceLog.totalAvailableCalls;}
this.sidebarElement.subtitle=this._alive?WebInspector.UIString("Capturing\u2026 %d calls",this._traceLogSize):WebInspector.UIString("Captured %d calls",this._traceLogSize);this.sidebarElement.wait=this._alive;if(this._alive){clearTimeout(this._requestStatusTimer);this._requestStatusTimer=setTimeout(this._requestCapturingStatus.bind(this),WebInspector.CanvasProfileView.TraceLogPollingInterval);}},_requestCapturingStatus:function()
{function didReceiveTraceLog(traceLog)
{if(!traceLog)
return;this._alive=traceLog.alive;this._traceLogSize=traceLog.totalAvailableCalls;this._updateCapturingStatus();}
this._traceLogPlayer.getTraceLog(0,0,didReceiveTraceLog.bind(this));},__proto__:WebInspector.ProfileHeader.prototype}
WebInspector.CanvasProfileDataGridHelper={createCallArgumentElement:function(callArgument)
{if(callArgument.enumName)
return WebInspector.CanvasProfileDataGridHelper.createEnumValueElement(callArgument.enumName,+callArgument.description);var element=document.createElement("span");element.className="canvas-call-argument";var description=callArgument.description;if(callArgument.type==="string"){const maxStringLength=150;element.createTextChild("\"");element.createChild("span","canvas-formatted-string").textContent=description.trimMiddle(maxStringLength);element.createTextChild("\"");element.__suppressPopover=(description.length<=maxStringLength&&!/[\r\n]/.test(description));if(!element.__suppressPopover)
element.__evalResult=WebInspector.RemoteObject.fromPrimitiveValue(description);}else{var type=callArgument.subtype||callArgument.type;if(type){element.addStyleClass("canvas-formatted-"+type);if(["null","undefined","boolean","number"].indexOf(type)>=0)
element.__suppressPopover=true;}
element.textContent=description;if(callArgument.remoteObject)
element.__evalResult=WebInspector.RemoteObject.fromPayload(callArgument.remoteObject);}
if(callArgument.resourceId){element.addStyleClass("canvas-formatted-resource");element.__resourceId=callArgument.resourceId;}
return element;},createEnumValueElement:function(enumName,enumValue)
{var element=document.createElement("span");element.className="canvas-call-argument canvas-formatted-number";element.textContent=enumName;element.__evalResult=WebInspector.RemoteObject.fromPrimitiveValue(enumValue);return element;}}
WebInspector.CanvasTraceLogPlayerProxy=function(traceLogId)
{this._traceLogId=traceLogId;this._currentResourceStates={};this._defaultResourceId=null;}
WebInspector.CanvasTraceLogPlayerProxy.Events={CanvasTraceLogReceived:"CanvasTraceLogReceived",CanvasReplayStateChanged:"CanvasReplayStateChanged",CanvasResourceStateReceived:"CanvasResourceStateReceived",}
WebInspector.CanvasTraceLogPlayerProxy.prototype={getTraceLog:function(startOffset,maxLength,userCallback)
{function callback(error,traceLog)
{if(error||!traceLog){userCallback(null);return;}
userCallback(traceLog);this.dispatchEventToListeners(WebInspector.CanvasTraceLogPlayerProxy.Events.CanvasTraceLogReceived,traceLog);}
CanvasAgent.getTraceLog(this._traceLogId,startOffset,maxLength,callback.bind(this));},dispose:function()
{this._currentResourceStates={};CanvasAgent.dropTraceLog(this._traceLogId);this.dispatchEventToListeners(WebInspector.CanvasTraceLogPlayerProxy.Events.CanvasReplayStateChanged);},getResourceState:function(resourceId,userCallback)
{resourceId=resourceId||this._defaultResourceId;if(!resourceId){userCallback(null);return;}
if(this._currentResourceStates[resourceId]){userCallback(this._currentResourceStates[resourceId]);return;}
function callback(error,resourceState)
{if(error||!resourceState){userCallback(null);return;}
this._currentResourceStates[resourceId]=resourceState;userCallback(resourceState);this.dispatchEventToListeners(WebInspector.CanvasTraceLogPlayerProxy.Events.CanvasResourceStateReceived,resourceState);}
CanvasAgent.getResourceState(this._traceLogId,resourceId,callback.bind(this));},replayTraceLog:function(index,userCallback)
{function callback(error,resourceState,replayTime)
{this._currentResourceStates={};if(error||!resourceState){resourceState=null;userCallback(null,replayTime);}else{this._defaultResourceId=resourceState.id;this._currentResourceStates[resourceState.id]=resourceState;userCallback(resourceState,replayTime);}
this.dispatchEventToListeners(WebInspector.CanvasTraceLogPlayerProxy.Events.CanvasReplayStateChanged);if(resourceState)
this.dispatchEventToListeners(WebInspector.CanvasTraceLogPlayerProxy.Events.CanvasResourceStateReceived,resourceState);}
CanvasAgent.replayTraceLog(this._traceLogId,index,callback.bind(this));},clearResourceStates:function()
{this._currentResourceStates={};this.dispatchEventToListeners(WebInspector.CanvasTraceLogPlayerProxy.Events.CanvasReplayStateChanged);},__proto__:WebInspector.Object.prototype};WebInspector.CanvasReplayStateView=function(traceLogPlayer)
{WebInspector.View.call(this);this.registerRequiredCSS("canvasProfiler.css");this.element.addStyleClass("canvas-replay-state-view");this._traceLogPlayer=traceLogPlayer;var controlsContainer=this.element.createChild("div","status-bar");this._prevButton=this._createControlButton(controlsContainer,"canvas-replay-state-prev",WebInspector.UIString("Previous resource."),this._onResourceNavigationClick.bind(this,false));this._nextButton=this._createControlButton(controlsContainer,"canvas-replay-state-next",WebInspector.UIString("Next resource."),this._onResourceNavigationClick.bind(this,true));this._createControlButton(controlsContainer,"canvas-replay-state-refresh",WebInspector.UIString("Refresh."),this._onStateRefreshClick.bind(this));this._resourceSelector=new WebInspector.StatusBarComboBox(this._onReplayResourceChanged.bind(this));this._currentOption=this._resourceSelector.createOption(WebInspector.UIString("<auto>"),WebInspector.UIString("Show state of the last replayed resource."),"");controlsContainer.appendChild(this._resourceSelector.element);this._resourceIdToDescription={};this._gridNodesExpandedState={};this._gridScrollPositions={};this._currentResourceId=null;this._prevOptionsStack=[];this._nextOptionsStack=[];this._highlightedGridNodes=[];var columns=[{title:WebInspector.UIString("Name"),sortable:false,width:"50%",disclosure:true},{title:WebInspector.UIString("Value"),sortable:false,width:"50%"}];this._stateGrid=new WebInspector.DataGrid(columns);this._stateGrid.element.addStyleClass("fill");this._stateGrid.show(this.element);this._traceLogPlayer.addEventListener(WebInspector.CanvasTraceLogPlayerProxy.Events.CanvasReplayStateChanged,this._onReplayResourceChanged,this);this._traceLogPlayer.addEventListener(WebInspector.CanvasTraceLogPlayerProxy.Events.CanvasTraceLogReceived,this._onCanvasTraceLogReceived,this);this._traceLogPlayer.addEventListener(WebInspector.CanvasTraceLogPlayerProxy.Events.CanvasResourceStateReceived,this._onCanvasResourceStateReceived,this);this._updateButtonsEnabledState();}
WebInspector.CanvasReplayStateView.prototype={selectResource:function(resourceId)
{if(resourceId===this._resourceSelector.selectedOption().value)
return;var option=this._resourceSelector.selectElement().firstChild;for(var index=0;option;++index,option=option.nextSibling){if(resourceId===option.value){this._resourceSelector.setSelectedIndex(index);this._onReplayResourceChanged();break;}}},_createControlButton:function(parent,className,title,clickCallback)
{var button=new WebInspector.StatusBarButton(title,className+" canvas-replay-button");parent.appendChild(button.element);button.makeLongClickEnabled();button.addEventListener("click",clickCallback,this);button.addEventListener("longClickDown",clickCallback,this);button.addEventListener("longClickPress",clickCallback,this);return button;},_onResourceNavigationClick:function(forward)
{var newOption=forward?this._nextOptionsStack.pop():this._prevOptionsStack.pop();if(!newOption)
return;(forward?this._prevOptionsStack:this._nextOptionsStack).push(this._currentOption);this._isNavigationButton=true;this.selectResource(newOption.value);delete this._isNavigationButton;this._updateButtonsEnabledState();},_onStateRefreshClick:function()
{this._traceLogPlayer.clearResourceStates();},_updateButtonsEnabledState:function()
{this._prevButton.setEnabled(this._prevOptionsStack.length>0);this._nextButton.setEnabled(this._nextOptionsStack.length>0);},_updateCurrentOption:function()
{const maxStackSize=256;var selectedOption=this._resourceSelector.selectedOption();if(this._currentOption===selectedOption)
return;if(!this._isNavigationButton){this._prevOptionsStack.push(this._currentOption);this._nextOptionsStack=[];if(this._prevOptionsStack.length>maxStackSize)
this._prevOptionsStack.shift();this._updateButtonsEnabledState();}
this._currentOption=selectedOption;},_collectResourcesFromTraceLog:function(traceLog)
{var collectedResources=[];var calls=traceLog.calls;for(var i=0,n=calls.length;i<n;++i){var call=calls[i];var args=call.arguments||[];for(var j=0;j<args.length;++j)
this._collectResourceFromCallArgument(args[j],collectedResources);this._collectResourceFromCallArgument(call.result,collectedResources);this._collectResourceFromCallArgument(call.value,collectedResources);}
var contexts=traceLog.contexts;for(var i=0,n=contexts.length;i<n;++i)
this._collectResourceFromCallArgument(contexts[i],collectedResources);this._addCollectedResourcesToSelector(collectedResources);},_collectResourcesFromResourceState:function(resourceState)
{var collectedResources=[];this._collectResourceFromResourceStateDescriptors(resourceState.descriptors,collectedResources);this._addCollectedResourcesToSelector(collectedResources);},_collectResourceFromResourceStateDescriptors:function(descriptors,output)
{if(!descriptors)
return;for(var i=0,n=descriptors.length;i<n;++i){var descriptor=descriptors[i];this._collectResourceFromCallArgument(descriptor.value,output);this._collectResourceFromResourceStateDescriptors(descriptor.values,output);}},_collectResourceFromCallArgument:function(argument,output)
{if(!argument)
return;var resourceId=argument.resourceId;if(!resourceId||this._resourceIdToDescription[resourceId])
return;this._resourceIdToDescription[resourceId]=argument.description;output.push(argument);},_addCollectedResourcesToSelector:function(collectedResources)
{if(!collectedResources.length)
return;function comparator(arg1,arg2)
{var a=arg1.description;var b=arg2.description;return String.naturalOrderComparator(a,b);}
collectedResources.sort(comparator);var selectElement=this._resourceSelector.selectElement();var currentOption=selectElement.firstChild;currentOption=currentOption.nextSibling;for(var i=0,n=collectedResources.length;i<n;++i){var argument=collectedResources[i];while(currentOption&&String.naturalOrderComparator(currentOption.text,argument.description)<0)
currentOption=currentOption.nextSibling;var option=this._resourceSelector.createOption(argument.description,WebInspector.UIString("Show state of this resource."),argument.resourceId);if(currentOption)
selectElement.insertBefore(option,currentOption);}},_onReplayResourceChanged:function()
{this._updateCurrentOption();var selectedResourceId=this._resourceSelector.selectedOption().value;function didReceiveResourceState(resourceState)
{if(selectedResourceId!==this._resourceSelector.selectedOption().value)
return;this._showResourceState(resourceState);}
this._traceLogPlayer.getResourceState(selectedResourceId,didReceiveResourceState.bind(this));},_onCanvasTraceLogReceived:function(event)
{var traceLog=(event.data);if(traceLog)
this._collectResourcesFromTraceLog(traceLog);},_onCanvasResourceStateReceived:function(event)
{var resourceState=(event.data);if(resourceState)
this._collectResourcesFromResourceState(resourceState);},_showResourceState:function(resourceState)
{this._saveExpandedState();this._saveScrollState();var rootNode=this._stateGrid.rootNode();if(!resourceState){this._currentResourceId=null;this._updateDataGridHighlights([]);rootNode.removeChildren();return;}
var nodesToHighlight=[];var nameToOldGridNodes={};function populateNameToNodesMap(map,node)
{if(!node)
return;for(var i=0,child;child=node.children[i];++i){var item={node:child,children:{}};map[child.name]=item;populateNameToNodesMap(item.children,child);}}
populateNameToNodesMap(nameToOldGridNodes,rootNode);rootNode.removeChildren();function comparator(d1,d2)
{var hasChildren1=!!d1.values;var hasChildren2=!!d2.values;if(hasChildren1!==hasChildren2)
return hasChildren1?1:-1;return String.naturalOrderComparator(d1.name,d2.name);}
function appendResourceStateDescriptors(descriptors,parent,nameToOldChildren)
{descriptors=descriptors||[];descriptors.sort(comparator);var oldChildren=nameToOldChildren||{};for(var i=0,n=descriptors.length;i<n;++i){var descriptor=descriptors[i];var childNode=this._createDataGridNode(descriptor);parent.appendChild(childNode);var oldChildrenItem=oldChildren[childNode.name]||{};var oldChildNode=oldChildrenItem.node;if(!oldChildNode||oldChildNode.element.textContent!==childNode.element.textContent)
nodesToHighlight.push(childNode);appendResourceStateDescriptors.call(this,descriptor.values,childNode,oldChildrenItem.children);}}
appendResourceStateDescriptors.call(this,resourceState.descriptors,rootNode,nameToOldGridNodes);var shouldHighlightChanges=(this._resourceKindId(this._currentResourceId)===this._resourceKindId(resourceState.id));this._currentResourceId=resourceState.id;this._restoreExpandedState();this._updateDataGridHighlights(shouldHighlightChanges?nodesToHighlight:[]);this._restoreScrollState();},_updateDataGridHighlights:function(nodes)
{for(var i=0,n=this._highlightedGridNodes.length;i<n;++i){var node=this._highlightedGridNodes[i];node.element.removeStyleClass("canvas-grid-node-highlighted");}
this._highlightedGridNodes=nodes;for(var i=0,n=this._highlightedGridNodes.length;i<n;++i){var node=this._highlightedGridNodes[i];node.element.addStyleClass("canvas-grid-node-highlighted");node.reveal();}},_resourceKindId:function(resourceId)
{var description=(resourceId&&this._resourceIdToDescription[resourceId])||"";return description.replace(/\d+/g,"");},_forEachGridNode:function(callback)
{function processRecursively(node,key)
{for(var i=0,child;child=node.children[i];++i){var childKey=key+"#"+child.name;callback(child,childKey);processRecursively(child,childKey);}}
processRecursively(this._stateGrid.rootNode(),"");},_saveExpandedState:function()
{if(!this._currentResourceId)
return;var expandedState={};var key=this._resourceKindId(this._currentResourceId);this._gridNodesExpandedState[key]=expandedState;function callback(node,key)
{if(node.expanded)
expandedState[key]=true;}
this._forEachGridNode(callback);},_restoreExpandedState:function()
{if(!this._currentResourceId)
return;var key=this._resourceKindId(this._currentResourceId);var expandedState=this._gridNodesExpandedState[key];if(!expandedState)
return;function callback(node,key)
{if(expandedState[key])
node.expand();}
this._forEachGridNode(callback);},_saveScrollState:function()
{if(!this._currentResourceId)
return;var key=this._resourceKindId(this._currentResourceId);this._gridScrollPositions[key]={scrollTop:this._stateGrid.scrollContainer.scrollTop,scrollLeft:this._stateGrid.scrollContainer.scrollLeft};},_restoreScrollState:function()
{if(!this._currentResourceId)
return;var key=this._resourceKindId(this._currentResourceId);var scrollState=this._gridScrollPositions[key];if(!scrollState)
return;this._stateGrid.scrollContainer.scrollTop=scrollState.scrollTop;this._stateGrid.scrollContainer.scrollLeft=scrollState.scrollLeft;},_createDataGridNode:function(descriptor)
{var name=descriptor.name;var callArgument=descriptor.value;var valueElement=callArgument?WebInspector.CanvasProfileDataGridHelper.createCallArgumentElement(callArgument):"";var nameElement=name;if(typeof descriptor.enumValueForName!=="undefined")
nameElement=WebInspector.CanvasProfileDataGridHelper.createEnumValueElement(name,+descriptor.enumValueForName);if(descriptor.isArray&&descriptor.values){if(typeof nameElement==="string")
nameElement+="["+descriptor.values.length+"]";else{var element=document.createElement("span");element.appendChild(nameElement);element.createTextChild("["+descriptor.values.length+"]");nameElement=element;}}
var data={};data[0]=nameElement;data[1]=valueElement;var node=new WebInspector.DataGridNode(data);node.selectable=false;node.name=name;return node;},__proto__:WebInspector.View.prototype};