// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "content/child/child_thread.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/media/vaapi_video_decode_accelerator.h"
#include "media/base/bind_to_loop.h"
#include "media/video/picture.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/scoped_binders.h"

static void ReportToUMA(
    content::VaapiH264Decoder::VAVDAH264DecoderFailure failure) {
  UMA_HISTOGRAM_ENUMERATION(
      "Media.VAVDAH264.DecoderFailure",
      failure,
      content::VaapiH264Decoder::VAVDA_H264_DECODER_FAILURES_MAX);
}

namespace content {

#define RETURN_AND_NOTIFY_ON_FAILURE(result, log, error_code, ret)  \
  do {                                                              \
    if (!(result)) {                                                \
      DVLOG(1) << log;                                              \
      NotifyError(error_code);                                      \
      return ret;                                                   \
    }                                                               \
  } while (0)

VaapiVideoDecodeAccelerator::InputBuffer::InputBuffer() : id(0), size(0) {
}

VaapiVideoDecodeAccelerator::InputBuffer::~InputBuffer() {
}

void VaapiVideoDecodeAccelerator::NotifyError(Error error) {
  if (message_loop_ != base::MessageLoop::current()) {
    DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &VaapiVideoDecodeAccelerator::NotifyError, weak_this_, error));
    return;
  }

  // Post Cleanup() as a task so we don't recursively acquire lock_.
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &VaapiVideoDecodeAccelerator::Cleanup, weak_this_));

  DVLOG(1) << "Notifying of error " << error;
  if (client_) {
    client_->NotifyError(error);
    client_ptr_factory_.InvalidateWeakPtrs();
  }
}

// TFPPicture allocates X Pixmaps and binds them to textures passed
// in PictureBuffers from clients to them. TFPPictures are created as
// a consequence of receiving a set of PictureBuffers from clients and released
// at the end of decode (or when a new set of PictureBuffers is required).
//
// TFPPictures are used for output, contents of VASurfaces passed from decoder
// are put into the associated pixmap memory and sent to client.
class VaapiVideoDecodeAccelerator::TFPPicture {
 public:
  ~TFPPicture();

  static linked_ptr<TFPPicture> Create(
      const base::Callback<bool(void)>& make_context_current,
      const GLXFBConfig& fb_config,
      Display* x_display,
      int32 picture_buffer_id,
      uint32 texture_id,
      gfx::Size size);

  int32 picture_buffer_id() {
    return picture_buffer_id_;
  }

  uint32 texture_id() {
    return texture_id_;
  }

  gfx::Size size() {
    return size_;
  }

  int x_pixmap() {
    return x_pixmap_;
  }

  // Bind texture to pixmap. Needs to be called every frame.
  bool Bind();

 private:
  TFPPicture(const base::Callback<bool(void)>& make_context_current,
             Display* x_display,
             int32 picture_buffer_id,
             uint32 texture_id,
             gfx::Size size);

  bool Initialize(const GLXFBConfig& fb_config);

  base::Callback<bool(void)> make_context_current_;

  Display* x_display_;

  // Output id for the client.
  int32 picture_buffer_id_;
  uint32 texture_id_;

  gfx::Size size_;

  // Pixmaps bound to this texture.
  Pixmap x_pixmap_;
  GLXPixmap glx_pixmap_;

  DISALLOW_COPY_AND_ASSIGN(TFPPicture);
};

VaapiVideoDecodeAccelerator::TFPPicture::TFPPicture(
    const base::Callback<bool(void)>& make_context_current,
    Display* x_display,
    int32 picture_buffer_id,
    uint32 texture_id,
    gfx::Size size)
    : make_context_current_(make_context_current),
      x_display_(x_display),
      picture_buffer_id_(picture_buffer_id),
      texture_id_(texture_id),
      size_(size),
      x_pixmap_(0),
      glx_pixmap_(0) {
  DCHECK(!make_context_current_.is_null());
};

linked_ptr<VaapiVideoDecodeAccelerator::TFPPicture>
VaapiVideoDecodeAccelerator::TFPPicture::Create(
    const base::Callback<bool(void)>& make_context_current,
    const GLXFBConfig& fb_config,
    Display* x_display,
    int32 picture_buffer_id,
    uint32 texture_id,
    gfx::Size size) {

  linked_ptr<TFPPicture> tfp_picture(
      new TFPPicture(make_context_current, x_display, picture_buffer_id,
                     texture_id, size));

  if (!tfp_picture->Initialize(fb_config))
    tfp_picture.reset();

  return tfp_picture;
}

