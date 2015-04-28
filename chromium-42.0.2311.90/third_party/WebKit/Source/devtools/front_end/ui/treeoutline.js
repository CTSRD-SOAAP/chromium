/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @constructor
 */
function TreeContainerNode()
{
    /** @type {?Array.<!TreeElement>} */
    this._children = null;
}

TreeContainerNode.prototype = {
    /**
     * @return {!Array.<!TreeElement>}
     */
    get children()
    {
        return this._children || [];
    },

    /**
     * @return {?TreeElement}
     */
    firstChild: function()
    {
        return this._children ? this._children[0] : null;
    },

    /**
     * @param {!TreeElement} child
     */
    appendChild: function(child)
    {
        if (!this._children)
            this._children = [];

        var insertionIndex;
        if (this.treeOutline && this.treeOutline._comparator)
            insertionIndex = insertionIndexForObjectInListSortedByFunction(child, this._children, this.treeOutline._comparator);
        else
            insertionIndex = this._children.length;
        this.insertChild(child, insertionIndex);
    },

    /**
     * @param {!TreeElement} child
     * @param {!TreeElement} beforeChild
     */
    insertBeforeChild: function(child, beforeChild)
    {
        if (!this._children)
            this._children = [];

        if (!child)
            throw("child can't be undefined or null");

        if (!beforeChild)
            throw("beforeChild can't be undefined or null");

        var childIndex = this._children.indexOf(beforeChild);
        if (childIndex === -1)
            throw("beforeChild not found in this node's children");

        this.insertChild(child, childIndex);
    },

    /**
     * @param {!TreeElement} child
     * @param {number} index
     */
    insertChild: function(child, index)
    {
        if (!this._children)
            this._children = [];

        if (!child)
            throw("child can't be undefined or null");

        console.assert(!child.parent, "Attempting to insert a child that is already in the tree, reparenting is not supported.");

        var previousChild = (index > 0 ? this._children[index - 1] : null);
        if (previousChild) {
            previousChild.nextSibling = child;
            child.previousSibling = previousChild;
        } else {
            child.previousSibling = null;
        }

        var nextChild = this._children[index];
        if (nextChild) {
            nextChild.previousSibling = child;
            child.nextSibling = nextChild;
        } else {
            child.nextSibling = null;
        }

        this._children.splice(index, 0, child);

        child.parent = this;
        this.hasChildren = true;

        if (this.treeOutline)
            this.treeOutline._bindTreeElement(child);
        for (var current = child.firstChild(); this.treeOutline && current; current = current.traverseNextTreeElement(false, child, true))
            this.treeOutline._bindTreeElement(current);

        child.onattach();
        child._ensureSelection();
        if (this.treeOutline)
            this.treeOutline._eventSink.dispatchEventToListeners(TreeOutline.Events.ElementAttached, child);
        var nextSibling = child.nextSibling ? child.nextSibling._listItemNode : null;
        this._childrenListNode.insertBefore(child._listItemNode, nextSibling);
        this._childrenListNode.insertBefore(child._childrenListNode, nextSibling);
        if (child.selected)
            child.select();
        if (child.expanded)
            child.expand();
    },

    /**
     * @param {number} childIndex
     */
    removeChildAtIndex: function(childIndex)
    {
        if (childIndex < 0 || childIndex >= this._children.length)
            throw("childIndex out of range");

        var child = this._children[childIndex];
        this._children.splice(childIndex, 1);

        var parent = child.parent;
        if (this.treeOutline && this.treeOutline.selectedTreeElement && this.treeOutline.selectedTreeElement.hasAncestorOrSelf(child)) {
            if (child.nextSibling)
                child.nextSibling.select(true);
            else if (child.previousSibling)
                child.previousSibling.select(true);
            else if (parent !== this.treeOutline)
                parent.select(true);
        }

        if (child.previousSibling)
            child.previousSibling.nextSibling = child.nextSibling;
        if (child.nextSibling)
            child.nextSibling.previousSibling = child.previousSibling;
        child.parent = null;

        if (this.treeOutline)
            this.treeOutline._unbindTreeElement(child);
        for (var current = child.firstChild(); this.treeOutline && current; current = current.traverseNextTreeElement(false, child, true))
            this.treeOutline._unbindTreeElement(current);

        child._detach();
    },

    /**
     * @param {!TreeElement} child
     */
    removeChild: function(child)
    {
        if (!child)
            throw("child can't be undefined or null");
        if (child.parent !== this)
            return;

        var childIndex = this._children.indexOf(child);
        if (childIndex === -1)
            throw("child not found in this node's children");

        this.removeChildAtIndex(childIndex);
    },

    removeChildren: function()
    {
        if (!this.root && this.treeOutline && this.treeOutline.selectedTreeElement && this.treeOutline.selectedTreeElement.hasAncestor(this))
            this.select(true);

        for (var i = 0; this._children && i < this._children.length; ++i) {
            var child = this._children[i];
            child.previousSibling = null
            child.nextSibling = null;
            child.parent = null;

            if (this.treeOutline)
                this.treeOutline._unbindTreeElement(child);
            for (var current = child.firstChild(); this.treeOutline && current; current = current.traverseNextTreeElement(false, child, true))
                this.treeOutline._unbindTreeElement(current);
            child._detach();
        }
        this._children = [];
    },

    expand: function()
    {
    },

    collapse: function()
    {
    }
}

