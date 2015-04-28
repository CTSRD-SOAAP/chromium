/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 * Copyright (C) 2012, 2013 Intel Corporation. All rights reserved.
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/html/canvas/CanvasRenderingContext2D.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/CSSPropertyNames.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/StylePropertySet.h"
#include "core/css/parser/CSSParser.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/DOMTypedArray.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/StyleEngine.h"
#include "core/events/Event.h"
#include "core/fetch/ImageResource.h"
#include "core/frame/ImageBitmap.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLMediaElement.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/ImageData.h"
#include "core/html/TextMetrics.h"
#include "core/html/canvas/CanvasGradient.h"
#include "core/html/canvas/CanvasPattern.h"
#include "core/html/canvas/CanvasStyle.h"
#include "core/html/canvas/Path2D.h"
#include "core/layout/Layer.h"
#include "core/layout/LayoutTheme.h"
#include "platform/fonts/FontCache.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/graphics/DrawLooperBuilder.h"
#include "platform/graphics/ExpensiveCanvasHeuristicParameters.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/text/BidiTextRun.h"
#include "platform/text/TextRun.h"
#include "wtf/ArrayBufferContents.h"
#include "wtf/CheckedArithmetic.h"
#include "wtf/MathExtras.h"
#include "wtf/OwnPtr.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

static const int defaultFontSize = 10;
static const char defaultFontFamily[] = "sans-serif";
static const char defaultFont[] = "10px sans-serif";
static const char inherit[] = "inherit";
static const char rtl[] = "rtl";
static const char ltr[] = "ltr";
static const double TryRestoreContextInterval = 0.5;
static const unsigned MaxTryRestoreContextAttempts = 4;
static const unsigned FetchedFontsCacheLimit = 50;

static bool contextLostRestoredEventsEnabled()
{
    return RuntimeEnabledFeatures::experimentalCanvasFeaturesEnabled();
}

// Drawing methods need to use this instead of SkAutoCanvasRestore in case overdraw
// detection substitutes the recording canvas (to discard overdrawn draw calls).
class CanvasRenderingContext2DAutoRestoreSkCanvas {
public:
    CanvasRenderingContext2DAutoRestoreSkCanvas(CanvasRenderingContext2D* context)
        : m_context(context)
        , m_saveCount(0)
    {
        ASSERT(m_context);
        SkCanvas* c = m_context->drawingCanvas();
        if (c) {
            m_saveCount = c->getSaveCount();
        }
    }

    ~CanvasRenderingContext2DAutoRestoreSkCanvas()
    {
        SkCanvas* c = m_context->drawingCanvas();
        if (c)
            c->restoreToCount(m_saveCount);
        m_context->validateStateStack();
    }
private:
    CanvasRenderingContext2D* m_context;
    int m_saveCount;
};

CanvasRenderingContext2D::CanvasRenderingContext2D(HTMLCanvasElement* canvas, const CanvasContextCreationAttributes& attrs, Document& document)
    : CanvasRenderingContext(canvas)
    , m_usesCSSCompatibilityParseMode(document.inQuirksMode())
    , m_clipAntialiasing(NotAntiAliased)
    , m_hasAlpha(attrs.alpha())
    , m_isContextLost(false)
    , m_contextRestorable(true)
    , m_tryRestoreContextAttemptCount(0)
    , m_dispatchContextLostEventTimer(this, &CanvasRenderingContext2D::dispatchContextLostEvent)
    , m_dispatchContextRestoredEventTimer(this, &CanvasRenderingContext2D::dispatchContextRestoredEvent)
    , m_tryRestoreContextEventTimer(this, &CanvasRenderingContext2D::tryRestoreContextEvent)
{
    if (document.settings() && document.settings()->antialiasedClips2dCanvasEnabled())
        m_clipAntialiasing = AntiAliased;
    m_stateStack.append(adoptPtrWillBeNoop(new State()));
}

void CanvasRenderingContext2D::unwindStateStack()
{
    if (size_t stackSize = m_stateStack.size()) {
        if (SkCanvas* skCanvas = canvas()->existingDrawingCanvas()) {
            while (--stackSize)
                skCanvas->restore();
        }
    }
}

CanvasRenderingContext2D::~CanvasRenderingContext2D()
{
}

void CanvasRenderingContext2D::validateStateStack()
{
#if ENABLE(ASSERT)
    SkCanvas* skCanvas = canvas()->existingDrawingCanvas();
    if (skCanvas && !m_isContextLost) {
        ASSERT(static_cast<size_t>(skCanvas->getSaveCount() - 1) == m_stateStack.size());
    }
#endif
}

bool CanvasRenderingContext2D::isAccelerated() const
{
    if (!canvas()->hasImageBuffer())
        return false;
    return canvas()->buffer()->isAccelerated();
}

bool CanvasRenderingContext2D::isContextLost() const
{
    return m_isContextLost;
}

void CanvasRenderingContext2D::loseContext()
{
    if (m_isContextLost)
        return;
    m_isContextLost = true;
    m_dispatchContextLostEventTimer.startOneShot(0, FROM_HERE);
}

void CanvasRenderingContext2D::restoreContext()
{
    if (!m_contextRestorable)
        return;
    // This code path is for restoring from an eviction
    // Restoring from surface failure is handled internally
    ASSERT(m_isContextLost && !canvas()->hasImageBuffer());

    if (canvas()->buffer()) {
        if (contextLostRestoredEventsEnabled()) {
            m_dispatchContextRestoredEventTimer.startOneShot(0, FROM_HERE);
        } else {
            // legacy synchronous context restoration.
            reset();
            m_isContextLost = false;
        }
    }
}

DEFINE_TRACE(CanvasRenderingContext2D)
{
#if ENABLE(OILPAN)
    visitor->trace(m_stateStack);
    visitor->trace(m_fetchedFonts);
    visitor->trace(m_hitRegionManager);
#endif
    CanvasRenderingContext::trace(visitor);
}

void CanvasRenderingContext2D::dispatchContextLostEvent(Timer<CanvasRenderingContext2D>*)
{
    if (contextLostRestoredEventsEnabled()) {
        RefPtrWillBeRawPtr<Event> event = Event::createCancelable(EventTypeNames::contextlost);
        canvas()->dispatchEvent(event);
        if (event->defaultPrevented()) {
            m_contextRestorable = false;
        }
    }

    // If an image buffer is present, it means the context was not lost due to
    // an eviction, but rather due to a surface failure (gpu context lost?)
    if (m_contextRestorable && canvas()->hasImageBuffer()) {
        m_tryRestoreContextAttemptCount = 0;
        m_tryRestoreContextEventTimer.startRepeating(TryRestoreContextInterval, FROM_HERE);
    }
}

void CanvasRenderingContext2D::tryRestoreContextEvent(Timer<CanvasRenderingContext2D>* timer)
{
    if (!m_isContextLost) {
        // Canvas was already restored (possibly thanks to a resize), so stop trying.
        m_tryRestoreContextEventTimer.stop();
        return;
    }
    if (canvas()->hasImageBuffer() && canvas()->buffer()->restoreSurface()) {
        m_tryRestoreContextEventTimer.stop();
        dispatchContextRestoredEvent(nullptr);
    }

    if (++m_tryRestoreContextAttemptCount > MaxTryRestoreContextAttempts)
        canvas()->discardImageBuffer();

    if (!canvas()->hasImageBuffer()) {
        // final attempt: allocate a brand new image buffer instead of restoring
        timer->stop();
        if (canvas()->buffer())
            dispatchContextRestoredEvent(nullptr);
    }
}

void CanvasRenderingContext2D::dispatchContextRestoredEvent(Timer<CanvasRenderingContext2D>*)
{
    if (!m_isContextLost)
        return;
    reset();
    m_isContextLost = false;
    if (contextLostRestoredEventsEnabled()) {
        RefPtrWillBeRawPtr<Event> event(Event::create(EventTypeNames::contextrestored));
        canvas()->dispatchEvent(event);
    }
}

void CanvasRenderingContext2D::reset()
{
    validateStateStack();
    unwindStateStack();
    m_stateStack.resize(1);
    m_stateStack.first() = adoptPtrWillBeNoop(new State());
    m_path.clear();
    validateStateStack();
}

void CanvasRenderingContext2D::restoreCanvasMatrixClipStack()
{
    SkCanvas* c = drawingCanvas();
    if (!c)
        return;
    WillBeHeapVector<OwnPtrWillBeMember<State>>::iterator currState;
    for (currState = m_stateStack.begin(); currState < m_stateStack.end(); currState++) {
        // The initial save accounts for the save installed by canvasElementElement::m_contextStateSaver
        c->save();
        c->setMatrix(SkMatrix::I());
        currState->get()->m_clipList.playback(c);
        c->setMatrix(affineTransformToSkMatrix(currState->get()->m_transform));
    }
}

// Important: Several of these properties are also stored in GraphicsContext's
// StrokeData. The default values that StrokeData uses may not the same values
// that the canvas 2d spec specifies. Make sure to sync the initial state of the
// GraphicsContext in HTMLCanvasElement::createImageBuffer()!
CanvasRenderingContext2D::State::State()
    : m_unrealizedSaveCount(0)
    , m_strokeStyle(CanvasStyle::createFromRGBA(Color::black))
    , m_fillStyle(CanvasStyle::createFromRGBA(Color::black))
    , m_lineWidth(1)
    , m_lineCap(ButtCap)
    , m_lineJoin(MiterJoin)
    , m_miterLimit(10)
    , m_shadowBlur(0)
    , m_shadowColor(Color::transparent)
    , m_globalAlpha(1)
    , m_globalComposite(SkXfermode::kSrcOver_Mode)
    , m_invertibleCTM(true)
    , m_lineDashOffset(0)
    , m_imageSmoothingEnabled(true)
    , m_textAlign(StartTextAlign)
    , m_textBaseline(AlphabeticTextBaseline)
    , m_direction(DirectionInherit)
    , m_unparsedFont(defaultFont)
    , m_realizedFont(false)
    , m_hasClip(false)
    , m_hasComplexClip(false)
{
}

CanvasRenderingContext2D::State::State(const State& other, ClipListCopyMode mode)
    : CSSFontSelectorClient()
    , m_unrealizedSaveCount(other.m_unrealizedSaveCount)
    , m_unparsedStrokeColor(other.m_unparsedStrokeColor)
    , m_unparsedFillColor(other.m_unparsedFillColor)
    , m_strokeStyle(other.m_strokeStyle)
    , m_fillStyle(other.m_fillStyle)
    , m_lineWidth(other.m_lineWidth)
    , m_lineCap(other.m_lineCap)
    , m_lineJoin(other.m_lineJoin)
    , m_miterLimit(other.m_miterLimit)
    , m_shadowOffset(other.m_shadowOffset)
    , m_shadowBlur(other.m_shadowBlur)
    , m_shadowColor(other.m_shadowColor)
    , m_globalAlpha(other.m_globalAlpha)
    , m_globalComposite(other.m_globalComposite)
    , m_transform(other.m_transform)
    , m_invertibleCTM(other.m_invertibleCTM)
    , m_lineDashOffset(other.m_lineDashOffset)
    , m_imageSmoothingEnabled(other.m_imageSmoothingEnabled)
    , m_textAlign(other.m_textAlign)
    , m_textBaseline(other.m_textBaseline)
    , m_direction(other.m_direction)
    , m_unparsedFont(other.m_unparsedFont)
    , m_font(other.m_font)
    , m_realizedFont(other.m_realizedFont)
    , m_hasClip(other.m_hasClip)
    , m_hasComplexClip(other.m_hasComplexClip)
{
    if (mode == CopyClipList) {
        m_clipList = other.m_clipList;
    }
    if (m_realizedFont)
        static_cast<CSSFontSelector*>(m_font.fontSelector())->registerForInvalidationCallbacks(this);
}

