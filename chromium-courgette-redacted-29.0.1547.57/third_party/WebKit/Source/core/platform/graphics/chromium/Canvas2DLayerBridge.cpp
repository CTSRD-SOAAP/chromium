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

#include "core/platform/graphics/chromium/Canvas2DLayerBridge.h"

#include "GrContext.h"
#include "SkDevice.h"
#include "SkSurface.h"
#include "core/platform/chromium/TraceEvent.h"
#include "core/platform/graphics/GraphicsContext3D.h"
#include "core/platform/graphics/GraphicsLayer.h"
#include "core/platform/graphics/chromium/Canvas2DLayerManager.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCompositorSupport.h"
#include "public/platform/WebGraphicsContext3D.h"

using WebKit::WebExternalTextureLayer;
using WebKit::WebGraphicsContext3D;
using WebKit::WebTextureUpdater;

namespace WebCore {

Canvas2DLayerBridge::Canvas2DLayerBridge(PassRefPtr<GraphicsContext3D> context, SkDeferredCanvas* canvas, OpacityMode opacityMode, ThreadMode threadMode)
    : m_canvas(canvas)
    , m_context(context)
    , m_bytesAllocated(0)
    , m_didRecordDrawCommand(false)
    , m_framesPending(0)
    , m_rateLimitingEnabled(false)
    , m_next(0)
    , m_prev(0)
#if ENABLE(CANVAS_USES_MAILBOX)
    , m_lastImageId(0)
#endif
{
    ASSERT(m_canvas);
    // Used by browser tests to detect the use of a Canvas2DLayerBridge.
    TRACE_EVENT_INSTANT0("test_gpu", "Canvas2DLayerBridgeCreation");
    m_canvas->setNotificationClient(this);
#if ENABLE(CANVAS_USES_MAILBOX)
    m_layer = adoptPtr(WebKit::Platform::current()->compositorSupport()->createExternalTextureLayerForMailbox(this));
#else
    m_layer = adoptPtr(WebKit::Platform::current()->compositorSupport()->createExternalTextureLayer(this));
    m_layer->setRateLimitContext(threadMode == SingleThread);
    GrRenderTarget* renderTarget = reinterpret_cast<GrRenderTarget*>(m_canvas->getDevice()->accessRenderTarget());
    if (renderTarget) {
        m_layer->setTextureId(renderTarget->asTexture()->getTextureHandle());
    }
#endif
    m_layer->setOpaque(opacityMode == Opaque);
    GraphicsLayer::registerContentsLayer(m_layer->layer());
    m_layer->setRateLimitContext(m_rateLimitingEnabled);
}

Canvas2DLayerBridge::~Canvas2DLayerBridge()
{
    GraphicsLayer::unregisterContentsLayer(m_layer->layer());
    Canvas2DLayerManager::get().layerToBeDestroyed(this);
    m_canvas->setNotificationClient(0);
    m_layer->clearTexture();
    m_layer.clear();
#if ENABLE(CANVAS_USES_MAILBOX)
    m_mailboxes.clear();
#endif
}

void Canvas2DLayerBridge::limitPendingFrames()
{
    if (m_didRecordDrawCommand) {
        m_framesPending++;
        m_didRecordDrawCommand = false;
        if (m_framesPending > 1) {
            // Turn on the rate limiter if this layer tends to accumulate a
            // non-discardable multi-frame backlog of draw commands.
            setRateLimitingEnabled(true);
        }
        if (m_rateLimitingEnabled) {
            flush();
        }
    }
}

void Canvas2DLayerBridge::prepareForDraw()
{
#if !ENABLE(CANVAS_USES_MAILBOX)
    m_layer->willModifyTexture();
#endif
    m_context->makeContextCurrent();
}

void Canvas2DLayerBridge::storageAllocatedForRecordingChanged(size_t bytesAllocated)
{
    intptr_t delta = (intptr_t)bytesAllocated - (intptr_t)m_bytesAllocated;
    m_bytesAllocated = bytesAllocated;
    Canvas2DLayerManager::get().layerAllocatedStorageChanged(this, delta);
}

size_t Canvas2DLayerBridge::storageAllocatedForRecording()
{
    return m_canvas->storageAllocatedForRecording();
}

void Canvas2DLayerBridge::flushedDrawCommands()
{
    storageAllocatedForRecordingChanged(storageAllocatedForRecording());
    m_framesPending = 0;
}

void Canvas2DLayerBridge::skippedPendingDrawCommands()
{
    // Stop triggering the rate limiter if SkDeferredCanvas is detecting
    // and optimizing overdraw.
    setRateLimitingEnabled(false);
    flushedDrawCommands();
}

void Canvas2DLayerBridge::setRateLimitingEnabled(bool enabled)
{
    if (m_rateLimitingEnabled != enabled) {
        m_rateLimitingEnabled = enabled;
        m_layer->setRateLimitContext(m_rateLimitingEnabled);
    }
}

size_t Canvas2DLayerBridge::freeMemoryIfPossible(size_t bytesToFree)
{
    size_t bytesFreed = m_canvas->freeMemoryIfPossible(bytesToFree);
    if (bytesFreed)
        Canvas2DLayerManager::get().layerAllocatedStorageChanged(this, -((intptr_t)bytesFreed));
    m_bytesAllocated -= bytesFreed;
    return bytesFreed;
}

void Canvas2DLayerBridge::flush()
{
    if (m_canvas->hasPendingCommands()) {
        TRACE_EVENT0("cc", "Canvas2DLayerBridge::flush");
        m_canvas->flush();
    }
}

unsigned Canvas2DLayerBridge::prepareTexture(WebTextureUpdater& updater)
{
#if ENABLE(CANVAS_USES_MAILBOX)
    ASSERT_NOT_REACHED();
    return 0;
#else
    m_context->makeContextCurrent();

    TRACE_EVENT0("cc", "Canvas2DLayerBridge::SkCanvas::flush");
    m_canvas->flush();
    m_context->flush();

    // Notify skia that the state of the backing store texture object will be touched by the compositor
    GrRenderTarget* renderTarget = reinterpret_cast<GrRenderTarget*>(m_canvas->getDevice()->accessRenderTarget());
    if (renderTarget) {
        GrTexture* texture = renderTarget->asTexture();
        texture->invalidateCachedState();
        return texture->getTextureHandle();
    }
    return 0;
#endif  // !ENABLE(CANVAS_USES_MAILBOX)
}

WebGraphicsContext3D* Canvas2DLayerBridge::context()
{
    return m_context->webContext();
}

bool Canvas2DLayerBridge::prepareMailbox(WebKit::WebExternalTextureMailbox* outMailbox)
{
#if ENABLE(CANVAS_USES_MAILBOX)
    // Release to skia textures that were previouosly released by the
    // compositor. We do this before acquiring the next snapshot in
    // order to cap maximum gpu memory consumption.
    m_context->makeContextCurrent();
    flush();
    Vector<MailboxInfo>::iterator mailboxInfo;
    for (mailboxInfo = m_mailboxes.begin(); mailboxInfo < m_mailboxes.end(); mailboxInfo++) {
        if (mailboxInfo->m_status == MailboxReleased) {
            if (mailboxInfo->m_mailbox.syncPoint) {
                context()->waitSyncPoint(mailboxInfo->m_mailbox.syncPoint);
                mailboxInfo->m_mailbox.syncPoint = 0;
            }
            // Invalidate texture state in case the compositor altered it since the copy-on-write.
            mailboxInfo->m_image->getTexture()->invalidateCachedState();
            mailboxInfo->m_image.reset(0);
            mailboxInfo->m_status = MailboxAvailable;
        }
    }
    SkAutoTUnref<SkImage> image(m_canvas->newImageSnapshot());
    // Early exit if canvas was not drawn to since last prepareMailbox
    if (image->uniqueID() == m_lastImageId)
        return false;
    m_lastImageId = image->uniqueID();

    mailboxInfo = createMailboxInfo();
    mailboxInfo->m_status = MailboxInUse;
    mailboxInfo->m_image.swap(&image);
    // Because of texture sharing with the compositor, we must invalidate
    // the state cached in skia so that the deferred copy on write
    // in SkSurface_Gpu does not make any false assumptions.
    mailboxInfo->m_image->getTexture()->invalidateCachedState();

    ASSERT(mailboxInfo->m_mailbox.syncPoint == 0);
    ASSERT(mailboxInfo->m_image.get());
    ASSERT(mailboxInfo->m_image->getTexture());

    m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, mailboxInfo->m_image->getTexture()->getTextureHandle());
    m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR);
    m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR);
    m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE);
    m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE);
    context()->produceTextureCHROMIUM(GraphicsContext3D::TEXTURE_2D, mailboxInfo->m_mailbox.name);
    context()->flush();
    mailboxInfo->m_mailbox.syncPoint = context()->insertSyncPoint();
    m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, 0);
    // Because we are changing the texture binding without going through skia,
    // we must dirty the context.
    // TODO(piman): expose finer granularity reset. We only really want to
    // 'dirty' the current texture binding.
    m_context->grContext()->resetContext();

    *outMailbox = mailboxInfo->m_mailbox;
    return true;