bool VaapiVideoDecodeAccelerator::TFPPicture::Initialize(
    const GLXFBConfig& fb_config) {
  // Check for NULL prevents unittests from crashing on nonexistent ChildThread.
  DCHECK(ChildThread::current() == NULL ||
      ChildThread::current()->message_loop() == base::MessageLoop::current());

  if (!make_context_current_.Run())
    return false;

  XWindowAttributes win_attr;
  int screen = DefaultScreen(x_display_);
  XGetWindowAttributes(x_display_, RootWindow(x_display_, screen), &win_attr);
  //TODO(posciak): pass the depth required by libva, not the RootWindow's depth
  x_pixmap_ = XCreatePixmap(x_display_, RootWindow(x_display_, screen),
                            size_.width(), size_.height(), win_attr.depth);
  if (!x_pixmap_) {
    DVLOG(1) << "Failed creating an X Pixmap for TFP";
    return false;
  }

  static const int pixmap_attr[] = {
    GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
    GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGB_EXT,
    GL_NONE,
  };

  glx_pixmap_ = glXCreatePixmap(x_display_, fb_config, x_pixmap_, pixmap_attr);
  if (!glx_pixmap_) {
    // x_pixmap_ will be freed in the destructor.
    DVLOG(1) << "Failed creating a GLX Pixmap for TFP";
    return false;
  }

  return true;
}

VaapiVideoDecodeAccelerator::TFPPicture::~TFPPicture() {
  // Check for NULL prevents unittests from crashing on nonexistent ChildThread.
  DCHECK(ChildThread::current() == NULL ||
      ChildThread::current()->message_loop() == base::MessageLoop::current());

  // Unbind surface from texture and deallocate resources.
  if (glx_pixmap_ && make_context_current_.Run()) {
    glXReleaseTexImageEXT(x_display_, glx_pixmap_, GLX_FRONT_LEFT_EXT);
    glXDestroyPixmap(x_display_, glx_pixmap_);
  }

  if (x_pixmap_)
    XFreePixmap(x_display_, x_pixmap_);
  XSync(x_display_, False);  // Needed to work around buggy vdpau-driver.
}

bool VaapiVideoDecodeAccelerator::TFPPicture::Bind() {
  DCHECK(x_pixmap_);
  DCHECK(glx_pixmap_);
  // Check for NULL prevents unittests from crashing on nonexistent ChildThread.
  DCHECK(ChildThread::current() == NULL ||
      ChildThread::current()->message_loop() == base::MessageLoop::current());

  if (!make_context_current_.Run())
    return false;

  gfx::ScopedTextureBinder texture_binder(GL_TEXTURE_2D, texture_id_);
  glXBindTexImageEXT(x_display_, glx_pixmap_, GLX_FRONT_LEFT_EXT, NULL);

  return true;
}

VaapiVideoDecodeAccelerator::TFPPicture*
    VaapiVideoDecodeAccelerator::TFPPictureById(int32 picture_buffer_id) {
  TFPPictures::iterator it = tfp_pictures_.find(picture_buffer_id);
  if (it == tfp_pictures_.end()) {
    DVLOG(1) << "Picture id " << picture_buffer_id << " does not exist";
    return NULL;
  }

  return it->second.get();
}

VaapiVideoDecodeAccelerator::VaapiVideoDecodeAccelerator(
    Display* x_display, GLXContext glx_context,
    Client* client,
    const base::Callback<bool(void)>& make_context_current)
    : x_display_(x_display),
      glx_context_(glx_context),
      make_context_current_(make_context_current),
      state_(kUninitialized),
      input_ready_(&lock_),
      surfaces_available_(&lock_),
      message_loop_(base::MessageLoop::current()),
      weak_this_(base::AsWeakPtr(this)),
      client_ptr_factory_(client),
      client_(client_ptr_factory_.GetWeakPtr()),
      decoder_thread_("VaapiDecoderThread"),
      num_frames_at_client_(0),
      num_stream_bufs_at_decoder_(0) {
  DCHECK(client);
}