CanvasRenderingContext2D::State& CanvasRenderingContext2D::State::operator=(const State& other)
{
    if (this == &other)
        return *this;

#if !ENABLE(OILPAN)
    if (m_realizedFont)
        static_cast<CSSFontSelector*>(m_font.fontSelector())->unregisterForInvalidationCallbacks(this);
#endif

    m_unrealizedSaveCount = other.m_unrealizedSaveCount;
    m_unparsedStrokeColor = other.m_unparsedStrokeColor;
    m_unparsedFillColor = other.m_unparsedFillColor;
    m_strokeStyle = other.m_strokeStyle;
    m_fillStyle = other.m_fillStyle;
    m_lineWidth = other.m_lineWidth;
    m_lineCap = other.m_lineCap;
    m_lineJoin = other.m_lineJoin;
    m_miterLimit = other.m_miterLimit;
    m_shadowOffset = other.m_shadowOffset;
    m_shadowBlur = other.m_shadowBlur;
    m_shadowColor = other.m_shadowColor;
    m_globalAlpha = other.m_globalAlpha;
    m_globalComposite = other.m_globalComposite;
    m_transform = other.m_transform;
    m_invertibleCTM = other.m_invertibleCTM;
    m_imageSmoothingEnabled = other.m_imageSmoothingEnabled;
    m_textAlign = other.m_textAlign;
    m_textBaseline = other.m_textBaseline;
    m_direction = other.m_direction;
    m_unparsedFont = other.m_unparsedFont;
    m_font = other.m_font;
    m_realizedFont = other.m_realizedFont;
    m_hasClip = other.m_hasClip;
    m_hasComplexClip = other.m_hasComplexClip;
    m_clipList = other.m_clipList;

    if (m_realizedFont)
        static_cast<CSSFontSelector*>(m_font.fontSelector())->registerForInvalidationCallbacks(this);

    return *this;
}

CanvasRenderingContext2D::State::~State()
{
#if !ENABLE(OILPAN)
    if (m_realizedFont)
        static_cast<CSSFontSelector*>(m_font.fontSelector())->unregisterForInvalidationCallbacks(this);
#endif
}

void CanvasRenderingContext2D::State::fontsNeedUpdate(CSSFontSelector* fontSelector)
{
    ASSERT_ARG(fontSelector, fontSelector == m_font.fontSelector());
    ASSERT(m_realizedFont);

    m_font.update(fontSelector);
}

DEFINE_TRACE(CanvasRenderingContext2D::State)
{
    visitor->trace(m_strokeStyle);
    visitor->trace(m_fillStyle);
    CSSFontSelectorClient::trace(visitor);
}

void CanvasRenderingContext2D::realizeSaves(SkCanvas* canvas)
{
    validateStateStack();
    if (state().m_unrealizedSaveCount) {
        ASSERT(m_stateStack.size() >= 1);
        // Reduce the current state's unrealized count by one now,
        // to reflect the fact we are saving one state.
        m_stateStack.last()->m_unrealizedSaveCount--;
        m_stateStack.append(adoptPtrWillBeNoop(new State(state(), DontCopyClipList)));
        // Set the new state's unrealized count to 0, because it has no outstanding saves.
        // We need to do this explicitly because the copy constructor and operator= used
        // by the Vector operations copy the unrealized count from the previous state (in
        // turn necessary to support correct resizing and unwinding of the stack).
        m_stateStack.last()->m_unrealizedSaveCount = 0;
        if (!canvas)
            canvas = drawingCanvas();
        if (canvas)
            canvas->save();
        validateStateStack();
    }
}

void CanvasRenderingContext2D::restore()
{
    validateStateStack();
    if (state().m_unrealizedSaveCount) {
        // We never realized the save, so just record that it was unnecessary.
        --m_stateStack.last()->m_unrealizedSaveCount;
        return;
    }
    ASSERT(m_stateStack.size() >= 1);
    if (m_stateStack.size() <= 1)
        return;
    m_path.transform(state().m_transform);
    m_stateStack.removeLast();
    m_path.transform(state().m_transform.inverse());
    SkCanvas* c = drawingCanvas();
    if (c)
        c->restore();

    // Temporary code while crbug.com/453113 is a WIP: GraphicsContext state stack
    // is no longer exercised so state stored still stored in GC must be re-installed
    // after a restore.
    GraphicsContext* gc = drawingContext();
    if (gc) {
        state().m_fillStyle->applyFillColor(gc);
        state().m_strokeStyle->applyStrokeColor(gc);
        gc->setStrokeThickness(state().m_lineWidth);
        gc->setLineCap(state().m_lineCap);
        gc->setLineJoin(state().m_lineJoin);
        gc->setMiterLimit(state().m_miterLimit);
        applyLineDash();
        gc->setAlphaAsFloat(state().m_globalAlpha);
        gc->setCompositeOperation(state().m_globalComposite);
        gc->setImageInterpolationQuality(state().m_imageSmoothingEnabled ? CanvasDefaultInterpolationQuality : InterpolationNone);
        applyShadow();
    }

    validateStateStack();
}

static inline void convertCanvasStyleToUnionType(CanvasStyle* style, StringOrCanvasGradientOrCanvasPattern& returnValue)
{
    if (CanvasGradient* gradient = style->canvasGradient()) {
        returnValue.setCanvasGradient(gradient);
        return;
    }
    if (CanvasPattern* pattern = style->canvasPattern()) {
        returnValue.setCanvasPattern(pattern);
        return;
    }
    returnValue.setString(style->color());
}

void CanvasRenderingContext2D::strokeStyle(StringOrCanvasGradientOrCanvasPattern& returnValue) const
{
    convertCanvasStyleToUnionType(state().m_strokeStyle.get(), returnValue);
}

void CanvasRenderingContext2D::setStrokeStyle(const StringOrCanvasGradientOrCanvasPattern& style)
{
    ASSERT(!style.isNull());

    String colorString;
    RefPtrWillBeRawPtr<CanvasStyle> canvasStyle;
    if (style.isString()) {
        colorString = style.getAsString();
        if (colorString == state().m_unparsedStrokeColor)
            return;
        RGBA32 parsedColor = 0;
        if (!parseColorOrCurrentColor(parsedColor, colorString, canvas()))
            return;
        if (state().m_strokeStyle->isEquivalentRGBA(parsedColor)) {
            realizeSaves(nullptr);
            modifiableState().m_unparsedStrokeColor = colorString;
            return;
        }
        canvasStyle = CanvasStyle::createFromRGBA(parsedColor);
    } else if (style.isCanvasGradient()) {
        canvasStyle = CanvasStyle::createFromGradient(style.getAsCanvasGradient());
    } else if (style.isCanvasPattern()) {
        RefPtrWillBeRawPtr<CanvasPattern> canvasPattern = style.getAsCanvasPattern();

        if (canvas()->originClean() && !canvasPattern->originClean())
            canvas()->setOriginTainted();

        canvasStyle = CanvasStyle::createFromPattern(canvasPattern);
    }

    ASSERT(canvasStyle);

    SkCanvas* c = drawingCanvas();
    realizeSaves(c);
    modifiableState().m_strokeStyle = canvasStyle.release();
    if (!c)
        return;
    state().m_strokeStyle->applyStrokeColor(drawingContext());
    modifiableState().m_unparsedStrokeColor = colorString;
}

void CanvasRenderingContext2D::fillStyle(StringOrCanvasGradientOrCanvasPattern& returnValue) const
{
    convertCanvasStyleToUnionType(state().m_fillStyle.get(), returnValue);
}

void CanvasRenderingContext2D::setFillStyle(const StringOrCanvasGradientOrCanvasPattern& style)
{
    ASSERT(!style.isNull());
    validateStateStack();
    String colorString;
    RefPtrWillBeRawPtr<CanvasStyle> canvasStyle;
    if (style.isString()) {
        colorString = style.getAsString();
        if (colorString == state().m_unparsedFillColor)
            return;
        RGBA32 parsedColor = 0;
        if (!parseColorOrCurrentColor(parsedColor, colorString, canvas()))
            return;
        if (state().m_fillStyle->isEquivalentRGBA(parsedColor)) {
            realizeSaves(nullptr);
            modifiableState().m_unparsedFillColor = colorString;
            return;
        }
        canvasStyle = CanvasStyle::createFromRGBA(parsedColor);
    } else if (style.isCanvasGradient()) {
        canvasStyle = CanvasStyle::createFromGradient(style.getAsCanvasGradient());
    } else if (style.isCanvasPattern()) {
        RefPtrWillBeRawPtr<CanvasPattern> canvasPattern = style.getAsCanvasPattern();

        if (canvas()->originClean() && !canvasPattern->originClean())
            canvas()->setOriginTainted();

        canvasStyle = CanvasStyle::createFromPattern(canvasPattern);
    }

    ASSERT(canvasStyle);
    SkCanvas* c = drawingCanvas();
    if (!c)
        return;
    realizeSaves(c);

    modifiableState().m_fillStyle = canvasStyle.release();
    modifiableState().m_unparsedFillColor = colorString;
    state().m_fillStyle->applyFillColor(drawingContext());
}

float CanvasRenderingContext2D::lineWidth() const
{
    return state().m_lineWidth;
}

void CanvasRenderingContext2D::setLineWidth(float width)
{
    if (!std::isfinite(width) || width <= 0)
        return;
    if (state().m_lineWidth == width)
        return;
    SkCanvas* c = drawingCanvas();
    realizeSaves(c);
    modifiableState().m_lineWidth = width;
    if (!c)
        return;
    drawingContext()->setStrokeThickness(width);
}

String CanvasRenderingContext2D::lineCap() const
{
    return lineCapName(state().m_lineCap);
}

void CanvasRenderingContext2D::setLineCap(const String& s)
{
    LineCap cap;
    if (!parseLineCap(s, cap))
        return;
    if (state().m_lineCap == cap)
        return;
    SkCanvas* c = drawingCanvas();
    realizeSaves(c);
    modifiableState().m_lineCap = cap;
    if (!c)
        return;
    drawingContext()->setLineCap(cap);
}

String CanvasRenderingContext2D::lineJoin() const
{
    return lineJoinName(state().m_lineJoin);
}

