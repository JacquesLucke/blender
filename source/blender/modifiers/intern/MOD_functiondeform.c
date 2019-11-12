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
#include "DNA_node_types.h"

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

void MOD_functiondeform_do(FunctionDeformModifierData *fdmd,
                           float (*vertexCos)[3],
                           int numVerts,
                           const ModifierEvalContext *ctx,
                           Mesh *mesh);

static void deformVerts(ModifierData *md,
                        const ModifierEvalContext *ctx,
                        Mesh *mesh,
                        float (*vertexCos)[3],
                        int numVerts)
{
  MOD_functiondeform_do((FunctionDeformModifierData *)md, vertexCos, numVerts, ctx, mesh);
}

static void deformVertsEM(ModifierData *md,
                          const ModifierEvalContext *ctx,
                          struct BMEditMesh *UNUSED(em),
                          Mesh *mesh,
                          float (*vertexCos)[3],
                          int numVerts)
{
  MOD_functiondeform_do((FunctionDeformModifierData *)md, vertexCos, numVerts, ctx, mesh);
}

static void initData(ModifierData *md)
{
  FunctionDeformModifierData *fdmd = (FunctionDeformModifierData *)md;
  fdmd->control1 = 1.0f;
  fdmd->control2 = 0;
}

static bool dependsOnTime(ModifierData *UNUSED(md))
{
  return true;
}

static void updateDepsgraph(ModifierData *UNUSED(md),
                            const ModifierUpdateDepsgraphContext *UNUSED(ctx))
{
}

static void foreachIDLink(ModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  FunctionDeformModifierData *fdmd = (FunctionDeformModifierData *)md;

  walk(userData, ob, (ID **)&fdmd->function_tree, IDWALK_CB_USER);
}

ModifierTypeInfo modifierType_FunctionDeform = {
    /* name */ "Function Deform",
    /* structName */ "FunctionDeformModifierData",
    /* structSize */ sizeof(FunctionDeformModifierData),
    /* type */ eModifierTypeType_OnlyDeform,
    /* flags */ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_SupportsEditmode,
    /* copyData */ modifier_copyData_generic,

    /* deformVerts */ deformVerts,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ deformVertsEM,
    /* deformMatricesEM */ NULL,
    /* applyModifier */ NULL,

    /* initData */ initData,
    /* requiredDataMask */ NULL,
    /* freeData */ NULL,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ dependsOnTime,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ NULL,
    /* foreachIDLink */ foreachIDLink,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
};
