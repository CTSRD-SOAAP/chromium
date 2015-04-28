/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2011 Dirk Schulze <krit@webkit.org>
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
 */

#include "config.h"
#include "core/layout/svg/LayoutSVGResourceClipper.h"

#include "core/SVGNames.h"
#include "core/dom/ElementTraversal.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/PaintInfo.h"
#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/svg/SVGUseElement.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/paint/ClipPathDisplayItem.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "wtf/TemporaryChange.h"

namespace blink {

LayoutSVGResourceClipper::LayoutSVGResourceClipper(SVGClipPathElement* node)
    : LayoutSVGResourceContainer(node)
    , m_inClipExpansion(false)
{
}

LayoutSVGResourceClipper::~LayoutSVGResourceClipper()
{
}

void LayoutSVGResourceClipper::removeAllClientsFromCache(bool markForInvalidation)
{
    m_clipContentPicture.clear();
    m_clipBoundaries = FloatRect();
    markAllClientsForInvalidation(markForInvalidation ? LayoutAndBoundariesInvalidation : ParentOnlyInvalidation);
}

void LayoutSVGResourceClipper::removeClientFromCache(LayoutObject* client, bool markForInvalidation)
{
    ASSERT(client);
    markClientForInvalidation(client, markForInvalidation ? BoundariesInvalidation : ParentOnlyInvalidation);
}

bool LayoutSVGResourceClipper::applyStatefulResource(LayoutObject* object, GraphicsContext*& context, ClipperState& clipperState)
{
    ASSERT(object);
    ASSERT(context);

    clearInvalidationMask();

    return applyClippingToContext(object, object->objectBoundingBox(), object->paintInvalidationRectInLocalCoordinates(), context, clipperState);
}

bool LayoutSVGResourceClipper::tryPathOnlyClipping(DisplayItemClient client, GraphicsContext* context,
    const AffineTransform& animatedLocalTransform, const FloatRect& objectBoundingBox) {
    // If the current clip-path gets clipped itself, we have to fallback to masking.
    if (!style()->svgStyle().clipperResource().isEmpty())
        return false;
    WindRule clipRule = RULE_NONZERO;
    Path clipPath = Path();

    for (SVGElement* childElement = Traversal<SVGElement>::firstChild(*element()); childElement; childElement = Traversal<SVGElement>::nextSibling(*childElement)) {
        LayoutObject* renderer = childElement->renderer();
        if (!renderer)
            continue;
        // Only shapes or paths are supported for direct clipping. We need to fallback to masking for texts.
        if (renderer->isSVGText())
            return false;
        if (!childElement->isSVGGraphicsElement())
            continue;
        SVGGraphicsElement* styled = toSVGGraphicsElement(childElement);
        const LayoutStyle* style = renderer->style();
        if (!style || style->display() == NONE || style->visibility() != VISIBLE)
            continue;
        const SVGLayoutStyle& svgStyle = style->svgStyle();
        // Current shape in clip-path gets clipped too. Fallback to masking.
        if (!svgStyle.clipperResource().isEmpty())
            return false;

        if (clipPath.isEmpty()) {
            // First clip shape.
            styled->toClipPath(clipPath);
            clipRule = svgStyle.clipRule();
            clipPath.setWindRule(clipRule);
            continue;
        }

        if (RuntimeEnabledFeatures::pathOpsSVGClippingEnabled()) {
            // Attempt to generate a combined clip path, fall back to masking if not possible.
            Path subPath;
            styled->toClipPath(subPath);
            subPath.setWindRule(svgStyle.clipRule());
            if (!clipPath.unionPath(subPath))
                return false;
        } else {
            return false;
        }
    }
    // Only one visible shape/path was found. Directly continue clipping and transform the content to userspace if necessary.
    if (clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        AffineTransform transform;
        transform.translate(objectBoundingBox.x(), objectBoundingBox.y());
        transform.scaleNonUniform(objectBoundingBox.width(), objectBoundingBox.height());
        clipPath.transform(transform);
    }

    // Transform path by animatedLocalTransform.
    clipPath.transform(animatedLocalTransform);

    // The SVG specification wants us to clip everything, if clip-path doesn't have a child.
    if (clipPath.isEmpty())
        clipPath.addRect(FloatRect());

    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        context->displayItemList()->add(BeginClipPathDisplayItem::create(client, clipPath, clipRule));
    } else {
        BeginClipPathDisplayItem clipPathDisplayItem(client, clipPath, clipRule);
        clipPathDisplayItem.replay(context);
    }

    return true;
}