void CanvasRenderingContext2D::setLineJoin(const String& s)
{
    LineJoin join;
    if (!parseLineJoin(s, join))
        return;
    if (state().m_lineJoin == join)
        return;
    SkCanvas* c = drawingCanvas();
    realizeSaves(c);
    modifiableState().m_lineJoin = join;
    if (!c)
        return;
    drawingContext()->setLineJoin(join);
}

float CanvasRenderingContext2D::miterLimit() const
{
    return state().m_miterLimit;
}

void CanvasRenderingContext2D::setMiterLimit(float limit)
{
    if (!std::isfinite(limit) || limit <= 0)
        return;
    if (state().m_miterLimit == limit)
        return;
    SkCanvas* c = drawingCanvas();
    realizeSaves(c);
    modifiableState().m_miterLimit = limit;
    if (!c)
        return;
    drawingContext()->setMiterLimit(limit);
}

float CanvasRenderingContext2D::shadowOffsetX() const
{
    return state().m_shadowOffset.width();
}

void CanvasRenderingContext2D::setShadowOffsetX(float x)
{
    if (!std::isfinite(x))
        return;
    if (state().m_shadowOffset.width() == x)
        return;
    realizeSaves(nullptr);
    modifiableState().m_shadowOffset.setWidth(x);
    applyShadow();
}

float CanvasRenderingContext2D::shadowOffsetY() const
{
    return state().m_shadowOffset.height();
}

void CanvasRenderingContext2D::setShadowOffsetY(float y)
{
    if (!std::isfinite(y))
        return;
    if (state().m_shadowOffset.height() == y)
        return;
    realizeSaves(nullptr);
    modifiableState().m_shadowOffset.setHeight(y);
    applyShadow();
}

float CanvasRenderingContext2D::shadowBlur() const
{
    return state().m_shadowBlur;
}

void CanvasRenderingContext2D::setShadowBlur(float blur)
{
    if (!std::isfinite(blur) || blur < 0)
        return;
    if (state().m_shadowBlur == blur)
        return;
    realizeSaves(nullptr);
    modifiableState().m_shadowBlur = blur;
    applyShadow();
}

String CanvasRenderingContext2D::shadowColor() const
{
    return Color(state().m_shadowColor).serialized();
}

void CanvasRenderingContext2D::setShadowColor(const String& color)
{
    RGBA32 rgba;
    if (!parseColorOrCurrentColor(rgba, color, canvas()))
        return;
    if (state().m_shadowColor == rgba)
        return;
    realizeSaves(nullptr);
    modifiableState().m_shadowColor = rgba;
    applyShadow();
}

const Vector<float>& CanvasRenderingContext2D::getLineDash() const
{
    return state().m_lineDash;
}

static bool lineDashSequenceIsValid(const Vector<float>& dash)
{
    for (size_t i = 0; i < dash.size(); i++) {
        if (!std::isfinite(dash[i]) || dash[i] < 0)
            return false;
    }
    return true;
}

void CanvasRenderingContext2D::setLineDash(const Vector<float>& dash)
{
    if (!lineDashSequenceIsValid(dash))
        return;

    realizeSaves(nullptr);
    modifiableState().m_lineDash = dash;
    // Spec requires the concatenation of two copies the dash list when the
    // number of elements is odd
    if (dash.size() % 2)
        modifiableState().m_lineDash.appendVector(dash);

    applyLineDash();
}

float CanvasRenderingContext2D::lineDashOffset() const
{
    return state().m_lineDashOffset;
}

void CanvasRenderingContext2D::setLineDashOffset(float offset)
{
    if (!std::isfinite(offset) || state().m_lineDashOffset == offset)
        return;

    realizeSaves(nullptr);
    modifiableState().m_lineDashOffset = offset;
    applyLineDash();
}

void CanvasRenderingContext2D::applyLineDash() const
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    DashArray convertedLineDash(state().m_lineDash.size());
    for (size_t i = 0; i < state().m_lineDash.size(); ++i)
        convertedLineDash[i] = static_cast<DashArrayElement>(state().m_lineDash[i]);
    c->setLineDash(convertedLineDash, state().m_lineDashOffset);
}

float CanvasRenderingContext2D::globalAlpha() const
{
    return state().m_globalAlpha;
}

void CanvasRenderingContext2D::setGlobalAlpha(float alpha)
{
    if (!(alpha >= 0 && alpha <= 1))
        return;
    if (state().m_globalAlpha == alpha)
        return;
    SkCanvas* c = drawingCanvas();
    realizeSaves(c);
    modifiableState().m_globalAlpha = alpha;
    if (!c)
        return;
    drawingContext()->setAlphaAsFloat(alpha);
}

String CanvasRenderingContext2D::globalCompositeOperation() const
{
    return compositeOperatorName(compositeOperatorFromSkia(state().m_globalComposite), blendModeFromSkia(state().m_globalComposite));
}

void CanvasRenderingContext2D::setGlobalCompositeOperation(const String& operation)
{
    CompositeOperator op = CompositeSourceOver;
    WebBlendMode blendMode = WebBlendModeNormal;
    // TODO(dshwang): Support nonstandard "darker" until M43. crbug.com/425628
    String operationName = operation;
    if (operation == "darker") {
        operationName = "darken";
        if (canvas())
            UseCounter::countDeprecation(canvas()->document(), UseCounter::CanvasRenderingContext2DCompositeOperationDarker);
    }
    if (!parseCompositeAndBlendOperator(operationName, op, blendMode))
        return;
    SkXfermode::Mode xfermode = WebCoreCompositeToSkiaComposite(op, blendMode);
    if (state().m_globalComposite == xfermode)
        return;
    SkCanvas* c = drawingCanvas();
    realizeSaves(c);
    modifiableState().m_globalComposite = xfermode;
    if (!c)
        return;
    drawingContext()->setCompositeOperation(xfermode);
}

void CanvasRenderingContext2D::setCurrentTransform(PassRefPtrWillBeRawPtr<SVGMatrixTearOff> passMatrixTearOff)
{
    RefPtrWillBeRawPtr<SVGMatrixTearOff> matrixTearOff = passMatrixTearOff;
    const AffineTransform& transform = matrixTearOff->value();
    setTransform(transform.a(), transform.b(), transform.c(), transform.d(), transform.e(), transform.f());
}

void CanvasRenderingContext2D::scale(float sx, float sy)
{
    SkCanvas* c = drawingCanvas();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;

    if (!std::isfinite(sx) || !std::isfinite(sy))
        return;

    AffineTransform newTransform = state().m_transform;
    newTransform.scaleNonUniform(sx, sy);
    if (state().m_transform == newTransform)
        return;

    realizeSaves(c);

    if (!newTransform.isInvertible()) {
        modifiableState().m_invertibleCTM = false;
        return;
    }

    modifiableState().m_transform = newTransform;
    c->scale(sx, sy);
    m_path.transform(AffineTransform().scaleNonUniform(1.0 / sx, 1.0 / sy));
}

void CanvasRenderingContext2D::rotate(float angleInRadians)
{
    SkCanvas* c = drawingCanvas();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;

    if (!std::isfinite(angleInRadians))
        return;

    AffineTransform newTransform = state().m_transform;
    newTransform.rotateRadians(angleInRadians);
    if (state().m_transform == newTransform)
        return;

    realizeSaves(c);

    if (!newTransform.isInvertible()) {
        modifiableState().m_invertibleCTM = false;
        return;
    }

    modifiableState().m_transform = newTransform;
    c->rotate(angleInRadians * (180.0f / piFloat));
    m_path.transform(AffineTransform().rotateRadians(-angleInRadians));
}

void CanvasRenderingContext2D::translate(float tx, float ty)
{
    SkCanvas* c = drawingCanvas();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;

    if (!std::isfinite(tx) || !std::isfinite(ty))
        return;

    AffineTransform newTransform = state().m_transform;
    newTransform.translate(tx, ty);
    if (state().m_transform == newTransform)
        return;

    realizeSaves(c);

    if (!newTransform.isInvertible()) {
        modifiableState().m_invertibleCTM = false;
        return;
    }

    modifiableState().m_transform = newTransform;
    c->translate(tx, ty);
    m_path.transform(AffineTransform().translate(-tx, -ty));
}

void CanvasRenderingContext2D::transform(float m11, float m12, float m21, float m22, float dx, float dy)
{
    SkCanvas* c = drawingCanvas();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;

    if (!std::isfinite(m11) || !std::isfinite(m21) || !std::isfinite(dx) || !std::isfinite(m12) || !std::isfinite(m22) || !std::isfinite(dy))
        return;

    AffineTransform transform(m11, m12, m21, m22, dx, dy);
    AffineTransform newTransform = state().m_transform * transform;
    if (state().m_transform == newTransform)
        return;

    realizeSaves(c);

    modifiableState().m_transform = newTransform;
    if (!newTransform.isInvertible()) {
        modifiableState().m_invertibleCTM = false;
        return;
    }

    c->concat(affineTransformToSkMatrix(transform));
    m_path.transform(transform.inverse());
}

void CanvasRenderingContext2D::resetTransform()
{
    SkCanvas* c = drawingCanvas();
    if (!c)
        return;

    AffineTransform ctm = state().m_transform;
    bool invertibleCTM = state().m_invertibleCTM;
    // It is possible that CTM is identity while CTM is not invertible.
    // When CTM becomes non-invertible, realizeSaves() can make CTM identity.
    if (ctm.isIdentity() && invertibleCTM)
        return;

    realizeSaves(c);
    // resetTransform() resolves the non-invertible CTM state.
    modifiableState().m_transform.makeIdentity();
    modifiableState().m_invertibleCTM = true;
    c->setMatrix(affineTransformToSkMatrix(canvas()->baseTransform()));

    if (invertibleCTM)
        m_path.transform(ctm);
    // When else, do nothing because all transform methods didn't update m_path when CTM became non-invertible.
    // It means that resetTransform() restores m_path just before CTM became non-invertible.
}

void CanvasRenderingContext2D::setTransform(float m11, float m12, float m21, float m22, float dx, float dy)
{
    SkCanvas* c = drawingCanvas();
    if (!c)
        return;

    if (!std::isfinite(m11) || !std::isfinite(m21) || !std::isfinite(dx) || !std::isfinite(m12) || !std::isfinite(m22) || !std::isfinite(dy))
        return;

    resetTransform();
    transform(m11, m12, m21, m22, dx, dy);
}

void CanvasRenderingContext2D::beginPath()
{
    m_path.clear();
}

static bool validateRectForCanvas(float& x, float& y, float& width, float& height)
{
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) || !std::isfinite(height))
        return false;

    if (!width && !height)
        return false;

    if (width < 0) {
        width = -width;
        x -= width;
    }

    if (height < 0) {
        height = -height;
        y -= height;
    }

    return true;
}

static bool isFullCanvasCompositeMode(SkXfermode::Mode op)
{
    // See 4.8.11.1.3 Compositing
    // CompositeSourceAtop and CompositeDestinationOut are not listed here as the platforms already
    // implement the specification's behavior.
    return op == SkXfermode::kSrcIn_Mode || op == SkXfermode::kSrcOut_Mode || op == SkXfermode::kDstIn_Mode || op == SkXfermode::kDstATop_Mode;
}

