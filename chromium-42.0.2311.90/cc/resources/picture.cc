// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/picture.h"

#include <algorithm>
#include <limits>
#include <set>

#include "base/base64.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "cc/base/util.h"
#include "cc/debug/picture_debug_util.h"
#include "cc/debug/traced_picture.h"
#include "cc/debug/traced_value.h"
#include "cc/layers/content_layer_client.h"
#include "skia/ext/pixel_ref_utils.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDrawPictureCallback.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/utils/SkNullCanvas.h"
#include "third_party/skia/include/utils/SkPictureUtils.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"

namespace cc {

namespace {

bool DecodeBitmap(const void* buffer, size_t size, SkBitmap* bm) {
  const unsigned char* data = static_cast<const unsigned char *>(buffer);

  // Try PNG first.
  if (gfx::PNGCodec::Decode(data, size, bm))
    return true;

  // Try JPEG.
  scoped_ptr<SkBitmap> decoded_jpeg(gfx::JPEGCodec::Decode(data, size));
  if (decoded_jpeg) {
    *bm = *decoded_jpeg;
    return true;
  }
  return false;
}

}  // namespace

scoped_refptr<Picture> Picture::Create(
    const gfx::Rect& layer_rect,
    ContentLayerClient* client,
    const gfx::Size& tile_grid_size,
    bool gather_pixel_refs,
    RecordingSource::RecordingMode recording_mode) {
  scoped_refptr<Picture> picture = make_scoped_refptr(new Picture(layer_rect));

  picture->Record(client, tile_grid_size, recording_mode);
  if (gather_pixel_refs)
    picture->GatherPixelRefs(tile_grid_size);

  return picture;
}

Picture::Picture(const gfx::Rect& layer_rect)
  : layer_rect_(layer_rect),
    cell_size_(layer_rect.size()) {
  // Instead of recording a trace event for object creation here, we wait for
  // the picture to be recorded in Picture::Record.
}

scoped_refptr<Picture> Picture::CreateFromSkpValue(const base::Value* value) {
  // Decode the picture from base64.
  std::string encoded;
  if (!value->GetAsString(&encoded))
    return NULL;

  std::string decoded;
  base::Base64Decode(encoded, &decoded);
  SkMemoryStream stream(decoded.data(), decoded.size());

  // Read the picture. This creates an empty picture on failure.
  SkPicture* skpicture = SkPicture::CreateFromStream(&stream, &DecodeBitmap);
  if (skpicture == NULL)
    return NULL;

  gfx::Rect layer_rect(gfx::SkIRectToRect(skpicture->cullRect().roundOut()));
  return make_scoped_refptr(new Picture(skpicture, layer_rect));
}

scoped_refptr<Picture> Picture::CreateFromValue(const base::Value* raw_value) {
  const base::DictionaryValue* value = NULL;
  if (!raw_value->GetAsDictionary(&value))
    return NULL;

  // Decode the picture from base64.
  std::string encoded;
  if (!value->GetString("skp64", &encoded))
    return NULL;

  std::string decoded;
  base::Base64Decode(encoded, &decoded);
  SkMemoryStream stream(decoded.data(), decoded.size());

  const base::Value* layer_rect_value = NULL;
  if (!value->Get("params.layer_rect", &layer_rect_value))
    return NULL;

  gfx::Rect layer_rect;
  if (!MathUtil::FromValue(layer_rect_value, &layer_rect))
    return NULL;

  // Read the picture. This creates an empty picture on failure.
  SkPicture* skpicture = SkPicture::CreateFromStream(&stream, &DecodeBitmap);
  if (skpicture == NULL)
    return NULL;

  return make_scoped_refptr(new Picture(skpicture, layer_rect));
}

Picture::Picture(SkPicture* picture, const gfx::Rect& layer_rect)
    : layer_rect_(layer_rect),
      picture_(skia::AdoptRef(picture)),
      cell_size_(layer_rect.size()) {
}

Picture::Picture(const skia::RefPtr<SkPicture>& picture,
                 const gfx::Rect& layer_rect,
                 const PixelRefMap& pixel_refs) :
    layer_rect_(layer_rect),
    picture_(picture),
    pixel_refs_(pixel_refs),
    cell_size_(layer_rect.size()) {
}

Picture::~Picture() {
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.picture"), "cc::Picture", this);
}

bool Picture::IsSuitableForGpuRasterization(const char** reason) const {
  DCHECK(picture_);

  // TODO(hendrikw): SkPicture::suitableForGpuRasterization takes a GrContext.
  // Currently the GrContext isn't used, and should probably be removed from
  // skia.
  return picture_->suitableForGpuRasterization(nullptr, reason);
}

int Picture::ApproximateOpCount() const {
  DCHECK(picture_);
  return picture_->approximateOpCount();
}

size_t Picture::ApproximateMemoryUsage() const {
  DCHECK(picture_);
  return SkPictureUtils::ApproximateBytesUsed(picture_.get());
}

bool Picture::HasText() const {
  DCHECK(picture_);
  return picture_->hasText();
}

void Picture::Record(ContentLayerClient* painter,
                     const gfx::Size& tile_grid_size,
                     RecordingSource::RecordingMode recording_mode) {
  TRACE_EVENT2("cc",
               "Picture::Record",
               "data",
               AsTraceableRecordData(),
               "recording_mode",
               recording_mode);

  DCHECK(!picture_);
  DCHECK(!tile_grid_size.IsEmpty());

  // TODO(mtklein): If SkRTree sticks, clean up tile_grid_info.  skbug.com/3085
  SkRTreeFactory factory;
  SkPictureRecorder recorder;

  skia::RefPtr<SkCanvas> canvas;
  canvas = skia::SharePtr(recorder.beginRecording(
      layer_rect_.width(), layer_rect_.height(), &factory,
      SkPictureRecorder::kComputeSaveLayerInfo_RecordFlag));

  ContentLayerClient::PaintingControlSetting painting_control =
      ContentLayerClient::PAINTING_BEHAVIOR_NORMAL;

  switch (recording_mode) {
    case RecordingSource::RECORD_NORMALLY:
      // Already setup for normal recording.
      break;
    case RecordingSource::RECORD_WITH_SK_NULL_CANVAS:
      canvas = skia::AdoptRef(SkCreateNullCanvas());
      break;
    case RecordingSource::RECORD_WITH_PAINTING_DISABLED:
      // We pass a disable flag through the paint calls when perfromance
      // testing (the only time this case should ever arise) when we want to
      // prevent the Blink GraphicsContext object from consuming any compute
      // time.
      canvas = skia::AdoptRef(SkCreateNullCanvas());
      painting_control = ContentLayerClient::DISPLAY_LIST_CONSTRUCTION_DISABLED;
      break;
    case RecordingSource::RECORD_WITH_CACHING_DISABLED:
      // This mode should give the same results as RECORD_NORMALLY.
      painting_control = ContentLayerClient::DISPLAY_LIST_CACHING_DISABLED;
      break;
    default:
      NOTREACHED();
  }

  canvas->save();
  canvas->translate(SkFloatToScalar(-layer_rect_.x()),
                    SkFloatToScalar(-layer_rect_.y()));

  SkRect layer_skrect = SkRect::MakeXYWH(layer_rect_.x(),
                                         layer_rect_.y(),
                                         layer_rect_.width(),
                                         layer_rect_.height());
  canvas->clipRect(layer_skrect);

  painter->PaintContents(canvas.get(), layer_rect_, painting_control);

  canvas->restore();
  picture_ = skia::AdoptRef(recorder.endRecording());
  DCHECK(picture_);

  EmitTraceSnapshot();
}

void Picture::GatherPixelRefs(const gfx::Size& tile_grid_size) {
  TRACE_EVENT2("cc", "Picture::GatherPixelRefs",
               "width", layer_rect_.width(),
               "height", layer_rect_.height());

  DCHECK(picture_);
  DCHECK(pixel_refs_.empty());
  if (!WillPlayBackBitmaps())
    return;
  cell_size_ = tile_grid_size;
  DCHECK_GT(cell_size_.width(), 0);
  DCHECK_GT(cell_size_.height(), 0);

  int min_x = std::numeric_limits<int>::max();
  int min_y = std::numeric_limits<int>::max();
  int max_x = 0;
  int max_y = 0;

  skia::DiscardablePixelRefList pixel_refs;
  skia::PixelRefUtils::GatherDiscardablePixelRefs(picture_.get(), &pixel_refs);
  for (skia::DiscardablePixelRefList::const_iterator it = pixel_refs.begin();
       it != pixel_refs.end();
       ++it) {
    gfx::Point min(
        RoundDown(static_cast<int>(it->pixel_ref_rect.x()),
                  cell_size_.width()),
        RoundDown(static_cast<int>(it->pixel_ref_rect.y()),
                  cell_size_.height()));
    gfx::Point max(
        RoundDown(static_cast<int>(std::ceil(it->pixel_ref_rect.right())),
                  cell_size_.width()),
        RoundDown(static_cast<int>(std::ceil(it->pixel_ref_rect.bottom())),
                  cell_size_.height()));

    for (int y = min.y(); y <= max.y(); y += cell_size_.height()) {
      for (int x = min.x(); x <= max.x(); x += cell_size_.width()) {
        PixelRefMapKey key(x, y);
        pixel_refs_[key].push_back(it->pixel_ref);
      }
    }

    min_x = std::min(min_x, min.x());
    min_y = std::min(min_y, min.y());
    max_x = std::max(max_x, max.x());
    max_y = std::max(max_y, max.y());
  }

  min_pixel_cell_ = gfx::Point(min_x, min_y);
  max_pixel_cell_ = gfx::Point(max_x, max_y);
}

int Picture::Raster(SkCanvas* canvas,
                    SkDrawPictureCallback* callback,
                    const Region& negated_content_region,
                    float contents_scale) const {
  TRACE_EVENT_BEGIN1(
      "cc",
      "Picture::Raster",
      "data",
      AsTraceableRasterData(contents_scale));

  DCHECK(picture_);

  canvas->save();

  for (Region::Iterator it(negated_content_region); it.has_rect(); it.next())
    canvas->clipRect(gfx::RectToSkRect(it.rect()), SkRegion::kDifference_Op);

  canvas->scale(contents_scale, contents_scale);
  canvas->translate(layer_rect_.x(), layer_rect_.y());
  if (callback) {
    // If we have a callback, we need to call |draw()|, |drawPicture()| doesn't
    // take a callback.  This is used by |AnalysisCanvas| to early out.
    picture_->playback(canvas, callback);
  } else {
    // Prefer to call |drawPicture()| on the canvas since it could place the
    // entire picture on the canvas instead of parsing the skia operations.
    canvas->drawPicture(picture_.get());
  }
  SkIRect bounds;
  canvas->getClipDeviceBounds(&bounds);
  canvas->restore();
  TRACE_EVENT_END1(
      "cc", "Picture::Raster",
      "num_pixels_rasterized", bounds.width() * bounds.height());
  return bounds.width() * bounds.height();
}

void Picture::Replay(SkCanvas* canvas) {
  TRACE_EVENT_BEGIN0("cc", "Picture::Replay");
  DCHECK(picture_);
  picture_->playback(canvas);
  SkIRect bounds;
  canvas->getClipDeviceBounds(&bounds);
  TRACE_EVENT_END1("cc", "Picture::Replay",
                   "num_pixels_replayed", bounds.width() * bounds.height());
}

scoped_ptr<base::Value> Picture::AsValue() const {
  // Encode the picture as base64.
  scoped_ptr<base::DictionaryValue> res(new base::DictionaryValue());
  res->Set("params.layer_rect", MathUtil::AsValue(layer_rect_).release());
  std::string b64_picture;
  PictureDebugUtil::SerializeAsBase64(picture_.get(), &b64_picture);
  res->SetString("skp64", b64_picture);
  return res.Pass();
}

void Picture::EmitTraceSnapshot() const {
  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.picture") ","
          TRACE_DISABLED_BY_DEFAULT("devtools.timeline.picture"),
      "cc::Picture",
      this,
      TracedPicture::AsTraceablePicture(this));
}

