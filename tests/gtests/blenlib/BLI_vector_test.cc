#include "testing/testing.h"
#include "BLI_vector.hpp"
#include "BLI_map.hpp"
#include <forward_list>

using BLI::Map;
using BLI::Vector;
using IntVector = Vector<int>;

TEST(small_vector, DefaultConstructor)
{
  IntVector vec;
  EXPECT_EQ(vec.size(), 0);
}

TEST(small_vector, SizeConstructor)
{
  IntVector vec(3);
  EXPECT_EQ(vec.size(), 3);
  EXPECT_EQ(vec[0], 0);
  EXPECT_EQ(vec[1], 0);
  EXPECT_EQ(vec[2], 0);
}

TEST(small_vector, SizeValueConstructor)
{
  IntVector vec(4, 10);
  EXPECT_EQ(vec.size(), 4);
  EXPECT_EQ(vec[0], 10);
  EXPECT_EQ(vec[1], 10);
  EXPECT_EQ(vec[2], 10);
  EXPECT_EQ(vec[3], 10);
}

TEST(small_vector, InitializerListConstructor)
{
  IntVector vec = {1, 3, 4, 6};
  EXPECT_EQ(vec.size(), 4);
  EXPECT_EQ(vec[0], 1);
  EXPECT_EQ(vec[1], 3);
  EXPECT_EQ(vec[2], 4);
  EXPECT_EQ(vec[3], 6);
}

TEST(small_vector, MappedArrayRefConstructor)
{
  Map<int, int> map;
  map.add(2, 5);
  map.add(1, 6);
  map.add(3, 2);

  IntVector keys = map.keys();
  IntVector values = map.values();

  EXPECT_EQ(keys.size(), 3);
  EXPECT_TRUE(keys.contains(1));
  EXPECT_TRUE(keys.contains(2));
  EXPECT_TRUE(keys.contains(3));

  EXPECT_EQ(values.size(), 3);
  EXPECT_TRUE(values.contains(2));
  EXPECT_TRUE(values.contains(5));
  EXPECT_TRUE(values.contains(6));
}

TEST(small_vector, NonIntrusiveListBaseConstructor)
{
  ListBase list = {0};
  BLI_addtail(&list, BLI_genericNodeN(POINTER_FROM_INT(42)));
  BLI_addtail(&list, BLI_genericNodeN(POINTER_FROM_INT(60)));
  BLI_addtail(&list, BLI_genericNodeN(POINTER_FROM_INT(90)));
  BLI::Vector<void *> vec(list, false);
  EXPECT_EQ(vec.size(), 3);
  EXPECT_EQ(POINTER_AS_INT(vec[0]), 42);
  EXPECT_EQ(POINTER_AS_INT(vec[1]), 60);
  EXPECT_EQ(POINTER_AS_INT(vec[2]), 90);
  BLI_freelistN(&list);
}

struct TestListValue {
  TestListValue *prev, *next;
  int value;
};

TEST(small_vector, IntrusiveListBaseConstructor)
{
  ListBase list = {0};
  BLI_addtail(&list, new TestListValue{0, 0, 4});
  BLI_addtail(&list, new TestListValue{0, 0, 6});
  BLI_addtail(&list, new TestListValue{0, 0, 7});
  Vector<TestListValue *> vec(list, true);
  EXPECT_EQ(vec.size(), 3);
  EXPECT_EQ(vec[0]->value, 4);
  EXPECT_EQ(vec[1]->value, 6);
  EXPECT_EQ(vec[2]->value, 7);

  delete vec[0];
  delete vec[1];
  delete vec[2];
}

TEST(small_vector, ContainerConstructor)
{
  std::forward_list<int> list;
  list.push_front(3);
  list.push_front(1);
  list.push_front(5);

  IntVector vec = IntVector::FromContainer(list);
  EXPECT_EQ(vec.size(), 3);
  EXPECT_EQ(vec[0], 5);
  EXPECT_EQ(vec[1], 1);
  EXPECT_EQ(vec[2], 3);
}

TEST(small_vector, CopyConstructor)
{
  IntVector vec1 = {1, 2, 3};
  IntVector vec2(vec1);
  EXPECT_EQ(vec2.size(), 3);
  EXPECT_EQ(vec2[0], 1);
  EXPECT_EQ(vec2[1], 2);
  EXPECT_EQ(vec2[2], 3);

  vec1[1] = 5;
  EXPECT_EQ(vec1[1], 5);
  EXPECT_EQ(vec2[1], 2);
}

TEST(small_vector, MoveAssignment)
{
  IntVector vec = {1, 2};
  EXPECT_EQ(vec.size(), 2);
  EXPECT_EQ(vec[0], 1);
  EXPECT_EQ(vec[1], 2);

  vec = IntVector({5});
  EXPECT_EQ(vec.size(), 1);
  EXPECT_EQ(vec[0], 5);
}

TEST(small_vector, CopyAssignment)
{
  IntVector vec1 = {1, 2, 3};
  IntVector vec2 = {4, 5};
  EXPECT_EQ(vec1.size(), 3);
  EXPECT_EQ(vec2.size(), 2);

  vec2 = vec1;
  EXPECT_EQ(vec2.size(), 3);

  vec1[0] = 7;
  EXPECT_EQ(vec1[0], 7);
  EXPECT_EQ(vec2[0], 1);
}

