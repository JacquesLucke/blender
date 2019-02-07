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
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
 * All rights reserved.
  * ***** END GPL LICENSE BLOCK *****
 *
 */
 /** \file blender/modifiers/intern/MOD_rigiddeform.c
 *  \ingroup modifiers
 *
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_deform.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "MOD_util.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "MOD_rigiddeform_system.h"

typedef float (*VectorArray)[3];
typedef RigidDeformModifierBindData BindData;

/* ************* Cache **************** */

typedef struct Cache {
	RigidDeformSystemRef system;
} Cache;

static Cache *cache_new(void)
{
	return MEM_callocN(sizeof(Cache), __func__);
}

static void cache_free(Cache *cache)
{
	if (cache->system) {
		RigidDeformSystem_free(cache->system);
	}
	MEM_freeN(cache);
}

static void ensure_cache_exists(RigidDeformModifierData *rdmd, RigidDeformModifierData *rdmd_orig)
{
	if (rdmd->cache == NULL) {
		rdmd_orig->cache = cache_new();
		rdmd->cache = rdmd_orig->cache;
	}
}


/* ************* Binding *************** */

static bool vertex_group_exists(Object *ob, Mesh *mesh, const char *name)
{
	MDeformVert *dvert = NULL;
	int group_index = -1;
	MOD_get_vgroup(ob, mesh, name, &dvert, &group_index);
	return group_index >= 0 && dvert != NULL;
}

static void get_all_vertex_weights(Object *ob, Mesh *mesh, const char *name, float *dst)
{
	MDeformVert *vertices;
	int group_index;
	MOD_get_vgroup(ob, mesh, name, &vertices, &group_index);
	for (int i = 0; i < mesh->totvert; i++) {
		dst[i] = defvert_find_weight(vertices + i, group_index);
	}
}

static int count_non_zero_indices(float *values, int length)
{
	int amount = 0;
	for (int i = 0; i < length; i++) {
		if (values[i] != 0) amount++;
	}
	return amount;
}