VaapiVideoDecodeAccelerator::~VaapiVideoDecodeAccelerator() {
  DCHECK_EQ(message_loop_, base::MessageLoop::current());
}

class ScopedPtrXFree {
 public:
  void operator()(void* x) const {
    ::XFree(x);
  }
};

bool VaapiVideoDecodeAccelerator::InitializeFBConfig() {
  const int fbconfig_attr[] = {
    GLX_DRAWABLE_TYPE, GLX_PIXMAP_BIT,
    GLX_BIND_TO_TEXTURE_TARGETS_EXT, GLX_TEXTURE_2D_BIT_EXT,
    GLX_BIND_TO_TEXTURE_RGB_EXT, GL_TRUE,
    GLX_Y_INVERTED_EXT, GL_TRUE,
    GL_NONE,
  };

  int num_fbconfigs;
  scoped_ptr_malloc<GLXFBConfig, ScopedPtrXFree> glx_fb_configs(
      glXChooseFBConfig(x_display_, DefaultScreen(x_display_), fbconfig_attr,
                        &num_fbconfigs));
  if (!glx_fb_configs)
    return false;
  if (!num_fbconfigs)
    return false;

  fb_config_ = glx_fb_configs.get()[0];
  return true;
}

bool VaapiVideoDecodeAccelerator::Initialize(
    media::VideoCodecProfile profile) {
  DCHECK_EQ(message_loop_, base::MessageLoop::current());

  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, kUninitialized);
  DVLOG(2) << "Initializing VAVDA, profile: " << profile;

  if (!make_context_current_.Run())
    return false;

  if (!InitializeFBConfig()) {
    DVLOG(1) << "Could not get a usable FBConfig";
    return false;
  }

  vaapi_wrapper_ = VaapiWrapper::Create(
      profile, x_display_,
      base::Bind(&ReportToUMA, content::VaapiH264Decoder::VAAPI_ERROR));

  if (!vaapi_wrapper_.get()) {
    DVLOG(1) << "Failed initializing VAAPI";
    return false;
  }

  decoder_.reset(
      new VaapiH264Decoder(
          vaapi_wrapper_.get(),
          media::BindToLoop(message_loop_->message_loop_proxy(), base::Bind(
              &VaapiVideoDecodeAccelerator::SurfaceReady, weak_this_)),
          base::Bind(&ReportToUMA)));

  CHECK(decoder_thread_.Start());

  state_ = kIdle;

  message_loop_->PostTask(FROM_HERE, base::Bind(
      &Client::NotifyInitializeDone, client_));
  return true;
}

void VaapiVideoDecodeAccelerator::SurfaceReady(
    int32 input_id,
    const scoped_refptr<VASurface>& va_surface) {
  DCHECK_EQ(message_loop_, base::MessageLoop::current());

  // Drop any requests to output if we are resetting or being destroyed.
  if (state_ == kResetting || state_ == kDestroying)
    return;

  pending_output_cbs_.push(
      base::Bind(&VaapiVideoDecodeAccelerator::OutputPicture,
                 weak_this_, va_surface, input_id));

  TryOutputSurface();
}

void VaapiVideoDecodeAccelerator::OutputPicture(
    const scoped_refptr<VASurface>& va_surface,
    int32 input_id,
    TFPPicture* tfp_picture) {
  DCHECK_EQ(message_loop_, base::MessageLoop::current());

  int32 output_id  = tfp_picture->picture_buffer_id();

  TRACE_EVENT2("Video Decoder", "VAVDA::OutputSurface",
               "input_id", input_id,
               "output_id", output_id);

  DVLOG(3) << "Outputting VASurface " << va_surface->id()
           << " into pixmap bound to picture buffer id " << output_id;

  RETURN_AND_NOTIFY_ON_FAILURE(tfp_picture->Bind(),
                               "Failed binding texture to pixmap",
                               PLATFORM_FAILURE, );

  RETURN_AND_NOTIFY_ON_FAILURE(
      vaapi_wrapper_->PutSurfaceIntoPixmap(va_surface->id(),
                                           tfp_picture->x_pixmap(),
                                           tfp_picture->size()),
      "Failed putting surface into pixmap", PLATFORM_FAILURE, );

  // Notify the client a picture is ready to be displayed.
  ++num_frames_at_client_;
  TRACE_COUNTER1("Video Decoder", "Textures at client", num_frames_at_client_);
  DVLOG(4) << "Notifying output picture id " << output_id
           << " for input "<< input_id << " is ready";
  client_->PictureReady(media::Picture(output_id, input_id));
}

