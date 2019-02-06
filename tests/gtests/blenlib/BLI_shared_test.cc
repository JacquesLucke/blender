#include "testing/testing.h"
#include "BLI_shared.hpp"

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

using SharedClass = Shared<MyTestClass>;
using RefCountedClass = RefCounted<MyTestClass>;

TEST(shared, OneReferenceAfterConstruction)
{
	SharedClass obj = SharedClass::New();
	ASSERT_EQ(obj.refcounter()->refcount(), 1);
}

TEST(shared, CopyConstructorIncreasesRefCount)
{
	SharedClass obj1 = SharedClass::New();
	ASSERT_EQ(obj1.refcounter()->refcount(), 1);
	SharedClass obj2(obj1);
	ASSERT_EQ(obj1.refcounter()->refcount(), 2);
	ASSERT_EQ(obj2.refcounter()->refcount(), 2);
}

TEST(shared, MoveConstructorKeepsRefCount)
{
	SharedClass obj(SharedClass::New());
	ASSERT_EQ(obj.refcounter()->refcount(), 1);
}

TEST(shared, DecreasedWhenScopeEnds)
{
	SharedClass obj1 = SharedClass::New();
	ASSERT_EQ(obj1.refcounter()->refcount(), 1);
	{
		SharedClass obj2 = obj1;
		ASSERT_EQ(obj1.refcounter()->refcount(), 2);
		ASSERT_EQ(obj2.refcounter()->refcount(), 2);
	}
	ASSERT_EQ(obj1.refcounter()->refcount(), 1);
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
	RefCountedClass *obj = new RefCountedClass(new MyTestClass());
	ASSERT_EQ(obj->refcount(), 1);
	obj->incref();
	ASSERT_EQ(obj->refcount(), 2);
}

TEST(shared, CustomDecRef)
{
	RefCountedClass *obj = new RefCountedClass(new MyTestClass());
	obj->incref();
	ASSERT_EQ(obj->refcount(), 2);
	obj->decref();
	ASSERT_EQ(obj->refcount(), 1);
}

TEST(shared, ExtractRefCounted)
{
	SharedClass obj = SharedClass::New();
	RefCountedClass *ref = obj.refcounter();
	ASSERT_EQ(obj.refcounter()->refcount(), 1);
	ref->incref();
	ASSERT_EQ(obj.refcounter()->refcount(), 2);
}

TEST(shared, DecRefToZero)
{
	bool alive = false;
	RefCountedClass *obj = new RefCountedClass(new MyTestClass(&alive));
	ASSERT_TRUE(alive);
	obj->decref();
	ASSERT_FALSE(alive);
}