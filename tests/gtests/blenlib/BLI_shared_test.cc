#include "testing/testing.h"
#include "BLI_shared.hpp"

#include <iostream>

#define DEFAULT_VALUE 42

class MyTestClass : public BLI::RefCountedBase {
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

TEST(shared, OneReferenceAfterConstruction)
{
  SharedClass obj = SharedClass::New();
  ASSERT_EQ(obj->refcount(), 1);
}

TEST(shared, CopyConstructorIncreasesRefCount)
{
  SharedClass obj1 = SharedClass::New();
  ASSERT_EQ(obj1->refcount(), 1);
  SharedClass obj2(obj1);
  ASSERT_EQ(obj1->refcount(), 2);
  ASSERT_EQ(obj2->refcount(), 2);
}

TEST(shared, MoveConstructorKeepsRefCount)
{
  SharedClass obj(SharedClass::New());
  ASSERT_EQ(obj->refcount(), 1);
}

TEST(shared, DecreasedWhenScopeEnds)
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

TEST(shared, DefaultConstructorCalled)
{
  SharedClass obj = SharedClass::New();
  ASSERT_EQ(obj->m_value, DEFAULT_VALUE);
}

TEST(shared, OtherConstructorCalled)
{
  SharedClass obj = SharedClass::New(123);
  ASSERT_EQ(obj->m_value, 123);
}

TEST(shared, DestructorCalled)
{
  bool alive = false;
  {
    SharedClass obj = SharedClass::New(&alive);
    ASSERT_TRUE(alive);
  }
  ASSERT_FALSE(alive);
}

TEST(shared, CustomIncRef)
{
  auto *ptr = new MyTestClass();
  ASSERT_EQ(ptr->refcount(), 1);
  ptr->incref();
  ASSERT_EQ(ptr->refcount(), 2);
  ptr->decref();
  ptr->decref();
}

TEST(shared, CustomDecRef)
{
  auto *ptr = new MyTestClass();
  ptr->incref();
  ASSERT_EQ(ptr->refcount(), 2);
  ptr->decref();
  ASSERT_EQ(ptr->refcount(), 1);
  ptr->decref();
}

TEST(shared, ExtractRefCounted)
{
  SharedClass obj = SharedClass::New();
  MyTestClass *ptr = obj.ptr();
  ASSERT_EQ(obj->refcount(), 1);
  ptr->incref();
  ASSERT_EQ(obj->refcount(), 2);
  ptr->decref();
}

TEST(shared, DecRefToZero)
{
  bool alive = false;
  auto *ptr = new MyTestClass(&alive);
  ASSERT_TRUE(alive);
  ptr->decref();
  ASSERT_FALSE(alive);
}

TEST(shared, Empty)
{
  SharedClass obj;
  ASSERT_EQ(obj.ptr(), nullptr);
}
