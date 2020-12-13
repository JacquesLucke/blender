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

static Array<float3> generate_random_float3s(int amount, int seed = 0)
{
  Array<float3> points(amount);
  RandomNumberGenerator rng(seed);
  for (const int i : IndexRange(amount)) {
    points[i] = {rng.get_float(), rng.get_float(), rng.get_float()};
  }
  return points;
}

TEST(kdtree, BuildPerformance)
{
  Array<float3> points = generate_random_float3s(1'000'000);

  for (int i = 0; i < 5; i++) {
    {
      SCOPED_TIMER("build new");
      KDTree<float3> kdtree_new{points};
    }
    {
      KDTree_3d *kdtree_old = BLI_kdtree_3d_new(points.size());
      {
        SCOPED_TIMER("build old");
        for (const int i : points.index_range()) {
          BLI_kdtree_3d_insert(kdtree_old, i, points[i]);
        }
        BLI_kdtree_3d_balance(kdtree_old);
      }
      BLI_kdtree_3d_free(kdtree_old);
    }
  }
}

TEST(kdtree, NearestPerformance)
{
  Array<float3> points = generate_random_float3s(1'000'000, 0);
  KDTree<float3> kdtree_new{points};
  KDTree_3d *kdtree_old = BLI_kdtree_3d_new(points.size());
  for (const int i : points.index_range()) {
    BLI_kdtree_3d_insert(kdtree_old, i, points[i]);
  }
  BLI_kdtree_3d_balance(kdtree_old);

  Array<float3> query_points = generate_random_float3s(100'000, 23);

  float sum_new = 0.0f;
  float sum_old = 0.0f;
  for (int i = 0; i < 5; i++) {
    {
      SCOPED_TIMER("new");
      for (const float3 &query_point : query_points) {
        const float3 *nearest = kdtree_new.find_nearest(query_point);
        sum_new += nearest->x;
      }
    }
    {
      SCOPED_TIMER("old");
      for (const float3 &query_point : query_points) {
        KDTreeNearest_3d nearest;
        BLI_kdtree_3d_find_nearest(kdtree_old, query_point, &nearest);
        sum_old += nearest.co[0];
      }
    }
  }

  EXPECT_EQ(sum_new, sum_old);

  BLI_kdtree_3d_free(kdtree_old);
}

TEST(kdtree, FindNearest_Large)
{
  Array<float3> points = generate_random_float3s(10'000, 0);
  KDTree<float3> kdtree_new{points};
  KDTree_3d *kdtree_old = BLI_kdtree_3d_new(points.size());
  for (const int i : points.index_range()) {
    BLI_kdtree_3d_insert(kdtree_old, i, points[i]);
  }
  BLI_kdtree_3d_balance(kdtree_old);

  Array<float3> query_points = generate_random_float3s(1'000, 23);

  for (const float3 &query_point : query_points) {
    const float3 *point_new = kdtree_new.find_nearest(query_point);
    KDTreeNearest_3d nearest_old;
    BLI_kdtree_3d_find_nearest(kdtree_old, query_point, &nearest_old);
    EXPECT_EQ(*point_new, float3(nearest_old.co));
  }

  BLI_kdtree_3d_free(kdtree_old);
}

TEST(kdtree, FindRange_Large)
{
  Array<float3> points = generate_random_float3s(100'000, 0);
  KDTree<float3> kdtree_new{points};
  KDTree_3d *kdtree_old = BLI_kdtree_3d_new(points.size());
  for (const int i : points.index_range()) {
    BLI_kdtree_3d_insert(kdtree_old, i, points[i]);
  }
  BLI_kdtree_3d_balance(kdtree_old);

  Array<float3> query_points = generate_random_float3s(10'000, 23);
  RandomNumberGenerator rng;

  for (const float3 &query_point : query_points) {
    const float radius = rng.get_float() * 0.05f;

    Vector<float3> points_new;
    kdtree_new.foreach_in_radius(
        query_point, radius, [&](const float3 &point, const float UNUSED(distance_sq)) {
          points_new.append(point);
        });

    struct OldKDTreeUserData {
      Vector<float3> points_old;
    } user_data;

    BLI_kdtree_3d_range_search_cb(
        kdtree_old,
        query_point,
        radius,
        [](void *user_data, int UNUSED(index), const float *co, float UNUSED(dist_sq)) {
          OldKDTreeUserData *data = static_cast<OldKDTreeUserData *>(user_data);
          data->points_old.append(co);
          return true;
        },
        &user_data);

    EXPECT_EQ(points_new.size(), user_data.points_old.size());
    for (const float3 &point : points_new) {
      EXPECT_TRUE(user_data.points_old.contains(point));
    }
  }

  BLI_kdtree_3d_free(kdtree_old);
}

}  // namespace blender::kdtree::tests
