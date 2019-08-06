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

#include "FN_all-c.h"

static FnFunction get_current_function(FunctionPointsModifierData *fpmd)
{
  bNodeTree *tree = (bNodeTree *)DEG_get_original_id((ID *)fpmd->function_tree);

  FnType float_ty = FN_type_borrow_float();
  FnType int32_ty = FN_type_borrow_int32();
  FnType float3_list_ty = FN_type_borrow_float3_list();

  FnType inputs[] = {float_ty, int32_ty, NULL};
  FnType outputs[] = {float3_list_ty, NULL};

  return FN_function_get_with_signature(tree, inputs, outputs);
}

static Mesh *build_point_mesh(FunctionPointsModifierData *fpmd)
{
  FnFunction fn = get_current_function(fpmd);
  if (fn == NULL) {
    modifier_setError(&fpmd->modifier, "Invalid function");
    return BKE_mesh_new_nomain(0, 0, 0, 0, 0);
  }

  FnTupleCallBody body = FN_tuple_call_get(fn);
  FN_TUPLE_CALL_PREPARE_STACK(body, fn_in, fn_out);

  FN_tuple_set_float(fn_in, 0, fpmd->control1);
  FN_tuple_set_int32(fn_in, 1, fpmd->control2);
  FN_tuple_call_invoke(body, fn_in, fn_out, __func__);
  FnList list = FN_tuple_relocate_out_list(fn_out, 0);

  FN_TUPLE_CALL_DESTRUCT_STACK(body, fn_in, fn_out);
  FN_function_free(fn);

  uint amount = FN_list_size(list);
  float *ptr = (float *)FN_list_storage(list);

  Mesh *mesh = BKE_mesh_new_nomain(amount, 0, 0, 0, 0);
  for (uint i = 0; i < amount; i++) {
    copy_v3_v3(mesh->mvert[i].co, ptr + (3 * i));
  }
  FN_list_free(list);

  return mesh;
}

static Mesh *applyModifier(ModifierData *md,
                           const struct ModifierEvalContext *UNUSED(ctx),
                           struct Mesh *UNUSED(mesh))
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

static void foreachIDLink(ModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  FunctionPointsModifierData *fpmd = (FunctionPointsModifierData *)md;

  walk(userData, ob, (ID **)&fpmd->function_tree, IDWALK_CB_USER);
}

ModifierTypeInfo modifierType_FunctionPoints = {
    /* name */ "Function Points",
    /* structName */ "FunctionPointsModifierData",
    /* structSize */ sizeof(FunctionPointsModifierData),
    /* type */ eModifierTypeType_Constructive,
    /* flags */ eModifierTypeFlag_AcceptsMesh,
    /* copyData */ modifier_copyData_generic,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* applyModifier */ applyModifier,

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
