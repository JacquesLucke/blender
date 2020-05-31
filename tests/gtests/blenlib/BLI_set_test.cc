#include <set>
#include <unordered_set>

#include "BLI_ghash.h"
#include "BLI_rand.h"
#include "BLI_set.hh"
#include "BLI_timeit.hh"
#include "BLI_vector.hh"
#include "testing/testing.h"

using namespace BLI;
using IntSet = Set<int>;

TEST(set, DefaultConstructor)
{
  IntSet set;
  EXPECT_EQ(set.size(), 0);
  EXPECT_TRUE(set.is_empty());
}

TEST(set, ContainsNotExistant)
{
  IntSet set;
  EXPECT_FALSE(set.contains(3));
}

TEST(set, ContainsExistant)
{
  IntSet set;
  EXPECT_FALSE(set.contains(5));
  EXPECT_TRUE(set.is_empty());
  set.add(5);
  EXPECT_TRUE(set.contains(5));
  EXPECT_FALSE(set.is_empty());
}

TEST(set, AddMany)
{
  IntSet set;
  for (int i = 0; i < 100; i++) {
    set.add(i);
  }

  for (int i = 50; i < 100; i++) {
    EXPECT_TRUE(set.contains(i));
  }
  for (int i = 100; i < 150; i++) {
    EXPECT_FALSE(set.contains(i));
  }
}

TEST(set, InitializerListConstructor)
{
  IntSet set = {4, 5, 6};
  EXPECT_EQ(set.size(), 3);
  EXPECT_TRUE(set.contains(4));
  EXPECT_TRUE(set.contains(5));
  EXPECT_TRUE(set.contains(6));
  EXPECT_FALSE(set.contains(2));
  EXPECT_FALSE(set.contains(3));
}

TEST(set, CopyConstructor)
{
  IntSet set = {3};
  EXPECT_TRUE(set.contains(3));
  EXPECT_FALSE(set.contains(4));

  IntSet set2(set);
  set2.add(4);
  EXPECT_TRUE(set2.contains(3));
  EXPECT_TRUE(set2.contains(4));

  EXPECT_FALSE(set.contains(4));
}

TEST(set, MoveConstructor)
{
  IntSet set = {1, 2, 3};
  EXPECT_EQ(set.size(), 3);
  IntSet set2(std::move(set));
  EXPECT_EQ(set.size(), 0);
  EXPECT_EQ(set2.size(), 3);
}

TEST(set, CopyAssignment)
{
  IntSet set = {3};
  EXPECT_TRUE(set.contains(3));
  EXPECT_FALSE(set.contains(4));

  IntSet set2;
  set2 = set;
  set2.add(4);
  EXPECT_TRUE(set2.contains(3));
  EXPECT_TRUE(set2.contains(4));

  EXPECT_FALSE(set.contains(4));
}

TEST(set, MoveAssignment)
{
  IntSet set = {1, 2, 3};
  EXPECT_EQ(set.size(), 3);
  IntSet set2;
  set2 = std::move(set);
  EXPECT_EQ(set.size(), 0);
  EXPECT_EQ(set2.size(), 3);
}

TEST(set, Remove)
{
  IntSet set = {3, 4, 5};
  EXPECT_TRUE(set.contains(3));
  EXPECT_TRUE(set.contains(4));
  EXPECT_TRUE(set.contains(5));
  set.remove(4);
  EXPECT_TRUE(set.contains(3));
  EXPECT_FALSE(set.contains(4));
  EXPECT_TRUE(set.contains(5));
  set.remove(3);
  EXPECT_FALSE(set.contains(3));
  EXPECT_FALSE(set.contains(4));
  EXPECT_TRUE(set.contains(5));
  set.remove(5);
  EXPECT_FALSE(set.contains(3));
  EXPECT_FALSE(set.contains(4));
  EXPECT_FALSE(set.contains(5));
}

TEST(set, RemoveMany)
{
  IntSet set;
  for (uint i = 0; i < 1000; i++) {
    set.add(i);
  }
  for (uint i = 100; i < 1000; i++) {
    set.remove(i);
  }
  for (uint i = 900; i < 1000; i++) {
    set.add(i);
  }

  for (uint i = 0; i < 1000; i++) {
    if (i < 100 || i >= 900) {
      EXPECT_TRUE(set.contains(i));
    }
    else {
      EXPECT_FALSE(set.contains(i));
    }
  }
}