void VaapiVideoDecodeAccelerator::TryOutputSurface() {
  DCHECK_EQ(message_loop_, base::MessageLoop::current());

  // Handle Destroy() arriving while pictures are queued for output.
  if (!client_)
    return;

  if (pending_output_cbs_.empty() || output_buffers_.empty())
    return;

  OutputCB output_cb = pending_output_cbs_.front();
  pending_output_cbs_.pop();

  TFPPicture* tfp_picture = TFPPictureById(output_buffers_.front());
  DCHECK(tfp_picture);
  output_buffers_.pop();

  output_cb.Run(tfp_picture);
}

void VaapiVideoDecodeAccelerator::MapAndQueueNewInputBuffer(
    const media::BitstreamBuffer& bitstream_buffer) {
  DCHECK_EQ(message_loop_, base::MessageLoop::current());
  TRACE_EVENT1("Video Decoder", "MapAndQueueNewInputBuffer", "input_id",
      bitstream_buffer.id());

  DVLOG(4) << "Mapping new input buffer id: " << bitstream_buffer.id()
           << " size: " << (int)bitstream_buffer.size();

  scoped_ptr<base::SharedMemory> shm(
      new base::SharedMemory(bitstream_buffer.handle(), true));
  RETURN_AND_NOTIFY_ON_FAILURE(shm->Map(bitstream_buffer.size()),
                              "Failed to map input buffer", UNREADABLE_INPUT,);

  base::AutoLock auto_lock(lock_);

  // Set up a new input buffer and queue it for later.
  linked_ptr<InputBuffer> input_buffer(new InputBuffer());
  input_buffer->shm.reset(shm.release());
  input_buffer->id = bitstream_buffer.id();
  input_buffer->size = bitstream_buffer.size();

  ++num_stream_bufs_at_decoder_;
  TRACE_COUNTER1("Video Decoder", "Stream buffers at decoder",
                 num_stream_bufs_at_decoder_);

  input_buffers_.push(input_buffer);
  input_ready_.Signal();
}

bool VaapiVideoDecodeAccelerator::GetInputBuffer_Locked() {
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  lock_.AssertAcquired();

  if (curr_input_buffer_.get())
    return true;

  // Will only wait if it is expected that in current state new buffers will
  // be queued from the client via Decode(). The state can change during wait.
  while (input_buffers_.empty() && (state_ == kDecoding || state_ == kIdle)) {
    input_ready_.Wait();
  }

  // We could have got woken up in a different state or never got to sleep
  // due to current state; check for that.
  switch (state_) {
    case kFlushing:
      // Here we are only interested in finishing up decoding buffers that are
      // already queued up. Otherwise will stop decoding.
      if (input_buffers_.empty())
        return false;
      // else fallthrough
    case kDecoding:
    case kIdle:
      DCHECK(!input_buffers_.empty());

      curr_input_buffer_ = input_buffers_.front();
      input_buffers_.pop();

      DVLOG(4) << "New current bitstream buffer, id: "
               << curr_input_buffer_->id
               << " size: " << curr_input_buffer_->size;

      decoder_->SetStream(
          static_cast<uint8*>(curr_input_buffer_->shm->memory()),
          curr_input_buffer_->size, curr_input_buffer_->id);
      return true;

    default:
      // We got woken up due to being destroyed/reset, ignore any already
      // queued inputs.
      return false;
  }
}

void VaapiVideoDecodeAccelerator::ReturnCurrInputBuffer_Locked() {
  lock_.AssertAcquired();
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  DCHECK(curr_input_buffer_.get());

  int32 id = curr_input_buffer_->id;
  curr_input_buffer_.reset();
  DVLOG(4) << "End of input buffer " << id;
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &Client::NotifyEndOfBitstreamBuffer, client_, id));

  --num_stream_bufs_at_decoder_;
  TRACE_COUNTER1("Video Decoder", "Stream buffers at decoder",
                 num_stream_bufs_at_decoder_);
}