static SkPath::FillType parseWinding(const String& windingRuleString)
{
    if (windingRuleString == "nonzero")
        return SkPath::kWinding_FillType;
    if (windingRuleString == "evenodd")
        return SkPath::kEvenOdd_FillType;

    ASSERT_NOT_REACHED();
    return SkPath::kEvenOdd_FillType;
}

static bool isPathExpensive(const Path& path)
{
    const SkPath& skPath = path.skPath();
    if (ExpensiveCanvasHeuristicParameters::ConcavePathsAreExpensive && !skPath.isConvex())
        return true;

    if (skPath.countPoints() > ExpensiveCanvasHeuristicParameters::ExpensivePathPointCount)
        return true;

    return false;
}

void CanvasRenderingContext2D::fillInternal(const Path& path, const String& windingRuleString)
{
    if (path.isEmpty()) {
        return;
    }
    GraphicsContext* c = drawingContext();
    if (!c) {
        return;
    }
    if (!state().m_invertibleCTM) {
        return;
    }
    FloatRect clipBounds;
    if (!c->getTransformedClipBounds(&clipBounds)) {
        return;
    }

    // If gradient size is zero, then paint nothing.
    Gradient* gradient = c->fillGradient();
    if (gradient && gradient->isZeroSize()) {
        return;
    }

    WindRule windRule = c->fillRule();
    c->setFillRule(SkFillTypeToWindRule(parseWinding(windingRuleString)));

    if (isFullCanvasCompositeMode(state().m_globalComposite)) {
        fullCanvasCompositedDraw(bind(&GraphicsContext::fillPath, c, path));
        didDraw(clipBounds);
    } else if (state().m_globalComposite == SkXfermode::kSrc_Mode) {
        clearCanvas();
        c->clearShadow();
        c->fillPath(path);
        applyShadow(DrawShadowAndForeground);
        didDraw(clipBounds);
    } else {
        FloatRect dirtyRect;
        if (computeDirtyRect(path.boundingRect(), clipBounds, &dirtyRect)) {
            c->fillPath(path);
            didDraw(dirtyRect);
        }
    }

    c->setFillRule(windRule);

    if (isPathExpensive(path)) {
        ImageBuffer* buffer = canvas()->buffer();
        if (buffer)
            buffer->setHasExpensiveOp();
    }
}

void CanvasRenderingContext2D::fill(const String& windingRuleString)
{
    fillInternal(m_path, windingRuleString);
}

void CanvasRenderingContext2D::fill(Path2D* domPath, const String& windingRuleString)
{
    fillInternal(domPath->path(), windingRuleString);
}

void CanvasRenderingContext2D::strokeInternal(const Path& path)
{
    if (path.isEmpty()) {
        return;
    }
    GraphicsContext* c = drawingContext();
    if (!c) {
        return;
    }
    if (!state().m_invertibleCTM) {
        return;
    }
    FloatRect clipBounds;
    if (!c->getTransformedClipBounds(&clipBounds))
        return;

    // If gradient size is zero, then paint nothing.
    Gradient* gradient = c->strokeGradient();
    if (gradient && gradient->isZeroSize()) {
        return;
    }

    if (isFullCanvasCompositeMode(state().m_globalComposite)) {
        fullCanvasCompositedDraw(bind(&GraphicsContext::strokePath, c, path));
        didDraw(clipBounds);
    } else if (state().m_globalComposite == SkXfermode::kSrc_Mode) {
        clearCanvas();
        c->clearShadow();
        c->strokePath(path);
        applyShadow(DrawShadowAndForeground);
        didDraw(clipBounds);
    } else {
        FloatRect bounds = path.boundingRect();
        inflateStrokeRect(bounds);
        FloatRect dirtyRect;
        if (computeDirtyRect(bounds, clipBounds, &dirtyRect)) {
            c->strokePath(path);
            didDraw(dirtyRect);
        }
    }

    if (isPathExpensive(path)) {
        ImageBuffer* buffer = canvas()->buffer();
        if (buffer)
            buffer->setHasExpensiveOp();
    }
}

void CanvasRenderingContext2D::stroke()
{
    strokeInternal(m_path);
}

void CanvasRenderingContext2D::stroke(Path2D* domPath)
{
    strokeInternal(domPath->path());
}

void CanvasRenderingContext2D::clipInternal(const Path& path, const String& windingRuleString)
{
    SkCanvas* c = drawingCanvas();
    if (!c) {
        return;
    }
    if (!state().m_invertibleCTM) {
        return;
    }

    realizeSaves(c);

    SkPath skPath = path.skPath();
    skPath.setFillType(parseWinding(windingRuleString));
    ImageBuffer* buffer = canvas()->buffer();
    if (buffer && buffer->needsClipTracking()) {
        modifiableState().m_clipList.clipPath(skPath, m_clipAntialiasing, affineTransformToSkMatrix(state().m_transform));
    }

    c->clipPath(skPath, SkRegion::kIntersect_Op, m_clipAntialiasing == AntiAliased);
    if (ExpensiveCanvasHeuristicParameters::ComplexClipsAreExpensive && !skPath.isRect(0)) {
        if (buffer)
            buffer->setHasExpensiveOp();
        modifiableState().m_hasComplexClip = true;
    }
    modifiableState().m_hasClip = true;
}

void CanvasRenderingContext2D::clip(const String& windingRuleString)
{
    clipInternal(m_path, windingRuleString);
}

void CanvasRenderingContext2D::clip(Path2D* domPath, const String& windingRuleString)
{
    clipInternal(domPath->path(), windingRuleString);
}

bool CanvasRenderingContext2D::isPointInPath(const float x, const float y, const String& windingRuleString)
{
    return isPointInPathInternal(m_path, x, y, windingRuleString);
}

bool CanvasRenderingContext2D::isPointInPath(Path2D* domPath, const float x, const float y, const String& windingRuleString)
{
    return isPointInPathInternal(domPath->path(), x, y, windingRuleString);
}

bool CanvasRenderingContext2D::isPointInPathInternal(const Path& path, const float x, const float y, const String& windingRuleString)
{
    SkCanvas* c = drawingCanvas();
    if (!c)
        return false;
    if (!state().m_invertibleCTM)
        return false;

    FloatPoint point(x, y);
    if (!std::isfinite(point.x()) || !std::isfinite(point.y()))
        return false;
    AffineTransform ctm = state().m_transform;
    FloatPoint transformedPoint = ctm.inverse().mapPoint(point);

    return path.contains(transformedPoint, SkFillTypeToWindRule(parseWinding(windingRuleString)));
}

bool CanvasRenderingContext2D::isPointInStroke(const float x, const float y)
{
    return isPointInStrokeInternal(m_path, x, y);
}

bool CanvasRenderingContext2D::isPointInStroke(Path2D* domPath, const float x, const float y)
{
    return isPointInStrokeInternal(domPath->path(), x, y);
}

bool CanvasRenderingContext2D::isPointInStrokeInternal(const Path& path, const float x, const float y)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return false;
    if (!state().m_invertibleCTM)
        return false;

    FloatPoint point(x, y);
    if (!std::isfinite(point.x()) || !std::isfinite(point.y()))
        return false;
    AffineTransform ctm = state().m_transform;
    FloatPoint transformedPoint = ctm.inverse().mapPoint(point);

    StrokeData strokeData;
    strokeData.setThickness(lineWidth());
    strokeData.setLineCap(getLineCap());
    strokeData.setLineJoin(getLineJoin());
    strokeData.setMiterLimit(miterLimit());
    strokeData.setLineDash(getLineDash(), lineDashOffset());
    return path.strokeContains(transformedPoint, strokeData);
}

void CanvasRenderingContext2D::scrollPathIntoView()
{
    scrollPathIntoViewInternal(m_path);
}

void CanvasRenderingContext2D::scrollPathIntoView(Path2D* path2d)
{
    scrollPathIntoViewInternal(path2d->path());
}

void CanvasRenderingContext2D::scrollPathIntoViewInternal(const Path& path)
{
    LayoutObject* renderer = canvas()->renderer();
    RenderBox* renderBox = canvas()->renderBox();
    if (!renderer || !renderBox || !state().m_invertibleCTM || path.isEmpty())
        return;

    canvas()->document().updateLayoutIgnorePendingStylesheets();

    // Apply transformation and get the bounding rect
    Path transformedPath = path;
    transformedPath.transform(state().m_transform);
    FloatRect boundingRect = transformedPath.boundingRect();

    // Offset by the canvas rect
    LayoutRect pathRect(boundingRect);
    IntRect canvasRect = renderBox->absoluteContentBox();
    pathRect.move(canvasRect.x(), canvasRect.y());

    renderer->scrollRectToVisible(
        pathRect, ScrollAlignment::alignCenterAlways, ScrollAlignment::alignTopAlways);

    // TODO: should implement "inform the user" that the caret and/or
    // selection the specified rectangle of the canvas. See http://crbug.com/357987
}

void CanvasRenderingContext2D::clearRect(float x, float y, float width, float height)
{
    if (!validateRectForCanvas(x, y, width, height))
        return;

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;

    c->clearShadow();
    c->setAlphaAsFloat(1);
    c->setCompositeOperation(SkXfermode::kClear_Mode);

    // call to didDraw is taken care of in fillRect
    fillRect(x, y, width, height);

    applyShadow(DrawShadowAndForeground);
    c->setAlphaAsFloat(state().m_globalAlpha);
    c->setCompositeOperation(state().m_globalComposite);

    if (m_hitRegionManager) {
        FloatRect rect(x, y, width, height);
        m_hitRegionManager->removeHitRegionsInRect(rect, state().m_transform);
    }
}

// FIXME(crbug.com/425531): Funtional.h cannot handle override function signature.
static void fillRectOnContext(GraphicsContext* context, const FloatRect& rect)
{
    context->fillRect(rect);
}

static void strokeRectOnContext(GraphicsContext* context, const FloatRect& rect)
{
    context->strokeRect(rect);
}

void CanvasRenderingContext2D::fillRect(float x, float y, float width, float height)
{
    if (!validateRectForCanvas(x, y, width, height))
        return;

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;
    FloatRect clipBounds;
    if (!c->getTransformedClipBounds(&clipBounds))
        return;

    // from the HTML5 Canvas spec:
    // If x0 = x1 and y0 = y1, then the linear gradient must paint nothing
    // If x0 = x1 and y0 = y1 and r0 = r1, then the radial gradient must paint nothing
    Gradient* gradient = c->fillGradient();
    if (gradient && gradient->isZeroSize())
        return;

    FloatRect rect(x, y, width, height);
    if (rectContainsTransformedRect(rect, clipBounds)) {
        checkOverdraw(rect, &c->fillPaint(), NoImage, ClipFill);
        c->fillRect(rect);
        didDraw(clipBounds);
    } else if (isFullCanvasCompositeMode(state().m_globalComposite)) {
        fullCanvasCompositedDraw(bind(&fillRectOnContext, c, rect));
        didDraw(clipBounds);
    } else if (state().m_globalComposite == SkXfermode::kSrc_Mode) {
        clearCanvas();
        c->clearShadow(); // Takes care of signaling the overdraw
        c->fillRect(rect);
        applyShadow(DrawShadowAndForeground);
        didDraw(clipBounds);
    } else {
        FloatRect dirtyRect;
        if (computeDirtyRect(rect, clipBounds, &dirtyRect)) {
            c->fillRect(rect);
            didDraw(dirtyRect);
        }
    }
}

