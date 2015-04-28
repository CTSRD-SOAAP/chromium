//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SurfaceGL.h: Defines the class interface for SurfaceGL.

#ifndef LIBANGLE_RENDERER_GL_SURFACEGL_H_
#define LIBANGLE_RENDERER_GL_SURFACEGL_H_

#include "libANGLE/renderer/SurfaceImpl.h"

namespace rx
{

class SurfaceGL : public SurfaceImpl
{
  public:
    SurfaceGL(egl::Display *display, const egl::Config *config,
              EGLint fixedSize, EGLint postSubBufferSupported, EGLenum textureFormat,
              EGLenum textureType);
    ~SurfaceGL() override;

  private:
    DISALLOW_COPY_AND_ASSIGN(SurfaceGL);
};

}

#endif // LIBANGLE_RENDERER_GL_SURFACEGL_H_
