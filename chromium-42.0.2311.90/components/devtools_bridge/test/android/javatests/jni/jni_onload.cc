// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/base_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "components/devtools_bridge/android/component_loader.h"

using namespace devtools_bridge::android;

JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!base::android::RegisterLibraryLoaderEntryHook(env)) {
    return -1;
  }
  if (!ComponentLoader::OnLoad(env)) {
    return -1;
  }
  return JNI_VERSION_1_4;
}