void CanvasRenderingContext2D::strokeRect(float x, float y, float width, float height)
{
    if (!validateRectForCanvas(x, y, width, height))
        return;

    if (!(state().m_lineWidth >= 0))
        return;

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;
    FloatRect clipBounds;
    if (!c->getTransformedClipBounds(&clipBounds))
        return;

    // If gradient size is zero, then paint nothing.
    Gradient* gradient = c->strokeGradient();
    if (gradient && gradient->isZeroSize())
        return;

    FloatRect rect(x, y, width, height);
    if (isFullCanvasCompositeMode(state().m_globalComposite)) {
        fullCanvasCompositedDraw(bind(&strokeRectOnContext, c, rect));
        didDraw(clipBounds);
    } else if (state().m_globalComposite == SkXfermode::kSrc_Mode) {
        clearCanvas();
        c->clearShadow();
        c->strokeRect(rect);
        applyShadow(DrawShadowAndForeground);
        didDraw(clipBounds);
    } else {
        FloatRect boundingRect = rect;
        boundingRect.inflate(state().m_lineWidth / 2);
        FloatRect dirtyRect;
        if (computeDirtyRect(boundingRect, clipBounds, &dirtyRect)) {
            c->strokeRect(rect);
            didDraw(dirtyRect);
        }
    }
}

void CanvasRenderingContext2D::applyShadow(ShadowMode shadowMode)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    if (shouldDrawShadows()) {
        c->setShadow(state().m_shadowOffset, state().m_shadowBlur, state().m_shadowColor,
            DrawLooperBuilder::ShadowIgnoresTransforms, DrawLooperBuilder::ShadowRespectsAlpha, shadowMode);
    } else {
        c->clearShadow();
    }
}

bool CanvasRenderingContext2D::shouldDrawShadows() const
{
    return alphaChannel(state().m_shadowColor) && (state().m_shadowBlur || !state().m_shadowOffset.isZero());
}

static inline FloatRect normalizeRect(const FloatRect& rect)
{
    return FloatRect(std::min(rect.x(), rect.maxX()),
        std::min(rect.y(), rect.maxY()),
        std::max(rect.width(), -rect.width()),
        std::max(rect.height(), -rect.height()));
}

static inline void clipRectsToImageRect(const FloatRect& imageRect, FloatRect* srcRect, FloatRect* dstRect)
{
    if (imageRect.contains(*srcRect))
        return;

    // Compute the src to dst transform
    FloatSize scale(dstRect->size().width() / srcRect->size().width(), dstRect->size().height() / srcRect->size().height());
    FloatPoint scaledSrcLocation = srcRect->location();
    scaledSrcLocation.scale(scale.width(), scale.height());
    FloatSize offset = dstRect->location() - scaledSrcLocation;

    srcRect->intersect(imageRect);

    // To clip the destination rectangle in the same proportion, transform the clipped src rect
    *dstRect = *srcRect;
    dstRect->scale(scale.width(), scale.height());
    dstRect->move(offset);
}

static inline CanvasImageSource* toImageSourceInternal(const CanvasImageSourceUnion& value)
{
    if (value.isHTMLImageElement())
        return value.getAsHTMLImageElement().get();
    if (value.isHTMLVideoElement())
        return value.getAsHTMLVideoElement().get();
    if (value.isHTMLCanvasElement())
        return value.getAsHTMLCanvasElement().get();
    if (value.isImageBitmap())
        return value.getAsImageBitmap().get();
    ASSERT_NOT_REACHED();
    return nullptr;
}

void CanvasRenderingContext2D::drawImage(const CanvasImageSourceUnion& imageSource, float x, float y, ExceptionState& exceptionState)
{
    CanvasImageSource* imageSourceInternal = toImageSourceInternal(imageSource);
    FloatSize sourceRectSize = imageSourceInternal->sourceSize();
    FloatSize destRectSize = imageSourceInternal->defaultDestinationSize();
    drawImage(imageSourceInternal, 0, 0, sourceRectSize.width(), sourceRectSize.height(), x, y, destRectSize.width(), destRectSize.height(), exceptionState);
}

void CanvasRenderingContext2D::drawImage(const CanvasImageSourceUnion& imageSource,
    float x, float y, float width, float height, ExceptionState& exceptionState)
{
    CanvasImageSource* imageSourceInternal = toImageSourceInternal(imageSource);
    FloatSize sourceRectSize = imageSourceInternal->sourceSize();
    drawImage(imageSourceInternal, 0, 0, sourceRectSize.width(), sourceRectSize.height(), x, y, width, height, exceptionState);
}

void CanvasRenderingContext2D::drawImage(const CanvasImageSourceUnion& imageSource,
    float sx, float sy, float sw, float sh,
    float dx, float dy, float dw, float dh, ExceptionState& exceptionState)
{
    CanvasImageSource* imageSourceInternal = toImageSourceInternal(imageSource);
    drawImage(imageSourceInternal, sx, sy, sw, sh, dx, dy, dw, dh, exceptionState);
}

static void drawVideo(SkCanvas* c, GraphicsContext* gc, CanvasImageSource* imageSource, FloatRect srcRect, FloatRect dstRect)
{
    HTMLVideoElement* video = static_cast<HTMLVideoElement*>(imageSource);
    c->save();
    c->clipRect(WebCoreFloatRectToSKRect(dstRect));
    c->translate(dstRect.x(), dstRect.y());
    c->scale(dstRect.width() / srcRect.width(), dstRect.height() / srcRect.height());
    c->translate(-srcRect.x(), -srcRect.y());
    video->paintCurrentFrameInContext(gc, IntRect(IntPoint(), IntSize(video->videoWidth(), video->videoHeight())));
    c->restore();
}

static void drawImageOnContext(SkCanvas* c, GraphicsContext* gc, CanvasImageSource* imageSource, Image* image, const FloatRect& srcRect, const FloatRect& dstRect)
{
    if (!imageSource->isVideoElement()) {
        gc->drawImage(image, dstRect, srcRect, gc->compositeOperation());
    } else {
        drawVideo(c, gc, static_cast<HTMLVideoElement*>(imageSource), srcRect, dstRect);
    }
}

void CanvasRenderingContext2D::drawImage(CanvasImageSource* imageSource,
    float sx, float sy, float sw, float sh,
    float dx, float dy, float dw, float dh, ExceptionState& exceptionState)
{
    RefPtr<Image> image;
    SourceImageStatus sourceImageStatus = InvalidSourceImageStatus;
    if (!imageSource->isVideoElement()) {
        SourceImageMode mode = canvas() == imageSource ? CopySourceImageIfVolatile : DontCopySourceImage; // Thunking for ==
        image = imageSource->getSourceImageForCanvas(mode, &sourceImageStatus);
        if (sourceImageStatus == UndecodableSourceImageStatus)
            exceptionState.throwDOMException(InvalidStateError, "The HTMLImageElement provided is in the 'broken' state.");
        if (!image || !image->width() || !image->height())
            return;
    }

    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    if (!state().m_invertibleCTM)
        return;

    if (!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(dw) || !std::isfinite(dh)
        || !std::isfinite(sx) || !std::isfinite(sy) || !std::isfinite(sw) || !std::isfinite(sh)
        || !dw || !dh || !sw || !sh)
        return;

    FloatRect clipBounds;
    if (!c->getTransformedClipBounds(&clipBounds))
        return;

    FloatRect srcRect = normalizeRect(FloatRect(sx, sy, sw, sh));
    FloatRect dstRect = normalizeRect(FloatRect(dx, dy, dw, dh));

    clipRectsToImageRect(FloatRect(FloatPoint(), imageSource->sourceSize()), &srcRect, &dstRect);

    imageSource->adjustDrawRects(&srcRect, &dstRect);

    if (srcRect.isEmpty())
        return;

    if (imageSource->isVideoElement())
        canvas()->buffer()->willDrawVideo();

    // FIXME: crbug.com/447218
    // We make the destination canvas fall out of display list mode by calling
    // willAccessPixels. This is to prevent run-away memory consumption caused by SkSurface
    // copyOnWrite when the source canvas is animated and consumed at a rate higher than the
    // presentation frame rate of the destination canvas.
    if (imageSource->isCanvasElement())
        canvas()->buffer()->willAccessPixels();

    if (rectContainsTransformedRect(dstRect, clipBounds)) {
        checkOverdraw(dstRect, &c->fillPaint(), imageSource->isOpaque() ? OpaqueImage : NonOpaqueImage, ClipFill);
        drawImageOnContext(drawingCanvas(), c, imageSource, image.get(), srcRect, dstRect);
        didDraw(clipBounds);
    } else if (isFullCanvasCompositeMode(state().m_globalComposite)) {
        fullCanvasCompositedDraw(bind(&drawImageOnContext, drawingCanvas(), c, imageSource, image.get(), srcRect, dstRect));
        didDraw(clipBounds);
    } else if (state().m_globalComposite == SkXfermode::kSrc_Mode) {
        clearCanvas(); // takes care of signaling an overdraw
        drawImageOnContext(drawingCanvas(), c, imageSource, image.get(), srcRect, dstRect);
        didDraw(clipBounds);
    } else {
        FloatRect dirtyRect;
        if (computeDirtyRect(dstRect, clipBounds, &dirtyRect)) {
            drawImageOnContext(drawingCanvas(), c, imageSource, image.get(), srcRect, dstRect);
            didDraw(dirtyRect);
        }
    }

    validateStateStack();

    if (ExpensiveCanvasHeuristicParameters::SVGImageSourcesAreExpensive && image && image->isSVGImage()) {
        ImageBuffer* buffer = canvas()->buffer();
        if (buffer)
            buffer->setHasExpensiveOp();
    }

    if (sourceImageStatus == ExternalSourceImageStatus && isAccelerated() && canvas()->buffer())
        canvas()->buffer()->flush();

    if (canvas()->originClean() && wouldTaintOrigin(imageSource))
        canvas()->setOriginTainted();
}

void CanvasRenderingContext2D::clearCanvas()
{
    FloatRect canvasRect(0, 0, canvas()->width(), canvas()->height());
    SkCanvas* c = drawingCanvas();
    if (!c)
        return;

    checkOverdraw(canvasRect, 0, NoImage, ClipFill);
    // Must not use 'c' beyond this point in case checkOverdraw substitutes the recording
    // canvas in order to clear a draw command backlog.
    drawingCanvas()->clear(m_hasAlpha ? SK_ColorTRANSPARENT : SK_ColorBLACK);
}

