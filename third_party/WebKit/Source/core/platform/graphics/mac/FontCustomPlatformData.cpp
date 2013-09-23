/*
 * Copyright (C) 2007, 2008, 2010 Apple Inc. All rights reserved.
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
#include "core/platform/graphics/mac/FontCustomPlatformData.h"

#include "core/platform/SharedBuffer.h"
#include "core/platform/graphics/FontPlatformData.h"
#include "core/platform/graphics/opentype/OpenTypeSanitizer.h"
#include "core/platform/graphics/skia/SkiaSharedBufferStream.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include <ApplicationServices/ApplicationServices.h>

namespace WebCore {

FontCustomPlatformData::~FontCustomPlatformData()
{
    SkSafeUnref(m_typeface);
    CGFontRelease(m_cgFont);
}

FontPlatformData FontCustomPlatformData::fontPlatformData(int size, bool bold, bool italic, FontOrientation orientation, FontWidthVariant widthVariant, FontRenderingMode)
{
    return FontPlatformData(m_cgFont, size, bold, italic, orientation, widthVariant);
}

FontCustomPlatformData* createFontCustomPlatformData(SharedBuffer* buffer)
{
    ASSERT_ARG(buffer, buffer);

    OpenTypeSanitizer sanitizer(buffer);
    RefPtr<SharedBuffer> transcodeBuffer = sanitizer.sanitize();
    if (!transcodeBuffer)
        return 0; // validation failed.
    buffer = transcodeBuffer.get();

    ATSFontContainerRef containerRef = 0;

    RetainPtr<CGFontRef> cgFontRef;

    RetainPtr<CFDataRef> bufferData(AdoptCF, CFDataCreate(0, reinterpret_cast<const UInt8*>(buffer->data()), buffer->size()));
    RetainPtr<CGDataProviderRef> dataProvider(AdoptCF, CGDataProviderCreateWithCFData(bufferData.get()));

    cgFontRef.adoptCF(CGFontCreateWithDataProvider(dataProvider.get()));
    if (!cgFontRef)
        return 0;

    FontCustomPlatformData* fontCustomPlatformData = new FontCustomPlatformData(containerRef, cgFontRef.leakRef());
    SkiaSharedBufferStream* stream = new SkiaSharedBufferStream(buffer);
    fontCustomPlatformData->m_typeface = SkTypeface::CreateFromStream(stream);
    stream->unref();
    return fontCustomPlatformData;
}

bool FontCustomPlatformData::supportsFormat(const String& format)
{
    return equalIgnoringCase(format, "truetype") || equalIgnoringCase(format, "opentype") || OpenTypeSanitizer::supportsFormat(format);
}

}