TEST(small_vector, Append)
{
  IntVector vec;
  vec.append(3);
  vec.append(6);
  vec.append(7);
  EXPECT_EQ(vec.size(), 3);
  EXPECT_EQ(vec[0], 3);
  EXPECT_EQ(vec[1], 6);
  EXPECT_EQ(vec[2], 7);
}

TEST(small_vector, Fill)
{
  IntVector vec(5);
  vec.fill(3);
  EXPECT_EQ(vec.size(), 5);
  EXPECT_EQ(vec[0], 3);
  EXPECT_EQ(vec[1], 3);
  EXPECT_EQ(vec[2], 3);
  EXPECT_EQ(vec[3], 3);
  EXPECT_EQ(vec[4], 3);
}

TEST(small_vector, Iterator)
{
  IntVector vec({1, 4, 9, 16});
  int i = 1;
  for (int value : vec) {
    EXPECT_EQ(value, i * i);
    i++;
  }
}

TEST(small_vector, BecomeLarge)
{
  Vector<int, 4> vec;
  for (int i = 0; i < 100; i++) {
    vec.append(i * 5);
  }
  EXPECT_EQ(vec.size(), 100);
  for (int i = 0; i < 100; i++) {
    EXPECT_EQ(vec[i], i * 5);
  }
}

IntVector return_by_value_helper()
{
  return IntVector({3, 5, 1});
}

TEST(small_vector, ReturnByValue)
{
  IntVector vec = return_by_value_helper();
  EXPECT_EQ(vec.size(), 3);
  EXPECT_EQ(vec[0], 3);
  EXPECT_EQ(vec[1], 5);
  EXPECT_EQ(vec[2], 1);
}

TEST(small_vector, VectorOfVectors_Append)
{
  Vector<IntVector> vec;
  EXPECT_EQ(vec.size(), 0);

  IntVector v({1, 2});
  vec.append(v);
  vec.append({7, 8});
  EXPECT_EQ(vec.size(), 2);
  EXPECT_EQ(vec[0][0], 1);
  EXPECT_EQ(vec[0][1], 2);
  EXPECT_EQ(vec[1][0], 7);
  EXPECT_EQ(vec[1][1], 8);
}

TEST(small_vector, VectorOfVectors_Fill)
{
  Vector<IntVector> vec(3);
  vec.fill({4, 5});

  EXPECT_EQ(vec[0][0], 4);
  EXPECT_EQ(vec[0][1], 5);
  EXPECT_EQ(vec[1][0], 4);
  EXPECT_EQ(vec[1][1], 5);
  EXPECT_EQ(vec[2][0], 4);
  EXPECT_EQ(vec[2][1], 5);
}

TEST(small_vector, RemoveLast)
{
  IntVector vec = {5, 6};
  EXPECT_EQ(vec.size(), 2);
  vec.remove_last();
  EXPECT_EQ(vec.size(), 1);
  vec.remove_last();
  EXPECT_EQ(vec.size(), 0);
}

TEST(small_vector, Empty)
{
  IntVector vec;
  EXPECT_TRUE(vec.empty());
  vec.append(1);
  EXPECT_FALSE(vec.empty());
  vec.remove_last();
  EXPECT_TRUE(vec.empty());
}

TEST(small_vector, RemoveReorder)
{
  IntVector vec = {4, 5, 6, 7};
  vec.remove_and_reorder(1);
  EXPECT_EQ(vec[0], 4);
  EXPECT_EQ(vec[1], 7);
  EXPECT_EQ(vec[2], 6);
  vec.remove_and_reorder(2);
  EXPECT_EQ(vec[0], 4);
  EXPECT_EQ(vec[1], 7);
  vec.remove_and_reorder(0);
  EXPECT_EQ(vec[0], 7);
  vec.remove_and_reorder(0);
  EXPECT_TRUE(vec.empty());
}

TEST(small_vector, AllEqual_False)
{
  IntVector a = {1, 2, 3};
  IntVector b = {1, 2, 4};
  bool result = IntVector::all_equal(a, b);
  EXPECT_FALSE(result);
}

TEST(small_vector, AllEqual_True)
{
  IntVector a = {4, 5, 6};
  IntVector b = {4, 5, 6};
  bool result = IntVector::all_equal(a, b);
  EXPECT_TRUE(result);
}

TEST(small_vector, ExtendSmallVector)
{
  IntVector a = {2, 3, 4};
  IntVector b = {11, 12};
  b.extend(a);
  EXPECT_EQ(b.size(), 5);
  EXPECT_EQ(b[0], 11);
  EXPECT_EQ(b[1], 12);
  EXPECT_EQ(b[2], 2);
  EXPECT_EQ(b[3], 3);
  EXPECT_EQ(b[4], 4);
}

TEST(small_vector, ExtendArray)
{
  int array[] = {3, 4, 5, 6};

  IntVector a;
  a.extend(array, 2);

  EXPECT_EQ(a.size(), 2);
  EXPECT_EQ(a[0], 3);
  EXPECT_EQ(a[1], 4);
}

TEST(small_vector, Last)
{
  IntVector a{3, 5, 7};
  EXPECT_EQ(a.last(), 7);
}

TEST(small_vector, AppendNTimes)
{
  IntVector a;
  a.append_n_times(5, 3);
  a.append_n_times(2, 2);
  EXPECT_EQ(a.size(), 5);
  EXPECT_EQ(a[0], 5);
  EXPECT_EQ(a[1], 5);
  EXPECT_EQ(a[2], 5);
  EXPECT_EQ(a[3], 2);
  EXPECT_EQ(a[4], 2);
}