void Picture::EmitTraceSnapshotAlias(Picture* original) const {
  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(
      TRACE_DISABLED_BY_DEFAULT("cc.debug.picture") ","
          TRACE_DISABLED_BY_DEFAULT("devtools.timeline.picture"),
      "cc::Picture",
      this,
      TracedPicture::AsTraceablePictureAlias(original));
}

base::LazyInstance<Picture::PixelRefs>
    Picture::PixelRefIterator::empty_pixel_refs_;

Picture::PixelRefIterator::PixelRefIterator()
    : picture_(NULL),
      current_pixel_refs_(empty_pixel_refs_.Pointer()),
      current_index_(0),
      min_point_(-1, -1),
      max_point_(-1, -1),
      current_x_(0),
      current_y_(0) {
}

Picture::PixelRefIterator::PixelRefIterator(
    const gfx::Rect& rect,
    const Picture* picture)
    : picture_(picture),
      current_pixel_refs_(empty_pixel_refs_.Pointer()),
      current_index_(0) {
  gfx::Rect layer_rect = picture->layer_rect_;
  gfx::Size cell_size = picture->cell_size_;
  DCHECK(!cell_size.IsEmpty());

  gfx::Rect query_rect(rect);
  // Early out if the query rect doesn't intersect this picture.
  if (!query_rect.Intersects(layer_rect)) {
    min_point_ = gfx::Point(0, 0);
    max_point_ = gfx::Point(0, 0);
    current_x_ = 1;
    current_y_ = 1;
    return;
  }

  // First, subtract the layer origin as cells are stored in layer space.
  query_rect.Offset(-layer_rect.OffsetFromOrigin());

  // We have to find a cell_size aligned point that corresponds to
  // query_rect. Point is a multiple of cell_size.
  min_point_ = gfx::Point(
      RoundDown(query_rect.x(), cell_size.width()),
      RoundDown(query_rect.y(), cell_size.height()));
  max_point_ = gfx::Point(
      RoundDown(query_rect.right() - 1, cell_size.width()),
      RoundDown(query_rect.bottom() - 1, cell_size.height()));

  // Limit the points to known pixel ref boundaries.
  min_point_ = gfx::Point(
      std::max(min_point_.x(), picture->min_pixel_cell_.x()),
      std::max(min_point_.y(), picture->min_pixel_cell_.y()));
  max_point_ = gfx::Point(
      std::min(max_point_.x(), picture->max_pixel_cell_.x()),
      std::min(max_point_.y(), picture->max_pixel_cell_.y()));

  // Make the current x be cell_size.width() less than min point, so that
  // the first increment will point at min_point_.
  current_x_ = min_point_.x() - cell_size.width();
  current_y_ = min_point_.y();
  if (current_y_ <= max_point_.y())
    ++(*this);
}