bool VaapiVideoDecodeAccelerator::FeedDecoderWithOutputSurfaces_Locked() {
  lock_.AssertAcquired();
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());

  while (available_va_surfaces_.empty() &&
         (state_ == kDecoding || state_ == kFlushing || state_ == kIdle)) {
    surfaces_available_.Wait();
  }

  if (state_ != kDecoding && state_ != kFlushing && state_ != kIdle)
    return false;

  VASurface::ReleaseCB va_surface_release_cb =
      media::BindToLoop(message_loop_->message_loop_proxy(), base::Bind(
          &VaapiVideoDecodeAccelerator::RecycleVASurfaceID, weak_this_));

  while (!available_va_surfaces_.empty()) {
    scoped_refptr<VASurface> va_surface(
        new VASurface(available_va_surfaces_.front(), va_surface_release_cb));
    available_va_surfaces_.pop_front();
    decoder_->ReuseSurface(va_surface);
  }

  return true;
}

void VaapiVideoDecodeAccelerator::DecodeTask() {
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  TRACE_EVENT0("Video Decoder", "VAVDA::DecodeTask");
  base::AutoLock auto_lock(lock_);

  // Main decode task.
  DVLOG(4) << "Decode task";

  // Try to decode what stream data is (still) in the decoder until we run out
  // of it.
  while (GetInputBuffer_Locked()) {
    DCHECK(curr_input_buffer_.get());

    VaapiH264Decoder::DecResult res;
    {
      // We are OK releasing the lock here, as decoder never calls our methods
      // directly and we will reacquire the lock before looking at state again.
      // This is the main decode function of the decoder and while keeping
      // the lock for its duration would be fine, it would defeat the purpose
      // of having a separate decoder thread.
      base::AutoUnlock auto_unlock(lock_);
      res = decoder_->Decode();
    }

    switch (res) {
      case VaapiH264Decoder::kAllocateNewSurfaces:
        state_ = kPicturesRequested;
        num_pics_ = decoder_->GetRequiredNumOfPictures();
        pic_size_ = decoder_->GetPicSize();
        DVLOG(1) << "Requesting " << num_pics_ << " pictures of size: "
                 << pic_size_.width() << "x" << pic_size_.height();
        message_loop_->PostTask(FROM_HERE, base::Bind(
            &Client::ProvidePictureBuffers, client_,
            num_pics_, pic_size_, GL_TEXTURE_2D));
        // We'll get rescheduled once ProvidePictureBuffers() finishes.
        return;

      case VaapiH264Decoder::kRanOutOfStreamData:
        ReturnCurrInputBuffer_Locked();
        break;

      case VaapiH264Decoder::kRanOutOfSurfaces:
        // No more output buffers in the decoder, try getting more or go to
        // sleep waiting for them.
        if (!FeedDecoderWithOutputSurfaces_Locked())
          return;

        break;

      case VaapiH264Decoder::kDecodeError:
        RETURN_AND_NOTIFY_ON_FAILURE(false, "Error decoding stream",
                                     PLATFORM_FAILURE, );
        return;
    }
  }
}

void VaapiVideoDecodeAccelerator::Decode(
    const media::BitstreamBuffer& bitstream_buffer) {
  DCHECK_EQ(message_loop_, base::MessageLoop::current());

  TRACE_EVENT1("Video Decoder", "VAVDA::Decode", "Buffer id",
               bitstream_buffer.id());

  // We got a new input buffer from the client, map it and queue for later use.
  MapAndQueueNewInputBuffer(bitstream_buffer);

  base::AutoLock auto_lock(lock_);
  switch (state_) {
    case kIdle:
      state_ = kDecoding;
      decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
          &VaapiVideoDecodeAccelerator::DecodeTask,
          base::Unretained(this)));
      break;

    case kPicturesRequested:
      // Waiting for pictures, fallthrough.
    case kDecoding:
      // Decoder already running, fallthrough.
    case kResetting:
      // When resetting, allow accumulating bitstream buffers, so that
      // the client can queue after-seek-buffers while we are finishing with
      // the before-seek one.
      break;

    default:
      RETURN_AND_NOTIFY_ON_FAILURE(false,
          "Decode request from client in invalid state: " << state_,
          PLATFORM_FAILURE, );
      return;
  }
}