TEST(set, Intersects)
{
  IntSet a = {3, 4, 5, 6};
  IntSet b = {1, 2, 5};
  EXPECT_TRUE(IntSet::Intersects(a, b));
  EXPECT_FALSE(IntSet::Disjoint(a, b));
}

TEST(set, Disjoint)
{
  IntSet a = {5, 6, 7, 8};
  IntSet b = {2, 3, 4, 9};
  EXPECT_FALSE(IntSet::Intersects(a, b));
  EXPECT_TRUE(IntSet::Disjoint(a, b));
}

TEST(set, AddMultiple)
{
  IntSet a;
  a.add_multiple({5, 7});
  EXPECT_TRUE(a.contains(5));
  EXPECT_TRUE(a.contains(7));
  EXPECT_FALSE(a.contains(4));
  a.add_multiple({2, 4, 7});
  EXPECT_TRUE(a.contains(4));
  EXPECT_TRUE(a.contains(2));
  EXPECT_EQ(a.size(), 4);
}

TEST(set, AddMultipleNew)
{
  IntSet a;
  a.add_multiple_new({5, 6});
  EXPECT_TRUE(a.contains(5));
  EXPECT_TRUE(a.contains(6));
}

TEST(set, Iterator)
{
  IntSet set = {1, 3, 2, 5, 4};
  BLI::Vector<int> vec;
  for (int value : set) {
    vec.append(value);
  }
  EXPECT_EQ(vec.size(), 5);
  EXPECT_TRUE(vec.contains(1));
  EXPECT_TRUE(vec.contains(3));
  EXPECT_TRUE(vec.contains(2));
  EXPECT_TRUE(vec.contains(5));
  EXPECT_TRUE(vec.contains(4));
}

TEST(set, OftenAddRemove)
{
  IntSet set;
  for (int i = 0; i < 100; i++) {
    set.add(42);
    EXPECT_EQ(set.size(), 1);
    set.remove(42);
    EXPECT_EQ(set.size(), 0);
  }
}

TEST(set, UniquePtrValues)
{
  Set<std::unique_ptr<int>> set;
  set.add_new(std::unique_ptr<int>(new int()));
  auto value1 = std::unique_ptr<int>(new int());
  set.add_new(std::move(value1));
  set.add(std::unique_ptr<int>(new int()));

  EXPECT_EQ(set.size(), 3);
}

TEST(set, Clear)
{
  Set<int> set = {3, 4, 6, 7};
  EXPECT_EQ(set.size(), 4);
  set.clear();
  EXPECT_EQ(set.size(), 0);
}

TEST(set, StringSet)
{
  Set<std::string> set;
  set.add("hello");
  set.add("world");
  EXPECT_EQ(set.size(), 2);
  EXPECT_TRUE(set.contains("hello"));
  EXPECT_TRUE(set.contains("world"));
  EXPECT_FALSE(set.contains("world2"));
}

TEST(set, PointerSet)
{
  int a, b, c;
  Set<int *> set;
  set.add(&a);
  set.add(&b);
  EXPECT_EQ(set.size(), 2);
  EXPECT_TRUE(set.contains(&a));
  EXPECT_TRUE(set.contains(&b));
  EXPECT_FALSE(set.contains(&c));
}

TEST(set, Discard)
{
  Set<int> set = {1, 2, 3, 4, 5, 6};
  EXPECT_EQ(set.size(), 6);
  EXPECT_TRUE(set.discard(2));
  EXPECT_EQ(set.size(), 5);
  EXPECT_FALSE(set.contains(2));
  EXPECT_FALSE(set.discard(2));
  EXPECT_EQ(set.size(), 5);
  EXPECT_TRUE(set.discard(5));
  EXPECT_EQ(set.size(), 4);
}

/**
 * Set this to 1 to activate the benchmark. It is disabled by default, because it prints a lot.
 */
