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

/** \file blender/modifiers/intern/MOD_functiondeform.c
 *  \ingroup modifiers
 *
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_scene.h"
#include "BKE_library_query.h"

#include "BKE_global.h"
#include "BKE_main.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "MOD_util.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"
#include "time.h"

#include "FN-C.h"

static FnFunction get_current_function(FunctionPointsModifierData *fpmd)
{
	bNodeTree *tree = (bNodeTree *)DEG_get_original_id((ID *)fpmd->function_tree);

	FnType float_ty = FN_type_borrow_float();
	FnType int32_ty = FN_type_borrow_int32();
	FnType fvec3_ty = FN_type_borrow_fvec3();

    FnType float_list_ty = FN_type_borrow_float_list();

	FnType inputs[] = { int32_ty, NULL };
	FnType outputs[] = { float_list_ty, NULL };

	return FN_function_get_with_signature(tree, inputs, outputs);
}

static Mesh *build_point_mesh(FunctionPointsModifierData *fpmd)
{
	Mesh *mesh = BKE_mesh_new_nomain(2, 0, 0, 0, 0);
	float vec1[] = {4, 6, 3};
	float vec2[] = {1, 2, 3};
	copy_v3_v3(mesh->mvert + 0, vec1);
	copy_v3_v3(mesh->mvert + 1, vec2);
	return mesh;
}

static Mesh *applyModifier(
        ModifierData *md,
        const struct ModifierEvalContext *ctx,
        struct Mesh *mesh)
{
	return build_point_mesh((FunctionPointsModifierData *)md);
}

static void initData(ModifierData *md)
{
	FunctionPointsModifierData *fpmd = (FunctionPointsModifierData *)md;
	fpmd->control1 = 1.0f;
	fpmd->control2 = 0;
}

static bool dependsOnTime(ModifierData *UNUSED(md))
{
	return true;
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
	FunctionPointsModifierData *fpmd = (FunctionPointsModifierData *)md;

	FnFunction fn = get_current_function(fpmd);
	if (fn) {
		FN_function_update_dependencies(fn, ctx->node);
		FN_function_free(fn);
	}
}

static void foreachIDLink(
        ModifierData *md, Object *ob,
        IDWalkFunc walk, void *userData)
{
	FunctionPointsModifierData *fpmd = (FunctionPointsModifierData *)md;

	walk(userData, ob, (ID **)&fpmd->function_tree, IDWALK_CB_USER);
}


ModifierTypeInfo modifierType_FunctionPoints = {
	/* name */              "Function Points",
	/* structName */        "FunctionPointsModifierData",
	/* structSize */        sizeof(FunctionPointsModifierData),
	/* type */              eModifierTypeType_Constructive,
	/* flags */             eModifierTypeFlag_AcceptsMesh,
	/* copyData */          modifier_copyData_generic,

	/* PointsVerts_DM */    NULL,
	/* deformMatrices_DM */ NULL,
	/* deformVertsEM_DM */  NULL,
	/* deformMatricesEM_DM*/NULL,
	/* applyModifier_DM */  NULL,

	/* deformVerts */       NULL,
	/* deformMatrices */    NULL,
	/* deformVertsEM */     NULL,
	/* deformMatricesEM */  NULL,
	/* applyModifier */     applyModifier,

	/* initData */          initData,
	/* requiredDataMask */  NULL,
	/* freeData */          NULL,
	/* isDisabled */        NULL,
	/* updateDepsgraph */   updateDepsgraph,
	/* dependsOnTime */     dependsOnTime,
	/* dependsOnNormals */	NULL,
	/* foreachObjectLink */ NULL,
	/* foreachIDLink */     foreachIDLink,
	/* foreachTexLink */    NULL,
};