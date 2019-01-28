/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2013 by the Blender Foundation.
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

#ifndef __RIGIDDEFORM_SYSTEM_H__
#define __RIGIDDEFORM_SYSTEM_H__

#ifdef __cplusplus
extern "C" {
#endif

struct Mesh;
typedef struct OpaqueRigidDeformSystem *RigidDeformSystemRef;

typedef float (*Vector3Ds)[3];
typedef uint (*TriangleIndices)[3];

RigidDeformSystemRef RigidDeformSystem_from_mesh(
        struct Mesh *mesh);

void RigidDeformSystem_set_anchors(
        RigidDeformSystemRef system,
        uint *anchor_indices,
        uint anchor_amount);

void RigidDeformSystem_correct_inner(
        RigidDeformSystemRef system,
        Vector3Ds positions,
        uint iterations);

void RigidDeformSystem_free(
        RigidDeformSystemRef system);

#ifdef __cplusplus
}
#endif

#endif  /* __RIGIDDEFORM_SYSTEM_H__ */