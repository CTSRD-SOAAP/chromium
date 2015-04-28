/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "core/dom/RenderTreeBuilder.h"

#include "core/HTMLNames.h"
#include "core/SVGNames.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/FirstLetterPseudoElement.h"
#include "core/dom/Fullscreen.h"
#include "core/dom/Node.h"
#include "core/dom/PseudoElement.h"
#include "core/dom/Text.h"
#include "core/layout/LayoutObject.h"
#include "core/rendering/RenderFullScreen.h"
#include "core/rendering/RenderText.h"
#include "core/rendering/RenderView.h"
#include "core/svg/SVGElement.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

RenderTreeBuilderForElement::RenderTreeBuilderForElement(Element& element, LayoutStyle* style)
    : RenderTreeBuilder(element, nullptr)
    , m_style(style)
{
    if (element.isFirstLetterPseudoElement()) {
        if (LayoutObject* nextRenderer = FirstLetterPseudoElement::firstLetterTextRenderer(element))
            m_renderingParent = nextRenderer->parent();
    } else if (ContainerNode* containerNode = NodeRenderingTraversal::parent(element)) {
        m_renderingParent = containerNode->renderer();
    }
}

LayoutObject* RenderTreeBuilderForElement::nextRenderer() const
{
    ASSERT(m_renderingParent);

    if (m_node->isInTopLayer())
        return NodeRenderingTraversal::nextInTopLayer(*m_node);

    if (m_node->isFirstLetterPseudoElement())
        return FirstLetterPseudoElement::firstLetterTextRenderer(*m_node);

    return RenderTreeBuilder::nextRenderer();
}

LayoutObject* RenderTreeBuilderForElement::parentRenderer() const
{
    LayoutObject* parentRenderer = RenderTreeBuilder::parentRenderer();

    if (parentRenderer) {
        // FIXME: Guarding this by parentRenderer isn't quite right as the spec for
        // top layer only talks about display: none ancestors so putting a <dialog> inside an
        // <optgroup> seems like it should still work even though this check will prevent it.
        if (m_node->isInTopLayer())
            return m_node->document().renderView();
    }

    return parentRenderer;
}

bool RenderTreeBuilderForElement::shouldCreateRenderer() const
{
    if (!m_renderingParent)
        return false;

    // FIXME: Should the following be in SVGElement::rendererIsNeeded()?
    if (m_node->isSVGElement()) {
        // SVG elements only render when inside <svg>, or if the element is an <svg> itself.
        if (!isSVGSVGElement(*m_node) && (!m_renderingParent->node() || !m_renderingParent->node()->isSVGElement()))
            return false;
        if (!toSVGElement(m_node)->isValid())
            return false;
    }

    LayoutObject* parentRenderer = this->parentRenderer();
    if (!parentRenderer)
        return false;
    if (!parentRenderer->canHaveChildren())
        return false;

    return m_node->rendererIsNeeded(style());
}

LayoutStyle& RenderTreeBuilderForElement::style() const
{
    if (!m_style)
        m_style = m_node->styleForRenderer();
    return *m_style;
}

void RenderTreeBuilderForElement::createRenderer()
{
    LayoutStyle& style = this->style();

    LayoutObject* newRenderer = m_node->createRenderer(style);
    if (!newRenderer)
        return;

    LayoutObject* parentRenderer = this->parentRenderer();

    if (!parentRenderer->isChildAllowed(newRenderer, style)) {
        newRenderer->destroy();
        return;
    }

    // Make sure the LayoutObject already knows it is going to be added to a LayoutFlowThread before we set the style
    // for the first time. Otherwise code using inLayoutFlowThread() in the styleWillChange and styleDidChange will fail.
    newRenderer->setFlowThreadState(parentRenderer->flowThreadState());

    LayoutObject* nextRenderer = this->nextRenderer();
    m_node->setRenderer(newRenderer);
    newRenderer->setStyle(&style); // setStyle() can depend on renderer() already being set.

    if (Fullscreen::isActiveFullScreenElement(*m_node)) {
        newRenderer = RenderFullScreen::wrapRenderer(newRenderer, parentRenderer, &m_node->document());
        if (!newRenderer)
            return;
    }

    // Note: Adding newRenderer instead of renderer(). renderer() may be a child of newRenderer.
    parentRenderer->addChild(newRenderer, nextRenderer);
}

void RenderTreeBuilderForText::createRenderer()
{
    LayoutObject* parentRenderer = this->parentRenderer();
    LayoutStyle* style = parentRenderer->style();

    ASSERT(m_node->textRendererIsNeeded(*style, *parentRenderer));

    RenderText* newRenderer = m_node->createTextRenderer(style);
    if (!parentRenderer->isChildAllowed(newRenderer, *style)) {
        newRenderer->destroy();
        return;
    }

    // Make sure the LayoutObject already knows it is going to be added to a LayoutFlowThread before we set the style
    // for the first time. Otherwise code using inLayoutFlowThread() in the styleWillChange and styleDidChange will fail.
    newRenderer->setFlowThreadState(parentRenderer->flowThreadState());

    LayoutObject* nextRenderer = this->nextRenderer();
    m_node->setRenderer(newRenderer);
    // Parent takes care of the animations, no need to call setAnimatableStyle.
    newRenderer->setStyle(style);
    parentRenderer->addChild(newRenderer, nextRenderer);
}

}
