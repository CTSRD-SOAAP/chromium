// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/tile.h"
#include "cc/resources/tile_priority.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_tile_manager.h"
#include "cc/test/fake_tile_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class FakePicturePileImpl : public PicturePileImpl {
 public:
  FakePicturePileImpl() : PicturePileImpl(false) {
    gfx::Size size(std::numeric_limits<int>::max(),
                   std::numeric_limits<int>::max());
    Resize(size);
    recorded_region_ = Region(gfx::Rect(size));
  }

 protected:
  virtual ~FakePicturePileImpl() {}
};

class TilePriorityForSoonBin : public TilePriority {
 public:
  TilePriorityForSoonBin() : TilePriority(
            HIGH_RESOLUTION,
            0.5,
            300.0) {}
};

class TilePriorityForEventualBin : public TilePriority {
 public:
    TilePriorityForEventualBin() : TilePriority(
            NON_IDEAL_RESOLUTION,
            1.0,
            315.0) {}
};

class TilePriorityForNowBin : public TilePriority {
 public:
    TilePriorityForNowBin() : TilePriority(
            HIGH_RESOLUTION,
            0,
            0) {}
};

class TilePriorityRequiredForActivation : public TilePriority {
 public:
    TilePriorityRequiredForActivation() : TilePriority(
            HIGH_RESOLUTION,
            0,
            0) {
      required_for_activation = true;
    }
};

class TileManagerTest : public testing::Test {
 public:
  typedef std::vector<scoped_refptr<Tile> > TileVector;

  void Initialize(int max_memory_tiles,
                  TileMemoryLimitPolicy memory_limit_policy,
                  TreePriority tree_priority) {
    output_surface_ = FakeOutputSurface::Create3d();
    resource_provider_ = ResourceProvider::Create(output_surface_.get(), 0);
    tile_manager_ = make_scoped_ptr(
        new FakeTileManager(&tile_manager_client_, resource_provider_.get()));

    memory_limit_policy_ = memory_limit_policy;
    max_memory_tiles_ = max_memory_tiles;
    GlobalStateThatImpactsTilePriority state;
    gfx::Size tile_size = settings_.default_tile_size;
    state.memory_limit_in_bytes =
        max_memory_tiles * 4 * tile_size.width() * tile_size.height();
    state.memory_limit_policy = memory_limit_policy;
    state.tree_priority = tree_priority;

    tile_manager_->SetGlobalState(state);
    picture_pile_ = make_scoped_refptr(new FakePicturePileImpl());
  }

  void SetTreePriority(TreePriority tree_priority) {
    GlobalStateThatImpactsTilePriority state;
    gfx::Size tile_size = settings_.default_tile_size;
    state.memory_limit_in_bytes =
        max_memory_tiles_ * 4 * tile_size.width() * tile_size.height();
    state.memory_limit_policy = memory_limit_policy_;
    state.tree_priority = tree_priority;
    tile_manager_->SetGlobalState(state);
  }

  virtual void TearDown() OVERRIDE {
    tile_manager_.reset(NULL);
    picture_pile_ = NULL;

    testing::Test::TearDown();
  }

  TileVector CreateTiles(int count,
                         TilePriority active_priority,
                         TilePriority pending_priority) {
    TileVector tiles;
    for (int i = 0; i < count; ++i) {
      scoped_refptr<Tile> tile =
          make_scoped_refptr(new Tile(tile_manager_.get(),
                                      picture_pile_.get(),
                                      settings_.default_tile_size,
                                      gfx::Rect(),
                                      gfx::Rect(),
                                      1.0,
                                      0,
                                      0));
      tile->SetPriority(ACTIVE_TREE, active_priority);
      tile->SetPriority(PENDING_TREE, pending_priority);
      tiles.push_back(tile);
    }
    return tiles;
  }

  FakeTileManager* tile_manager() {
    return tile_manager_.get();
  }

  int AssignedMemoryCounts(const TileVector& tiles) {
    int has_memory_count = 0;
    for (TileVector::const_iterator it = tiles.begin();
         it != tiles.end();
         ++it) {
      if (tile_manager_->HasBeenAssignedMemory(*it))
        ++has_memory_count;
    }
    return has_memory_count;
  }

 private:
  FakeTileManagerClient tile_manager_client_;
  LayerTreeSettings settings_;
  scoped_ptr<FakeTileManager> tile_manager_;
  scoped_refptr<FakePicturePileImpl> picture_pile_;
  scoped_ptr<FakeOutputSurface> output_surface_;
  scoped_ptr<ResourceProvider> resource_provider_;
  TileMemoryLimitPolicy memory_limit_policy_;
  int max_memory_tiles_;
};

