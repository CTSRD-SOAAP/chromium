// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "../../include/fxcrt/fx_xml.h"
#include "xml_int.h"
void FX_XML_SplitQualifiedName(FX_BSTR bsFullName, CFX_ByteStringC &bsSpace, CFX_ByteStringC &bsName)
{
    if (bsFullName.IsEmpty()) {
        return;
    }
    FX_INT32 iStart = 0;
    for (; iStart < bsFullName.GetLength(); iStart ++) {
        if (bsFullName.GetAt(iStart) == ':') {
            break;
        }
    }
    if (iStart >= bsFullName.GetLength()) {
        bsName = bsFullName;
    } else {
        bsSpace = CFX_ByteStringC(bsFullName.GetCStr(), iStart);
        iStart ++;
        bsName = CFX_ByteStringC(bsFullName.GetCStr() + iStart, bsFullName.GetLength() - iStart);
    }
}
void CXML_Element::SetTag(FX_BSTR qSpace, FX_BSTR tagname)
{
    m_QSpaceName = qSpace;
    m_TagName = tagname;
}
void CXML_Element::SetTag(FX_BSTR qTagName)
{
    ASSERT(!qTagName.IsEmpty());
    CFX_ByteStringC bsSpace, bsName;
    FX_XML_SplitQualifiedName(qTagName, bsSpace, bsName);
    m_QSpaceName = bsSpace;
    m_TagName = bsName;
}
