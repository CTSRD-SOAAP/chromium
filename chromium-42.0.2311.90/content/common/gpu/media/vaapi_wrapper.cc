// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/vaapi_wrapper.h"

#include <dlfcn.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/sys_info.h"
// Auto-generated for dlopen libva libraries
#include "content/common/gpu/media/va_stubs.h"
#include "content/common/gpu/media/vaapi_picture.h"
#include "third_party/libyuv/include/libyuv.h"
#include "ui/gl/gl_bindings.h"
#if defined(USE_X11)
#include "ui/gfx/x/x11_types.h"
#elif defined(USE_OZONE)
#include "third_party/libva/va/drm/va_drm.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#endif  // USE_X11

using content_common_gpu_media::kModuleVa;
#if defined(USE_X11)
using content_common_gpu_media::kModuleVa_x11;
#elif defined(USE_OZONE)
using content_common_gpu_media::kModuleVa_drm;
#endif  // USE_X11
using content_common_gpu_media::InitializeStubs;
using content_common_gpu_media::StubPathMap;

#define LOG_VA_ERROR_AND_REPORT(va_error, err_msg)         \
  do {                                                     \
    LOG(ERROR) << err_msg                                  \
             << " VA error: " << vaErrorStr(va_error);     \
    report_error_to_uma_cb_.Run();                         \
  } while (0)

#define VA_LOG_ON_ERROR(va_error, err_msg)                 \
  do {                                                     \
    if ((va_error) != VA_STATUS_SUCCESS)                   \
      LOG_VA_ERROR_AND_REPORT(va_error, err_msg);          \
  } while (0)

#define VA_SUCCESS_OR_RETURN(va_error, err_msg, ret)       \
  do {                                                     \
    if ((va_error) != VA_STATUS_SUCCESS) {                 \
      LOG_VA_ERROR_AND_REPORT(va_error, err_msg);          \
      return (ret);                                        \
    }                                                      \
  } while (0)

