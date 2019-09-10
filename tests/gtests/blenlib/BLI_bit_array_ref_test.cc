#include "testing/testing.h"
#include "BLI_bit_array_ref.hpp"

using namespace BLI;

TEST(mutable_bit_array_ref, Constructor)
{
  uint8_t value = 0;
  MutableBitArrayRef ref(value);

  EXPECT_EQ(ref.size(), 8);
  EXPECT_FALSE(ref[0].is_set());
  EXPECT_FALSE(ref[1].is_set());
  EXPECT_FALSE(ref[2].is_set());
  EXPECT_FALSE(ref[3].is_set());
  EXPECT_FALSE(ref[4].is_set());
  EXPECT_FALSE(ref[5].is_set());
  EXPECT_FALSE(ref[6].is_set());
  EXPECT_FALSE(ref[7].is_set());
}

TEST(mutable_bit_array_ref, Constructor2)
{
  uint8_t value = 130;
  MutableBitArrayRef ref(value);
  EXPECT_EQ(ref.size(), 8);

  EXPECT_FALSE(ref[0].is_set());
  EXPECT_TRUE(ref[1].is_set());
  EXPECT_FALSE(ref[2].is_set());
  EXPECT_FALSE(ref[3].is_set());
  EXPECT_FALSE(ref[4].is_set());
  EXPECT_FALSE(ref[5].is_set());
  EXPECT_FALSE(ref[6].is_set());
  EXPECT_TRUE(ref[7].is_set());
}

TEST(mutable_bit_array_ref, SetBitInByte)
{
  uint8_t value = 0;
  MutableBitArrayRef ref(value);

  ref[0].set();
  EXPECT_EQ(value, 1);
  ref[4].set();
  EXPECT_EQ(value, 17);
  ref[2].set();
  EXPECT_EQ(value, 21);
}

TEST(mutable_bit_array_ref, UnsetBitInByte)
{
  uint8_t value = 0xFF;
  MutableBitArrayRef ref(value);

  ref[7].unset();
  EXPECT_EQ(value, 127);
  ref[2].unset();
  EXPECT_EQ(value, 123);
  ref[0].unset();
  EXPECT_EQ(value, 122);
}

TEST(mutable_bit_array_ref, Slice)
{
  uint8_t value[4] = {0};
  MutableBitArrayRef ref(value, 32);

  EXPECT_EQ(ref.size(), 32);
  auto sliced_ref = ref.slice(10, 5);
  EXPECT_EQ(sliced_ref.size(), 5);
  sliced_ref[2].set();
  EXPECT_EQ(value[1], 16);
}

TEST(mutable_bit_array_ref, IsSet)
{
  uint8_t value[2] = {0};
  value[0] = 0b00100110;
  value[1] = 0b10000100;
  MutableBitArrayRef ref(value, 16);

  EXPECT_FALSE(ref.is_set(0));
  EXPECT_FALSE(ref.is_set(3));
  EXPECT_FALSE(ref.is_set(4));
  EXPECT_FALSE(ref.is_set(6));
  EXPECT_FALSE(ref.is_set(7));
  EXPECT_FALSE(ref.is_set(8));
  EXPECT_FALSE(ref.is_set(9));
  EXPECT_FALSE(ref.is_set(11));
  EXPECT_FALSE(ref.is_set(12));
  EXPECT_FALSE(ref.is_set(13));
  EXPECT_FALSE(ref.is_set(14));

  EXPECT_TRUE(ref.is_set(1));
  EXPECT_TRUE(ref.is_set(2));
  EXPECT_TRUE(ref.is_set(5));
  EXPECT_TRUE(ref.is_set(10));
  EXPECT_TRUE(ref.is_set(15));
}
