// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/layout/style/PathStyleMotionPath.h"

#include "core/svg/SVGPathUtilities.h"

namespace blink {

PathStyleMotionPath::PathStyleMotionPath(const String& pathString)
    : m_pathString(pathString)
{
    buildPathFromString(pathString, m_path);
    m_length = m_path.length();
}

} // namespace blink
