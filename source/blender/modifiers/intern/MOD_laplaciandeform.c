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
 * Contributor(s): Alexander Pinzon Fernandez
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/modifiers/intern/MOD_laplaciandeform.c
 *  \ingroup modifiers
 */

#include "BLI_utildefines.h"
#include "BLI_utildefines_stack.h"
#include "BLI_math.h"
#include "BLI_string.h"

#include "MEM_guardedalloc.h"

#include "BKE_deform.h"
#include "BKE_editmesh.h"
#include "BKE_library.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_runtime.h"
#include "BKE_particle.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"

#include "MOD_util.h"

#include "DEG_depsgraph_query.h"

#include "MOD_laplacian_system.h"

typedef LaplacianDeformModifierBindData BindData;


/* Cache
**************************************************/

typedef struct {
	struct LaplacianSystem *system;
} Cache;

static Cache *newCache(void)
{
	Cache *cache = MEM_callocN(sizeof(Cache), __func__);
	return cache;
}

static void freeCache(Cache *cache)
{
	MEM_freeN(cache);
}

static Cache *getCache(LaplacianDeformModifierData *lmd)
{
	Cache *cache = (Cache *)lmd->cache;
	BLI_assert(cache);
	return cache;
}

static void ensureCacheExists(
        LaplacianDeformModifierData *lmd,
        LaplacianDeformModifierData *lmd_orig)
{
	if (lmd->cache == NULL) {
		lmd_orig->cache = newCache();
		lmd->cache = lmd_orig->cache;
	}
}


/* Find anchor indices based on vertex group.
**************************************************/

static bool vertexGroupExists(Object *ob, Mesh *mesh, const char *group_name)
{
	MDeformVert *dvert = NULL;
	int group_index = -1;
	MOD_get_vgroup(ob, mesh, group_name, &dvert, &group_index);
	return group_index >= 0 && dvert != NULL;
}

static void getAllVertexWeights(Object *ob, Mesh *mesh, const char *group_name, float *dst)
{
	MDeformVert *vertices;
	int group_index;

	MOD_get_vgroup(ob, mesh, group_name, &vertices, &group_index);

	for (int i = 0; i < mesh->totvert; i++) {
		dst[i] = defvert_find_weight(vertices + i, group_index);
	}
}

static int countNonZeroIndices(float *values, int length)
{
	int amount = 0;
	for (int i = 0; i < length; i++) {
		if (values[i] != 0) amount++;
	}
	return amount;
}

static void getNonZeroIndices(float *values, int length, int **r_indices, int *r_amount)
{
	int amount = countNonZeroIndices(values, length);
	int *indices = MEM_malloc_arrayN(amount, sizeof(int), __func__);

	int index = 0;
	for (int i = 0; i < length; i++) {
		if (values[i] != 0) {
			indices[index] = i;
			index++;
		}
	}

	*r_indices = indices;
	*r_amount = amount;
}

static void getNonZeroWeightIndices(
        Object *ob, Mesh *mesh, const char *weight_group_name,
        int **r_indices, int *r_amount)
{
	int vertex_amount = mesh->totvert;
	float *weights = MEM_malloc_arrayN(vertex_amount, sizeof(float), __func__);
	getAllVertexWeights(ob, mesh, weight_group_name, weights);
	getNonZeroIndices(weights, vertex_amount, r_indices, r_amount);
}

static void getAnchorIndices(
        Object *ob, Mesh *mesh, const char *anchor_group_name,
        int **r_indices, int *r_amount)
{
	getNonZeroWeightIndices(ob, mesh, anchor_group_name, r_indices, r_amount);
}


/* Calculate bind data.
**************************************************/

static BindData *calculate_bind_data(
        LaplacianDeformModifierData *lmd, Object *ob, Mesh *mesh, float (*vertexCos)[3])
{
	BindData *bind_data = MEM_callocN(sizeof(BindData), __func__);

	int vertex_amount = mesh->totvert;
	bind_data->vertex_amount = vertex_amount;
	bind_data->initial_positions = MEM_malloc_arrayN(vertex_amount, sizeof(float) * 3, __func__);
	memcpy(bind_data->initial_positions, vertexCos, sizeof(float) * 3 * vertex_amount);

	getAnchorIndices(
	        ob, mesh, lmd->anchor_group_name,
	        &bind_data->anchor_indices, &bind_data->anchor_amount);

	return bind_data;
}

static void free_bind_data(BindData *bind_data)
{
	MEM_freeN(bind_data->initial_positions);
	MEM_freeN(bind_data->anchor_indices);
	MEM_freeN(bind_data);
}

static void bind_current_mesh_to_modifier(
        LaplacianDeformModifierData *lmd,
        LaplacianDeformModifierData *lmd_orig,
        Object *ob, Mesh *mesh, Vector3Ds vertexCos)
{
	if (lmd->cache) {
			freeCache(lmd->cache);
			lmd->cache = NULL;
		}
		if (lmd->bind_data) {
			free_bind_data(lmd->bind_data);
		}
		lmd_orig->bind_data = calculate_bind_data(lmd, ob, mesh, vertexCos);
		lmd->bind_data = lmd_orig->bind_data;
}