bool CanvasRenderingContext2D::rectContainsTransformedRect(const FloatRect& rect, const FloatRect& transformedRect) const
{
    FloatQuad quad(rect);
    FloatQuad transformedQuad(transformedRect);
    return state().m_transform.mapQuad(quad).containsQuad(transformedQuad);
}

void CanvasRenderingContext2D::fullCanvasCompositedDraw(PassOwnPtr<Closure> draw)
{
    ASSERT(isFullCanvasCompositeMode(state().m_globalComposite));

    GraphicsContext* c = drawingContext();
    ASSERT(c);

    if (shouldDrawShadows()) {
        // unroll into two independently composited passes if drawing shadows
        c->beginLayer(1, state().m_globalComposite);
        c->setCompositeOperation(SkXfermode::kSrcOver_Mode);
        applyShadow(DrawShadowOnly);
        (*draw)();
        c->setCompositeOperation(state().m_globalComposite);
        c->endLayer();
    }

    c->beginLayer(1, state().m_globalComposite);
    c->clearShadow();
    c->setCompositeOperation(SkXfermode::kSrcOver_Mode);
    (*draw)();
    c->setCompositeOperation(state().m_globalComposite);
    c->endLayer();
    applyShadow(DrawShadowAndForeground); // go back to normal shadows mode
}

PassRefPtrWillBeRawPtr<CanvasGradient> CanvasRenderingContext2D::createLinearGradient(float x0, float y0, float x1, float y1)
{
    RefPtrWillBeRawPtr<CanvasGradient> gradient = CanvasGradient::create(FloatPoint(x0, y0), FloatPoint(x1, y1));
    return gradient.release();
}

PassRefPtrWillBeRawPtr<CanvasGradient> CanvasRenderingContext2D::createRadialGradient(float x0, float y0, float r0, float x1, float y1, float r1, ExceptionState& exceptionState)
{
    if (r0 < 0 || r1 < 0) {
        exceptionState.throwDOMException(IndexSizeError, String::format("The %s provided is less than 0.", r0 < 0 ? "r0" : "r1"));
        return nullptr;
    }

    RefPtrWillBeRawPtr<CanvasGradient> gradient = CanvasGradient::create(FloatPoint(x0, y0), r0, FloatPoint(x1, y1), r1);
    return gradient.release();
}