TEST_F(TileManagerTest, EnoughMemoryAllowAnything) {
  // A few tiles of each type of priority, with enough memory for all tiles.

  Initialize(10, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_now =
      CreateTiles(3, TilePriorityForNowBin(), TilePriority());
  TileVector pending_now =
      CreateTiles(3, TilePriority(), TilePriorityForNowBin());
  TileVector active_pending_soon = CreateTiles(
      3, TilePriorityForSoonBin(), TilePriorityForSoonBin());
  TileVector never_bin = CreateTiles(1, TilePriority(), TilePriority());

  tile_manager()->AssignMemoryToTiles();

  EXPECT_EQ(3, AssignedMemoryCounts(active_now));
  EXPECT_EQ(3, AssignedMemoryCounts(pending_now));
  EXPECT_EQ(3, AssignedMemoryCounts(active_pending_soon));
  EXPECT_EQ(0, AssignedMemoryCounts(never_bin));

  active_now.clear();
  pending_now.clear();
  active_pending_soon.clear();
  never_bin.clear();

  TearDown();
}

TEST_F(TileManagerTest, EnoughMemoryAllowPrepaintOnly) {
  // A few tiles of each type of priority, with enough memory for all tiles,
  // with the exception of never bin.

  Initialize(10, ALLOW_PREPAINT_ONLY, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_now =
      CreateTiles(3, TilePriorityForNowBin(), TilePriority());
  TileVector pending_now =
      CreateTiles(3, TilePriority(), TilePriorityForNowBin());
  TileVector active_pending_soon = CreateTiles(
      3, TilePriorityForSoonBin(), TilePriorityForSoonBin());
  TileVector never_bin = CreateTiles(1, TilePriority(), TilePriority());

  tile_manager()->AssignMemoryToTiles();

  EXPECT_EQ(3, AssignedMemoryCounts(active_now));
  EXPECT_EQ(3, AssignedMemoryCounts(pending_now));
  EXPECT_EQ(3, AssignedMemoryCounts(active_pending_soon));
  EXPECT_EQ(0, AssignedMemoryCounts(never_bin));

  active_now.clear();
  pending_now.clear();
  active_pending_soon.clear();
  never_bin.clear();
  TearDown();
}

TEST_F(TileManagerTest, EnoughMemoryAllowAbsoluteMinimum) {
  // A few tiles of each type of priority, with enough memory for all tiles,
  // with the exception of never and soon bins.

  Initialize(10, ALLOW_ABSOLUTE_MINIMUM, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_now =
      CreateTiles(3, TilePriorityForNowBin(), TilePriority());
  TileVector pending_now =
      CreateTiles(3, TilePriority(), TilePriorityForNowBin());
  TileVector active_pending_soon = CreateTiles(
      3, TilePriorityForSoonBin(), TilePriorityForSoonBin());
  TileVector never_bin = CreateTiles(1, TilePriority(), TilePriority());

  tile_manager()->AssignMemoryToTiles();

  EXPECT_EQ(3, AssignedMemoryCounts(active_now));
  EXPECT_EQ(3, AssignedMemoryCounts(pending_now));
  EXPECT_EQ(0, AssignedMemoryCounts(active_pending_soon));
  EXPECT_EQ(0, AssignedMemoryCounts(never_bin));

  active_now.clear();
  pending_now.clear();
  active_pending_soon.clear();
  never_bin.clear();
  TearDown();
}

TEST_F(TileManagerTest, EnoughMemoryAllowNothing) {
  // A few tiles of each type of priority, with enough memory for all tiles,
  // but allow nothing should not assign any memory.

  Initialize(10, ALLOW_NOTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_now =
      CreateTiles(3, TilePriorityForNowBin(), TilePriority());
  TileVector pending_now =
      CreateTiles(3, TilePriority(), TilePriorityForNowBin());
  TileVector active_pending_soon = CreateTiles(
      3, TilePriorityForSoonBin(), TilePriorityForSoonBin());
  TileVector never_bin = CreateTiles(1, TilePriority(), TilePriority());

  tile_manager()->AssignMemoryToTiles();

  EXPECT_EQ(0, AssignedMemoryCounts(active_now));
  EXPECT_EQ(0, AssignedMemoryCounts(pending_now));
  EXPECT_EQ(0, AssignedMemoryCounts(active_pending_soon));
  EXPECT_EQ(0, AssignedMemoryCounts(never_bin));

  active_now.clear();
  pending_now.clear();
  active_pending_soon.clear();
  never_bin.clear();
  TearDown();
}

TEST_F(TileManagerTest, DISABLED_PartialOOMMemoryToPending) {
  // 5 tiles on active tree eventually bin, 5 tiles on pending tree that are
  // required for activation, but only enough memory for 8 tiles. The result
  // is all pending tree tiles get memory, and 3 of the active tree tiles
  // get memory.

  Initialize(8, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_tree_tiles =
      CreateTiles(5, TilePriorityForEventualBin(), TilePriority());
  TileVector pending_tree_tiles =
      CreateTiles(5, TilePriority(), TilePriorityRequiredForActivation());

  tile_manager()->AssignMemoryToTiles();

  EXPECT_EQ(3, AssignedMemoryCounts(active_tree_tiles));
  EXPECT_EQ(5, AssignedMemoryCounts(pending_tree_tiles));

  SetTreePriority(SAME_PRIORITY_FOR_BOTH_TREES);
  tile_manager()->AssignMemoryToTiles();

  EXPECT_EQ(3, AssignedMemoryCounts(active_tree_tiles));
  EXPECT_EQ(5, AssignedMemoryCounts(pending_tree_tiles));
}

TEST_F(TileManagerTest, PartialOOMMemoryToActive) {
  // 5 tiles on active tree eventually bin, 5 tiles on pending tree now bin,
  // but only enough memory for 8 tiles. The result is all active tree tiles
  // get memory, and 3 of the pending tree tiles get memory.

  Initialize(8, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_tree_tiles =
      CreateTiles(5, TilePriorityForNowBin(), TilePriority());
  TileVector pending_tree_tiles =
      CreateTiles(5, TilePriority(), TilePriorityForNowBin());

  tile_manager()->AssignMemoryToTiles();

  EXPECT_EQ(5, AssignedMemoryCounts(active_tree_tiles));
  EXPECT_EQ(3, AssignedMemoryCounts(pending_tree_tiles));

  pending_tree_tiles.clear();
  active_tree_tiles.clear();
  TearDown();
}

TEST_F(TileManagerTest, DISABLED_TotalOOMMemoryToPending) {
  // 5 tiles on active tree eventually bin, 5 tiles on pending tree that are
  // required for activation, but only enough memory for 4 tiles. The result
  // is 4 pending tree tiles get memory, and none of the active tree tiles
  // get memory.

  Initialize(4, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_tree_tiles =
      CreateTiles(5, TilePriorityForEventualBin(), TilePriority());
  TileVector pending_tree_tiles =
      CreateTiles(5, TilePriority(), TilePriorityRequiredForActivation());

  tile_manager()->AssignMemoryToTiles();

  EXPECT_EQ(0, AssignedMemoryCounts(active_tree_tiles));
  EXPECT_EQ(4, AssignedMemoryCounts(pending_tree_tiles));

  SetTreePriority(SAME_PRIORITY_FOR_BOTH_TREES);
  tile_manager()->AssignMemoryToTiles();

  EXPECT_EQ(0, AssignedMemoryCounts(active_tree_tiles));
  EXPECT_EQ(4, AssignedMemoryCounts(pending_tree_tiles));
}

TEST_F(TileManagerTest, DISABLED_TotalOOMActiveSoonMemoryToPending) {
  // 5 tiles on active tree soon bin, 5 tiles on pending tree that are
  // required for activation, but only enough memory for 4 tiles. The result
  // is 4 pending tree tiles get memory, and none of the active tree tiles
  // get memory.

  Initialize(4, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_tree_tiles =
      CreateTiles(5, TilePriorityForSoonBin(), TilePriority());
  TileVector pending_tree_tiles =
      CreateTiles(5, TilePriority(), TilePriorityRequiredForActivation());

  tile_manager()->AssignMemoryToTiles();

  EXPECT_EQ(0, AssignedMemoryCounts(active_tree_tiles));
  EXPECT_EQ(4, AssignedMemoryCounts(pending_tree_tiles));

  SetTreePriority(SAME_PRIORITY_FOR_BOTH_TREES);
  tile_manager()->AssignMemoryToTiles();

  EXPECT_EQ(0, AssignedMemoryCounts(active_tree_tiles));
  EXPECT_EQ(4, AssignedMemoryCounts(pending_tree_tiles));
}

TEST_F(TileManagerTest, TotalOOMMemoryToActive) {
  // 5 tiles on active tree eventually bin, 5 tiles on pending tree now bin,
  // but only enough memory for 4 tiles. The result is 5 active tree tiles
  // get memory, and none of the pending tree tiles get memory.

  Initialize(4, ALLOW_ANYTHING, SMOOTHNESS_TAKES_PRIORITY);
  TileVector active_tree_tiles =
      CreateTiles(5, TilePriorityForNowBin(), TilePriority());
  TileVector pending_tree_tiles =
      CreateTiles(5, TilePriority(), TilePriorityForNowBin());

  tile_manager()->AssignMemoryToTiles();

  EXPECT_EQ(4, AssignedMemoryCounts(active_tree_tiles));
  EXPECT_EQ(0, AssignedMemoryCounts(pending_tree_tiles));

  pending_tree_tiles.clear();
  active_tree_tiles.clear();
  TearDown();
}

}  // namespace
}  // namespace cc
