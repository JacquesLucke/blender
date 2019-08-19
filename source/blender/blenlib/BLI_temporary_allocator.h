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
 * This allocation method should be used when a chunk of memory is only used for a short amount
 * of time. This makes it possible to cache potentially large buffers for reuse, without the fear
 * of running out of memory because too many large buffers are allocated.
 *
 * Many cpu-bound algorithms can benefit from being split up into several stages, whereby the
 * output of one stage is written into an array that is read by the next stage. This improves
 * debugability as well as profilability. Often a reason this is not done is that the memory
 * allocation might be expensive. The goal of this allocator is to make this a non-issue, by
 * reusing the same long buffers over and over again.
 *
 * The number of allocated buffers should stay in O(number of threads * max depth of stack trace).
 * Since these numbers are pretty much constant in Blender, the number of chunks allocated should
 * not increase over time.
 */

#ifndef __BLI_TEMPORARY_ALLOCATOR_H__
#define __BLI_TEMPORARY_ALLOCATOR_H__

#include "BLI_utildefines.h"

#ifdef __cplusplus
extern "C" {
#endif

void *BLI_temporary_allocate(uint size);
void BLI_temporary_deallocate(void *buffer);

#ifdef __cplusplus
}
#endif

#endif /* __BLI_TEMPORARY_ALLOCATOR_H__ */
