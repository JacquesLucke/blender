#include "testing/testing.h"
#include "BLI_refcount.hpp"

#include <iostream>

#define DEFAULT_VALUE 42

class MyTestClass {
public:
	int m_value;
	bool *m_alive = nullptr;

	MyTestClass()
		: m_value(DEFAULT_VALUE) {}

	MyTestClass(int value)
		: m_value(value) {}

	MyTestClass(bool *alive)
		: m_alive(alive)
	{
		*alive = true;
	}

	~MyTestClass()
	{
		if (this->m_alive) *this->m_alive = false;
	}
};

using namespace BLI;

using TestObj = RefCount<MyTestClass>;

TEST(refcount, OneReferenceAfterConstruction)
{
	TestObj obj = TestObj::make();
	ASSERT_EQ(obj.refcount(), 1);
}

TEST(refcount, IncRefIncreasesRefCount)
{
	TestObj obj = TestObj::make();
	ASSERT_EQ(obj.refcount(), 1);
	obj.incref();
	ASSERT_EQ(obj.refcount(), 2);
}

TEST(refcount, DecRefDecreasesRefCount)
{
	TestObj obj = TestObj::make();
	obj.incref();
	ASSERT_EQ(obj.refcount(), 2);
	obj.decref();
	ASSERT_EQ(obj.refcount(), 1);
}

TEST(refcount, CopyConstructorIncreasesRefCount)
{
	TestObj obj1 = TestObj::make();
	ASSERT_EQ(obj1.refcount(), 1);
	TestObj obj2(obj1);
	ASSERT_EQ(obj1.refcount(), 2);
	ASSERT_EQ(obj2.refcount(), 2);
}

TEST(refcount, MoveConstructorKeepsRefCount)
{
	TestObj obj(TestObj::make());
	ASSERT_EQ(obj.refcount(), 1);
}

TEST(refcount, DecreasedWhenScopeEnds)
{
	TestObj obj1 = TestObj::make();
	ASSERT_EQ(obj1.refcount(), 1);
	{
		TestObj obj2 = obj1;
		ASSERT_EQ(obj1.refcount(), 2);
		ASSERT_EQ(obj2.refcount(), 2);
	}
	ASSERT_EQ(obj1.refcount(), 1);
}

TEST(refcount, DefaultConstructorCalled)
{
	TestObj obj = TestObj::make();
	ASSERT_EQ(obj->m_value, DEFAULT_VALUE);
}

TEST(refcount, OtherConstructorCalled)
{
	TestObj obj = TestObj::make(123);
	ASSERT_EQ(obj->m_value, 123);
}

TEST(refcount, DestructorCalled)
{
	bool alive = false;
	{
		TestObj obj = TestObj::make(&alive);
		ASSERT_TRUE(alive);
	}
	ASSERT_FALSE(alive);
}