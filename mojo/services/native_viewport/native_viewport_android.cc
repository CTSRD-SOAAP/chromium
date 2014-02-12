// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/native_viewport_android.h"

#include <android/native_window_jni.h>
#include "gpu/command_buffer/client/gl_in_process_context.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "mojo/services/native_viewport/android/mojo_viewport.h"
#include "mojo/shell/context.h"

namespace mojo {
namespace services {

NativeViewportAndroid::NativeViewportAndroid(NativeViewportDelegate* delegate)
    : delegate_(delegate),
      window_(NULL),
      weak_factory_(this) {
}

NativeViewportAndroid::~NativeViewportAndroid() {
  if (window_)
    ReleaseWindow();
}

void NativeViewportAndroid::OnNativeWindowCreated(ANativeWindow* window) {
  DCHECK(!window_);
  window_ = window;

  gpu::GLInProcessContextAttribs attribs;
  gl_context_.reset(gpu::GLInProcessContext::CreateContext(
      false, window_, size_, false, attribs, gfx::PreferDiscreteGpu));
  gl_context_->SetContextLostCallback(base::Bind(
      &NativeViewportAndroid::OnGLContextLost, base::Unretained(this)));

  delegate_->OnGLContextAvailable(gl_context_->GetImplementation());
}

void NativeViewportAndroid::OnGLContextLost() {
  gl_context_.reset();
  delegate_->OnGLContextLost();
}

void NativeViewportAndroid::OnNativeWindowDestroyed() {
  DCHECK(window_);
  ReleaseWindow();
}

void NativeViewportAndroid::OnResized(const gfx::Size& size) {
  size_ = size;
  delegate_->OnResized(size);
}

void NativeViewportAndroid::ReleaseWindow() {
  gl_context_.reset();
  ANativeWindow_release(window_);
  window_ = NULL;
}

void NativeViewportAndroid::Close() {
  // TODO(beng): close activity containing MojoView?

  // TODO(beng): perform this in response to view destruction.
  delegate_->OnDestroyed();
}

// static
scoped_ptr<NativeViewport> NativeViewport::Create(
    shell::Context* context,
    NativeViewportDelegate* delegate) {
  scoped_ptr<NativeViewportAndroid> native_viewport(
      new NativeViewportAndroid(delegate));

  MojoViewportInit* init = new MojoViewportInit();
  init->ui_runner = context->task_runners()->ui_runner();
  init->native_viewport = native_viewport->GetWeakPtr();

  context->task_runners()->java_runner()->PostTask(FROM_HERE,
      base::Bind(MojoViewport::CreateForActivity,
                 context->activity(),
                 init));

   return scoped_ptr<NativeViewport>(native_viewport.Pass());
}

}  // namespace services
}  // namespace mojo