#else
    ASSERT_NOT_REACHED();
    return false;
#endif
}

#if ENABLE(CANVAS_USES_MAILBOX)
Canvas2DLayerBridge::MailboxInfo* Canvas2DLayerBridge::createMailboxInfo() {
    MailboxInfo* mailboxInfo;
    for (mailboxInfo = m_mailboxes.begin(); mailboxInfo < m_mailboxes.end(); mailboxInfo++) {
        if (mailboxInfo->m_status == MailboxAvailable) {
            return mailboxInfo;
        }
    }

    // No available mailbox: create one.
    m_mailboxes.grow(m_mailboxes.size() + 1);
    mailboxInfo = &m_mailboxes.last();
    context()->genMailboxCHROMIUM(mailboxInfo->m_mailbox.name);
    // Worst case, canvas is triple buffered.  More than 3 active mailboxes
    // means there is a problem.
    // For the single-threaded case, this value needs to be at least
    // kMaxSwapBuffersPending+1 (in render_widget.h).
    // Because of crbug.com/247874, it needs to be kMaxSwapBuffersPending+2.
    // TODO(piman): fix this.
    ASSERT(m_mailboxes.size() <= 4);
    ASSERT(mailboxInfo < m_mailboxes.end());
    return mailboxInfo;
}
#endif

