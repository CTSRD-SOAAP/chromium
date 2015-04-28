/*
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 * Copyright (C) 2006 Apple Computer Inc.
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

#ifndef LayoutSVGInline_h
#define LayoutSVGInline_h

#include "core/rendering/RenderInline.h"

namespace blink {

class LayoutSVGInline : public RenderInline {
public:
    explicit LayoutSVGInline(Element*);

    virtual const char* renderName() const override { return "LayoutSVGInline"; }
    virtual LayerType layerTypeRequired() const override final { return NoLayer; }
    virtual bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectSVG || type == LayoutObjectSVGInline || RenderInline::isOfType(type); }

    virtual bool isChildAllowed(LayoutObject*, const LayoutStyle&) const override;

    // Chapter 10.4 of the SVG Specification say that we should use the
    // object bounding box of the parent text element.
    // We search for the root text element and take its bounding box.
    // It is also necessary to take the stroke and paint invalidation rect of
    // this element, since we need it for filters.
    virtual FloatRect objectBoundingBox() const override final;
    virtual FloatRect strokeBoundingBox() const override final;
    virtual FloatRect paintInvalidationRectInLocalCoordinates() const override final;

    virtual LayoutRect clippedOverflowRectForPaintInvalidation(const LayoutLayerModelObject* paintInvalidationContainer, const PaintInvalidationState* = 0) const override final;
    virtual void mapLocalToContainer(const LayoutLayerModelObject* paintInvalidationContainer, TransformState&, MapCoordinatesFlags = ApplyContainerFlip, bool* wasFixed = 0, const PaintInvalidationState* = 0) const override final;
    virtual const LayoutObject* pushMappingToContainer(const LayoutLayerModelObject* ancestorToStopAt, LayoutGeometryMap&) const override final;
    virtual void absoluteQuads(Vector<FloatQuad>&, bool* wasFixed) const override final;

private:
    virtual InlineFlowBox* createInlineFlowBox() override final;

    virtual void willBeDestroyed() override final;
    virtual void styleDidChange(StyleDifference, const LayoutStyle* oldStyle) override final;

    virtual void addChild(LayoutObject* child, LayoutObject* beforeChild = 0) override final;
    virtual void removeChild(LayoutObject*) override final;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutSVGInline, isSVGInline());

}

#endif // LayoutSVGInline_H