/**
 * @constructor
 * @extends {TreeContainerNode}
 * @param {boolean=} nonFocusable
 */
function TreeOutline(nonFocusable)
{
    TreeContainerNode.call(this);
    var listNode = createElement("ol");

    this._treeElementSymbol = Symbol("TreeElement");

    this.selectedTreeElement = null;
    this._listItemNode = createElement("li");
    this._childrenListNode = listNode;
    this.childrenListElement = this._childrenListNode;
    this._childrenListNode.removeChildren();
    this.expandTreeElementsWhenArrowing = false;
    this.root = true;
    this._hasChildren = false;
    this.expanded = true;
    this.selected = false;
    this.treeOutline = this;
    /** @type {?function(!TreeElement, !TreeElement):number} */
    this._comparator = null;

    this.setFocusable(!nonFocusable);
    this._childrenListNode.addEventListener("keydown", this._treeKeyDown.bind(this), true);
    this.element = listNode;
    this._eventSink = new WebInspector.Object();
}

TreeOutline.Events = {
    ElementAttached: "ElementAttached",
    ElementExpanded: "ElementExpanded",
    ElementCollapsed: "ElementCollapsed"
}

TreeOutline.prototype = {
    /**
     * @param {string} eventType
     * @param {function(!WebInspector.Event)} listener
     * @param {!Object=} thisObject
     */
    addEventListener: function(eventType, listener, thisObject)
    {
        this._eventSink.addEventListener(eventType, listener, thisObject);
    },

    /**
     * @param {string} eventType
     * @param {function(!WebInspector.Event)} listener
     * @param {!Object=} thisObject
     */
    removeEventListener: function(eventType, listener, thisObject)
    {
        this._eventSink.removeEventListener(eventType, listener, thisObject);
    },

    /**
     * @param {number} x
     * @param {number} y
     * @return {?TreeElement}
     */
    treeElementFromPoint: function(x, y)
    {
        var node = this._childrenListNode.ownerDocument.deepElementFromPoint(x, y);
        if (!node)
            return null;

        var listNode = node.enclosingNodeOrSelfWithNodeNameInArray(["ol", "li"]);
        if (listNode)
            return listNode.parentTreeElement || listNode.treeElement;
        return null;
    },

    /**
     * @param {?Event} event
     * @return {?TreeElement}
     */
    treeElementFromEvent: function(event)
    {
        return event ? this.treeElementFromPoint(event.pageX, event.pageY) : null;
    },

    /**
     * @param {?function(!TreeElement, !TreeElement):number} comparator
     */
    setComparator: function(comparator)
    {
        this._comparator = comparator;
    },

    /**
     * @param {boolean} focusable
     */
    setFocusable: function(focusable)
    {
        if (focusable)
            this._childrenListNode.setAttribute("tabIndex", 0);
        else
            this._childrenListNode.removeAttribute("tabIndex");
    },

    /**
     * @param {!TreeElement} element
     */
    _bindTreeElement: function(element)
    {
        if (element.treeOutline)
            console.error("Binding element for the second time: " + new Error().stack);

        var existingElement = element.representedObject[this._treeElementSymbol];
        console.assert(!existingElement, "A tree element with given represented object already exists: " + (existingElement && existingElement.title ? existingElement.title.textContent : ""));
        element.representedObject[this._treeElementSymbol] = element;
        element.treeOutline = this;
    },

    /**
     * @param {!TreeElement} element
     */
    _unbindTreeElement: function(element)
    {
        if (!element.treeOutline)
            console.error("Unbinding element that was not bound: " + new Error().stack);

        element.deselect();
        delete element.representedObject[this._treeElementSymbol];
        element.treeOutline = null;
    },

    /**
     * @param {?Object} representedObject
     * @return {?TreeElement}
     */
    getCachedTreeElement: function(representedObject)
    {
        if (!representedObject)
            return null;

        return representedObject[this._treeElementSymbol] || null;
    },

    /**
     * @param {!Event} event
     */
    _treeKeyDown: function(event)
    {
        if (event.target !== this._childrenListNode)
            return;

        if (!this.selectedTreeElement || event.shiftKey || event.metaKey || event.ctrlKey)
            return;

        var handled = false;
        var nextSelectedElement;
        if (event.keyIdentifier === "Up" && !event.altKey) {
            nextSelectedElement = this.selectedTreeElement.traversePreviousTreeElement(true);
            while (nextSelectedElement && !nextSelectedElement.selectable)
                nextSelectedElement = nextSelectedElement.traversePreviousTreeElement(!this.expandTreeElementsWhenArrowing);
            handled = nextSelectedElement ? true : false;
        } else if (event.keyIdentifier === "Down" && !event.altKey) {
            nextSelectedElement = this.selectedTreeElement.traverseNextTreeElement(true);
            while (nextSelectedElement && !nextSelectedElement.selectable)
                nextSelectedElement = nextSelectedElement.traverseNextTreeElement(!this.expandTreeElementsWhenArrowing);
            handled = nextSelectedElement ? true : false;
        } else if (event.keyIdentifier === "Left") {
            if (this.selectedTreeElement.expanded) {
                if (event.altKey)
                    this.selectedTreeElement.collapseRecursively();
                else
                    this.selectedTreeElement.collapse();
                handled = true;
            } else if (this.selectedTreeElement.parent && !this.selectedTreeElement.parent.root) {
                handled = true;
                if (this.selectedTreeElement.parent.selectable) {
                    nextSelectedElement = this.selectedTreeElement.parent;
                    while (nextSelectedElement && !nextSelectedElement.selectable)
                        nextSelectedElement = nextSelectedElement.parent;
                    handled = nextSelectedElement ? true : false;
                } else if (this.selectedTreeElement.parent)
                    this.selectedTreeElement.parent.collapse();
            }
        } else if (event.keyIdentifier === "Right") {
            if (!this.selectedTreeElement.revealed()) {
                this.selectedTreeElement.reveal();
                handled = true;
            } else if (this.selectedTreeElement.hasChildren) {
                handled = true;
                if (this.selectedTreeElement.expanded) {
                    nextSelectedElement = this.selectedTreeElement.firstChild();
                    while (nextSelectedElement && !nextSelectedElement.selectable)
                        nextSelectedElement = nextSelectedElement.nextSibling;
                    handled = nextSelectedElement ? true : false;
                } else {
                    if (event.altKey)
                        this.selectedTreeElement.expandRecursively();
                    else
                        this.selectedTreeElement.expand();
                }
            }
        } else if (event.keyCode === 8 /* Backspace */ || event.keyCode === 46 /* Delete */)
            handled = this.selectedTreeElement.ondelete();
        else if (isEnterKey(event))
            handled = this.selectedTreeElement.onenter();
        else if (event.keyCode === WebInspector.KeyboardShortcut.Keys.Space.code)
            handled = this.selectedTreeElement.onspace();

        if (nextSelectedElement) {
            nextSelectedElement.reveal();
            nextSelectedElement.select(false, true);
        }

        if (handled)
            event.consume(true);
    },

    __proto__: TreeContainerNode.prototype
}