void Canvas2DLayerBridge::mailboxReleased(const WebKit::WebExternalTextureMailbox& mailbox)
{
#if ENABLE(CANVAS_USES_MAILBOX)
    Vector<MailboxInfo>::iterator mailboxInfo;
    for (mailboxInfo = m_mailboxes.begin(); mailboxInfo < m_mailboxes.end(); mailboxInfo++) {
         if (!memcmp(mailboxInfo->m_mailbox.name, mailbox.name, sizeof(mailbox.name))) {
             mailboxInfo->m_mailbox.syncPoint = mailbox.syncPoint;
             ASSERT(mailboxInfo->m_status == MailboxInUse);
             mailboxInfo->m_status = MailboxReleased;
             return;
         }
     }
#endif
     ASSERT_NOT_REACHED();
}

WebKit::WebLayer* Canvas2DLayerBridge::layer()
{
    return m_layer->layer();
}

void Canvas2DLayerBridge::contextAcquired()
{
    Canvas2DLayerManager::get().layerDidDraw(this);
    m_didRecordDrawCommand = true;
}

unsigned Canvas2DLayerBridge::backBufferTexture()
{
    contextAcquired();
    m_canvas->flush();
    m_context->flush();
    GrRenderTarget* renderTarget = reinterpret_cast<GrRenderTarget*>(m_canvas->getDevice()->accessRenderTarget());
    if (renderTarget) {
        return renderTarget->asTexture()->getTextureHandle();
    }
    return 0;
}

#if ENABLE(CANVAS_USES_MAILBOX)
Canvas2DLayerBridge::MailboxInfo::MailboxInfo(const MailboxInfo& other) {
    // This copy constructor should only be used for Vector reallocation
    // Assuming 'other' is to be destroyed, we swap m_image ownership
    // rather than do a refcount dance.
    memcpy(&m_mailbox, &other.m_mailbox, sizeof(m_mailbox));
    m_image.swap(const_cast<SkAutoTUnref<SkImage>*>(&other.m_image));
    m_status = other.m_status;
}
#endif

}