Picture::PixelRefIterator::~PixelRefIterator() {
}

Picture::PixelRefIterator& Picture::PixelRefIterator::operator++() {
  ++current_index_;
  // If we're not at the end of the list, then we have the next item.
  if (current_index_ < current_pixel_refs_->size())
    return *this;

  DCHECK(current_y_ <= max_point_.y());
  while (true) {
    gfx::Size cell_size = picture_->cell_size_;

    // Advance the current grid cell.
    current_x_ += cell_size.width();
    if (current_x_ > max_point_.x()) {
      current_y_ += cell_size.height();
      current_x_ = min_point_.x();
      if (current_y_ > max_point_.y()) {
        current_pixel_refs_ = empty_pixel_refs_.Pointer();
        current_index_ = 0;
        break;
      }
    }

    // If there are no pixel refs at this grid cell, keep incrementing.
    PixelRefMapKey key(current_x_, current_y_);
    PixelRefMap::const_iterator iter = picture_->pixel_refs_.find(key);
    if (iter == picture_->pixel_refs_.end())
      continue;

    // We found a non-empty list: store it and get the first pixel ref.
    current_pixel_refs_ = &iter->second;
    current_index_ = 0;
    break;
  }
  return *this;
}

scoped_refptr<base::trace_event::ConvertableToTraceFormat>
    Picture::AsTraceableRasterData(float scale) const {
  scoped_refptr<base::trace_event::TracedValue> raster_data =
      new base::trace_event::TracedValue();
  TracedValue::SetIDRef(this, raster_data.get(), "picture_id");
  raster_data->SetDouble("scale", scale);
  return raster_data;
}

scoped_refptr<base::trace_event::ConvertableToTraceFormat>
    Picture::AsTraceableRecordData() const {
  scoped_refptr<base::trace_event::TracedValue> record_data =
      new base::trace_event::TracedValue();
  TracedValue::SetIDRef(this, record_data.get(), "picture_id");
  MathUtil::AddToTracedValue("layer_rect", layer_rect_, record_data.get());
  return record_data;
}

}  // namespace cc