/**
 * @constructor
 * @extends {TreeOutline}
 */
function TreeOutlineInShadow()
{
    TreeOutline.call(this);
    var innerElement = this.element;
    innerElement.classList.add("tree-outline");

    // Redefine element to the external one.
    this.element = createElement("div");

    this._shadowRoot = this.element.createShadowRoot();
    this._shadowRoot.appendChild(WebInspector.View.createStyleElement("ui/treeoutline.css"));
    this._shadowRoot.appendChild(innerElement);
    this._renderSelection = true;
}

TreeOutlineInShadow.prototype = {
    focus: function()
    {
        this._childrenListNode.focus();
    },

    __proto__: TreeOutline.prototype
}

/**
 * @constructor
 * @extends {TreeContainerNode}
 * @param {string|!Node} title
 * @param {?Object=} representedObject
 * @param {boolean=} hasChildren
 */
function TreeElement(title, representedObject, hasChildren)
{
    TreeContainerNode.call(this);
    this.representedObject = representedObject || {};

    this.root = false;

    /** @type {?TreeOutline} */
    this.treeOutline = null;
    this.parent = null;
    this.previousSibling = null;
    this.nextSibling = null;

    this._listItemNode = createElement("li");
    this._listItemNode.treeElement = this;
    this.title = title;
    if (typeof title === "string")
        this.tooltip = title;
    this._listItemNode.addEventListener("mousedown", this._handleMouseDown.bind(this), false);
    this._listItemNode.addEventListener("selectstart", this._treeElementSelectStart.bind(this), false);
    this._listItemNode.addEventListener("click", this._treeElementToggled.bind(this), false);
    this._listItemNode.addEventListener("dblclick", this._handleDoubleClick.bind(this), false);

    this._childrenListNode = createElement("ol");
    this._childrenListNode.parentTreeElement = this;
    this._childrenListNode.classList.add("children");

    this._hidden = false;
    this._selectable = true;
    this.expanded = false;
    this.selected = false;
    this.hasChildren = hasChildren || false;
}

