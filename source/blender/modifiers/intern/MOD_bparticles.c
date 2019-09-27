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
 * The Original Code is Copyright (C) 2019 by the Blender Foundation.
 * All rights reserved.
 * ***** END GPL LICENSE BLOCK *****
 *
 */

/** \file blender/modifiers/intern/MOD_bparticles.c
 *  \ingroup modifiers
 *
 */

#include "MEM_guardedalloc.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_scene.h"
#include "BKE_library_query.h"

#include "BLI_math.h"

#include "MOD_bparticles.h"
#include "MOD_util.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

typedef struct RuntimeData {
  BParticlesSimulationState simulation_state;
  float last_simulated_frame;
} RuntimeData;

static RuntimeData *get_or_create_runtime_struct(BParticlesModifierData *bpmd)
{
  if (bpmd->modifier.runtime == NULL) {
    RuntimeData *runtime = MEM_callocN(sizeof(RuntimeData), __func__);
    runtime->simulation_state = NULL;
    runtime->last_simulated_frame = 0.0f;
    bpmd->modifier.runtime = runtime;
  }

  return bpmd->modifier.runtime;
}

static RuntimeData *get_runtime_struct(BParticlesModifierData *bpmd)
{
  return bpmd->modifier.runtime;
}

static void free_runtime_data(RuntimeData *runtime)
{
  BParticles_simulation_free(runtime->simulation_state);
  MEM_freeN(runtime);
}

static void free_modifier_runtime_data(BParticlesModifierData *bpmd)
{
  RuntimeData *runtime = (RuntimeData *)bpmd->modifier.runtime;
  if (runtime != NULL) {
    free_runtime_data(runtime);
    bpmd->modifier.runtime = NULL;
  }
}

BParticlesSimulationState MOD_bparticles_find_simulation_state(Object *object)
{
  BLI_assert(object != NULL);
  BParticlesModifierData *bpmd = (BParticlesModifierData *)modifiers_findByType(
      object, eModifierType_BParticles);
  if (bpmd == NULL) {
    return NULL;
  }
  RuntimeData *runtime = get_runtime_struct(bpmd);
  if (runtime == NULL) {
    return NULL;
  }
  return runtime->simulation_state;
}

static Mesh *applyModifier(ModifierData *md, const struct ModifierEvalContext *ctx, Mesh *mesh)
{
  BParticlesModifierData *bpmd = (BParticlesModifierData *)md;
  BParticlesModifierData *bpmd_orig = (BParticlesModifierData *)modifier_get_original(
      &bpmd->modifier);

  Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
  float current_frame = BKE_scene_frame_get(scene);

  RuntimeData *runtime = get_or_create_runtime_struct(bpmd);

  if (runtime->simulation_state == NULL) {
    runtime->simulation_state = BParticles_new_simulation();
  }

  if (current_frame == runtime->last_simulated_frame) {
    /* do nothing */
  }
  else if (current_frame == runtime->last_simulated_frame + 1.0f) {
    BParticles_simulate_modifier(bpmd, ctx->depsgraph, runtime->simulation_state, 1.0f / FPS);
    runtime->last_simulated_frame = current_frame;
  }
  else {
    free_modifier_runtime_data(bpmd);
    runtime = get_or_create_runtime_struct(bpmd);
    runtime->simulation_state = BParticles_new_simulation();
    runtime->last_simulated_frame = current_frame;
    BParticles_modifier_free_cache(bpmd_orig);

    BParticles_simulate_modifier(bpmd, ctx->depsgraph, runtime->simulation_state, 0.0f);
    runtime->last_simulated_frame = current_frame;
  }

  if (bpmd->output_type == MOD_BPARTICLES_OUTPUT_POINTS) {
    return BParticles_modifier_point_mesh_from_state(runtime->simulation_state);
  }
  else if (bpmd->output_type == MOD_BPARTICLES_OUTPUT_TETRAHEDONS) {
    Mesh *new_mesh = BParticles_modifier_mesh_from_state(runtime->simulation_state);
    BKE_mesh_copy_settings(new_mesh, mesh);
    return new_mesh;
  }
  else {
    return BKE_mesh_new_nomain(0, 0, 0, 0, 0);
  }
}

static void initData(ModifierData *UNUSED(md))
{
}

static void freeData(ModifierData *md)
{
  BParticlesModifierData *bpmd = (BParticlesModifierData *)md;
  free_modifier_runtime_data(bpmd);
  BParticles_modifier_free_cache(bpmd);
}

static void copyData(const ModifierData *md, ModifierData *target, const int flag)
{
  BParticlesModifierData *tbpmd = (BParticlesModifierData *)target;

  modifier_copyData_generic(md, target, flag);
  tbpmd->num_cached_frames = 0;
  tbpmd->cached_frames = NULL;
}

static void freeRuntimeData(void *runtime_data_v)
{
  if (runtime_data_v == NULL) {
    return;
  }
  RuntimeData *runtime = (RuntimeData *)runtime_data_v;
  free_runtime_data(runtime);
}

static bool dependsOnTime(ModifierData *UNUSED(md))
{
  return true;
}

static void updateDepsgraph(ModifierData *UNUSED(md),
                            const ModifierUpdateDepsgraphContext *UNUSED(ctx))
{
}

static void foreachObjectLink(ModifierData *UNUSED(md),
                              Object *UNUSED(ob),
                              ObjectWalkFunc UNUSED(walk),
                              void *UNUSED(userData))
{
}

static void foreachIDLink(ModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  BParticlesModifierData *bpmd = (BParticlesModifierData *)md;
  walk(userData, ob, (ID **)&bpmd->bparticles_tree, IDWALK_CB_NOP);

  foreachObjectLink(md, ob, (ObjectWalkFunc)walk, userData);
}

ModifierTypeInfo modifierType_BParticles = {
    /* name */ "BParticles",
    /* structName */ "BParticlesModifierData",
    /* structSize */ sizeof(BParticlesModifierData),
    /* type */ eModifierTypeType_Constructive,
    /* flags */ eModifierTypeFlag_AcceptsMesh,
    /* copyData */ copyData,

    /* deformVerts */ NULL,
    /* deformMatrices */ NULL,
    /* deformVertsEM */ NULL,
    /* deformMatricesEM */ NULL,
    /* applyModifier */ applyModifier,

    /* initData */ initData,
    /* requiredDataMask */ NULL,
    /* freeData */ freeData,
    /* isDisabled */ NULL,
    /* updateDepsgraph */ updateDepsgraph,
    /* dependsOnTime */ dependsOnTime,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ foreachObjectLink,
    /* foreachIDLink */ foreachIDLink,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ freeRuntimeData,
};