PassRefPtrWillBeRawPtr<CanvasPattern> CanvasRenderingContext2D::createPattern(const CanvasImageSourceUnion& imageSource,
    const String& repetitionType, ExceptionState& exceptionState)
{
    Pattern::RepeatMode repeatMode = CanvasPattern::parseRepetitionType(repetitionType, exceptionState);
    if (exceptionState.hadException())
        return nullptr;

    SourceImageStatus status;
    CanvasImageSource* imageSourceInternal = toImageSourceInternal(imageSource);
    RefPtr<Image> imageForRendering = imageSourceInternal->getSourceImageForCanvas(CopySourceImageIfVolatile, &status);

    switch (status) {
    case NormalSourceImageStatus:
        break;
    case ZeroSizeCanvasSourceImageStatus:
        exceptionState.throwDOMException(InvalidStateError, String::format("The canvas %s is 0.", imageSourceInternal->sourceSize().width() ? "height" : "width"));
        return nullptr;
    case UndecodableSourceImageStatus:
        exceptionState.throwDOMException(InvalidStateError, "Source image is in the 'broken' state.");
        return nullptr;
    case InvalidSourceImageStatus:
        imageForRendering = Image::nullImage();
        break;
    case IncompleteSourceImageStatus:
        return nullptr;
    default:
    case ExternalSourceImageStatus: // should not happen when mode is CopySourceImageIfVolatile
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    ASSERT(imageForRendering);

    bool originClean = !wouldTaintOrigin(imageSourceInternal);

    return CanvasPattern::create(imageForRendering.release(), repeatMode, originClean);
}

bool CanvasRenderingContext2D::computeDirtyRect(const FloatRect& localRect, FloatRect* dirtyRect)
{
    FloatRect clipBounds;
    if (!drawingContext()->getTransformedClipBounds(&clipBounds))
        return false;
    return computeDirtyRect(localRect, clipBounds, dirtyRect);
}

bool CanvasRenderingContext2D::computeDirtyRect(const FloatRect& localRect, const FloatRect& transformedClipBounds, FloatRect* dirtyRect)
{
    FloatRect canvasRect = state().m_transform.mapRect(localRect);

    if (alphaChannel(state().m_shadowColor)) {
        FloatRect shadowRect(canvasRect);
        shadowRect.move(state().m_shadowOffset);
        shadowRect.inflate(state().m_shadowBlur);
        canvasRect.unite(shadowRect);
    }

    canvasRect.intersect(transformedClipBounds);
    if (canvasRect.isEmpty())
        return false;

    if (dirtyRect)
        *dirtyRect = canvasRect;

    return true;
}

void CanvasRenderingContext2D::didDraw(const FloatRect& dirtyRect)
{
    if (dirtyRect.isEmpty())
        return;

    if (ExpensiveCanvasHeuristicParameters::BlurredShadowsAreExpensive && shouldDrawShadows() && state().m_shadowBlur > 0) {
        ImageBuffer* buffer = canvas()->buffer();
        if (buffer)
            buffer->setHasExpensiveOp();
    }

    canvas()->didDraw(dirtyRect);
}

SkCanvas* CanvasRenderingContext2D::drawingCanvas() const
{
    if (isContextLost())
        return nullptr;
    return canvas()->drawingCanvas();
}

GraphicsContext* CanvasRenderingContext2D::drawingContext() const
{
    if (isContextLost())
        return nullptr;
    return canvas()->drawingContext();
}

PassRefPtrWillBeRawPtr<ImageData> CanvasRenderingContext2D::createImageData(PassRefPtrWillBeRawPtr<ImageData> imageData) const
{
    return ImageData::create(imageData->size());
}

PassRefPtrWillBeRawPtr<ImageData> CanvasRenderingContext2D::createImageData(float sw, float sh, ExceptionState& exceptionState) const
{
    if (!sw || !sh) {
        exceptionState.throwDOMException(IndexSizeError, String::format("The source %s is 0.", sw ? "height" : "width"));
        return nullptr;
    }

    FloatSize logicalSize(fabs(sw), fabs(sh));
    if (!logicalSize.isExpressibleAsIntSize())
        return nullptr;

    IntSize size = expandedIntSize(logicalSize);
    if (size.width() < 1)
        size.setWidth(1);
    if (size.height() < 1)
        size.setHeight(1);

    return ImageData::create(size);
}

PassRefPtrWillBeRawPtr<ImageData> CanvasRenderingContext2D::getImageData(float sx, float sy, float sw, float sh, ExceptionState& exceptionState) const
{
    if (!canvas()->originClean())
        exceptionState.throwSecurityError("The canvas has been tainted by cross-origin data.");
    else if (!sw || !sh)
        exceptionState.throwDOMException(IndexSizeError, String::format("The source %s is 0.", sw ? "height" : "width"));

    if (exceptionState.hadException())
        return nullptr;

    if (sw < 0) {
        sx += sw;
        sw = -sw;
    }
    if (sh < 0) {
        sy += sh;
        sh = -sh;
    }

    FloatRect logicalRect(sx, sy, sw, sh);
    if (logicalRect.width() < 1)
        logicalRect.setWidth(1);
    if (logicalRect.height() < 1)
        logicalRect.setHeight(1);
    if (!logicalRect.isExpressibleAsIntRect())
        return nullptr;

    IntRect imageDataRect = enclosingIntRect(logicalRect);
    ImageBuffer* buffer = canvas()->buffer();
    if (!buffer || isContextLost())
        return ImageData::create(imageDataRect.size());

    WTF::ArrayBufferContents contents;
    if (!buffer->getImageData(Unmultiplied, imageDataRect, contents))
        return nullptr;

    RefPtr<DOMArrayBuffer> arrayBuffer = DOMArrayBuffer::create(contents);
    return ImageData::create(
        imageDataRect.size(),
        DOMUint8ClampedArray::create(arrayBuffer, 0, arrayBuffer->byteLength()));
}

void CanvasRenderingContext2D::putImageData(ImageData* data, float dx, float dy)
{
    putImageData(data, dx, dy, 0, 0, data->width(), data->height());
}

void CanvasRenderingContext2D::putImageData(ImageData* data, float dx, float dy, float dirtyX, float dirtyY, float dirtyWidth, float dirtyHeight)
{
    ImageBuffer* buffer = canvas()->buffer();
    if (!buffer)
        return;

    if (dirtyWidth < 0) {
        dirtyX += dirtyWidth;
        dirtyWidth = -dirtyWidth;
    }

    if (dirtyHeight < 0) {
        dirtyY += dirtyHeight;
        dirtyHeight = -dirtyHeight;
    }

    FloatRect clipRect(dirtyX, dirtyY, dirtyWidth, dirtyHeight);
    clipRect.intersect(IntRect(0, 0, data->width(), data->height()));
    IntSize destOffset(static_cast<int>(dx), static_cast<int>(dy));
    IntRect destRect = enclosingIntRect(clipRect);
    destRect.move(destOffset);
    destRect.intersect(IntRect(IntPoint(), buffer->size()));
    if (destRect.isEmpty())
        return;
    IntRect sourceRect(destRect);
    sourceRect.move(-destOffset);

    checkOverdraw(destRect, 0, NoImage, UntransformedUnclippedFill);

    buffer->putByteArray(Unmultiplied, data->data()->data(), IntSize(data->width(), data->height()), sourceRect, IntPoint(destOffset));

    didDraw(destRect);
}

String CanvasRenderingContext2D::font() const
{
    if (!state().m_realizedFont)
        return defaultFont;

    StringBuilder serializedFont;
    const FontDescription& fontDescription = state().m_font.fontDescription();

    if (fontDescription.style() == FontStyleItalic)
        serializedFont.appendLiteral("italic ");
    if (fontDescription.weight() == FontWeightBold)
        serializedFont.appendLiteral("bold ");
    if (fontDescription.variant() == FontVariantSmallCaps)
        serializedFont.appendLiteral("small-caps ");

    serializedFont.appendNumber(fontDescription.computedPixelSize());
    serializedFont.appendLiteral("px");

    const FontFamily& firstFontFamily = fontDescription.family();
    for (const FontFamily* fontFamily = &firstFontFamily; fontFamily; fontFamily = fontFamily->next()) {
        if (fontFamily != &firstFontFamily)
            serializedFont.append(',');

        // FIXME: We should append family directly to serializedFont rather than building a temporary string.
        String family = fontFamily->family();
        if (family.startsWith("-webkit-"))
            family = family.substring(8);
        if (family.contains(' '))
            family = "\"" + family + "\"";

        serializedFont.append(' ');
        serializedFont.append(family);
    }

    return serializedFont.toString();
}

void CanvasRenderingContext2D::setFont(const String& newFont)
{
    // The style resolution required for rendering text is not available in frame-less documents.
    if (!canvas()->document().frame())
        return;

    RefPtrWillBeRawPtr<MutableStylePropertySet> parsedStyle;
    MutableStylePropertyMap::iterator i = m_fetchedFonts.find(newFont);
    if (i != m_fetchedFonts.end()) {
        parsedStyle = i->value;
        m_fetchedFontsLRUList.remove(newFont);
    } else {
        parsedStyle = MutableStylePropertySet::create();
        CSSParserMode mode = m_usesCSSCompatibilityParseMode ? HTMLQuirksMode : HTMLStandardMode;
        CSSParser::parseValue(parsedStyle.get(), CSSPropertyFont, newFont, true, mode, 0);
        if (m_fetchedFonts.size() >= FetchedFontsCacheLimit) {
            m_fetchedFonts.remove(m_fetchedFontsLRUList.first());
            m_fetchedFontsLRUList.removeFirst();
        }
        m_fetchedFonts.add(newFont, parsedStyle);
    }
    m_fetchedFontsLRUList.add(newFont);

    if (parsedStyle->isEmpty())
        return;

    String fontValue = parsedStyle->getPropertyValue(CSSPropertyFont);

    // According to http://lists.w3.org/Archives/Public/public-html/2009Jul/0947.html,
    // the "inherit" and "initial" values must be ignored.
    if (fontValue == "inherit" || fontValue == "initial")
        return;

    // The parse succeeded.
    String newFontSafeCopy(newFont); // Create a string copy since newFont can be deleted inside realizeSaves.
    realizeSaves(nullptr);
    modifiableState().m_unparsedFont = newFontSafeCopy;

    // Map the <canvas> font into the text style. If the font uses keywords like larger/smaller, these will work
    // relative to the canvas.
    RefPtr<LayoutStyle> newStyle = LayoutStyle::create();
    canvas()->document().updateRenderTreeIfNeeded();
    if (LayoutStyle* computedStyle = canvas()->computedStyle()) {
        FontDescription elementFontDescription(computedStyle->fontDescription());
        // Reset the computed size to avoid inheriting the zoom factor from the <canvas> element.
        elementFontDescription.setComputedSize(elementFontDescription.specifiedSize());
        newStyle->setFontDescription(elementFontDescription);
    } else {
        FontFamily fontFamily;
        fontFamily.setFamily(defaultFontFamily);

        FontDescription defaultFontDescription;
        defaultFontDescription.setFamily(fontFamily);
        defaultFontDescription.setSpecifiedSize(defaultFontSize);
        defaultFontDescription.setComputedSize(defaultFontSize);

        newStyle->setFontDescription(defaultFontDescription);
    }

    newStyle->font().update(newStyle->font().fontSelector());

    // Now map the font property longhands into the style.
    CSSPropertyValue properties[] = {
        CSSPropertyValue(CSSPropertyFontFamily, *parsedStyle),
        CSSPropertyValue(CSSPropertyFontStyle, *parsedStyle),
        CSSPropertyValue(CSSPropertyFontVariant, *parsedStyle),
        CSSPropertyValue(CSSPropertyFontWeight, *parsedStyle),
        CSSPropertyValue(CSSPropertyFontSize, *parsedStyle),
        CSSPropertyValue(CSSPropertyLineHeight, *parsedStyle),
    };

    StyleResolver& styleResolver = canvas()->document().ensureStyleResolver();
    styleResolver.applyPropertiesToStyle(properties, WTF_ARRAY_LENGTH(properties), newStyle.get());

#if !ENABLE(OILPAN)
    if (state().m_realizedFont)
        static_cast<CSSFontSelector*>(state().m_font.fontSelector())->unregisterForInvalidationCallbacks(&modifiableState());
#endif
    modifiableState().m_font = newStyle->font();
    modifiableState().m_font.update(canvas()->document().styleEngine()->fontSelector());
    modifiableState().m_realizedFont = true;
    canvas()->document().styleEngine()->fontSelector()->registerForInvalidationCallbacks(&modifiableState());
}

String CanvasRenderingContext2D::textAlign() const
{
    return textAlignName(state().m_textAlign);
}

void CanvasRenderingContext2D::setTextAlign(const String& s)
{
    TextAlign align;
    if (!parseTextAlign(s, align))
        return;
    if (state().m_textAlign == align)
        return;
    realizeSaves(nullptr);
    modifiableState().m_textAlign = align;
}

String CanvasRenderingContext2D::textBaseline() const
{
    return textBaselineName(state().m_textBaseline);
}

void CanvasRenderingContext2D::setTextBaseline(const String& s)
{
    TextBaseline baseline;
    if (!parseTextBaseline(s, baseline))
        return;
    if (state().m_textBaseline == baseline)
        return;
    realizeSaves(nullptr);
    modifiableState().m_textBaseline = baseline;
}

inline TextDirection CanvasRenderingContext2D::toTextDirection(Direction direction, LayoutStyle** computedStyle) const
{
    LayoutStyle* style = (computedStyle || direction == DirectionInherit) ? canvas()->computedStyle() : nullptr;
    if (computedStyle)
        *computedStyle = style;
    switch (direction) {
    case DirectionInherit:
        return style ? style->direction() : LTR;
    case DirectionRTL:
        return RTL;
    case DirectionLTR:
        return LTR;
    }
    ASSERT_NOT_REACHED();
    return LTR;
}

String CanvasRenderingContext2D::direction() const
{
    if (state().m_direction == DirectionInherit)
        canvas()->document().updateRenderTreeIfNeeded();
    return toTextDirection(state().m_direction) == RTL ? rtl : ltr;
}

void CanvasRenderingContext2D::setDirection(const String& directionString)
{
    Direction direction;
    if (directionString == inherit)
        direction = DirectionInherit;
    else if (directionString == rtl)
        direction = DirectionRTL;
    else if (directionString == ltr)
        direction = DirectionLTR;
    else
        return;

    if (state().m_direction == direction)
        return;

    realizeSaves(nullptr);
    modifiableState().m_direction = direction;
}

void CanvasRenderingContext2D::fillText(const String& text, float x, float y)
{
    drawTextInternal(text, x, y, true);
}

void CanvasRenderingContext2D::fillText(const String& text, float x, float y, float maxWidth)
{
    drawTextInternal(text, x, y, true, maxWidth, true);
}

void CanvasRenderingContext2D::strokeText(const String& text, float x, float y)
{
    drawTextInternal(text, x, y, false);
}

void CanvasRenderingContext2D::strokeText(const String& text, float x, float y, float maxWidth)
{
    drawTextInternal(text, x, y, false, maxWidth, true);
}

PassRefPtrWillBeRawPtr<TextMetrics> CanvasRenderingContext2D::measureText(const String& text)
{
    RefPtrWillBeRawPtr<TextMetrics> metrics = TextMetrics::create();

    // The style resolution required for rendering text is not available in frame-less documents.
    if (!canvas()->document().frame())
        return metrics.release();

    canvas()->document().updateRenderTreeIfNeeded();
    const Font& font = accessFont();

    bool hasStrongDirectionality;
    TextDirection direction;
    if (state().m_direction == DirectionInherit)
        direction = determineDirectionality(text, hasStrongDirectionality);
    else
        direction = toTextDirection(state().m_direction);
    const TextRun textRun(text, 0, 0, TextRun::AllowTrailingExpansion | TextRun::ForbidLeadingExpansion, direction, false, true);
    FloatRect textBounds = font.selectionRectForText(textRun, FloatPoint(), font.fontDescription().computedSize(), 0, -1, true);

    // x direction
    metrics->setWidth(font.width(textRun));
    metrics->setActualBoundingBoxLeft(-textBounds.x());
    metrics->setActualBoundingBoxRight(textBounds.maxX());

    // y direction
    const FontMetrics& fontMetrics = font.fontMetrics();
    const float ascent = fontMetrics.floatAscent();
    const float descent = fontMetrics.floatDescent();
    const float baselineY = getFontBaseline(fontMetrics);

    metrics->setFontBoundingBoxAscent(ascent - baselineY);
    metrics->setFontBoundingBoxDescent(descent + baselineY);
    metrics->setActualBoundingBoxAscent(-textBounds.y() - baselineY);
    metrics->setActualBoundingBoxDescent(textBounds.maxY() + baselineY);

    // Note : top/bottom and ascend/descend are currently the same, so there's no difference
    //        between the EM box's top and bottom and the font's ascend and descend
    metrics->setEmHeightAscent(0);
    metrics->setEmHeightDescent(0);

    metrics->setHangingBaseline(-0.8f * ascent + baselineY);
    metrics->setAlphabeticBaseline(baselineY);
    metrics->setIdeographicBaseline(descent + baselineY);
    return metrics.release();
}

void CanvasRenderingContext2D::drawTextInternal(const String& text, float x, float y, bool fill, float maxWidth, bool useMaxWidth)
{
    // The style resolution required for rendering text is not available in frame-less documents.
    if (!canvas()->document().frame())
        return;

    // accessFont needs the style to be up to date, but updating style can cause script to run,
    // (e.g. due to autofocus) which can free the GraphicsContext, so update style before grabbing
    // the GraphicsContext.
    canvas()->document().updateRenderTreeIfNeeded();

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;
    if (!std::isfinite(x) || !std::isfinite(y))
        return;
    if (useMaxWidth && (!std::isfinite(maxWidth) || maxWidth <= 0))
        return;

    // If gradient size is zero, then paint nothing.
    Gradient* gradient = c->strokeGradient();
    if (!fill && gradient && gradient->isZeroSize())
        return;

    gradient = c->fillGradient();
    if (fill && gradient && gradient->isZeroSize())
        return;

    FontCachePurgePreventer fontCachePurgePreventer;

    const Font& font = accessFont();
    const FontMetrics& fontMetrics = font.fontMetrics();

    // FIXME: Need to turn off font smoothing.

    LayoutStyle* computedStyle;
    TextDirection direction = toTextDirection(state().m_direction, &computedStyle);
    bool isRTL = direction == RTL;
    bool override = computedStyle ? isOverride(computedStyle->unicodeBidi()) : false;

    TextRun textRun(text, 0, 0, TextRun::AllowTrailingExpansion, direction, override, true);
    // Draw the item text at the correct point.
    FloatPoint location(x, y + getFontBaseline(fontMetrics));
    float fontWidth = font.width(textRun);

    useMaxWidth = (useMaxWidth && maxWidth < fontWidth);
    float width = useMaxWidth ? maxWidth : fontWidth;

    TextAlign align = state().m_textAlign;
    if (align == StartTextAlign)
        align = isRTL ? RightTextAlign : LeftTextAlign;
    else if (align == EndTextAlign)
        align = isRTL ? LeftTextAlign : RightTextAlign;

    switch (align) {
    case CenterTextAlign:
        location.setX(location.x() - width / 2);
        break;
    case RightTextAlign:
        location.setX(location.x() - width);
        break;
    default:
        break;
    }

    // The slop built in to this mask rect matches the heuristic used in FontCGWin.cpp for GDI text.
    TextRunPaintInfo textRunPaintInfo(textRun);
    textRunPaintInfo.bounds = FloatRect(location.x() - fontMetrics.height() / 2,
                                        location.y() - fontMetrics.ascent() - fontMetrics.lineGap(),
                                        width + fontMetrics.height(),
                                        fontMetrics.lineSpacing());
    if (!fill)
        inflateStrokeRect(textRunPaintInfo.bounds);

    c->setTextDrawingMode(fill ? TextModeFill : TextModeStroke);

    CanvasRenderingContext2DAutoRestoreSkCanvas stateRestorer(this);
    if (useMaxWidth) {
        SkCanvas* canvas = drawingCanvas();
        canvas->save();
        canvas->translate(location.x(), location.y());
        // We draw when fontWidth is 0 so compositing operations (eg, a "copy" op) still work.
        canvas->scale((fontWidth > 0 ? (width / fontWidth) : 0), 1);
        location = FloatPoint();
    }

    FloatRect clipBounds;
    if (!c->getTransformedClipBounds(&clipBounds)) {
        return;
    }

    if (isFullCanvasCompositeMode(state().m_globalComposite)) {
        fullCanvasCompositedDraw(bind(&GraphicsContext::drawBidiText, c, font, textRunPaintInfo, location, Font::UseFallbackIfFontNotReady));
        didDraw(clipBounds);
    } else if (state().m_globalComposite == SkXfermode::kSrc_Mode) {
        clearCanvas();
        c->clearShadow();
        c->drawBidiText(font, textRunPaintInfo, location, Font::UseFallbackIfFontNotReady);
        applyShadow(DrawShadowAndForeground);
        didDraw(clipBounds);
    } else {
        FloatRect dirtyRect;
        if (computeDirtyRect(textRunPaintInfo.bounds, clipBounds, &dirtyRect)) {
            c->drawBidiText(font, textRunPaintInfo, location, Font::UseFallbackIfFontNotReady);
            didDraw(dirtyRect);
        }
    }
}

void CanvasRenderingContext2D::inflateStrokeRect(FloatRect& rect) const
{
    // Fast approximation of the stroke's bounding rect.
    // This yields a slightly oversized rect but is very fast
    // compared to Path::strokeBoundingRect().
    static const float root2 = sqrtf(2);
    float delta = state().m_lineWidth / 2;
    if (state().m_lineJoin == MiterJoin)
        delta *= state().m_miterLimit;
    else if (state().m_lineCap == SquareCap)
        delta *= root2;

    rect.inflate(delta);
}

const Font& CanvasRenderingContext2D::accessFont()
{
    // This needs style to be up to date, but can't assert so because drawTextInternal
    // can invalidate style before this is called (e.g. drawingContext invalidates style).
    if (!state().m_realizedFont)
        setFont(state().m_unparsedFont);
    return state().m_font;
}

int CanvasRenderingContext2D::getFontBaseline(const FontMetrics& fontMetrics) const
{
    switch (state().m_textBaseline) {
    case TopTextBaseline:
        return fontMetrics.ascent();
    case HangingTextBaseline:
        // According to http://wiki.apache.org/xmlgraphics-fop/LineLayout/AlignmentHandling
        // "FOP (Formatting Objects Processor) puts the hanging baseline at 80% of the ascender height"
        return (fontMetrics.ascent() * 4) / 5;
    case BottomTextBaseline:
    case IdeographicTextBaseline:
        return -fontMetrics.descent();
    case MiddleTextBaseline:
        return -fontMetrics.descent() + fontMetrics.height() / 2;
    case AlphabeticTextBaseline:
    default:
        // Do nothing.
        break;
    }
    return 0;
}

void CanvasRenderingContext2D::setIsHidden(bool hidden)
{
    if (canvas()->hasImageBuffer())
        canvas()->buffer()->setIsHidden(hidden);
}

WebLayer* CanvasRenderingContext2D::platformLayer() const
{
    return canvas()->buffer() ? canvas()->buffer()->platformLayer() : 0;
}

bool CanvasRenderingContext2D::imageSmoothingEnabled() const
{
    return state().m_imageSmoothingEnabled;
}

void CanvasRenderingContext2D::setImageSmoothingEnabled(bool enabled)
{
    if (enabled == state().m_imageSmoothingEnabled)
        return;

    SkCanvas* c = drawingCanvas();
    realizeSaves(c);
    modifiableState().m_imageSmoothingEnabled = enabled;
    if (c)
        drawingContext()->setImageInterpolationQuality(enabled ? CanvasDefaultInterpolationQuality : InterpolationNone);
}

void CanvasRenderingContext2D::getContextAttributes(Canvas2DContextAttributes& attrs) const
{
    attrs.setAlpha(m_hasAlpha);
}

void CanvasRenderingContext2D::drawFocusIfNeeded(Element* element)
{
    drawFocusIfNeededInternal(m_path, element);
}

void CanvasRenderingContext2D::drawFocusIfNeeded(Path2D* path2d, Element* element)
{
    drawFocusIfNeededInternal(path2d->path(), element);
}

void CanvasRenderingContext2D::drawFocusIfNeededInternal(const Path& path, Element* element)
{
    if (!focusRingCallIsValid(path, element))
        return;

    // Note: we need to check document->focusedElement() rather than just calling
    // element->focused(), because element->focused() isn't updated until after
    // focus events fire.
    if (element->document().focusedElement() == element)
        drawFocusRing(path);
}

bool CanvasRenderingContext2D::focusRingCallIsValid(const Path& path, Element* element)
{
    ASSERT(element);
    if (!state().m_invertibleCTM)
        return false;
    if (path.isEmpty())
        return false;
    if (!element->isDescendantOf(canvas()))
        return false;

    return true;
}

void CanvasRenderingContext2D::drawFocusRing(const Path& path)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    // These should match the style defined in html.css.
    Color focusRingColor = LayoutTheme::theme().focusRingColor();
    const int focusRingWidth = 5;
    const int focusRingOutline = 0;

    // We need to add focusRingWidth to dirtyRect.
    StrokeData strokeData;
    strokeData.setThickness(focusRingWidth);

    FloatRect dirtyRect;
    if (!computeDirtyRect(path.strokeBoundingRect(strokeData), &dirtyRect))
        return;

    c->setAlphaAsFloat(1.0);
    c->clearShadow();
    c->setCompositeOperation(SkXfermode::kSrcOver_Mode);
    c->drawFocusRing(path, focusRingWidth, focusRingOutline, focusRingColor);
    applyShadow(DrawShadowAndForeground);
    c->setAlphaAsFloat(state().m_globalAlpha);
    c->setCompositeOperation(state().m_globalComposite);

    validateStateStack();
    didDraw(dirtyRect);
}