/** @const */
TreeElement._ArrowToggleWidth = 10;

TreeElement.prototype = {
    /**
     * @param {?TreeElement} ancestor
     * @return {boolean}
     */
    hasAncestor: function(ancestor)
    {
        if (!ancestor)
            return false;

        var currentNode = this.parent;
        while (currentNode) {
            if (ancestor === currentNode)
                return true;
            currentNode = currentNode.parent;
        }

        return false;
    },

    /**
     * @param {?TreeElement} ancestor
     * @return {boolean}
     */
    hasAncestorOrSelf: function(ancestor)
    {
        return this === ancestor || this.hasAncestor(ancestor);
    },

    /**
     * @override
     */
    removeChildren: function()
    {
        if (this.treeOutline && this.treeOutline.selectedTreeElement && this.treeOutline.selectedTreeElement.hasAncestorOrSelf(this))
            this.select(true);
        TreeContainerNode.prototype.removeChildren.call(this);
    },

    get selectable()
    {
        if (this._hidden)
            return false;
        return this._selectable;
    },

    set selectable(x)
    {
        this._selectable = x;
    },

    get listItemElement()
    {
        return this._listItemNode;
    },

    get childrenListElement()
    {
        return this._childrenListNode;
    },

    get title()
    {
        return this._title;
    },

    set title(x)
    {
        this._title = x;
        if (typeof this._title === "string")
            this._listItemNode.textContent = this._title;
        else {
            this._listItemNode.removeChildren();
            if (this._title)
                this._listItemNode.appendChild(this._title);
            this._ensureSelection();
        }
    },

    /**
     * @param {string} x
     */
    set tooltip(x)
    {
        if (x)
            this._listItemNode.setAttribute("title", x);
        else
            this._listItemNode.removeAttribute("title");
    },

    get hasChildren()
    {
        return this._hasChildren;
    },

    /**
     * Used inside subclasses.
     *
     * @param {boolean} hasChildren
     */
    setHasChildren: function(hasChildren)
    {
        this.hasChildren = hasChildren;
    },

    set hasChildren(x)
    {
        if (this._hasChildren === x)
            return;

        this._hasChildren = x;

        this._listItemNode.classList.toggle("parent", x);
        if (!x)
            this.collapse();
    },

    get hidden()
    {
        return this._hidden;
    },

    set hidden(x)
    {
        if (this._hidden === x)
            return;

        this._hidden = x;

        this._listItemNode.classList.toggle("hidden", x);
        this._childrenListNode.classList.toggle("hidden", x);
    },

    invalidateChildren: function()
    {
        if (this._children) {
            this.removeChildren();
            this._children = null;
        }
    },

    _ensureSelection: function()
    {
        if (!this.treeOutline || !this.treeOutline._renderSelection)
            return;
        if (!this._selectionElement)
            this._selectionElement = createElementWithClass("div", "selection");
        this._listItemNode.insertBefore(this._selectionElement, this.listItemElement.firstChild);
    },

    /**
     * @param {!Event} event
     */
    _treeElementSelectStart: function(event)
    {
        event.currentTarget._selectionStarted = true;
    },

    /**
     * @param {!Event} event
     */
    _treeElementToggled: function(event)
    {
        var element = event.currentTarget;
        if (element._selectionStarted) {
            delete element._selectionStarted;
            var selection = element.getComponentSelection();
            if (selection && !selection.isCollapsed && element.isSelfOrAncestor(selection.anchorNode) && element.isSelfOrAncestor(selection.focusNode))
                return;
        }

        if (element.treeElement !== this)
            return;

        var toggleOnClick = this.toggleOnClick && !this.selectable;
        var isInTriangle = this.isEventWithinDisclosureTriangle(event);
        if (!toggleOnClick && !isInTriangle)
            return;

        if (event.target && event.target.enclosingNodeOrSelfWithNodeName("a"))
            return;

        if (this.expanded) {
            if (event.altKey)
                this.collapseRecursively();
            else
                this.collapse();
        } else {
            if (event.altKey)
                this.expandRecursively();
            else
                this.expand();
        }
        event.consume();
    },

    /**
     * @param {!Event} event
     */
    _handleMouseDown: function(event)
    {
        var element = event.currentTarget;
        if (!element)
            return;
        delete element._selectionStarted;

        if (!this.selectable)
            return;
        if (element.treeElement !== this)
            return;

        if (this.isEventWithinDisclosureTriangle(event))
            return;

        this.selectOnMouseDown(event);
    },

    /**
     * @param {!Event} event
     */
    _handleDoubleClick: function(event)
    {
        var element = event.currentTarget;
        if (!element || element.treeElement !== this)
            return;

        var handled = this.ondblclick(event);
        if (handled)
            return;
        if (this.hasChildren && !this.expanded)
            this.expand();
    },

    _detach: function()
    {
        this._listItemNode.remove();
        this._childrenListNode.remove();
    },

    /**
     * @return {*}
     */
    elementIdentity: function()
    {
        return this.representedObject;
    },

    /**
     * @override
     */
    collapse: function()
    {
        if (!this.expanded)
            return;
        this._listItemNode.classList.remove("expanded");
        this._childrenListNode.classList.remove("expanded");
        this.expanded = false;
        this.oncollapse();
        if (this.treeOutline)
            this.treeOutline._eventSink.dispatchEventToListeners(TreeOutline.Events.ElementCollapsed, this);
    },

    collapseRecursively: function()
    {
        var item = this;
        while (item) {
            if (item.expanded)
                item.collapse();
            item = item.traverseNextTreeElement(false, this, true);
        }
    },

    /**
     * @override
     */
    expand: function()
    {
        if (!this.hasChildren || (this.expanded && this._children))
            return;

        // Set this before onpopulate. Since onpopulate can add elements, this makes
        // sure the expanded flag is true before calling those functions. This prevents the possibility
        // of an infinite loop if onpopulate were to call expand.

        this.expanded = true;

        if (!this._children) {
            this._children = [];
            this.onpopulate();
        }

        this._listItemNode.classList.add("expanded");
        this._childrenListNode.classList.add("expanded");

        this.onexpand();
        if (this.treeOutline)
            this.treeOutline._eventSink.dispatchEventToListeners(TreeOutline.Events.ElementExpanded, this);
    },

    /**
     * @param {number=} maxDepth
     */
    expandRecursively: function(maxDepth)
    {
        var item = this;
        var info = {};
        var depth = 0;

        // The Inspector uses TreeOutlines to represents object properties, so recursive expansion
        // in some case can be infinite, since JavaScript objects can hold circular references.
        // So default to a recursion cap of 3 levels, since that gives fairly good results.
        if (isNaN(maxDepth))
            maxDepth = 3;

        while (item) {
            if (depth < maxDepth)
                item.expand();
            item = item.traverseNextTreeElement(false, this, (depth >= maxDepth), info);
            depth += info.depthChange;
        }
    },

    reveal: function()
    {
        var currentAncestor = this.parent;
        while (currentAncestor && !currentAncestor.root) {
            if (!currentAncestor.expanded)
                currentAncestor.expand();
            currentAncestor = currentAncestor.parent;
        }

        this.listItemElement.scrollIntoViewIfNeeded();

        this.onreveal();
    },

    /**
     * @return {boolean}
     */
    revealed: function()
    {
        var currentAncestor = this.parent;
        while (currentAncestor && !currentAncestor.root) {
            if (!currentAncestor.expanded)
                return false;
            currentAncestor = currentAncestor.parent;
        }

        return true;
    },

    selectOnMouseDown: function(event)
    {
        if (this.select(false, true))
            event.consume(true);
    },

    /**
     * @param {boolean=} omitFocus
     * @param {boolean=} selectedByUser
     * @return {boolean}
     */
    select: function(omitFocus, selectedByUser)
    {
        if (!this.treeOutline || !this.selectable || this.selected)
            return false;

        if (this.treeOutline.selectedTreeElement)
            this.treeOutline.selectedTreeElement.deselect();

        this.selected = true;

        if (!omitFocus)
            this.treeOutline._childrenListNode.focus();

        // Focusing on another node may detach "this" from tree.
        if (!this.treeOutline)
            return false;
        this.treeOutline.selectedTreeElement = this;
        this._listItemNode.classList.add("selected");

        return this.onselect(selectedByUser);
    },

    /**
     * @param {boolean=} omitFocus
     */
    revealAndSelect: function(omitFocus)
    {
        this.reveal();
        this.select(omitFocus);
    },

    /**
     * @param {boolean=} supressOnDeselect
     */
    deselect: function(supressOnDeselect)
    {
        if (!this.treeOutline || this.treeOutline.selectedTreeElement !== this || !this.selected)
            return;

        this.selected = false;
        this.treeOutline.selectedTreeElement = null;
        this._listItemNode.classList.remove("selected");
    },

    onpopulate: function()
    {
        // Overridden by subclasses.
    },

    /**
     * @return {boolean}
     */
    onenter: function()
    {
        return false;
    },

    /**
     * @return {boolean}
     */
    ondelete: function()
    {
        return false;
    },

    /**
     * @return {boolean}
     */
    onspace: function()
    {
        return false;
    },

    onattach: function()
    {
    },

    onexpand: function()
    {
    },

    oncollapse: function()
    {
    },

    /**
     * @param {!Event} e
     * @return {boolean}
     */
    ondblclick: function(e)
    {
        return false;
    },

    onreveal: function()
    {
    },

    /**
     * @param {boolean=} selectedByUser
     * @return {boolean}
     */
    onselect: function(selectedByUser)
    {
        return false;
    },

    /**
     * @param {boolean} skipUnrevealed
     * @param {?TreeContainerNode=} stayWithin
     * @param {boolean=} dontPopulate
     * @param {!Object=} info
     * @return {?TreeElement}
     */
    traverseNextTreeElement: function(skipUnrevealed, stayWithin, dontPopulate, info)
    {
        if (!dontPopulate && this.hasChildren)
            this.onpopulate();

        if (info)
            info.depthChange = 0;

        var element = skipUnrevealed ? (this.revealed() ? this.firstChild() : null) : this.firstChild();
        if (element && (!skipUnrevealed || (skipUnrevealed && this.expanded))) {
            if (info)
                info.depthChange = 1;
            return element;
        }

        if (this === stayWithin)
            return null;

        element = skipUnrevealed ? (this.revealed() ? this.nextSibling : null) : this.nextSibling;
        if (element)
            return element;

        element = this;
        while (element && !element.root && !(skipUnrevealed ? (element.revealed() ? element.nextSibling : null) : element.nextSibling) && element.parent !== stayWithin) {
            if (info)
                info.depthChange -= 1;
            element = element.parent;
        }

        if (!element || element.root)
            return null;

        return (skipUnrevealed ? (element.revealed() ? element.nextSibling : null) : element.nextSibling);
    },

    /**
     * @param {boolean} skipUnrevealed
     * @param {boolean=} dontPopulate
     * @return {?TreeElement}
     */
    traversePreviousTreeElement: function(skipUnrevealed, dontPopulate)
    {
        var element = skipUnrevealed ? (this.revealed() ? this.previousSibling : null) : this.previousSibling;
        if (!dontPopulate && element && element.hasChildren)
            element.onpopulate();

        while (element && (skipUnrevealed ? (element.revealed() && element.expanded ? element.children[element.children.length - 1] : null) : element.children[element.children.length - 1])) {
            if (!dontPopulate && element.hasChildren)
                element.onpopulate();
            element = (skipUnrevealed ? (element.revealed() && element.expanded ? element.children[element.children.length - 1] : null) : element.children[element.children.length - 1]);
        }

        if (element)
            return element;

        if (!this.parent || this.parent.root)
            return null;

        return this.parent;
    },

    /**
     * @return {boolean}
     */
    isEventWithinDisclosureTriangle: function(event)
    {
        // FIXME: We should not use getComputedStyle(). For that we need to get rid of using ::before for disclosure triangle. (http://webk.it/74446)
        var paddingLeftValue = window.getComputedStyle(this._listItemNode).paddingLeft;
        console.assert(paddingLeftValue.endsWith("px"));
        var computedLeftPadding = parseFloat(paddingLeftValue);
        var left = this._listItemNode.totalOffsetLeft() + computedLeftPadding;
        return event.pageX >= left && event.pageX <= left + TreeElement._ArrowToggleWidth && this.hasChildren;
    },

    __proto__: TreeContainerNode.prototype
}
