/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "config.h"
#include "core/html/HTMLDialogElement.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/NodeTraversal.h"
#include "core/html/HTMLFormControlElement.h"
#include "core/frame/FrameView.h"
#include "core/rendering/RenderBlock.h"
#include "core/rendering/style/RenderStyle.h"

namespace WebCore {

using namespace HTMLNames;

static void runAutofocus(HTMLDialogElement* dialog)
{
    Node* next = 0;
    for (Node* node = dialog->firstChild(); node; node = next) {
        if (node->isElementNode() && toElement(node)->isFormControlElement()) {
            HTMLFormControlElement* control = toHTMLFormControlElement(node);
            if (control->isAutofocusable()) {
                control->focus();
                control->setAutofocused();
                return;
            }
        }
        if (node->hasTagName(dialogTag))
            next = NodeTraversal::nextSkippingChildren(node, dialog);
        else
            next = NodeTraversal::next(node, dialog);
    }
}

HTMLDialogElement::HTMLDialogElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
    , m_centeringMode(Uninitialized)
    , m_centeredPosition(0)
    , m_returnValue("")
{
    ASSERT(hasTagName(dialogTag));
    ScriptWrappable::init(this);
}

PassRefPtr<HTMLDialogElement> HTMLDialogElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(new HTMLDialogElement(tagName, document));
}

void HTMLDialogElement::close(const String& returnValue, ExceptionState& es)
{
    if (!fastHasAttribute(openAttr)) {
        es.throwUninformativeAndGenericDOMException(InvalidStateError);
        return;
    }
    closeDialog(returnValue);
}

void HTMLDialogElement::closeDialog(const String& returnValue)
{
    if (!fastHasAttribute(openAttr))
        return;
    setBooleanAttribute(openAttr, false);
    document().removeFromTopLayer(this);

    if (!returnValue.isNull())
        m_returnValue = returnValue;

    dispatchScopedEvent(Event::create(EventTypeNames::close));
}

void HTMLDialogElement::forceLayoutForCentering()
{
    m_centeringMode = Uninitialized;
    document().updateLayoutIgnorePendingStylesheets();
    if (m_centeringMode == Uninitialized)
        m_centeringMode = NotCentered;
}

void HTMLDialogElement::show()
{
    if (fastHasAttribute(openAttr))
        return;
    setBooleanAttribute(openAttr, true);
    forceLayoutForCentering();
}

void HTMLDialogElement::showModal(ExceptionState& es)
{
    if (fastHasAttribute(openAttr) || !inDocument()) {
        es.throwUninformativeAndGenericDOMException(InvalidStateError);
        return;
    }
    document().addToTopLayer(this);
    setBooleanAttribute(openAttr, true);

    runAutofocus(this);
    forceLayoutForCentering();
}

void HTMLDialogElement::setCentered(LayoutUnit centeredPosition)
{
    ASSERT(m_centeringMode == Uninitialized);
    m_centeredPosition = centeredPosition;
    m_centeringMode = Centered;
}

void HTMLDialogElement::setNotCentered()
{
    ASSERT(m_centeringMode == Uninitialized);
    m_centeringMode = NotCentered;
}

bool HTMLDialogElement::isPresentationAttribute(const QualifiedName& name) const
{
    // FIXME: Workaround for <https://bugs.webkit.org/show_bug.cgi?id=91058>: modifying an attribute for which there is an attribute selector
    // in html.css sometimes does not trigger a style recalc.
    if (name == openAttr)
        return true;

    return HTMLElement::isPresentationAttribute(name);
}

void HTMLDialogElement::defaultEventHandler(Event* event)
{
    if (event->type() == EventTypeNames::cancel) {
        closeDialog();
        event->setDefaultHandled();
        return;
    }
    HTMLElement::defaultEventHandler(event);
}

bool HTMLDialogElement::shouldBeReparentedUnderRenderView(const RenderStyle* style) const
{
    if (style && style->position() == AbsolutePosition)
        return true;
    return Element::shouldBeReparentedUnderRenderView(style);
}

} // namespace WebCore