static void get_non_zero_indices(float *values, int length, int **r_indices, int *r_amount)
{
	int amount = count_non_zero_indices(values, length);
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

static void get_non_zero_weight_indices(
        Object *ob, Mesh *mesh, const char *weight_group_name,
        int **r_indices, int *r_amount)
{
	int vertex_amount = mesh->totvert;
	float *weights = MEM_malloc_arrayN(vertex_amount, sizeof(float), __func__);
	get_all_vertex_weights(ob, mesh, weight_group_name, weights);
	get_non_zero_indices(weights, vertex_amount, r_indices, r_amount);
	MEM_freeN(weights);
}

static void get_anchor_indices(
        Object *ob, Mesh *mesh, const char *anchor_group_name,
        int **r_indices, int *r_amount)
{
	get_non_zero_weight_indices(ob, mesh, anchor_group_name, r_indices, r_amount);
}

static BindData *bind_data_calculate(
        RigidDeformModifierData *rdmd, Object *ob, Mesh *mesh, VectorArray vertex_cos)
{
	if (rdmd->anchor_group_name[0] == '\0') {
		modifier_setError(&rdmd->modifier, "No vertex group selected.");
		return NULL;
	}
	if (!vertex_group_exists(ob, mesh, rdmd->anchor_group_name)) {
		modifier_setError(&rdmd->modifier, "Vertex group '%s' does not exist.", rdmd->anchor_group_name);
		return NULL;
	}

	BindData *bind_data = MEM_callocN(sizeof(BindData), __func__);

	int vertex_amount = mesh->totvert;
	bind_data->vertex_amount = vertex_amount;
	bind_data->initial_positions = MEM_malloc_arrayN(vertex_amount, sizeof(float) * 3, __func__);
	memcpy(bind_data->initial_positions, vertex_cos, sizeof(float) * 3 * vertex_amount);

	get_anchor_indices(
	        ob, mesh, rdmd->anchor_group_name,
	        &bind_data->anchor_indices, &bind_data->anchor_amount);

	return bind_data;
}

static void bind_data_free(BindData *bind_data)
{
	MEM_freeN(bind_data->initial_positions);
	MEM_freeN(bind_data->anchor_indices);
	MEM_freeN(bind_data);
}

static void bind_current_mesh_to_modifier(
        RigidDeformModifierData *rdmd,
        RigidDeformModifierData *rdmd_orig,
        Object *ob, Mesh *mesh, VectorArray vertex_cos)
{
	if (rdmd->bind_data) {
		bind_data_free(rdmd->bind_data);
		rdmd->bind_data = NULL;
	}
	if (rdmd->cache) {
		cache_free((Cache * )rdmd->cache);
		rdmd->cache = NULL;
	}

	rdmd_orig->bind_data = bind_data_calculate(rdmd, ob, mesh, vertex_cos);
	rdmd->bind_data = rdmd_orig->bind_data;
}

static void update_bound_anchors(
        RigidDeformModifierData *rdmd,
        Object *ob, Mesh *mesh)
{
	BindData *bind_data = rdmd->bind_data;
	MEM_freeN(bind_data->anchor_indices);
	get_anchor_indices(
	        ob, mesh, rdmd->anchor_group_name,
	        &bind_data->anchor_indices, &bind_data->anchor_amount);

	if (rdmd->cache != NULL) {
		RigidDeformSystem_set_anchors(
			((Cache *)rdmd->cache)->system,
			(uint *)rdmd->bind_data->anchor_indices,
			(uint)rdmd->bind_data->anchor_amount);
	}
}

/* ********** Calculate new positions *********** */

static void deform_vertices(RigidDeformModifierData *rdmd, Mesh *mesh, VectorArray vertex_cos)
{
	Cache *cache = (Cache *)rdmd->cache;

	if (cache->system == NULL) {
		cache->system = RigidDeformSystem_from_mesh(mesh);
		RigidDeformSystem_set_anchors(
			cache->system,
			(uint *)rdmd->bind_data->anchor_indices,
			(uint)rdmd->bind_data->anchor_amount);
	}

	RigidDeformSystem_correct_inner(cache->system, vertex_cos, rdmd->iterations);
}

static RigidDeformModifierData *get_original_modifier_data(
        RigidDeformModifierData *rdmd, const ModifierEvalContext *ctx)
{
	Object *ob_orig = DEG_get_original_object(ctx->object);
	return (RigidDeformModifierData *)modifiers_findByName(ob_orig, rdmd->modifier.name);
}

static void run_modifier(
        RigidDeformModifierData *md, const ModifierEvalContext *ctx,
        Mesh *mesh, VectorArray vertex_cos)
{
	Object *ob = ctx->object;
	RigidDeformModifierData *rdmd = (RigidDeformModifierData *)md;
	RigidDeformModifierData *rdmd_orig = get_original_modifier_data(rdmd, ctx);

	if (rdmd->bind_next_execution) {
		bind_current_mesh_to_modifier(rdmd, rdmd_orig, ob, mesh, vertex_cos);
		rdmd->bind_next_execution = false;
	}

	if (rdmd->update_anchors_next_execution) {
		update_bound_anchors(rdmd, ob, mesh);
		rdmd->update_anchors_next_execution = false;
	}

	if (rdmd->bind_data != NULL) {
		ensure_cache_exists(rdmd, rdmd_orig);
		deform_vertices(rdmd, mesh, vertex_cos);
	}
}

static void deformVerts(
        ModifierData *md, const ModifierEvalContext *ctx,
        Mesh *mesh, VectorArray vertex_cos, int vertex_amount)
{
	Mesh *mesh_src = MOD_deform_mesh_eval_get(ctx->object, NULL, mesh, NULL, vertex_amount, false, false);

	run_modifier((RigidDeformModifierData *)md, ctx, mesh_src, vertex_cos);

	if (mesh_src != mesh) {
		BKE_id_free(NULL, mesh_src);
	}
}

static void deformVertsEM(
        ModifierData *md, const ModifierEvalContext *ctx, struct BMEditMesh *editData,
        Mesh *mesh, VectorArray vertex_cos, int vertex_amount)
{
	Mesh *mesh_src = MOD_deform_mesh_eval_get(ctx->object, editData, mesh, NULL, vertex_amount, false, false);

	run_modifier((RigidDeformModifierData *)md, ctx, mesh_src, vertex_cos);

	if (mesh_src != mesh) {
		BKE_id_free(NULL, mesh_src);
	}
}

static void initData(ModifierData *md)
{
	RigidDeformModifierData *rdmd = (RigidDeformModifierData *)md;
	rdmd->anchor_group_name[0] = '\0';
	rdmd->bind_data = NULL;
	rdmd->bind_next_execution = false;
	rdmd->update_anchors_next_execution = false;
	rdmd->cache = NULL;
	rdmd->iterations = 5;
	rdmd->is_main = true;
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *UNUSED(md))
{
	CustomDataMask dataMask = 0;
	dataMask |= CD_MASK_MDEFORMVERT;
	return dataMask;
}

static void copyData(const ModifierData *md, ModifierData *target, const int flag)
{
	modifier_copyData_generic(md, target, flag);

	RigidDeformModifierData *rdmd_target = (RigidDeformModifierData *)target;
	rdmd_target->is_main = (flag & LIB_ID_CREATE_NO_MAIN) == 0;
}

static void freeData(ModifierData *md)
{
	RigidDeformModifierData *rdmd = (RigidDeformModifierData *)md;
	if (rdmd->is_main) {
		if (rdmd->cache) {
			cache_free((Cache * )rdmd->cache);
		}
		if (rdmd->bind_data) {
			bind_data_free(rdmd->bind_data);
		}
	}
}

 ModifierTypeInfo modifierType_RigidDeform = {
	/* name */              "Rigid Deform",
	/* structName */        "RigidDeformModifierData",
	/* structSize */        sizeof(RigidDeformModifierData),
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
	/* isDisabled */        NULL,
	/* updateDepsgraph */   NULL,
	/* dependsOnTime */     NULL,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     NULL,
	/* foreachTexLink */    NULL,
};