void CanvasRenderingContext2D::addHitRegion(const HitRegionOptions& options, ExceptionState& exceptionState)
{
    if (options.id().isEmpty() && !options.control()) {
        exceptionState.throwDOMException(NotSupportedError, "Both id and control are null.");
        return;
    }

    Path hitRegionPath = options.hasPath() ? options.path()->path() : m_path;

    FloatRect clipBounds;
    GraphicsContext* context = drawingContext();

    if (hitRegionPath.isEmpty() || !context || !state().m_invertibleCTM
        || !context->getTransformedClipBounds(&clipBounds)) {
        exceptionState.throwDOMException(NotSupportedError, "The specified path has no pixels.");
        return;
    }

    hitRegionPath.transform(state().m_transform);

    if (hasClip()) {
        // FIXME: The hit regions should take clipping region into account.
        // However, we have no way to get the region from canvas state stack by now.
        // See http://crbug.com/387057
        exceptionState.throwDOMException(NotSupportedError, "The specified path has no pixels.");
        return;
    }

    if (!m_hitRegionManager)
        m_hitRegionManager = HitRegionManager::create();

    // Remove previous region (with id or control)
    m_hitRegionManager->removeHitRegionById(options.id());
    m_hitRegionManager->removeHitRegionByControl(options.control().get());

    RefPtrWillBeRawPtr<HitRegion> hitRegion = HitRegion::create(hitRegionPath, options);
    hitRegion->updateAccessibility(canvas());
    m_hitRegionManager->addHitRegion(hitRegion.release());
}

void CanvasRenderingContext2D::removeHitRegion(const String& id)
{
    if (m_hitRegionManager)
        m_hitRegionManager->removeHitRegionById(id);
}

void CanvasRenderingContext2D::clearHitRegions()
{
    if (m_hitRegionManager)
        m_hitRegionManager->removeAllHitRegions();
}

HitRegion* CanvasRenderingContext2D::hitRegionAtPoint(const LayoutPoint& point)
{
    if (m_hitRegionManager)
        return m_hitRegionManager->getHitRegionAtPoint(point);

    return nullptr;
}

unsigned CanvasRenderingContext2D::hitRegionsCount() const
{
    if (m_hitRegionManager)
        return m_hitRegionManager->getHitRegionsCount();

    return 0;
}

void CanvasRenderingContext2D::checkOverdraw(const SkRect& rect, const SkPaint* paint, ImageType imageType, DrawType drawType)
{
    SkCanvas* c = drawingCanvas();
    if (!c || !canvas()->buffer()->isRecording())
        return;

    SkRect deviceRect;
    if (drawType == UntransformedUnclippedFill) {
        deviceRect = rect;
    } else {
        ASSERT(drawType == ClipFill);
        if (state().m_hasComplexClip)
            return;

        SkIRect skIBounds;
        if (!c->getClipDeviceBounds(&skIBounds))
            return;
        deviceRect = SkRect::Make(skIBounds);
    }

    const SkImageInfo& imageInfo = c->imageInfo();
    if (!deviceRect.contains(SkRect::MakeWH(imageInfo.width(), imageInfo.height())))
        return;

    bool isSourceOver = true;
    unsigned alpha = 0xFF;
    if (paint) {
        if (paint->getLooper() || paint->getImageFilter() || paint->getMaskFilter())
            return;

        SkXfermode* xfermode = paint->getXfermode();
        if (xfermode) {
            SkXfermode::Mode mode;
            if (xfermode->asMode(&mode)) {
                isSourceOver = mode == SkXfermode::kSrcOver_Mode;
                if (!isSourceOver && mode != SkXfermode::kSrc_Mode && mode != SkXfermode::kClear_Mode)
                    return; // The code below only knows how to handle Src, SrcOver, and Clear
            } else {
                // unknown xfermode
                ASSERT_NOT_REACHED();
                return;
            }
        }

        alpha = paint->getAlpha();

        if (isSourceOver && imageType == NoImage) {
            SkShader* shader = paint->getShader();
            if (shader) {
                if (shader->isOpaque() && alpha == 0xFF)
                    canvas()->buffer()->willOverwriteCanvas();
                return;
            }
        }
    }

    if (isSourceOver) {
        // With source over, we need to certify that alpha == 0xFF for all pixels
        if (imageType == NonOpaqueImage)
            return;
        if (alpha < 0xFF)
            return;
    }

    canvas()->buffer()->willOverwriteCanvas();
}

} // namespace blink