bool LayoutSVGResourceClipper::applyClippingToContext(LayoutObject* target, const FloatRect& targetBoundingBox,
    const FloatRect& paintInvalidationRect, GraphicsContext* context, ClipperState& clipperState)
{
    ASSERT(target);
    ASSERT(context);
    ASSERT(clipperState == ClipperNotApplied);
    ASSERT_WITH_SECURITY_IMPLICATION(!needsLayout());

    if (paintInvalidationRect.isEmpty() || m_inClipExpansion)
        return false;
    TemporaryChange<bool> inClipExpansionChange(m_inClipExpansion, true);

    AffineTransform animatedLocalTransform = toSVGClipPathElement(element())->calculateAnimatedLocalTransform();
    // When drawing a clip for non-SVG elements, the CTM does not include the zoom factor.
    // In this case, we need to apply the zoom scale explicitly - but only for clips with
    // userSpaceOnUse units (the zoom is accounted for objectBoundingBox-resolved lengths).
    if (!target->isSVG() && clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE) {
        ASSERT(style());
        animatedLocalTransform.scale(style()->effectiveZoom());
    }

    // First, try to apply the clip as a clipPath.
    if (tryPathOnlyClipping(target->displayItemClient(), context, animatedLocalTransform, targetBoundingBox)) {
        clipperState = ClipperAppliedPath;
        return true;
    }

    // Fall back to masking.
    clipperState = ClipperAppliedMask;

    // Mask layer start
    context->beginTransparencyLayer(1, &paintInvalidationRect);
    {
        GraphicsContextStateSaver maskContentSaver(*context);
        context->concatCTM(animatedLocalTransform);

        // clipPath can also be clipped by another clipPath.
        SVGResources* resources = SVGResourcesCache::cachedResourcesForLayoutObject(this);
        LayoutSVGResourceClipper* clipPathClipper = resources ? resources->clipper() : 0;
        ClipperState clipPathClipperState = ClipperNotApplied;
        if (clipPathClipper && !clipPathClipper->applyClippingToContext(this, targetBoundingBox, paintInvalidationRect, context, clipPathClipperState)) {
            // FIXME: Awkward state micro-management. Ideally, GraphicsContextStateSaver should
            //   a) pop saveLayers also
            //   b) pop multiple states if needed (similarly to SkCanvas::restoreToCount())
            // Then we should be able to replace this mess with a single, top-level GCSS.
            maskContentSaver.restore();
            context->endLayer();
            return false;
        }

        drawClipMaskContent(context, targetBoundingBox);

        if (clipPathClipper)
            clipPathClipper->postApplyStatefulResource(this, context, clipPathClipperState);
    }

    // Masked content layer start.
    context->beginLayer(1, SkXfermode::kSrcIn_Mode, &paintInvalidationRect);

    return true;
}

