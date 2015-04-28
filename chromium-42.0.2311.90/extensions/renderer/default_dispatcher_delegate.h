// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_DEFAULT_DISPATCHER_DELEGATE_H_
#define EXTENSIONS_RENDERER_DEFAULT_DISPATCHER_DELEGATE_H_

#include "extensions/renderer/dispatcher_delegate.h"

namespace extensions {

class DefaultDispatcherDelegate : public DispatcherDelegate {
 public:
  DefaultDispatcherDelegate();
  ~DefaultDispatcherDelegate() override;

  // DispatcherDelegate implementation.
  scoped_ptr<ScriptContext> CreateScriptContext(
      const v8::Handle<v8::Context>& v8_context,
      blink::WebFrame* frame,
      const Extension* extension,
      Feature::Context context_type,
      const Extension* effective_extension,
      Feature::Context effective_context_type) override;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_DEFAULT_DISPATCHER_DELEGATE_H_
