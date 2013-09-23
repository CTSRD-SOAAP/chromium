// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_MANAGED_TILE_STATE_H_
#define CC_RESOURCES_MANAGED_TILE_STATE_H_

#include "base/memory/scoped_ptr.h"
#include "cc/resources/platform_color.h"
#include "cc/resources/raster_worker_pool.h"
#include "cc/resources/resource_pool.h"
#include "cc/resources/resource_provider.h"

namespace cc {

class TileManager;

// Tile manager classifying tiles into a few basic bins:
enum ManagedTileBin {
  NOW_BIN = 0,         // Needed ASAP.
  SOON_BIN = 1,        // Impl-side version of prepainting.
  EVENTUALLY_BIN = 2,  // Nice to have, if we've got memory and time.
  NEVER_BIN = 3,       // Dont bother.
  NUM_BINS = 4
  // Be sure to update ManagedTileBinAsValue when adding new fields.
};
scoped_ptr<base::Value> ManagedTileBinAsValue(
    ManagedTileBin bin);

enum ManagedTileBinPriority {
  HIGH_PRIORITY_BIN = 0,
  LOW_PRIORITY_BIN = 1,
  NUM_BIN_PRIORITIES = 2
};
scoped_ptr<base::Value> ManagedTileBinPriorityAsValue(
    ManagedTileBinPriority bin);

// This is state that is specific to a tile that is
// managed by the TileManager.
class CC_EXPORT ManagedTileState {
 public:
  class CC_EXPORT TileVersion {
    public:
      enum Mode {
        RESOURCE_MODE,
        SOLID_COLOR_MODE,
        PICTURE_PILE_MODE,
        NUM_MODES
      };

      TileVersion();
      ~TileVersion();

      Mode mode() const {
        return mode_;
      }

      bool IsReadyToDraw() const;

      ResourceProvider::ResourceId get_resource_id() const {
        DCHECK(mode_ == RESOURCE_MODE);
        DCHECK(resource_);

        return resource_->id();
      }

      SkColor get_solid_color() const {
        DCHECK(mode_ == SOLID_COLOR_MODE);

        return solid_color_;
      }

      bool contents_swizzled() const {
        DCHECK(resource_);
        return !PlatformColor::SameComponentOrder(resource_->format());
      }

      bool requires_resource() const {
        return mode_ == RESOURCE_MODE ||
               mode_ == PICTURE_PILE_MODE;
      }

      size_t GPUMemoryUsageInBytes() const;

      void SetResourceForTesting(scoped_ptr<ResourcePool::Resource> resource) {
        resource_ = resource.Pass();
      }

      const ResourcePool::Resource* GetResourceForTesting() const {
        return resource_.get();
      }

    private:
      friend class TileManager;
      friend class Tile;
      friend class ManagedTileState;

      void set_use_resource() {
        mode_ = RESOURCE_MODE;
      }

      void set_solid_color(const SkColor& color) {
        mode_ = SOLID_COLOR_MODE;
        solid_color_ = color;
      }

      void set_has_text(bool has_text) {
        has_text_ = has_text;
      }

      void set_rasterize_on_demand() {
        mode_ = PICTURE_PILE_MODE;
      }

      Mode mode_;
      SkColor solid_color_;
      bool has_text_;
      scoped_ptr<ResourcePool::Resource> resource_;
      RasterWorkerPool::RasterTask raster_task_;
  };

  ManagedTileState();
  ~ManagedTileState();

  scoped_ptr<base::Value> AsValue() const;

  // Persisted state: valid all the time.
  TileVersion tile_versions[NUM_RASTER_MODES];
  RasterMode raster_mode;

  // Ephemeral state, valid only during TileManager::ManageTiles.
  bool is_in_never_bin_on_both_trees() const {
    return bin[HIGH_PRIORITY_BIN] == NEVER_BIN &&
           bin[LOW_PRIORITY_BIN] == NEVER_BIN;
  }
  bool is_in_now_bin_on_either_tree() const {
    return bin[HIGH_PRIORITY_BIN] == NOW_BIN ||
           bin[LOW_PRIORITY_BIN] == NOW_BIN;
  }

  ManagedTileBin bin[NUM_BIN_PRIORITIES];
  ManagedTileBin tree_bin[NUM_TREES];

  // The bin that the tile would have if the GPU memory manager had
  // a maximally permissive policy, send to the GPU memory manager
  // to determine policy.
  ManagedTileBin gpu_memmgr_stats_bin;
  TileResolution resolution;
  bool required_for_activation;
  float time_to_needed_in_seconds;
  float distance_to_visible_in_pixels;
  bool visible_and_ready_to_draw;
};

}  // namespace cc

#endif  // CC_RESOURCES_MANAGED_TILE_STATE_H_
