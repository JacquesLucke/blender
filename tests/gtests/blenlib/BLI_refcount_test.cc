#include "testing/testing.h"
#include "BLI_refcount.h"

#include <iostream>

#define DEFAULT_VALUE 42

class MyTestClass : public BLI::RefCounter {
 public:
  int m_value;
  bool *m_alive = nullptr;

  MyTestClass() : m_value(DEFAULT_VALUE)
  {
  }

  MyTestClass(int value) : m_value(value)
  {
  }

  MyTestClass(bool *alive) : m_alive(alive)
  {
    *alive = true;
  }

  ~MyTestClass()
  {
    if (m_alive)
      *m_alive = false;
  }
};

using namespace BLI;

using SharedClass = AutoRefCount<MyTestClass>;

TEST(ref_count, OneReferenceAfterConstruction)
{
  SharedClass obj = SharedClass::New();
  ASSERT_EQ(obj->refcount(), 1);
}

TEST(ref_count, CopyConstructorIncreasesRefCount)
{
  SharedClass obj1 = SharedClass::New();
  ASSERT_EQ(obj1->refcount(), 1);
  SharedClass obj2(obj1);
  ASSERT_EQ(obj1->refcount(), 2);
  ASSERT_EQ(obj2->refcount(), 2);
}

TEST(ref_count, MoveConstructorKeepsRefCount)
{
  SharedClass obj(SharedClass::New());
  ASSERT_EQ(obj->refcount(), 1);
}

TEST(ref_count, DecreasedWhenScopeEnds)
{
  SharedClass obj1 = SharedClass::New();
  ASSERT_EQ(obj1->refcount(), 1);
  {
    SharedClass obj2 = obj1;
    ASSERT_EQ(obj1->refcount(), 2);
    ASSERT_EQ(obj2->refcount(), 2);
  }
  ASSERT_EQ(obj1->refcount(), 1);
}

TEST(ref_count, DefaultConstructorCalled)
{
  SharedClass obj = SharedClass::New();
  ASSERT_EQ(obj->m_value, DEFAULT_VALUE);
}

TEST(ref_count, OtherConstructorCalled)
{
  SharedClass obj = SharedClass::New(123);
  ASSERT_EQ(obj->m_value, 123);
}

TEST(ref_count, DestructorCalled)
{
  bool alive = false;
  {
    SharedClass obj = SharedClass::New(&alive);
    ASSERT_TRUE(alive);
  }
  ASSERT_FALSE(alive);
}

TEST(ref_count, CustomIncRef)
{
  auto *ptr = new MyTestClass();
  ASSERT_EQ(ptr->refcount(), 1);
  ptr->incref();
  ASSERT_EQ(ptr->refcount(), 2);
  ptr->decref();
  ptr->decref();
}

TEST(ref_count, CustomDecRef)
{
  auto *ptr = new MyTestClass();
  ptr->incref();
  ASSERT_EQ(ptr->refcount(), 2);
  ptr->decref();
  ASSERT_EQ(ptr->refcount(), 1);
  ptr->decref();
}

TEST(ref_count, ExtractRefCounted)
{
  SharedClass obj = SharedClass::New();
  MyTestClass *ptr = obj.ptr();
  ASSERT_EQ(obj->refcount(), 1);
  ptr->incref();
  ASSERT_EQ(obj->refcount(), 2);
  ptr->decref();
}

TEST(ref_count, DecRefToZero)
{
  bool alive = false;
  auto *ptr = new MyTestClass(&alive);
  ASSERT_TRUE(alive);
  ptr->decref();
  ASSERT_FALSE(alive);
}

TEST(ref_count, Empty)
{
  SharedClass obj;
  ASSERT_EQ(obj.ptr(), nullptr);
}
