// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/property_tree.h"

#include "cc/test/geometry_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

TEST(PropertyTreeTest, ComputeTransformRoot) {
  TransformTree tree;
  TransformNode& root = *tree.Node(0);
  root.data.local.Translate(2, 2);
  root.data.target_id = 0;
  tree.UpdateTransforms(0);

  gfx::Transform expected;
  gfx::Transform transform;
  bool success = tree.ComputeTransform(0, 0, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

  transform.MakeIdentity();
  expected.Translate(2, 2);
  success = tree.ComputeTransform(0, -1, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

  transform.MakeIdentity();
  expected.MakeIdentity();
  expected.Translate(-2, -2);
  success = tree.ComputeTransform(-1, 0, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);
}

TEST(PropertyTreeTest, ComputeTransformChild) {
  TransformTree tree;
  TransformNode& root = *tree.Node(0);
  root.data.local.Translate(2, 2);
  root.data.target_id = 0;
  tree.UpdateTransforms(0);

  TransformNode child;
  child.data.local.Translate(3, 3);
  child.data.target_id = 0;

  tree.Insert(child, 0);
  tree.UpdateTransforms(1);

  gfx::Transform expected;
  gfx::Transform transform;

  expected.Translate(3, 3);
  bool success = tree.ComputeTransform(1, 0, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

  transform.MakeIdentity();
  expected.MakeIdentity();
  expected.Translate(-3, -3);
  success = tree.ComputeTransform(0, 1, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

  transform.MakeIdentity();
  expected.MakeIdentity();
  expected.Translate(5, 5);
  success = tree.ComputeTransform(1, -1, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

  transform.MakeIdentity();
  expected.MakeIdentity();
  expected.Translate(-5, -5);
  success = tree.ComputeTransform(-1, 1, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);
}

TEST(PropertyTreeTest, ComputeTransformSibling) {
  TransformTree tree;
  TransformNode& root = *tree.Node(0);
  root.data.local.Translate(2, 2);
  root.data.target_id = 0;
  tree.UpdateTransforms(0);

  TransformNode child;
  child.data.local.Translate(3, 3);
  child.data.target_id = 0;

  TransformNode sibling;
  sibling.data.local.Translate(7, 7);
  sibling.data.target_id = 0;

  tree.Insert(child, 0);
  tree.Insert(sibling, 0);

  tree.UpdateTransforms(1);
  tree.UpdateTransforms(2);

  gfx::Transform expected;
  gfx::Transform transform;

  expected.Translate(4, 4);
  bool success = tree.ComputeTransform(2, 1, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

  transform.MakeIdentity();
  expected.MakeIdentity();
  expected.Translate(-4, -4);
  success = tree.ComputeTransform(1, 2, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);
}

TEST(PropertyTreeTest, ComputeTransformSiblingSingularAncestor) {
  // In this test, we have the following tree:
  // root
  //   + singular
  //     + child
  //     + sibling
  // Now singular has a singular transform, so we cannot use screen space
  // transforms to compute change of basis transforms between |child| and
  // |sibling|.
  TransformTree tree;
  TransformNode& root = *tree.Node(0);
  root.data.local.Translate(2, 2);
  root.data.target_id = 0;
  tree.UpdateTransforms(0);

  TransformNode singular;
  singular.data.local.matrix().set(2, 2, 0.0);
  singular.data.target_id = 0;

  TransformNode child;
  child.data.local.Translate(3, 3);
  child.data.target_id = 0;

  TransformNode sibling;
  sibling.data.local.Translate(7, 7);
  sibling.data.target_id = 0;

  tree.Insert(singular, 0);
  tree.Insert(child, 1);
  tree.Insert(sibling, 1);

  tree.UpdateTransforms(1);
  tree.UpdateTransforms(2);
  tree.UpdateTransforms(3);

  gfx::Transform expected;
  gfx::Transform transform;

  expected.Translate(4, 4);
  bool success = tree.ComputeTransform(3, 2, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

  transform.MakeIdentity();
  expected.MakeIdentity();
  expected.Translate(-4, -4);
  success = tree.ComputeTransform(2, 3, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);
}

TEST(PropertyTreeTest, MultiplicationOrder) {
  TransformTree tree;
  TransformNode& root = *tree.Node(0);
  root.data.local.Translate(2, 2);
  root.data.target_id = 0;
  tree.UpdateTransforms(0);

  TransformNode child;
  child.data.local.Scale(2, 2);
  child.data.target_id = 0;

  tree.Insert(child, 0);
  tree.UpdateTransforms(1);

  gfx::Transform expected;
  expected.Translate(2, 2);
  expected.Scale(2, 2);

  gfx::Transform transform;
  gfx::Transform inverse;

  bool success = tree.ComputeTransform(1, -1, &transform);
  EXPECT_TRUE(success);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);

  success = tree.ComputeTransform(-1, 1, &inverse);
  EXPECT_TRUE(success);

  transform = transform * inverse;
  expected.MakeIdentity();
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, transform);
}

}  // namespace cc
