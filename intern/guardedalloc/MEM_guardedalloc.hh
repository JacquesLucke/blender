/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup MEM
 *
 * This extends `MEM_guardedalloc.h` with C++ features.
 */

#pragma once

#include "MEM_guardedalloc.h"

/* Allocation functions (for C++ only). */
#define MEM_CXX_CLASS_ALLOC_FUNCS(_id) \
 public: \
  void *operator new(size_t num_bytes) \
  { \
    return MEM_mallocN(num_bytes, _id); \
  } \
  void operator delete(void *mem) \
  { \
    if (mem) { \
      MEM_freeN(mem); \
    } \
  } \
  void *operator new[](size_t num_bytes) \
  { \
    return MEM_mallocN(num_bytes, _id "[]"); \
  } \
  void operator delete[](void *mem) \
  { \
    if (mem) { \
      MEM_freeN(mem); \
    } \
  } \
  void *operator new(size_t /*count*/, void *ptr) \
  { \
    return ptr; \
  } \
  /* This is the matching delete operator to the placement-new operator above. Both parameters \
   * will have the same value. Without this, we get the warning C4291 on windows. */ \
  void operator delete(void * /*ptr_to_free*/, void * /*ptr*/) \
  { \
  }

/* Needed when type includes a namespace, then the namespace should not be
 * specified after ~, so using a macro fails. */
template<class T> inline void OBJECT_GUARDED_DESTRUCTOR(T *what)
{
  what->~T();
}

#if defined __GNUC__
#  define OBJECT_GUARDED_NEW(type, args...) new (MEM_mallocN(sizeof(type), __func__)) type(args)
#else
#  define OBJECT_GUARDED_NEW(type, ...) \
    new (MEM_mallocN(sizeof(type), __FUNCTION__)) type(__VA_ARGS__)
#endif
#define OBJECT_GUARDED_DELETE(what, type) \
  { \
    if (what) { \
      OBJECT_GUARDED_DESTRUCTOR((type *)what); \
      MEM_freeN(what); \
    } \
  } \
  (void)0
#define OBJECT_GUARDED_SAFE_DELETE(what, type) \
  { \
    if (what) { \
      OBJECT_GUARDED_DESTRUCTOR((type *)what); \
      MEM_freeN(what); \
      what = NULL; \
    } \
  } \
  (void)0
