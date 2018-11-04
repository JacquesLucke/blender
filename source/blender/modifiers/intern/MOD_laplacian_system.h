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

#ifndef __LAPLACIAN_SYSTEM_H__
#define __LAPLACIAN_SYSTEM_H__

#ifdef __cplusplus
extern "C" {
#endif

struct Mesh;
struct LaplacianDeformModifierBindData;

struct SparseMatrix;
struct SystemMatrix;
struct SolverCache;

struct SystemMatrix *buildConstraintLaplacianSystemMatrix(
        struct Mesh *mesh,
        const float (*positions)[3],
        int *anchor_indices, int anchor_amount);

void calculateInitialInnerDiff(
        struct SystemMatrix *system_matrix,
        float (*positions)[3],
        float (*r_inner_diff)[3]);

void solveLaplacianSystem(
        struct SystemMatrix *matrix,
        const float (*inner_diff_pos)[3], const float (*anchor_pos)[3], struct SolverCache *cache,
        float (*r_result)[3]);

struct SolverCache *SolverCache_new(void);
void SolverCache_delete(struct SolverCache *cache);
void SolverCache_matrix_changed(struct SolverCache *cache);

#ifdef __cplusplus
}
#endif

#endif  /* __LAPLACIAN_SYSTEM_H__ */