/* Modifier Stuff
********************************************/

static LaplacianDeformModifierData *get_original_modifier_data(
        LaplacianDeformModifierData *lmd,
        const ModifierEvalContext *ctx)
{
	Object *ob_orig = DEG_get_original_object(ctx->object);
	return (LaplacianDeformModifierData *)modifiers_findByName(ob_orig, lmd->modifier.name);
}

static void LaplacianDeformModifier_do(
        LaplacianDeformModifierData *lmd,
        const ModifierEvalContext *ctx, Mesh *mesh,
        Vector3Ds vertexCos)
{
	Object *ob = ctx->object;
	LaplacianDeformModifierData *lmd_orig = get_original_modifier_data(lmd, ctx);

	if (lmd->bind_next_execution) {
		bind_current_mesh_to_modifier(lmd, lmd_orig, ob, mesh, vertexCos);
		lmd_orig->bind_next_execution = false;
	}

	if (lmd->bind_data == NULL) return;
	BindData *bind_data = lmd->bind_data;

	ensureCacheExists(lmd, lmd_orig);
	Cache *cache = getCache(lmd);

	if (cache->system == NULL) {
		struct LaplacianSystem *system = LaplacianSystem_new(mesh);
		LaplacianSystem_setAnchors(system, bind_data->anchor_indices, bind_data->anchor_amount);
		cache->system = system;
	}

	LaplacianSystem_correctNonAnchors(cache->system, vertexCos);
}

static void initData(ModifierData *md)
{
	LaplacianDeformModifierData *lmd = (LaplacianDeformModifierData *)md;
	lmd->anchor_group_name[0] = '\0';
	lmd->bind_data = NULL;
	lmd->cache = NULL;
	lmd->bind_next_execution = false;
}

static void copyData(const ModifierData *md, ModifierData *target, const int flag)
{
	const LaplacianDeformModifierData *UNUSED(lmd) = (const LaplacianDeformModifierData *)md;
	LaplacianDeformModifierData *UNUSED(tlmd) = (LaplacianDeformModifierData *)target;

	modifier_copyData_generic(md, target, flag);
}

static bool isDisabled(const struct Scene *UNUSED(scene), ModifierData *md, bool UNUSED(useRenderParams))
{
	LaplacianDeformModifierData *UNUSED(lmd) = (LaplacianDeformModifierData *)md;
	return false;
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	LaplacianDeformModifierData *UNUSED(lmd) = (LaplacianDeformModifierData *)md;
	CustomDataMask dataMask = 0;
	dataMask |= CD_MASK_MDEFORMVERT;
	return dataMask;
}

static void deformVerts(
        ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh,
        Vector3Ds vertexCos, int numVerts)
{
	Mesh *mesh_src = MOD_deform_mesh_eval_get(ctx->object, NULL, mesh, NULL, numVerts, false, false);

	LaplacianDeformModifier_do(
	        (LaplacianDeformModifierData *)md,
	        ctx, mesh_src, vertexCos);

	if (mesh_src != mesh) {
		BKE_id_free(NULL, mesh_src);
	}
}

static void deformVertsEM(
        ModifierData *md, const ModifierEvalContext *ctx, struct BMEditMesh *editData,
        Mesh *mesh, Vector3Ds vertexCos, int numVerts)
{
	Mesh *mesh_src = MOD_deform_mesh_eval_get(ctx->object, editData, mesh, NULL, numVerts, false, false);

	LaplacianDeformModifier_do(
	        (LaplacianDeformModifierData *)md,
	        ctx, mesh_src, vertexCos);

	if (mesh_src != mesh) {
		BKE_id_free(NULL, mesh_src);
	}
}

static void freeData(ModifierData *md)
{
	LaplacianDeformModifierData *UNUSED(lmd) = (LaplacianDeformModifierData *)md;
}

ModifierTypeInfo modifierType_LaplacianDeform = {
	/* name */              "LaplacianDeform",
	/* structName */        "LaplacianDeformModifierData",
	/* structSize */        sizeof(LaplacianDeformModifierData),
	/* type */              eModifierTypeType_OnlyDeform,
	/* flags */             eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsEditmode,
	/* copyData */          copyData,

	/* deformVerts_DM */    NULL,
	/* deformMatrices_DM */ NULL,
	/* deformVertsEM_DM */  NULL,
	/* deformMatricesEM_DM*/NULL,
	/* applyModifier_DM */  NULL,

	/* deformVerts */       deformVerts,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     deformVertsEM,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     NULL,

	/* initData */          initData,
	/* requiredDataMask */  requiredDataMask,
	/* freeData */          freeData,
	/* isDisabled */        isDisabled,
	/* updateDepsgraph */   NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};
