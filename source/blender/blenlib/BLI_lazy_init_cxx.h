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
 * \ingroup bli
 *
 * These macros help to define functions that should only be
 * executed once to initialize some data. The initialized data
 * will only be freed when Blender quits.
 *
 * Requirements:
 *   - Very simple usage, without exposing the implementation details.
 *   - No additional heap allocation for every lazy initialized piece of data.
 *   - Blender has to be able to free all lazy-initialized data shortly
 *     before it quits. This is to make Blenders leak detection not detect
 *     false positives. It would, when we would just depend on static variables.
 *     These are destructed, after Blender prints non-freed memory blocks.
 */

#pragma once

#include <functional>
#include <BLI_optional.h>

#include "BLI_lazy_init.h"

namespace BLI {

void lazy_init_register(std::function<void()> free_func, const char *name);

}  // namespace BLI

#define BLI_LAZY_INIT(type, func_name) \
  static type func_name##_impl(void); \
  static type &func_name##_builder(void) \
  { \
    static BLI::Optional<type> value = func_name##_impl(); \
    BLI::lazy_init_register([]() { value.reset(); }, #func_name); \
    return value.value(); \
  } \
  type &func_name(void) \
  { \
    static type &value = func_name##_builder(); \
    return value; \
  } \
  type func_name##_impl(void)

#define BLI_LAZY_INIT_STATIC(type, func_name) \
  static type &func_name(void); \
  BLI_LAZY_INIT(type, func_name)
