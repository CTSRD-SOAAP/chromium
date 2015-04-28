//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// VertexArrayGL.h: Defines the class interface for VertexArrayGL.

#ifndef LIBANGLE_RENDERER_GL_VERTEXARRAYGL_H_
#define LIBANGLE_RENDERER_GL_VERTEXARRAYGL_H_

#include "libANGLE/renderer/VertexArrayImpl.h"

namespace rx
{

class VertexArrayGL : public VertexArrayImpl
{
  public:
    VertexArrayGL();
    ~VertexArrayGL() override;

    void setElementArrayBuffer(const gl::Buffer *buffer) override;
    void setAttribute(size_t idx, const gl::VertexAttribute &attr) override;
    void setAttributeDivisor(size_t idx, GLuint divisor) override;
    void enableAttribute(size_t idx, bool enabledState) override;

  private:
    DISALLOW_COPY_AND_ASSIGN(VertexArrayGL);
};

}

#endif // LIBANGLE_RENDERER_GL_VERTEXARRAYGL_H_
