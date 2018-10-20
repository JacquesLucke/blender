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

#include "MOD_util.h"

#include "eigen_capi.h"


static void LaplacianDeformModifier_do(
        LaplacianDeformModifierData *lmd, Object *ob, Mesh *mesh,
        float (*vertexCos)[3], int numVerts)
{

}

static void initData(ModifierData *md)
{
	LaplacianDeformModifierData *lmd = (LaplacianDeformModifierData *)md;
	lmd->anchor_group_name[0] = '\0';
	lmd->bind_data = NULL;
	lmd->cache = NULL;
}

static void copyData(const ModifierData *md, ModifierData *target, const int flag)
{
	const LaplacianDeformModifierData *lmd = (const LaplacianDeformModifierData *)md;
	LaplacianDeformModifierData *tlmd = (LaplacianDeformModifierData *)target;

	modifier_copyData_generic(md, target, flag);
}

static bool isDisabled(const struct Scene *UNUSED(scene), ModifierData *md, bool UNUSED(useRenderParams))
{
	LaplacianDeformModifierData *lmd = (LaplacianDeformModifierData *)md;
	return false;
}

static CustomDataMask requiredDataMask(Object *UNUSED(ob), ModifierData *md)
{
	LaplacianDeformModifierData *lmd = (LaplacianDeformModifierData *)md;
	CustomDataMask dataMask = 0;
	dataMask |= CD_MASK_MDEFORMVERT;
	return dataMask;
}

static void deformVerts(
        ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh,
        float (*vertexCos)[3], int numVerts)
{
	Mesh *mesh_src = MOD_deform_mesh_eval_get(ctx->object, NULL, mesh, NULL, numVerts, false, false);

	LaplacianDeformModifier_do((LaplacianDeformModifierData *)md, ctx->object, mesh_src, vertexCos, numVerts);

	if (!ELEM(mesh_src, NULL, mesh)) {
		BKE_id_free(NULL, mesh_src);
	}
}

static void deformVertsEM(
        ModifierData *md, const ModifierEvalContext *ctx, struct BMEditMesh *editData,
        Mesh *mesh, float (*vertexCos)[3], int numVerts)
{
	Mesh *mesh_src = MOD_deform_mesh_eval_get(ctx->object, editData, mesh, NULL, numVerts, false, false);

	LaplacianDeformModifier_do((LaplacianDeformModifierData *)md, ctx->object, mesh_src,
	                           vertexCos, numVerts);

	if (!ELEM(mesh_src, NULL, mesh)) {
		BKE_id_free(NULL, mesh_src);
	}
}

static void freeData(ModifierData *md)
{
	LaplacianDeformModifierData *lmd = (LaplacianDeformModifierData *)md;
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