namespace content {

// Config attributes common for both encode and decode.
static const VAConfigAttrib kCommonVAConfigAttribs[] = {
    {VAConfigAttribRTFormat, VA_RT_FORMAT_YUV420},
};

// Attributes required for encode.
static const VAConfigAttrib kEncodeVAConfigAttribs[] = {
    {VAConfigAttribRateControl, VA_RC_CBR},
    {VAConfigAttribEncPackedHeaders,
     VA_ENC_PACKED_HEADER_SEQUENCE | VA_ENC_PACKED_HEADER_PICTURE},
};

struct ProfileMap {
  media::VideoCodecProfile profile;
  VAProfile va_profile;
};

// A map between VideoCodecProfile and VAProfile.
static const ProfileMap kProfileMap[] = {
    {media::H264PROFILE_BASELINE, VAProfileH264Baseline},
    {media::H264PROFILE_MAIN, VAProfileH264Main},
    // TODO(posciak): See if we can/want support other variants of
    // media::H264PROFILE_HIGH*.
    {media::H264PROFILE_HIGH, VAProfileH264High},
};

static std::vector<VAConfigAttrib> GetRequiredAttribs(
    VaapiWrapper::CodecMode mode) {
  std::vector<VAConfigAttrib> required_attribs;
  required_attribs.insert(
      required_attribs.end(),
      kCommonVAConfigAttribs,
      kCommonVAConfigAttribs + arraysize(kCommonVAConfigAttribs));
  if (mode == VaapiWrapper::kEncode) {
    required_attribs.insert(
        required_attribs.end(),
        kEncodeVAConfigAttribs,
        kEncodeVAConfigAttribs + arraysize(kEncodeVAConfigAttribs));
  }
  return required_attribs;
}

// Maps Profile enum values to VaProfile values.
static VAProfile ProfileToVAProfile(
    media::VideoCodecProfile profile,
    const std::vector<VAProfile>& supported_profiles) {

  VAProfile va_profile = VAProfileNone;
  for (size_t i = 0; i < arraysize(kProfileMap); i++) {
    if (kProfileMap[i].profile == profile) {
      va_profile = kProfileMap[i].va_profile;
      break;
    }
  }

  bool supported = std::find(supported_profiles.begin(),
                             supported_profiles.end(),
                             va_profile) != supported_profiles.end();

  if (!supported && va_profile == VAProfileH264Baseline) {
    // crbug.com/345569: media::ProfileIDToVideoCodecProfile() currently strips
    // the information whether the profile is constrained or not, so we have no
    // way to know here. Try for baseline first, but if it is not supported,
    // try constrained baseline and hope this is what it actually is
    // (which in practice is true for a great majority of cases).
    if (std::find(supported_profiles.begin(),
                  supported_profiles.end(),
                  VAProfileH264ConstrainedBaseline) !=
        supported_profiles.end()) {
      va_profile = VAProfileH264ConstrainedBaseline;
      DVLOG(1) << "Falling back to constrained baseline profile.";
    }
  }

  return va_profile;
}

VASurface::VASurface(VASurfaceID va_surface_id,
                     const gfx::Size& size,
                     const ReleaseCB& release_cb)
    : va_surface_id_(va_surface_id), size_(size), release_cb_(release_cb) {
  DCHECK(!release_cb_.is_null());
}

VASurface::~VASurface() {
  release_cb_.Run(va_surface_id_);
}

VaapiWrapper::VaapiWrapper()
    : va_display_(NULL),
      va_config_id_(VA_INVALID_ID),
      va_context_id_(VA_INVALID_ID),
      va_initialized_(false),
      va_vpp_config_id_(VA_INVALID_ID),
      va_vpp_context_id_(VA_INVALID_ID),
      va_vpp_buffer_id_(VA_INVALID_ID) {
}

VaapiWrapper::~VaapiWrapper() {
  DestroyPendingBuffers();
  DestroyCodedBuffers();
  DestroySurfaces();
  DeinitializeVpp();
  Deinitialize();
}

scoped_ptr<VaapiWrapper> VaapiWrapper::Create(
    CodecMode mode,
    VAProfile va_profile,
    const base::Closure& report_error_to_uma_cb) {
  scoped_ptr<VaapiWrapper> vaapi_wrapper(new VaapiWrapper());

  if (!vaapi_wrapper->VaInitialize(report_error_to_uma_cb))
    return nullptr;
  if (!vaapi_wrapper->Initialize(mode, va_profile))
    return nullptr;

  return vaapi_wrapper.Pass();
}

scoped_ptr<VaapiWrapper> VaapiWrapper::CreateForVideoCodec(
    CodecMode mode,
    media::VideoCodecProfile profile,
    const base::Closure& report_error_to_uma_cb) {
  scoped_ptr<VaapiWrapper> vaapi_wrapper(new VaapiWrapper());

  if (!vaapi_wrapper->VaInitialize(report_error_to_uma_cb))
    return nullptr;

  std::vector<VAProfile> supported_va_profiles;
  if (!vaapi_wrapper->GetSupportedVaProfiles(&supported_va_profiles))
    return nullptr;

  VAProfile va_profile = ProfileToVAProfile(profile, supported_va_profiles);
  if (!vaapi_wrapper->Initialize(mode, va_profile))
    return nullptr;

  return vaapi_wrapper.Pass();
}

std::vector<media::VideoCodecProfile> VaapiWrapper::GetSupportedEncodeProfiles(
    const base::Closure& report_error_to_uma_cb) {
  std::vector<media::VideoCodecProfile> supported_profiles;

  scoped_ptr<VaapiWrapper> wrapper(new VaapiWrapper());
  if (!wrapper->VaInitialize(report_error_to_uma_cb)) {
    return supported_profiles;
  }

  std::vector<VAProfile> va_profiles;
  if (!wrapper->GetSupportedVaProfiles(&va_profiles))
    return supported_profiles;

  std::vector<VAConfigAttrib> required_attribs = GetRequiredAttribs(kEncode);
  for (size_t i = 0; i < arraysize(kProfileMap); i++) {
    VAProfile va_profile =
        ProfileToVAProfile(kProfileMap[i].profile, va_profiles);
    if (va_profile != VAProfileNone &&
        wrapper->IsEntrypointSupported(va_profile, VAEntrypointEncSlice) &&
        wrapper->AreAttribsSupported(
            va_profile, VAEntrypointEncSlice, required_attribs)) {
      supported_profiles.push_back(kProfileMap[i].profile);
    }
  }
  return supported_profiles;
}

void VaapiWrapper::TryToSetVADisplayAttributeToLocalGPU() {
  base::AutoLock auto_lock(va_lock_);
  VADisplayAttribute item = {VADisplayAttribRenderMode,
                             1,  // At least support '_LOCAL_OVERLAY'.
                             -1,  // The maximum possible support 'ALL'.
                             VA_RENDER_MODE_LOCAL_GPU,
                             VA_DISPLAY_ATTRIB_SETTABLE};

  VAStatus va_res = vaSetDisplayAttributes(va_display_, &item, 1);
  if (va_res != VA_STATUS_SUCCESS)
    DVLOG(2) << "vaSetDisplayAttributes unsupported, ignoring by default.";
}

bool VaapiWrapper::VaInitialize(const base::Closure& report_error_to_uma_cb) {
  static bool vaapi_functions_initialized = PostSandboxInitialization();
  if (!vaapi_functions_initialized) {
    bool running_on_chromeos = false;
#if defined(OS_CHROMEOS)
    // When chrome runs on linux with chromeos=1, do not log error message
    // without VAAPI libraries.
    running_on_chromeos = base::SysInfo::IsRunningOnChromeOS();
#endif
    static const char kErrorMsg[] = "Failed to initialize VAAPI libs";
    if (running_on_chromeos)
      LOG(ERROR) << kErrorMsg;
    else
      DVLOG(1) << kErrorMsg;
    return false;
  }

  report_error_to_uma_cb_ = report_error_to_uma_cb;

  base::AutoLock auto_lock(va_lock_);

#if defined(USE_X11)
  va_display_ = vaGetDisplay(gfx::GetXDisplay());
#elif defined(USE_OZONE)
  ui::OzonePlatform* platform = ui::OzonePlatform::GetInstance();
  ui::SurfaceFactoryOzone* factory = platform->GetSurfaceFactoryOzone();

  va_display_ = vaGetDisplayDRM(factory->GetDrmFd());
#endif  // USE_X11

  if (!vaDisplayIsValid(va_display_)) {
    LOG(ERROR) << "Could not get a valid VA display";
    return false;
  }

  VAStatus va_res = vaInitialize(va_display_, &major_version_, &minor_version_);
  VA_SUCCESS_OR_RETURN(va_res, "vaInitialize failed", false);
  va_initialized_ = true;
  DVLOG(1) << "VAAPI version: " << major_version_ << "." << minor_version_;

  if (VAAPIVersionLessThan(0, 34)) {
    LOG(ERROR) << "VAAPI version < 0.34 is not supported.";
    return false;
  }
  return true;
}

bool VaapiWrapper::GetSupportedVaProfiles(std::vector<VAProfile>* profiles) {
  base::AutoLock auto_lock(va_lock_);
  // Query the driver for supported profiles.
  int max_profiles = vaMaxNumProfiles(va_display_);
  std::vector<VAProfile> supported_profiles(
      base::checked_cast<size_t>(max_profiles));

  int num_supported_profiles;
  VAStatus va_res = vaQueryConfigProfiles(
      va_display_, &supported_profiles[0], &num_supported_profiles);
  VA_SUCCESS_OR_RETURN(va_res, "vaQueryConfigProfiles failed", false);
  if (num_supported_profiles < 0 || num_supported_profiles > max_profiles) {
    LOG(ERROR) << "vaQueryConfigProfiles returned: " << num_supported_profiles;
    return false;
  }

  supported_profiles.resize(base::checked_cast<size_t>(num_supported_profiles));
  *profiles = supported_profiles;
  return true;
}

bool VaapiWrapper::IsEntrypointSupported(VAProfile va_profile,
                                         VAEntrypoint entrypoint) {
  base::AutoLock auto_lock(va_lock_);
  // Query the driver for supported entrypoints.
  int max_entrypoints = vaMaxNumEntrypoints(va_display_);
  std::vector<VAEntrypoint> supported_entrypoints(
      base::checked_cast<size_t>(max_entrypoints));

  int num_supported_entrypoints;
  VAStatus va_res = vaQueryConfigEntrypoints(va_display_,
                                             va_profile,
                                             &supported_entrypoints[0],
                                             &num_supported_entrypoints);
  VA_SUCCESS_OR_RETURN(va_res, "vaQueryConfigEntrypoints failed", false);
  if (num_supported_entrypoints < 0 ||
      num_supported_entrypoints > max_entrypoints) {
    LOG(ERROR) << "vaQueryConfigEntrypoints returned: "
               << num_supported_entrypoints;
    return false;
  }

  if (std::find(supported_entrypoints.begin(),
                supported_entrypoints.end(),
                entrypoint) == supported_entrypoints.end()) {
    DVLOG(1) << "Unsupported entrypoint";
    return false;
  }
  return true;
}

bool VaapiWrapper::AreAttribsSupported(
    VAProfile va_profile,
    VAEntrypoint entrypoint,
    const std::vector<VAConfigAttrib>& required_attribs) {
  base::AutoLock auto_lock(va_lock_);
  // Query the driver for required attributes.
  std::vector<VAConfigAttrib> attribs = required_attribs;
  for (size_t i = 0; i < required_attribs.size(); ++i)
    attribs[i].value = 0;

  VAStatus va_res = vaGetConfigAttributes(
      va_display_, va_profile, entrypoint, &attribs[0], attribs.size());
  VA_SUCCESS_OR_RETURN(va_res, "vaGetConfigAttributes failed", false);

  for (size_t i = 0; i < required_attribs.size(); ++i) {
    if (attribs[i].type != required_attribs[i].type ||
        (attribs[i].value & required_attribs[i].value) !=
            required_attribs[i].value) {
      DVLOG(1) << "Unsupported value " << required_attribs[i].value
               << " for attribute type " << required_attribs[i].type;
      return false;
    }
  }
  return true;
}

bool VaapiWrapper::Initialize(CodecMode mode, VAProfile va_profile) {
  if (va_profile == VAProfileNone) {
    DVLOG(1) << "Unsupported profile";
    return false;
  }
  VAEntrypoint entrypoint =
      (mode == kEncode ? VAEntrypointEncSlice : VAEntrypointVLD);
  if (!IsEntrypointSupported(va_profile, entrypoint))
    return false;
  std::vector<VAConfigAttrib> required_attribs = GetRequiredAttribs(mode);
  if (!AreAttribsSupported(va_profile, entrypoint, required_attribs))
    return false;

  TryToSetVADisplayAttributeToLocalGPU();

  base::AutoLock auto_lock(va_lock_);
  VAStatus va_res = vaCreateConfig(va_display_,
                                   va_profile,
                                   entrypoint,
                                   &required_attribs[0],
                                   required_attribs.size(),
                                   &va_config_id_);
  VA_SUCCESS_OR_RETURN(va_res, "vaCreateConfig failed", false);

  return true;
}

void VaapiWrapper::Deinitialize() {
  base::AutoLock auto_lock(va_lock_);

  if (va_config_id_ != VA_INVALID_ID) {
    VAStatus va_res = vaDestroyConfig(va_display_, va_config_id_);
    VA_LOG_ON_ERROR(va_res, "vaDestroyConfig failed");
  }

  // Must check if vaInitialize completed successfully, to work around a bug in
  // libva. The bug was fixed upstream:
  // http://lists.freedesktop.org/archives/libva/2013-July/001807.html
  // TODO(mgiuca): Remove this check, and the |va_initialized_| variable, once
  // the fix has rolled out sufficiently.
  if (va_initialized_ && va_display_) {
    VAStatus va_res = vaTerminate(va_display_);
    VA_LOG_ON_ERROR(va_res, "vaTerminate failed");
  }

  va_config_id_ = VA_INVALID_ID;
  va_display_ = NULL;
  va_initialized_ = false;
}

bool VaapiWrapper::VAAPIVersionLessThan(int major, int minor) {
  return (major_version_ < major) ||
      (major_version_ == major && minor_version_ < minor);
}

bool VaapiWrapper::CreateSurfaces(const gfx::Size& size,
                                  size_t num_surfaces,
                                  std::vector<VASurfaceID>* va_surfaces) {
  base::AutoLock auto_lock(va_lock_);
  DVLOG(2) << "Creating " << num_surfaces << " surfaces";

  DCHECK(va_surfaces->empty());
  DCHECK(va_surface_ids_.empty());
  va_surface_ids_.resize(num_surfaces);

  // Allocate surfaces in driver.
  VAStatus va_res = vaCreateSurfaces(va_display_,
                                     VA_RT_FORMAT_YUV420,
                                     size.width(), size.height(),
                                     &va_surface_ids_[0],
                                     va_surface_ids_.size(),
                                     NULL, 0);

  VA_LOG_ON_ERROR(va_res, "vaCreateSurfaces failed");
  if (va_res != VA_STATUS_SUCCESS) {
    va_surface_ids_.clear();
    return false;
  }

  // And create a context associated with them.
  va_res = vaCreateContext(va_display_, va_config_id_,
                           size.width(), size.height(), VA_PROGRESSIVE,
                           &va_surface_ids_[0], va_surface_ids_.size(),
                           &va_context_id_);

  VA_LOG_ON_ERROR(va_res, "vaCreateContext failed");
  if (va_res != VA_STATUS_SUCCESS) {
    DestroySurfaces();
    return false;
  }

  *va_surfaces = va_surface_ids_;
  return true;
}

void VaapiWrapper::DestroySurfaces() {
  base::AutoLock auto_lock(va_lock_);
  DVLOG(2) << "Destroying " << va_surface_ids_.size()  << " surfaces";

  if (va_context_id_ != VA_INVALID_ID) {
    VAStatus va_res = vaDestroyContext(va_display_, va_context_id_);
    VA_LOG_ON_ERROR(va_res, "vaDestroyContext failed");
  }

  if (!va_surface_ids_.empty()) {
    VAStatus va_res = vaDestroySurfaces(va_display_, &va_surface_ids_[0],
                                        va_surface_ids_.size());
    VA_LOG_ON_ERROR(va_res, "vaDestroySurfaces failed");
  }

  va_surface_ids_.clear();
  va_context_id_ = VA_INVALID_ID;
}

scoped_refptr<VASurface> VaapiWrapper::CreateUnownedSurface(
    unsigned int va_format,
    const gfx::Size& size,
    const std::vector<VASurfaceAttrib>& va_attribs) {
  base::AutoLock auto_lock(va_lock_);

  std::vector<VASurfaceAttrib> attribs(va_attribs);
  VASurfaceID va_surface_id;
  VAStatus va_res =
      vaCreateSurfaces(va_display_, va_format, size.width(), size.height(),
                       &va_surface_id, 1, &attribs[0], attribs.size());

  scoped_refptr<VASurface> va_surface;
  VA_SUCCESS_OR_RETURN(va_res, "Failed to create unowned VASurface",
                       va_surface);

  // This is safe to use Unretained() here, because the VDA takes care
  // of the destruction order. All the surfaces will be destroyed
  // before VaapiWrapper.
  va_surface = new VASurface(
      va_surface_id, size,
      base::Bind(&VaapiWrapper::DestroyUnownedSurface, base::Unretained(this)));

  return va_surface;
}

void VaapiWrapper::DestroyUnownedSurface(VASurfaceID va_surface_id) {
  base::AutoLock auto_lock(va_lock_);

  VAStatus va_res = vaDestroySurfaces(va_display_, &va_surface_id, 1);
  VA_LOG_ON_ERROR(va_res, "vaDestroySurfaces on surface failed");
}

bool VaapiWrapper::SubmitBuffer(VABufferType va_buffer_type,
                                size_t size,
                                void* buffer) {
  base::AutoLock auto_lock(va_lock_);

  VABufferID buffer_id;
  VAStatus va_res = vaCreateBuffer(va_display_, va_context_id_,
                                   va_buffer_type, size,
                                   1, buffer, &buffer_id);
  VA_SUCCESS_OR_RETURN(va_res, "Failed to create a VA buffer", false);

  switch (va_buffer_type) {
    case VASliceParameterBufferType:
    case VASliceDataBufferType:
    case VAEncSliceParameterBufferType:
      pending_slice_bufs_.push_back(buffer_id);
      break;

    default:
      pending_va_bufs_.push_back(buffer_id);
      break;
  }

  return true;
}

bool VaapiWrapper::SubmitVAEncMiscParamBuffer(
    VAEncMiscParameterType misc_param_type,
    size_t size,
    void* buffer) {
  base::AutoLock auto_lock(va_lock_);

  VABufferID buffer_id;
  VAStatus va_res = vaCreateBuffer(va_display_,
                                   va_context_id_,
                                   VAEncMiscParameterBufferType,
                                   sizeof(VAEncMiscParameterBuffer) + size,
                                   1,
                                   NULL,
                                   &buffer_id);
  VA_SUCCESS_OR_RETURN(va_res, "Failed to create a VA buffer", false);

  void* data_ptr = NULL;
  va_res = vaMapBuffer(va_display_, buffer_id, &data_ptr);
  VA_LOG_ON_ERROR(va_res, "vaMapBuffer failed");
  if (va_res != VA_STATUS_SUCCESS) {
    vaDestroyBuffer(va_display_, buffer_id);
    return false;
  }

  DCHECK(data_ptr);

  VAEncMiscParameterBuffer* misc_param =
      reinterpret_cast<VAEncMiscParameterBuffer*>(data_ptr);
  misc_param->type = misc_param_type;
  memcpy(misc_param->data, buffer, size);
  va_res = vaUnmapBuffer(va_display_, buffer_id);
  VA_LOG_ON_ERROR(va_res, "vaUnmapBuffer failed");

  pending_va_bufs_.push_back(buffer_id);
  return true;
}

void VaapiWrapper::DestroyPendingBuffers() {
  base::AutoLock auto_lock(va_lock_);

  for (size_t i = 0; i < pending_va_bufs_.size(); ++i) {
    VAStatus va_res = vaDestroyBuffer(va_display_, pending_va_bufs_[i]);
    VA_LOG_ON_ERROR(va_res, "vaDestroyBuffer failed");
  }

  for (size_t i = 0; i < pending_slice_bufs_.size(); ++i) {
    VAStatus va_res = vaDestroyBuffer(va_display_, pending_slice_bufs_[i]);
    VA_LOG_ON_ERROR(va_res, "vaDestroyBuffer failed");
  }

  pending_va_bufs_.clear();
  pending_slice_bufs_.clear();
}

bool VaapiWrapper::CreateCodedBuffer(size_t size, VABufferID* buffer_id) {
  base::AutoLock auto_lock(va_lock_);
  VAStatus va_res = vaCreateBuffer(va_display_,
                                   va_context_id_,
                                   VAEncCodedBufferType,
                                   size,
                                   1,
                                   NULL,
                                   buffer_id);
  VA_SUCCESS_OR_RETURN(va_res, "Failed to create a coded buffer", false);

  DCHECK(coded_buffers_.insert(*buffer_id).second);
  return true;
}

void VaapiWrapper::DestroyCodedBuffers() {
  base::AutoLock auto_lock(va_lock_);

  for (std::set<VABufferID>::const_iterator iter = coded_buffers_.begin();
       iter != coded_buffers_.end();
       ++iter) {
    VAStatus va_res = vaDestroyBuffer(va_display_, *iter);
    VA_LOG_ON_ERROR(va_res, "vaDestroyBuffer failed");
  }

  coded_buffers_.clear();
}

bool VaapiWrapper::Execute(VASurfaceID va_surface_id) {
  base::AutoLock auto_lock(va_lock_);

  DVLOG(4) << "Pending VA bufs to commit: " << pending_va_bufs_.size();
  DVLOG(4) << "Pending slice bufs to commit: " << pending_slice_bufs_.size();
  DVLOG(4) << "Target VA surface " << va_surface_id;

  // Get ready to execute for given surface.
  VAStatus va_res = vaBeginPicture(va_display_, va_context_id_,
                                   va_surface_id);
  VA_SUCCESS_OR_RETURN(va_res, "vaBeginPicture failed", false);

  if (pending_va_bufs_.size() > 0) {
    // Commit parameter and slice buffers.
    va_res = vaRenderPicture(va_display_,
                             va_context_id_,
                             &pending_va_bufs_[0],
                             pending_va_bufs_.size());
    VA_SUCCESS_OR_RETURN(va_res, "vaRenderPicture for va_bufs failed", false);
  }

  if (pending_slice_bufs_.size() > 0) {
    va_res = vaRenderPicture(va_display_,
                             va_context_id_,
                             &pending_slice_bufs_[0],
                             pending_slice_bufs_.size());
    VA_SUCCESS_OR_RETURN(va_res, "vaRenderPicture for slices failed", false);
  }

  // Instruct HW codec to start processing committed buffers.
  // Does not block and the job is not finished after this returns.
  va_res = vaEndPicture(va_display_, va_context_id_);
  VA_SUCCESS_OR_RETURN(va_res, "vaEndPicture failed", false);

  return true;
}

bool VaapiWrapper::ExecuteAndDestroyPendingBuffers(VASurfaceID va_surface_id) {
  bool result = Execute(va_surface_id);
  DestroyPendingBuffers();
  return result;
}

#if defined(USE_X11)
bool VaapiWrapper::PutSurfaceIntoPixmap(VASurfaceID va_surface_id,
                                        Pixmap x_pixmap,
                                        gfx::Size dest_size) {
  base::AutoLock auto_lock(va_lock_);

  VAStatus va_res = vaSyncSurface(va_display_, va_surface_id);
  VA_SUCCESS_OR_RETURN(va_res, "Failed syncing surface", false);

  // Put the data into an X Pixmap.
  va_res = vaPutSurface(va_display_,
                        va_surface_id,
                        x_pixmap,
                        0, 0, dest_size.width(), dest_size.height(),
                        0, 0, dest_size.width(), dest_size.height(),
                        NULL, 0, 0);
  VA_SUCCESS_OR_RETURN(va_res, "Failed putting surface to pixmap", false);
  return true;
}
#endif  // USE_X11

bool VaapiWrapper::GetDerivedVaImage(VASurfaceID va_surface_id,
                                     VAImage* image,
                                     void** mem) {
  base::AutoLock auto_lock(va_lock_);

  VAStatus va_res = vaSyncSurface(va_display_, va_surface_id);
  VA_SUCCESS_OR_RETURN(va_res, "Failed syncing surface", false);

  // Derive a VAImage from the VASurface
  va_res = vaDeriveImage(va_display_, va_surface_id, image);
  VA_LOG_ON_ERROR(va_res, "vaDeriveImage failed");
  if (va_res != VA_STATUS_SUCCESS)
    return false;

  // Map the VAImage into memory
  va_res = vaMapBuffer(va_display_, image->buf, mem);
  VA_LOG_ON_ERROR(va_res, "vaMapBuffer failed");
  if (va_res == VA_STATUS_SUCCESS)
    return true;

  va_res = vaDestroyImage(va_display_, image->image_id);
  VA_LOG_ON_ERROR(va_res, "vaDestroyImage failed");

  return false;
}

bool VaapiWrapper::GetVaImage(VASurfaceID va_surface_id,
                              VAImageFormat* format,
                              const gfx::Size& size,
                              VAImage* image,
                              void** mem) {
  base::AutoLock auto_lock(va_lock_);

  VAStatus va_res = vaSyncSurface(va_display_, va_surface_id);
  VA_SUCCESS_OR_RETURN(va_res, "Failed syncing surface", false);

  va_res =
      vaCreateImage(va_display_, format, size.width(), size.height(), image);
  VA_SUCCESS_OR_RETURN(va_res, "vaCreateImage failed", false);

  va_res = vaGetImage(va_display_, va_surface_id, 0, 0, size.width(),
                      size.height(), image->image_id);
  VA_LOG_ON_ERROR(va_res, "vaGetImage failed");

  if (va_res == VA_STATUS_SUCCESS) {
    // Map the VAImage into memory
    va_res = vaMapBuffer(va_display_, image->buf, mem);
    VA_LOG_ON_ERROR(va_res, "vaMapBuffer failed");
  }

  if (va_res != VA_STATUS_SUCCESS) {
    va_res = vaDestroyImage(va_display_, image->image_id);
    VA_LOG_ON_ERROR(va_res, "vaDestroyImage failed");
    return false;
  }

  return true;
}

void VaapiWrapper::ReturnVaImage(VAImage* image) {
  base::AutoLock auto_lock(va_lock_);

  VAStatus va_res = vaUnmapBuffer(va_display_, image->buf);
  VA_LOG_ON_ERROR(va_res, "vaUnmapBuffer failed");

  va_res = vaDestroyImage(va_display_, image->image_id);
  VA_LOG_ON_ERROR(va_res, "vaDestroyImage failed");
}

static void DestroyVAImage(VADisplay va_display, VAImage image) {
  if (image.image_id != VA_INVALID_ID)
    vaDestroyImage(va_display, image.image_id);
}

bool VaapiWrapper::UploadVideoFrameToSurface(
    const scoped_refptr<media::VideoFrame>& frame,
    VASurfaceID va_surface_id) {
  base::AutoLock auto_lock(va_lock_);

  VAImage image;
  VAStatus va_res = vaDeriveImage(va_display_, va_surface_id, &image);
  VA_SUCCESS_OR_RETURN(va_res, "vaDeriveImage failed", false);
  base::ScopedClosureRunner vaimage_deleter(
      base::Bind(&DestroyVAImage, va_display_, image));

  if (image.format.fourcc != VA_FOURCC_NV12) {
    LOG(ERROR) << "Unsupported image format: " << image.format.fourcc;
    return false;
  }

  if (gfx::Rect(image.width, image.height) < gfx::Rect(frame->coded_size())) {
    LOG(ERROR) << "Buffer too small to fit the frame.";
    return false;
  }

  void* image_ptr = NULL;
  va_res = vaMapBuffer(va_display_, image.buf, &image_ptr);
  VA_SUCCESS_OR_RETURN(va_res, "vaMapBuffer failed", false);
  DCHECK(image_ptr);

  int ret = 0;
  {
    base::AutoUnlock auto_unlock(va_lock_);
    ret = libyuv::I420ToNV12(frame->data(media::VideoFrame::kYPlane),
                             frame->stride(media::VideoFrame::kYPlane),
                             frame->data(media::VideoFrame::kUPlane),
                             frame->stride(media::VideoFrame::kUPlane),
                             frame->data(media::VideoFrame::kVPlane),
                             frame->stride(media::VideoFrame::kVPlane),
                             static_cast<uint8*>(image_ptr) + image.offsets[0],
                             image.pitches[0],
                             static_cast<uint8*>(image_ptr) + image.offsets[1],
                             image.pitches[1],
                             image.width,
                             image.height);
  }

  va_res = vaUnmapBuffer(va_display_, image.buf);
  VA_LOG_ON_ERROR(va_res, "vaUnmapBuffer failed");

  return ret == 0;
}

bool VaapiWrapper::DownloadAndDestroyCodedBuffer(VABufferID buffer_id,
                                                 VASurfaceID sync_surface_id,
                                                 uint8* target_ptr,
                                                 size_t target_size,
                                                 size_t* coded_data_size) {
  base::AutoLock auto_lock(va_lock_);

  VAStatus va_res = vaSyncSurface(va_display_, sync_surface_id);
  VA_SUCCESS_OR_RETURN(va_res, "Failed syncing surface", false);

  VACodedBufferSegment* buffer_segment = NULL;
  va_res = vaMapBuffer(
      va_display_, buffer_id, reinterpret_cast<void**>(&buffer_segment));
  VA_SUCCESS_OR_RETURN(va_res, "vaMapBuffer failed", false);
  DCHECK(target_ptr);

  {
    base::AutoUnlock auto_unlock(va_lock_);
    *coded_data_size = 0;

    while (buffer_segment) {
      DCHECK(buffer_segment->buf);

      if (buffer_segment->size > target_size) {
        LOG(ERROR) << "Insufficient output buffer size";
        break;
      }

      memcpy(target_ptr, buffer_segment->buf, buffer_segment->size);

      target_ptr += buffer_segment->size;
      *coded_data_size += buffer_segment->size;
      target_size -= buffer_segment->size;

      buffer_segment =
          reinterpret_cast<VACodedBufferSegment*>(buffer_segment->next);
    }
  }

  va_res = vaUnmapBuffer(va_display_, buffer_id);
  VA_LOG_ON_ERROR(va_res, "vaUnmapBuffer failed");

  va_res = vaDestroyBuffer(va_display_, buffer_id);
  VA_LOG_ON_ERROR(va_res, "vaDestroyBuffer failed");

  DCHECK(coded_buffers_.erase(buffer_id));

  return buffer_segment == NULL;
}

bool VaapiWrapper::BlitSurface(VASurfaceID va_surface_id_src,
                               const gfx::Size& src_size,
                               VASurfaceID va_surface_id_dest,
                               const gfx::Size& dest_size) {
  base::AutoLock auto_lock(va_lock_);

  // Initialize the post processing engine if not already done.
  if (va_vpp_buffer_id_ == VA_INVALID_ID) {
    if (!InitializeVpp_Locked())
      return false;
  }

  VAProcPipelineParameterBuffer* pipeline_param;
  VA_SUCCESS_OR_RETURN(vaMapBuffer(va_display_, va_vpp_buffer_id_,
                                   reinterpret_cast<void**>(&pipeline_param)),
                       "Couldn't map vpp buffer", false);

  memset(pipeline_param, 0, sizeof *pipeline_param);

  VARectangle input_region;
  input_region.x = input_region.y = 0;
  input_region.width = src_size.width();
  input_region.height = src_size.height();
  pipeline_param->surface_region = &input_region;
  pipeline_param->surface = va_surface_id_src;
  pipeline_param->surface_color_standard = VAProcColorStandardNone;

  VARectangle output_region;
  output_region.x = output_region.y = 0;
  output_region.width = dest_size.width();
  output_region.height = dest_size.height();
  pipeline_param->output_region = &output_region;
  pipeline_param->output_background_color = 0xff000000;
  pipeline_param->output_color_standard = VAProcColorStandardNone;

  VA_SUCCESS_OR_RETURN(vaUnmapBuffer(va_display_, va_vpp_buffer_id_),
                       "Couldn't unmap vpp buffer", false);

  VA_SUCCESS_OR_RETURN(
      vaBeginPicture(va_display_, va_vpp_context_id_, va_surface_id_dest),
      "Couldn't begin picture", false);

  VA_SUCCESS_OR_RETURN(
      vaRenderPicture(va_display_, va_vpp_context_id_, &va_vpp_buffer_id_, 1),
      "Couldn't render picture", false);

  VA_SUCCESS_OR_RETURN(vaEndPicture(va_display_, va_vpp_context_id_),
                       "Couldn't end picture", false);

  return true;
}

bool VaapiWrapper::InitializeVpp_Locked() {
  va_lock_.AssertAcquired();

  VA_SUCCESS_OR_RETURN(
      vaCreateConfig(va_display_, VAProfileNone, VAEntrypointVideoProc, NULL, 0,
                     &va_vpp_config_id_),
      "Couldn't create config", false);

  // The size of the picture for the context is irrelevant in the case
  // of the VPP, just passing 1x1.
  VA_SUCCESS_OR_RETURN(vaCreateContext(va_display_, va_vpp_config_id_, 1, 1, 0,
                                       NULL, 0, &va_vpp_context_id_),
                       "Couldn't create context", false);

  VA_SUCCESS_OR_RETURN(vaCreateBuffer(va_display_, va_vpp_context_id_,
                                      VAProcPipelineParameterBufferType,
                                      sizeof(VAProcPipelineParameterBuffer), 1,
                                      NULL, &va_vpp_buffer_id_),
                       "Couldn't create buffer", false);

  return true;
}

void VaapiWrapper::DeinitializeVpp() {
  base::AutoLock auto_lock(va_lock_);

  if (va_vpp_buffer_id_ != VA_INVALID_ID) {
    vaDestroyBuffer(va_display_, va_vpp_buffer_id_);
    va_vpp_buffer_id_ = VA_INVALID_ID;
  }
  if (va_vpp_context_id_ != VA_INVALID_ID) {
    vaDestroyContext(va_display_, va_vpp_context_id_);
    va_vpp_context_id_ = VA_INVALID_ID;
  }
  if (va_vpp_config_id_ != VA_INVALID_ID) {
    vaDestroyConfig(va_display_, va_vpp_config_id_);
    va_vpp_config_id_ = VA_INVALID_ID;
  }
}

// static
bool VaapiWrapper::PostSandboxInitialization() {
  StubPathMap paths;

  paths[kModuleVa].push_back("libva.so.1");

#if defined(USE_X11)
  paths[kModuleVa_x11].push_back("libva-x11.so.1");
#elif defined(USE_OZONE)
  paths[kModuleVa_drm].push_back("libva-drm.so.1");
#endif

  return InitializeStubs(paths);
}

}  // namespace content
