WebInspector.LayerTreeModel=function()
{WebInspector.Object.call(this);this._layersById={};this._lastPaintRectByLayerId={};InspectorBackend.registerLayerTreeDispatcher(new WebInspector.LayerTreeDispatcher(this));WebInspector.domAgent.addEventListener(WebInspector.DOMAgent.Events.DocumentUpdated,this._onDocumentUpdated,this);}
WebInspector.LayerTreeModel.Events={LayerTreeChanged:"LayerTreeChanged",LayerPainted:"LayerPainted",}
WebInspector.LayerTreeModel.prototype={disable:function()
{if(!this._enabled)
return;this._enabled=false;LayerTreeAgent.disable();},enable:function(callback)
{if(this._enabled)
return;this._enabled=true;WebInspector.domAgent.requestDocument(onDocumentAvailable.bind(this));function onDocumentAvailable()
{if(!this._enabled)
return;LayerTreeAgent.enable();}},root:function()
{return this._root;},contentRoot:function()
{return this._contentRoot;},forEachLayer:function(callback,root)
{if(!root){root=this.root();if(!root)
return false;}
return callback(root)||root.children().some(this.forEachLayer.bind(this,callback));},layerById:function(id)
{return this._layersById[id];},_repopulate:function(payload)
{var oldLayersById=this._layersById;this._layersById={};for(var i=0;i<payload.length;++i){var layerId=payload[i].layerId;var layer=oldLayersById[layerId];if(layer)
layer._reset(payload[i]);else
layer=new WebInspector.Layer(payload[i]);this._layersById[layerId]=layer;var parentId=layer.parentId();if(!this._contentRoot&&layer.nodeId())
this._contentRoot=layer;var lastPaintRect=this._lastPaintRectByLayerId[layerId];if(lastPaintRect)
layer._lastPaintRect=lastPaintRect;if(parentId){var parent=this._layersById[parentId];if(!parent)
console.assert(parent,"missing parent "+parentId+" for layer "+layerId);parent.addChild(layer);}else{if(this._root)
console.assert(false,"Multiple root layers");this._root=layer;}}
this._lastPaintRectByLayerId={};},_layerTreeChanged:function(payload)
{this._root=null;this._contentRoot=null;if(payload)
this._repopulate(payload);this.dispatchEventToListeners(WebInspector.LayerTreeModel.Events.LayerTreeChanged);},_layerPainted:function(layerId,clipRect)
{var layer=this._layersById[layerId];if(!layer){this._lastPaintRectByLayerId[layerId]=clipRect;return;}
layer._didPaint(clipRect);this.dispatchEventToListeners(WebInspector.LayerTreeModel.Events.LayerPainted,layer);},_onDocumentUpdated:function()
{this.disable();this.enable();},__proto__:WebInspector.Object.prototype}
WebInspector.Layer=function(layerPayload)
{this._reset(layerPayload);}
WebInspector.Layer.prototype={id:function()
{return this._layerPayload.layerId;},parentId:function()
{return this._layerPayload.parentLayerId;},parent:function()
{return this._parent;},isRoot:function()
{return!this.parentId();},children:function()
{return this._children;},addChild:function(child)
{if(child._parent)
console.assert(false,"Child already has a parent");this._children.push(child);child._parent=this;},nodeId:function()
{return this._layerPayload.nodeId;},nodeIdForSelfOrAncestor:function()
{for(var layer=this;layer;layer=layer._parent){var nodeId=layer._layerPayload.nodeId;if(nodeId)
return nodeId;}
return null;},offsetX:function()
{return this._layerPayload.offsetX;},offsetY:function()
{return this._layerPayload.offsetY;},width:function()
{return this._layerPayload.width;},height:function()
{return this._layerPayload.height;},transform:function()
{return this._layerPayload.transform;},anchorPoint:function()
{return[this._layerPayload.anchorX||0,this._layerPayload.anchorY||0,this._layerPayload.anchorZ||0,];},invisible:function()
{return this._layerPayload.invisible;},paintCount:function()
{return this._paintCount||this._layerPayload.paintCount;},lastPaintRect:function()
{return this._lastPaintRect;},requestCompositingReasons:function(callback)
{function callbackWrapper(error,compositingReasons)
{if(error){console.error("LayerTreeAgent.reasonsForCompositingLayer(): "+error);callback(null);return;}
callback(compositingReasons);}
LayerTreeAgent.compositingReasons(this.id(),callbackWrapper.bind(this));},_didPaint:function(rect)
{this._lastPaintRect=rect;this._paintCount=this.paintCount()+1;},_reset:function(layerPayload)
{this._children=[];this._parent=null;this._paintCount=0;this._layerPayload=layerPayload;}}
WebInspector.LayerTreeDispatcher=function(layerTreeModel)
{this._layerTreeModel=layerTreeModel;}
WebInspector.LayerTreeDispatcher.prototype={layerTreeDidChange:function(payload)
{this._layerTreeModel._layerTreeChanged(payload);},layerPainted:function(layerId,clipRect)
{this._layerTreeModel._layerPainted(layerId,clipRect);}};WebInspector.LayerTree=function(model,treeOutline)
{WebInspector.Object.call(this);this._model=model;this._treeOutline=treeOutline;this._treeOutline.childrenListElement.addEventListener("mousemove",this._onMouseMove.bind(this),false);this._treeOutline.childrenListElement.addEventListener("mouseout",this._onMouseMove.bind(this),false);this._treeOutline.childrenListElement.addEventListener("contextmenu",this._onContextMenu.bind(this),true);this._model.addEventListener(WebInspector.LayerTreeModel.Events.LayerTreeChanged,this._update.bind(this));this._lastHoveredNode=null;}
WebInspector.LayerTree.Events={LayerHovered:"LayerHovered",LayerSelected:"LayerSelected"}
WebInspector.LayerTree.prototype={selectLayer:function(layer)
{this.hoverLayer(null);var node=layer&&this._treeOutline.getCachedTreeElement(layer);if(node)
node.revealAndSelect(true);else if(this._treeOutline.selectedTreeElement)
this._treeOutline.selectedTreeElement.deselect();},hoverLayer:function(layer)
{var node=layer&&this._treeOutline.getCachedTreeElement(layer);if(node===this._lastHoveredNode)
return;if(this._lastHoveredNode)
this._lastHoveredNode.setHovered(false);if(node)
node.setHovered(true);this._lastHoveredNode=node;},_update:function()
{var seenLayers={};function updateLayer(layer)
{var id=layer.id();if(seenLayers[id])
console.assert(false,"Duplicate layer id: "+id);seenLayers[id]=true;var node=this._treeOutline.getCachedTreeElement(layer);var parent=layer===this._model.contentRoot()?this._treeOutline:this._treeOutline.getCachedTreeElement(layer.parent());if(!parent)
console.assert(false,"Parent is not in the tree");if(!node){node=new WebInspector.LayerTreeElement(this,layer);parent.appendChild(node);}else{var oldParentId=node.parent.representedObject&&node.parent.representedObject.id();if(oldParentId!==layer.parentId()){(node.parent||this._treeOutline).removeChild(node);parent.appendChild(node);}
node._update();}}
if(this._model.contentRoot())
this._model.forEachLayer(updateLayer.bind(this),this._model.contentRoot());for(var node=(this._treeOutline.children[0]);node&&!node.root;){if(seenLayers[node.representedObject.id()]){node=node.traverseNextTreeElement(false);}else{var nextNode=node.nextSibling||node.parent;node.parent.removeChild(node);if(node===this._lastHoveredNode)
this._lastHoveredNode=null;node=nextNode;}}},_onMouseMove:function(event)
{var node=this._treeOutline.treeElementFromPoint(event.pageX,event.pageY);if(node===this._lastHoveredNode)
return;this.dispatchEventToListeners(WebInspector.LayerTree.Events.LayerHovered,node&&node.representedObject);},_selectedNodeChanged:function(node)
{var layer=(node.representedObject);this.dispatchEventToListeners(WebInspector.LayerTree.Events.LayerSelected,layer);},_onContextMenu:function(event)
{var node=this._treeOutline.treeElementFromPoint(event.pageX,event.pageY);if(!node||!node.representedObject)
return;var layer=(node.representedObject);if(!layer)
return;var nodeId=layer.nodeId();if(!nodeId)
return;var domNode=WebInspector.domAgent.nodeForId(nodeId);if(!domNode)
return;var contextMenu=new WebInspector.ContextMenu(event);contextMenu.appendApplicableItems(domNode);contextMenu.show();},__proto__:WebInspector.Object.prototype}
WebInspector.LayerTreeElement=function(tree,layer)
{TreeElement.call(this,"",layer);this._layerTree=tree;this._update();}
WebInspector.LayerTreeElement.prototype={onattach:function()
{var selection=document.createElement("div");selection.className="selection";this.listItemElement.insertBefore(selection,this.listItemElement.firstChild);},_update:function()
{var layer=(this.representedObject);var nodeId=layer.nodeIdForSelfOrAncestor();var node=nodeId&&WebInspector.domAgent.nodeForId(nodeId);var title=document.createDocumentFragment();title.createChild("div","selection");title.appendChild(document.createTextNode(node?node.appropriateSelectorFor(false):"#"+layer.id()));var details=title.createChild("span","dimmed");details.textContent=WebInspector.UIString(" (%d × %d)",layer.width(),layer.height());this.title=title;},onselect:function()
{this._layerTree._selectedNodeChanged(this);},setHovered:function(hovered)
{this.listItemElement.enableStyleClass("hovered",hovered);},__proto__:TreeElement.prototype};WebInspector.Layers3DView=function(model)
{WebInspector.View.call(this);this.element.classList.add("fill");this.element.classList.add("layers-3d-view");this._emptyView=new WebInspector.EmptyView(WebInspector.UIString("Not in the composited mode.\nConsider forcing composited mode in Settings."));this._model=model;this._model.addEventListener(WebInspector.LayerTreeModel.Events.LayerTreeChanged,this._update,this);this._model.addEventListener(WebInspector.LayerTreeModel.Events.LayerPainted,this._onLayerPainted,this);this._rotatingContainerElement=this.element.createChild("div","fill rotating-container");this.element.addEventListener("mousemove",this._onMouseMove.bind(this),false);this.element.addEventListener("mouseout",this._onMouseMove.bind(this),false);this.element.addEventListener("mousedown",this._onMouseDown.bind(this),false);this.element.addEventListener("mouseup",this._onMouseUp.bind(this),false);this.element.addEventListener("contextmenu",this._onContextMenu.bind(this),false);this.element.addEventListener("click",this._onClick.bind(this),false);this._elementsByLayerId={};this._rotateX=0;this._rotateY=0;this._scaleAdjustmentStylesheet=this.element.ownerDocument.head.createChild("style");this._scaleAdjustmentStylesheet.disabled=true;this._lastOutlinedElement={};WebInspector.settings.showPaintRects.addChangeListener(this._update,this);}
WebInspector.Layers3DView.OutlineType={Hovered:"hovered",Selected:"selected"}
WebInspector.Layers3DView.Events={LayerHovered:"LayerHovered",LayerSelected:"LayerSelected"}
WebInspector.Layers3DView.PaintRectColors=[WebInspector.Color.fromRGBA([0xFF,0,0]),WebInspector.Color.fromRGBA([0xFF,0,0xFF]),WebInspector.Color.fromRGBA([0,0,0xFF])]
WebInspector.Layers3DView.prototype={onResize:function()
{this._update();},willHide:function()
{this._scaleAdjustmentStylesheet.disabled=true;},wasShown:function()
{this._scaleAdjustmentStylesheet.disabled=false;if(this._needsUpdate)
this._update();},_setOutline:function(type,layer)
{var element=layer?this._elementForLayer(layer):null;var previousElement=this._lastOutlinedElement[type];if(previousElement===element)
return;this._lastOutlinedElement[type]=element;if(previousElement){previousElement.removeStyleClass(type);this._updateElementColor(previousElement);}
if(element){element.addStyleClass(type);this._updateElementColor(element);}},hoverLayer:function(layer)
{this._setOutline(WebInspector.Layers3DView.OutlineType.Hovered,layer);},selectLayer:function(layer)
{this._setOutline(WebInspector.Layers3DView.OutlineType.Hovered,null);this._setOutline(WebInspector.Layers3DView.OutlineType.Selected,layer);},_scaleToFit:function()
{var root=this._model.contentRoot();if(!root)
return;const padding=40;var scaleX=this._clientWidth/(root.width()+2*padding);var scaleY=this._clientHeight/(root.height()+2*padding);this._scale=Math.min(scaleX,scaleY);const screenLayerSpacing=20;this._layerSpacing=Math.ceil(screenLayerSpacing/this._scale)+"px";const screenLayerThickness=4;var layerThickness=Math.ceil(screenLayerThickness/this._scale)+"px";var stylesheetContent=".layer-container .side-wall { height: "+layerThickness+"; width: "+layerThickness+"; } "+".layer-container .back-wall { -webkit-transform: translateZ(-"+layerThickness+"); } "+".layer-container { -webkit-transform: translateZ("+this._layerSpacing+"); }";var stylesheetTextNode=this._scaleAdjustmentStylesheet.firstChild;if(!stylesheetTextNode||stylesheetTextNode.nodeType!==Node.TEXT_NODE||stylesheetTextNode.nextSibling)
this._scaleAdjustmentStylesheet.textContent=stylesheetContent;else
stylesheetTextNode.nodeValue=stylesheetContent;var element=this._elementForLayer(root);element.style.webkitTransform="scale3d("+this._scale+","+this._scale+","+this._scale+")";element.style.webkitTransformOrigin="";element.style.left=((this._clientWidth-root.width()*this._scale)>>1)+"px";element.style.top=((this._clientHeight-root.height()*this._scale)>>1)+"px";},_update:function()
{if(!this.isShowing()){this._needsUpdate=true;return;}
if(!this._model.contentRoot()){this._emptyView.show(this.element);this._rotatingContainerElement.removeChildren();return;}
this._emptyView.detach();function updateLayer(layer)
{this._updateLayerElement(this._elementForLayer(layer));}
this._clientWidth=this.element.clientWidth;this._clientHeight=this.element.clientHeight;for(var layerId in this._elementsByLayerId){if(this._model.layerById(layerId))
continue;this._elementsByLayerId[layerId].remove();delete this._elementsByLayerId[layerId];}
this._scaleToFit();this._model.forEachLayer(updateLayer.bind(this),this._model.contentRoot());this._needsUpdate=false;},_onLayerPainted:function(event)
{var layer=(event.data);this._updatePaintRect(this._elementForLayer(layer));},_elementForLayer:function(layer)
{var element=this._elementsByLayerId[layer.id()];if(element){element.__layerDetails.layer=layer;return element;}
element=document.createElement("div");element.className="layer-container";["fill back-wall","side-wall top","side-wall right","side-wall bottom","side-wall left"].forEach(element.createChild.bind(element,"div"));element.__layerDetails=new WebInspector.LayerDetails(layer,element.createChild("div","paint-rect"));this._elementsByLayerId[layer.id()]=element;return element;},_updateLayerElement:function(element)
{var layer=element.__layerDetails.layer;var style=element.style;var isContentRoot=layer===this._model.contentRoot();var parentElement=isContentRoot?this._rotatingContainerElement:this._elementForLayer(layer.parent());element.__layerDetails.depth=parentElement.__layerDetails?parentElement.__layerDetails.depth+1:0;element.enableStyleClass("invisible",layer.invisible());this._updateElementColor(element);if(parentElement!==element.parentElement)
parentElement.appendChild(element);style.width=layer.width()+"px";style.height=layer.height()+"px";this._updatePaintRect(element);if(isContentRoot)
return;style.left=layer.offsetX()+"px";style.top=layer.offsetY()+"px";var transform=layer.transform();if(transform){function toFixed5(x)
{return x.toFixed(5);}
style.webkitTransform="matrix3d("+transform.map(toFixed5).join(",")+") translateZ("+this._layerSpacing+")";var anchor=layer.anchorPoint();style.webkitTransformOrigin=Math.round(anchor[0]*100)+"% "+Math.round(anchor[1]*100)+"% "+anchor[2];}else{style.webkitTransform="";style.webkitTransformOrigin="";}},_updatePaintRect:function(element)
{var details=element.__layerDetails;var paintRect=details.layer.lastPaintRect();var paintRectElement=details.paintRectElement;if(!paintRect||!WebInspector.settings.showPaintRects.get()){paintRectElement.addStyleClass("hidden");return;}
paintRectElement.removeStyleClass("hidden");if(details.paintCount===details.layer.paintCount())
return;details.paintCount=details.layer.paintCount();var style=paintRectElement.style;style.left=paintRect.x+"px";style.top=paintRect.y+"px";style.width=paintRect.width+"px";style.height=paintRect.height+"px";var color=WebInspector.Layers3DView.PaintRectColors[details.paintCount%WebInspector.Layers3DView.PaintRectColors.length];style.borderWidth=Math.ceil(1/this._scale)+"px";style.borderColor=color.toString(WebInspector.Color.Format.RGBA);},_updateElementColor:function(element)
{var color;if(element===this._lastOutlinedElement[WebInspector.Layers3DView.OutlineType.Selected])
color=WebInspector.Color.PageHighlight.Content.toString(WebInspector.Color.Format.RGBA)||"";else{const base=144;var component=base+20*((element.__layerDetails.depth-1)%5);color="rgba("+component+","+component+","+component+", 0.8)";}
element.style.backgroundColor=color;},_onMouseDown:function(event)
{if(event.which!==1)
return;this._setReferencePoint(event);},_setReferencePoint:function(event)
{this._originX=event.clientX;this._originY=event.clientY;this._oldRotateX=this._rotateX;this._oldRotateY=this._rotateY;},_resetReferencePoint:function()
{delete this._originX;delete this._originY;delete this._oldRotateX;delete this._oldRotateY;},_onMouseUp:function(event)
{if(event.which!==1)
return;this._resetReferencePoint();},_layerFromEventPoint:function(event)
{var element=this.element.ownerDocument.elementFromPoint(event.pageX,event.pageY);if(!element)
return null;element=element.enclosingNodeOrSelfWithClass("layer-container");return element&&element.__layerDetails&&element.__layerDetails.layer;},_onMouseMove:function(event)
{if(!event.which){this.dispatchEventToListeners(WebInspector.Layers3DView.Events.LayerHovered,this._layerFromEventPoint(event));return;}
if(event.which===1){if(typeof this._originX!=="number")
this._setReferencePoint(event);this._rotateX=this._oldRotateX+(this._originY-event.clientY)/2;this._rotateY=this._oldRotateY-(this._originX-event.clientX)/4;this._rotatingContainerElement.style.webkitTransform="translateZ(10000px) rotateX("+this._rotateX+"deg) rotateY("+this._rotateY+"deg)";}},_onContextMenu:function(event)
{var layer=this._layerFromEventPoint(event);var nodeId=layer&&layer.nodeId();if(!nodeId)
return;var domNode=WebInspector.domAgent.nodeForId(nodeId);if(!domNode)
return;var contextMenu=new WebInspector.ContextMenu(event);contextMenu.appendApplicableItems(domNode);contextMenu.show();},_onClick:function(event)
{this.dispatchEventToListeners(WebInspector.Layers3DView.Events.LayerSelected,this._layerFromEventPoint(event));},__proto__:WebInspector.View.prototype}
WebInspector.LayerDetails=function(layer,paintRectElement)
{this.layer=layer;this.depth=0;this.paintRectElement=paintRectElement;this.paintCount=0;};WebInspector.LayerDetailsView=function()
{WebInspector.View.call(this);this.element.classList.add("fill");this.element.classList.add("layer-details-view");this._emptyView=new WebInspector.EmptyView(WebInspector.UIString("Select a layer to see its details"));this._createTable();this.showLayer(null);}
WebInspector.LayerDetailsView.CompositingReasonDetail={"transform3D":WebInspector.UIString("Composition due to association with an element with a CSS 3D transform."),"video":WebInspector.UIString("Composition due to association with a <video> element."),"canvas":WebInspector.UIString("Composition due to the element being a <canvas> element."),"plugin":WebInspector.UIString("Composition due to association with a plugin."),"iFrame":WebInspector.UIString("Composition due to association with an <iframe> element."),"backfaceVisibilityHidden":WebInspector.UIString("Composition due to association with an element with a \"backface-visibility: hidden\" style."),"animation":WebInspector.UIString("Composition due to association with an animated element."),"filters":WebInspector.UIString("Composition due to association with an element with CSS filters applied."),"positionFixed":WebInspector.UIString("Composition due to association with an element with a \"position: fixed\" style."),"positionSticky":WebInspector.UIString("Composition due to association with an element with a \"position: sticky\" style."),"overflowScrollingTouch":WebInspector.UIString("Composition due to association with an element with a \"overflow-scrolling: touch\" style."),"blending":WebInspector.UIString("Composition due to association with an element that has blend mode other than \"normal\"."),"assumedOverlap":WebInspector.UIString("Composition due to association with an element that may overlap other composited elements."),"overlap":WebInspector.UIString("Composition due to association with an element overlapping other composited elements."),"negativeZIndexChildren":WebInspector.UIString("Composition due to association with an element with descendants that have a negative z-index."),"transformWithCompositedDescendants":WebInspector.UIString("Composition due to association with an element with composited descendants."),"opacityWithCompositedDescendants":WebInspector.UIString("Composition due to association with an element with opacity applied and composited descendants."),"maskWithCompositedDescendants":WebInspector.UIString("Composition due to association with a masked element and composited descendants."),"reflectionWithCompositedDescendants":WebInspector.UIString("Composition due to association with an element with a reflection and composited descendants."),"filterWithCompositedDescendants":WebInspector.UIString("Composition due to association with an element with CSS filters applied and composited descendants."),"blendingWithCompositedDescendants":WebInspector.UIString("Composition due to association with an element with CSS blending applied and composited descendants."),"clipsCompositingDescendants":WebInspector.UIString("Composition due to association with an element clipping compositing descendants."),"perspective":WebInspector.UIString("Composition due to association with an element with perspective applied."),"preserve3D":WebInspector.UIString("Composition due to association with an element with a \"transform-style: preserve-3d\" style."),"root":WebInspector.UIString("Root layer."),"layerForClip":WebInspector.UIString("Layer for clip."),"layerForScrollbar":WebInspector.UIString("Layer for scrollbar."),"layerForScrollingContainer":WebInspector.UIString("Layer for scrolling container."),"layerForForeground":WebInspector.UIString("Layer for foreground."),"layerForBackground":WebInspector.UIString("Layer for background."),"layerForMask":WebInspector.UIString("Layer for mask."),"layerForVideoOverlay":WebInspector.UIString("Layer for video overlay.")};WebInspector.LayerDetailsView.prototype={showLayer:function(layer)
{if(!layer){this._tableElement.remove();this._emptyView.show(this.element);return;}
this._emptyView.detach();this.element.appendChild(this._tableElement);this._positionCell.textContent=WebInspector.UIString("%d,%d",layer.offsetX(),layer.offsetY());this._sizeCell.textContent=WebInspector.UIString("%d × %d",layer.width(),layer.height());this._paintCountCell.textContent=layer.paintCount();const bytesPerPixel=4;this._memoryEstimateCell.textContent=Number.bytesToString(layer.invisible()?0:layer.width()*layer.height()*bytesPerPixel);layer.requestCompositingReasons(this._updateCompositingReasons.bind(this));},_createTable:function()
{this._tableElement=this.element.createChild("table");this._tbodyElement=this._tableElement.createChild("tbody");this._positionCell=this._createRow(WebInspector.UIString("Position in parent:"));this._sizeCell=this._createRow(WebInspector.UIString("Size:"));this._compositingReasonsCell=this._createRow(WebInspector.UIString("Compositing Reasons:"));this._memoryEstimateCell=this._createRow(WebInspector.UIString("Memory estimate:"));this._paintCountCell=this._createRow(WebInspector.UIString("Paint count:"));},_createRow:function(title)
{var tr=this._tbodyElement.createChild("tr");var titleCell=tr.createChild("td");titleCell.textContent=title;return tr.createChild("td");},_updateCompositingReasons:function(compositingReasons)
{if(!compositingReasons||!compositingReasons.length){this._compositingReasonsCell.textContent="n/a";return;}
var fragment=document.createDocumentFragment();for(var i=0;i<compositingReasons.length;++i){if(i)
fragment.appendChild(document.createTextNode(","));var span=document.createElement("span");span.title=WebInspector.LayerDetailsView.CompositingReasonDetail[compositingReasons[i]]||"";span.textContent=compositingReasons[i];fragment.appendChild(span);}
this._compositingReasonsCell.removeChildren();this._compositingReasonsCell.appendChild(fragment);},updatePaintCount:function(paintCount)
{this._paintCountCell.textContent=paintCount;},__proto__:WebInspector.View.prototype};WebInspector.LayersPanel=function()
{WebInspector.Panel.call(this,"layers");this.registerRequiredCSS("layersPanel.css");const initialLayerTreeSidebarWidth=225;const minimumMainWidthPercent=0.5;this.createSidebarViewWithTree();this.sidebarElement.addStyleClass("outline-disclosure");this.sidebarTreeElement.removeStyleClass("sidebar-tree");this._model=new WebInspector.LayerTreeModel();this._model.addEventListener(WebInspector.LayerTreeModel.Events.LayerTreeChanged,this._onLayerTreeUpdated,this);this._model.addEventListener(WebInspector.LayerTreeModel.Events.LayerPainted,this._onLayerPainted,this);this._currentlySelectedLayer=null;this._currentlyHoveredLayer=null;this._layerTree=new WebInspector.LayerTree(this._model,this.sidebarTree);this._layerTree.addEventListener(WebInspector.LayerTree.Events.LayerSelected,this._onLayerSelected,this);this._layerTree.addEventListener(WebInspector.LayerTree.Events.LayerHovered,this._onLayerHovered,this);this._layerDetailsSplitView=new WebInspector.SplitView(false,"layerDetailsSplitView");this._layerDetailsSplitView.show(this.splitView.mainElement);this._layers3DView=new WebInspector.Layers3DView(this._model);this._layers3DView.show(this._layerDetailsSplitView.firstElement());this._layers3DView.addEventListener(WebInspector.Layers3DView.Events.LayerSelected,this._onLayerSelected,this);this._layers3DView.addEventListener(WebInspector.Layers3DView.Events.LayerHovered,this._onLayerHovered,this);this._layerDetailsView=new WebInspector.LayerDetailsView();this._layerDetailsView.show(this._layerDetailsSplitView.secondElement());}
WebInspector.LayersPanel.prototype={wasShown:function()
{WebInspector.Panel.prototype.wasShown.call(this);this.sidebarTreeElement.focus();this._model.enable();},willHide:function()
{this._model.disable();WebInspector.Panel.prototype.willHide.call(this);},_onLayerTreeUpdated:function()
{if(this._currentlySelectedLayer&&!this._model.layerById(this._currentlySelectedLayer.id()))
this._selectLayer(null);if(this._currentlyHoveredLayer&&!this._model.layerById(this._currentlyHoveredLayer.id()))
this._hoverLayer(null);if(this._currentlySelectedLayer)
this._layerDetailsView.showLayer(this._currentlySelectedLayer);},_onLayerPainted:function(event)
{var layer=(event.data);if(this._currentlySelectedLayer===layer)
this._layerDetailsView.updatePaintCount(this._currentlySelectedLayer.paintCount());},_onLayerSelected:function(event)
{var layer=(event.data);this._selectLayer(layer);},_onLayerHovered:function(event)
{var layer=(event.data);this._hoverLayer(layer);},_selectLayer:function(layer)
{if(this._currentlySelectedLayer===layer)
return;this._currentlySelectedLayer=layer;var nodeId=layer&&layer.nodeIdForSelfOrAncestor();if(nodeId)
WebInspector.domAgent.highlightDOMNodeForTwoSeconds(nodeId);else
WebInspector.domAgent.hideDOMNodeHighlight();this._layerTree.selectLayer(layer);this._layers3DView.selectLayer(layer);this._layerDetailsView.showLayer(layer);},_hoverLayer:function(layer)
{if(this._currentlyHoveredLayer===layer)
return;this._currentlyHoveredLayer=layer;var nodeId=layer&&layer.nodeIdForSelfOrAncestor();if(nodeId)
WebInspector.domAgent.highlightDOMNode(nodeId);else
WebInspector.domAgent.hideDOMNodeHighlight();this._layerTree.hoverLayer(layer);this._layers3DView.hoverLayer(layer);},__proto__:WebInspector.Panel.prototype}