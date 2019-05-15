#include "testing/testing.h"
#include "BLI_string_collection.hpp"

using BLI::StringCollection;
using BLI::StringCollectionBuilder;

TEST(string_collection, Empty)
{
  StringCollectionBuilder builder;

  StringCollection *collection = builder.build();
  EXPECT_EQ(collection->size(), 0);
}

TEST(string_collection, Single)
{
  StringCollectionBuilder builder;
  builder.insert("Hello");

  StringCollection *collection = builder.build();
  EXPECT_EQ(collection->size(), 1);
  EXPECT_EQ(collection->get_ref(0).size(), 5);
  EXPECT_EQ(collection->get_ref(0).to_std_string(), "Hello");
}

TEST(string_collection, Multiple)
{
  StringCollectionBuilder builder;
  EXPECT_EQ(builder.insert("asd"), 0);
  EXPECT_EQ(builder.insert("qwef"), 1);
  EXPECT_EQ(builder.insert("zxcvb"), 2);

  StringCollection *collection = builder.build();
  EXPECT_EQ(collection->size(), 3);
  EXPECT_EQ(collection->get_ref(0).to_std_string(), "asd");
  EXPECT_EQ(collection->get_ref(1).to_std_string(), "qwef");
  EXPECT_EQ(collection->get_ref(2).to_std_string(), "zxcvb");
}

TEST(string_collection, CreatesCopy)
{
  const char *str = "Test";

  StringCollectionBuilder builder;
  builder.insert(str);

  StringCollection *collection = builder.build();
  EXPECT_EQ(collection->size(), 1);
  EXPECT_NE(collection->get_ref(0).data(), str);
}