#if 0
template<typename SetT>
BLI_NOINLINE void benchmark_random_ints(StringRef name, uint amount, uint factor)
{
  RNG *rng = BLI_rng_new(0);
  Vector<int> values;
  for (uint i = 0; i < amount; i++) {
    values.append(BLI_rng_get_int(rng) * factor);
  }
  BLI_rng_free(rng);

  SetT set;
  {
    SCOPED_TIMER(name + " Add");
    for (int value : values) {
      set.add(value);
    }
  }
  int count = 0;
  {
    SCOPED_TIMER(name + " Contains");
    for (int value : values) {
      count += set.contains(value);
    }
  }
  {
    SCOPED_TIMER(name + " Discard");
    for (int value : values) {
      count += set.discard(value);
    }
  }

  /* Print the value for simple error checking and to avoid some compiler optimizations. */
  std::cout << "Count: " << count << "\n";
}

TEST(set, Benchmark)
{
  for (uint i = 0; i < 3; i++) {
    benchmark_random_ints<BLI::Set<int>>("BLI::Set          ", 100000, 1);
    benchmark_random_ints<BLI::StdUnorderedSetWrapper<int>>("std::unordered_set", 100000, 1);
  }
  std::cout << "\n";
  for (uint i = 0; i < 3; i++) {
    uint32_t factor = (3 << 10);
    benchmark_random_ints<BLI::Set<int>>("BLI::Set          ", 100000, factor);
    benchmark_random_ints<BLI::StdUnorderedSetWrapper<int>>("std::unordered_set", 100000, factor);
  }
}
/**
 * Output of the rudimentary benchmark above on my hardware.
 *
 * Timer 'BLI::Set           Add' took 5.5573 ms
 * Timer 'BLI::Set           Contains' took 0.807384 ms
 * Timer 'BLI::Set           Discard' took 0.953436 ms
 * Count: 199998
 * Timer 'std::unordered_set Add' took 12.551 ms
 * Timer 'std::unordered_set Contains' took 2.3323 ms
 * Timer 'std::unordered_set Discard' took 5.07082 ms
 * Count: 199998
 * Timer 'BLI::Set           Add' took 2.62526 ms
 * Timer 'BLI::Set           Contains' took 0.407499 ms
 * Timer 'BLI::Set           Discard' took 0.472981 ms
 * Count: 199998
 * Timer 'std::unordered_set Add' took 6.26945 ms
 * Timer 'std::unordered_set Contains' took 1.17236 ms
 * Timer 'std::unordered_set Discard' took 3.77402 ms
 * Count: 199998
 * Timer 'BLI::Set           Add' took 2.59152 ms
 * Timer 'BLI::Set           Contains' took 0.415254 ms
 * Timer 'BLI::Set           Discard' took 0.477559 ms
 * Count: 199998
 * Timer 'std::unordered_set Add' took 6.28129 ms
 * Timer 'std::unordered_set Contains' took 1.17562 ms
 * Timer 'std::unordered_set Discard' took 3.77811 ms
 * Count: 199998
 *
 * Timer 'BLI::Set           Add' took 3.16514 ms
 * Timer 'BLI::Set           Contains' took 0.732895 ms
 * Timer 'BLI::Set           Discard' took 1.08171 ms
 * Count: 198790
 * Timer 'std::unordered_set Add' took 6.57377 ms
 * Timer 'std::unordered_set Contains' took 1.17008 ms
 * Timer 'std::unordered_set Discard' took 3.7946 ms
 * Count: 198790
 * Timer 'BLI::Set           Add' took 3.11439 ms
 * Timer 'BLI::Set           Contains' took 0.740159 ms
 * Timer 'BLI::Set           Discard' took 1.06749 ms
 * Count: 198790
 * Timer 'std::unordered_set Add' took 6.35597 ms
 * Timer 'std::unordered_set Contains' took 1.17713 ms
 * Timer 'std::unordered_set Discard' took 3.77826 ms
 * Count: 198790
 * Timer 'BLI::Set           Add' took 3.09876 ms
 * Timer 'BLI::Set           Contains' took 0.742072 ms
 * Timer 'BLI::Set           Discard' took 1.06622 ms
 * Count: 198790
 * Timer 'std::unordered_set Add' took 6.4469 ms
 * Timer 'std::unordered_set Contains' took 1.16515 ms
 * Timer 'std::unordered_set Discard' took 3.80639 ms
 * Count: 198790
 */

#endif