void LayoutSVGResourceClipper::postApplyStatefulResource(LayoutObject* target, GraphicsContext*& context, ClipperState& clipperState)
{
    switch (clipperState) {
    case ClipperAppliedPath:
        // Path-only clipping, no layers to restore but we need to emit an end to the clip path display item.
        {
            if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
                context->displayItemList()->add(EndClipPathDisplayItem::create(target->displayItemClient()));
            } else {
                EndClipPathDisplayItem endClipPathDisplayItem(target->displayItemClient());
                endClipPathDisplayItem.replay(context);
            }
        }
        break;
    case ClipperAppliedMask:
        // Transfer content layer -> mask layer (SrcIn)
        context->endLayer();
        // Transfer mask layer -> bg layer (SrcOver)
        context->endLayer();
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

void LayoutSVGResourceClipper::drawClipMaskContent(GraphicsContext* context, const FloatRect& targetBoundingBox)
{
    ASSERT(context);

    AffineTransform contentTransformation;
    if (clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        contentTransformation.translate(targetBoundingBox.x(), targetBoundingBox.y());
        contentTransformation.scaleNonUniform(targetBoundingBox.width(), targetBoundingBox.height());
        context->concatCTM(contentTransformation);
    }

    if (!m_clipContentPicture) {
        SubtreeContentTransformScope contentTransformScope(contentTransformation);
        createPicture(context);
    }

    context->drawPicture(m_clipContentPicture.get());
}

void LayoutSVGResourceClipper::createPicture(GraphicsContext* context)
{
    ASSERT(context);
    ASSERT(frame());

    // Using strokeBoundingBox (instead of paintInvalidationRectInLocalCoordinates) to avoid the intersection
    // with local clips/mask, which may yield incorrect results when mixing objectBoundingBox and
    // userSpaceOnUse units (http://crbug.com/294900).
    FloatRect bounds = strokeBoundingBox();
    context->beginRecording(bounds);

    for (SVGElement* childElement = Traversal<SVGElement>::firstChild(*element()); childElement; childElement = Traversal<SVGElement>::nextSibling(*childElement)) {
        LayoutObject* renderer = childElement->renderer();
        if (!renderer)
            continue;

        const LayoutStyle* style = renderer->style();
        if (!style || style->display() == NONE || style->visibility() != VISIBLE)
            continue;

        WindRule newClipRule = style->svgStyle().clipRule();
        bool isUseElement = isSVGUseElement(*childElement);
        if (isUseElement) {
            SVGUseElement& useElement = toSVGUseElement(*childElement);
            renderer = useElement.rendererClipChild();
            if (!renderer)
                continue;
            if (!useElement.hasAttribute(SVGNames::clip_ruleAttr))
                newClipRule = renderer->style()->svgStyle().clipRule();
        }

        // Only shapes, paths and texts are allowed for clipping.
        if (!renderer->isSVGShape() && !renderer->isSVGText())
            continue;

        context->setFillRule(newClipRule);

        if (isUseElement)
            renderer = childElement->renderer();

        // Switch to a paint behavior where all children of this <clipPath> will be rendered using special constraints:
        // - fill-opacity/stroke-opacity/opacity set to 1
        // - masker/filter not applied when rendering the children
        // - fill is set to the initial fill paint server (solid, black)
        // - stroke is set to the initial stroke paint server (none)
        PaintInfo info(context, LayoutRect::infiniteIntRect(), PaintPhaseForeground, PaintBehaviorRenderingClipPathAsMask);
        renderer->paint(info, IntPoint());
    }

    m_clipContentPicture = context->endRecording();
}

void LayoutSVGResourceClipper::calculateClipContentPaintInvalidationRect()
{
    // This is a rough heuristic to appraise the clip size and doesn't consider clip on clip.
    for (SVGElement* childElement = Traversal<SVGElement>::firstChild(*element()); childElement; childElement = Traversal<SVGElement>::nextSibling(*childElement)) {
        LayoutObject* renderer = childElement->renderer();
        if (!renderer)
            continue;
        if (!renderer->isSVGShape() && !renderer->isSVGText() && !isSVGUseElement(*childElement))
            continue;
        const LayoutStyle* style = renderer->style();
        if (!style || style->display() == NONE || style->visibility() != VISIBLE)
            continue;
        m_clipBoundaries.unite(renderer->localToParentTransform().mapRect(renderer->paintInvalidationRectInLocalCoordinates()));
    }
    m_clipBoundaries = toSVGClipPathElement(element())->calculateAnimatedLocalTransform().mapRect(m_clipBoundaries);
}

bool LayoutSVGResourceClipper::hitTestClipContent(const FloatRect& objectBoundingBox, const FloatPoint& nodeAtPoint)
{
    FloatPoint point = nodeAtPoint;
    if (!SVGLayoutSupport::pointInClippingArea(this, point))
        return false;

    if (clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        AffineTransform transform;
        transform.translate(objectBoundingBox.x(), objectBoundingBox.y());
        transform.scaleNonUniform(objectBoundingBox.width(), objectBoundingBox.height());
        point = transform.inverse().mapPoint(point);
    }

    AffineTransform animatedLocalTransform = toSVGClipPathElement(element())->calculateAnimatedLocalTransform();
    if (!animatedLocalTransform.isInvertible())
        return false;

    point = animatedLocalTransform.inverse().mapPoint(point);

    for (SVGElement* childElement = Traversal<SVGElement>::firstChild(*element()); childElement; childElement = Traversal<SVGElement>::nextSibling(*childElement)) {
        LayoutObject* renderer = childElement->renderer();
        if (!renderer)
            continue;
        if (!renderer->isSVGShape() && !renderer->isSVGText() && !isSVGUseElement(*childElement))
            continue;
        IntPoint hitPoint;
        HitTestResult result(hitPoint);
        if (renderer->nodeAtFloatPoint(HitTestRequest(HitTestRequest::SVGClipContent), result, point, HitTestForeground))
            return true;
    }

    return false;
}

FloatRect LayoutSVGResourceClipper::resourceBoundingBox(const LayoutObject* object)
{
    // Resource was not layouted yet. Give back the boundingBox of the object.
    if (selfNeedsLayout())
        return object->objectBoundingBox();

    if (m_clipBoundaries.isEmpty())
        calculateClipContentPaintInvalidationRect();

    if (clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX) {
        FloatRect objectBoundingBox = object->objectBoundingBox();
        AffineTransform transform;
        transform.translate(objectBoundingBox.x(), objectBoundingBox.y());
        transform.scaleNonUniform(objectBoundingBox.width(), objectBoundingBox.height());
        return transform.mapRect(m_clipBoundaries);
    }

    return m_clipBoundaries;
}

}
