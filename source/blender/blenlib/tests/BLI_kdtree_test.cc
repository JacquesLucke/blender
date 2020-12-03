/* Apache License, Version 2.0 */

#include "testing/testing.h"

#include "BLI_float2.hh"
#include "BLI_float3.hh"
#include "BLI_kdtree.h"
#include "BLI_kdtree.hh"
#include "BLI_rand.hh"
#include "BLI_timeit.hh"

namespace blender::kdtree {
struct Float1PointAdapter {
  static constexpr int DIM = 1;

  float get(const float &value, const int dim) const
  {
    BLI_assert(dim == 0);
    UNUSED_VARS_NDEBUG(dim);
    return value;
  }
};

struct Float2PointAdapter {
  static constexpr int DIM = 2;

  float get(const float2 &value, const int dim) const
  {
    return value[dim];
  }
};

struct Float3PointAdapater {
  static constexpr int DIM = 3;

  float get(const float3 &value, const int dim) const
  {
    return value[dim];
  }
};

template<> struct DefaultPointAdapter<float> {
  using type = Float1PointAdapter;
};

template<> struct DefaultPointAdapter<float2> {
  using type = Float2PointAdapter;
};

template<> struct DefaultPointAdapter<float3> {
  using type = Float3PointAdapater;
};

}  // namespace blender::kdtree

namespace blender::kdtree::tests {

static RawVector<float> test_points_1d = {-1, 2, 5, 3, 10, 2, 4};
static RawVector<float2> test_points_2d = {
    {-1, -1}, {0, 1}, {0, -1}, {0.5f, 0.5f}, {-10, 0}, {2, 3}, {5, -2}, {7, 3}, {6, -1}, {-2, -2}};

TEST(kdtree, FindNearest1D)
{
  KDTree<float, 1> kdtree{test_points_1d};

  EXPECT_EQ(*kdtree.find_nearest(3.4f), 3.0f);
  EXPECT_EQ(*kdtree.find_nearest(-10.0f), -1.0f);
  EXPECT_EQ(*kdtree.find_nearest(2.0f), 2.0f);
  EXPECT_EQ(*kdtree.find_nearest(100.0f), 10.0f);
  EXPECT_EQ(*kdtree.find_nearest(4.7f), 5.0f);
}

TEST(kdtree, FindInRadius1D)
{
  KDTree<float, 1> kdtree{test_points_1d};
  Vector<float> found_points;

  kdtree.foreach_in_radius(3.3f, 2.0f, [&](float point, const float UNUSED(distance_sq)) {
    found_points.append(point);
  });

  EXPECT_EQ(found_points.size(), 5);
  EXPECT_TRUE(found_points.contains(3.0f));
  EXPECT_TRUE(found_points.contains(4.0f));
  EXPECT_TRUE(found_points.contains(5.0f));
  EXPECT_TRUE(found_points.contains(2.0f));
  EXPECT_TRUE(found_points.contains(2.0f));
}

TEST(kdtree, FindNearest2D)
{
  KDTree<float2, 1> kdtree{test_points_2d};

  EXPECT_EQ(*kdtree.find_nearest({0.1f, -0.9f}), float2(0, -1));
  EXPECT_EQ(*kdtree.find_nearest({4, -1}), float2(5, -2));
  EXPECT_EQ(*kdtree.find_nearest({100, 100}), float2(7, 3));
}

TEST(kdtree, FindInRadius2D)
{
  KDTree<float2, 1> kdtree{test_points_2d};
  Vector<float2> found_points;

  kdtree.foreach_in_radius({-5, 0}, 6, [&](float2 point, const float UNUSED(distance_sq)) {
    found_points.append(point);
  });

  EXPECT_EQ(found_points.size(), 6);
  EXPECT_TRUE(found_points.contains({-10, 0}));
  EXPECT_TRUE(found_points.contains({-2, -2}));
  EXPECT_TRUE(found_points.contains({-1, -1}));
  EXPECT_TRUE(found_points.contains({0, -1}));
  EXPECT_TRUE(found_points.contains({0, 1}));
  EXPECT_TRUE(found_points.contains({0.5, 0.5}));
}

TEST(kdtree, PerformanceTest)
{
  const int point_amount = 1'000'000;
  Vector<float3> all_points(point_amount);

  RandomNumberGenerator rng;

  for (const int i : IndexRange(point_amount)) {
    all_points[i] = {rng.get_float(), rng.get_float(), rng.get_float()};
  }

  for (int i = 0; i < 5; i++) {
    {
      SCOPED_TIMER("build new");
      Array<float3> data = all_points.as_span();
      KDTree<float3> kdtree_new{data};
    }
    {
      KDTree_3d *kdtree_old = BLI_kdtree_3d_new(point_amount);
      {
        SCOPED_TIMER("build old");
        for (const int i : IndexRange(point_amount)) {
          BLI_kdtree_3d_insert(kdtree_old, i, all_points[i]);
        }
        BLI_kdtree_3d_balance(kdtree_old);
      }
      BLI_kdtree_3d_free(kdtree_old);
    }
  }
}

}  // namespace blender::kdtree::tests