void VaapiVideoDecodeAccelerator::RecycleVASurfaceID(
    VASurfaceID va_surface_id) {
  DCHECK_EQ(message_loop_, base::MessageLoop::current());

  base::AutoLock auto_lock(lock_);

  available_va_surfaces_.push_back(va_surface_id);
  surfaces_available_.Signal();
}

void VaapiVideoDecodeAccelerator::AssignPictureBuffers(
    const std::vector<media::PictureBuffer>& buffers) {
  DCHECK_EQ(message_loop_, base::MessageLoop::current());

  base::AutoLock auto_lock(lock_);
  DCHECK_EQ(state_, kPicturesRequested);
  DCHECK(tfp_pictures_.empty());

  RETURN_AND_NOTIFY_ON_FAILURE(
      buffers.size() == num_pics_,
      "Got an invalid number of picture buffers. (Got " << buffers.size()
      << ", requested " << num_pics_ << ")", INVALID_ARGUMENT, );

  std::vector<VASurfaceID> va_surface_ids;
  RETURN_AND_NOTIFY_ON_FAILURE(
      vaapi_wrapper_->CreateSurfaces(pic_size_,
                                     buffers.size(),
                                     &va_surface_ids),
      "Failed creating VA Surfaces", PLATFORM_FAILURE, );
  DCHECK_EQ(va_surface_ids.size(), buffers.size());

  for (size_t i = 0; i < buffers.size(); ++i) {
    DVLOG(2) << "Assigning picture id: " << buffers[i].id()
             << " to texture id: " << buffers[i].texture_id()
             << " VASurfaceID: " << va_surface_ids[i];

    linked_ptr<TFPPicture> tfp_picture(
        TFPPicture::Create(make_context_current_, fb_config_, x_display_,
                           buffers[i].id(), buffers[i].texture_id(),
                           pic_size_));

    RETURN_AND_NOTIFY_ON_FAILURE(
        tfp_picture.get(), "Failed assigning picture buffer to a texture.",
        PLATFORM_FAILURE, );

    bool inserted = tfp_pictures_.insert(std::make_pair(
        buffers[i].id(), tfp_picture)).second;
    DCHECK(inserted);

    output_buffers_.push(buffers[i].id());
    available_va_surfaces_.push_back(va_surface_ids[i]);
    surfaces_available_.Signal();
  }

  state_ = kDecoding;
  decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &VaapiVideoDecodeAccelerator::DecodeTask, base::Unretained(this)));
}

void VaapiVideoDecodeAccelerator::ReusePictureBuffer(int32 picture_buffer_id) {
  DCHECK_EQ(message_loop_, base::MessageLoop::current());
  TRACE_EVENT1("Video Decoder", "VAVDA::ReusePictureBuffer", "Picture id",
               picture_buffer_id);

  --num_frames_at_client_;
  TRACE_COUNTER1("Video Decoder", "Textures at client", num_frames_at_client_);

  output_buffers_.push(picture_buffer_id);
  TryOutputSurface();
}

void VaapiVideoDecodeAccelerator::FlushTask() {
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());
  DVLOG(1) << "Flush task";

  // First flush all the pictures that haven't been outputted, notifying the
  // client to output them.
  bool res = decoder_->Flush();
  RETURN_AND_NOTIFY_ON_FAILURE(res, "Failed flushing the decoder.",
                               PLATFORM_FAILURE, );

  // Put the decoder in idle state, ready to resume.
  decoder_->Reset();

  message_loop_->PostTask(FROM_HERE, base::Bind(
      &VaapiVideoDecodeAccelerator::FinishFlush, weak_this_));
}

void VaapiVideoDecodeAccelerator::Flush() {
  DCHECK_EQ(message_loop_, base::MessageLoop::current());
  DVLOG(1) << "Got flush request";

  base::AutoLock auto_lock(lock_);
  state_ = kFlushing;
  // Queue a flush task after all existing decoding tasks to clean up.
  decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &VaapiVideoDecodeAccelerator::FlushTask, base::Unretained(this)));

  input_ready_.Signal();
  surfaces_available_.Signal();
}

