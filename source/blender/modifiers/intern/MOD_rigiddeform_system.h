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
struct RigidDeformSystem;

typedef float (*Vector3Ds)[3];

struct RigidDeformSystem *RigidDeformSystem_new(struct Mesh *mesh);

void RigidDeformSystem_setAnchors(
        struct RigidDeformSystem *system,
        int *anchor_indices, int anchor_amount);

void RigidDeformSystem_correctNonAnchors(
        struct RigidDeformSystem *system, Vector3Ds positions, int iterations);

void RigidDeformSystem_free(
        struct RigidDeformSystem *system);

#ifdef __cplusplus
}
#endif

#endif  /* __RIGIDDEFORM_SYSTEM_H__ */