void VaapiVideoDecodeAccelerator::FinishFlush() {
  DCHECK_EQ(message_loop_, base::MessageLoop::current());

  base::AutoLock auto_lock(lock_);
  if (state_ != kFlushing) {
    DCHECK_EQ(state_, kDestroying);
    return;  // We could've gotten destroyed already.
  }

  state_ = kIdle;

  message_loop_->PostTask(FROM_HERE, base::Bind(
      &Client::NotifyFlushDone, client_));

  DVLOG(1) << "Flush finished";
}

void VaapiVideoDecodeAccelerator::ResetTask() {
  DCHECK_EQ(decoder_thread_.message_loop(), base::MessageLoop::current());

  // All the decoding tasks from before the reset request from client are done
  // by now, as this task was scheduled after them and client is expected not
  // to call Decode() after Reset() and before NotifyResetDone.
  decoder_->Reset();

  base::AutoLock auto_lock(lock_);

  // Return current input buffer, if present.
  if (curr_input_buffer_.get())
    ReturnCurrInputBuffer_Locked();

  // And let client know that we are done with reset.
  message_loop_->PostTask(FROM_HERE, base::Bind(
      &VaapiVideoDecodeAccelerator::FinishReset, weak_this_));
}

void VaapiVideoDecodeAccelerator::Reset() {
  DCHECK_EQ(message_loop_, base::MessageLoop::current());
  DVLOG(1) << "Got reset request";

  // This will make any new decode tasks exit early.
  base::AutoLock auto_lock(lock_);
  state_ = kResetting;

  // Drop all remaining input buffers, if present.
  while (!input_buffers_.empty()) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &Client::NotifyEndOfBitstreamBuffer, client_,
        input_buffers_.front()->id));
    input_buffers_.pop();
  }

  decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &VaapiVideoDecodeAccelerator::ResetTask, base::Unretained(this)));

  input_ready_.Signal();
  surfaces_available_.Signal();
}

void VaapiVideoDecodeAccelerator::FinishReset() {
  DCHECK_EQ(message_loop_, base::MessageLoop::current());

  base::AutoLock auto_lock(lock_);
  if (state_ != kResetting) {
    DCHECK(state_ == kDestroying || state_ == kUninitialized) << state_;
    return;  // We could've gotten destroyed already.
  }

  state_ = kIdle;
  num_stream_bufs_at_decoder_ = 0;

  while(!pending_output_cbs_.empty())
    pending_output_cbs_.pop();

  message_loop_->PostTask(FROM_HERE, base::Bind(
      &Client::NotifyResetDone, client_));

  // The client might have given us new buffers via Decode() while we were
  // resetting and might be waiting for our move, and not call Decode() anymore
  // until we return something. Post a DecodeTask() so that we won't
  // sleep forever waiting for Decode() in that case. Having two of them
  // in the pipe is harmless, the additional one will return as soon as it sees
  // that we are back in kDecoding state.
  if (!input_buffers_.empty()) {
    state_ = kDecoding;
    decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
      &VaapiVideoDecodeAccelerator::DecodeTask,
      base::Unretained(this)));
  }

  DVLOG(1) << "Reset finished";
}

void VaapiVideoDecodeAccelerator::Cleanup() {
  DCHECK_EQ(message_loop_, base::MessageLoop::current());

  if (state_ == kUninitialized || state_ == kDestroying)
    return;

  DVLOG(1) << "Destroying VAVDA";
  base::AutoLock auto_lock(lock_);
  state_ = kDestroying;

  client_ptr_factory_.InvalidateWeakPtrs();

  {
    base::AutoUnlock auto_unlock(lock_);
    // Post a dummy task to the decoder_thread_ to ensure it is drained.
    base::WaitableEvent waiter(false, false);
    decoder_thread_.message_loop()->PostTask(FROM_HERE, base::Bind(
        &base::WaitableEvent::Signal, base::Unretained(&waiter)));
    input_ready_.Signal();
    surfaces_available_.Signal();
    waiter.Wait();
    decoder_thread_.Stop();
  }

  state_ = kUninitialized;
}

void VaapiVideoDecodeAccelerator::Destroy() {
  DCHECK_EQ(message_loop_, base::MessageLoop::current());
  Cleanup();
  delete this;
}

}  // namespace content
