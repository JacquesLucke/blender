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

/** \file blender/modifiers/intern/MOD_nodeparticles.c
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

#include "MOD_util.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "BParticles.h"

typedef struct RuntimeData {
  BParticlesState state;
  float last_simulated_frame;
} RuntimeData;

static RuntimeData *get_runtime_struct(NodeParticlesModifierData *npmd)
{
  if (npmd->modifier.runtime == NULL) {
    RuntimeData *runtime = MEM_callocN(sizeof(RuntimeData), __func__);
    runtime->state = NULL;
    runtime->last_simulated_frame = 0.0f;
    npmd->modifier.runtime = runtime;
  }

  return npmd->modifier.runtime;
}

static void free_runtime_data(RuntimeData *runtime)
{
  BParticles_state_free(runtime->state);
  MEM_freeN(runtime);
}

static void free_modifier_runtime_data(NodeParticlesModifierData *npmd)
{
  RuntimeData *runtime = (RuntimeData *)npmd->modifier.runtime;
  if (runtime != NULL) {
    free_runtime_data(runtime);
    npmd->modifier.runtime = NULL;
  }
}

static Mesh *point_mesh_from_particle_state(BParticlesState state)
{
  uint point_amount = BParticles_state_particle_count(state);
  Mesh *mesh = BKE_mesh_new_nomain(point_amount, 0, 0, 0, 0);

  float(*positions)[3] = MEM_malloc_arrayN(point_amount, sizeof(float[3]), __func__);
  BParticles_state_get_positions(state, positions);

  for (uint i = 0; i < point_amount; i++) {
    copy_v3_v3(mesh->mvert[i].co, positions[i]);
  }

  MEM_freeN(positions);
  return mesh;
}

static Mesh *tetrahedon_mesh_from_particle_state(BParticlesState state, float scale)
{
  uint point_amount = BParticles_state_particle_count(state);
  Mesh *mesh = BKE_mesh_new_nomain(point_amount * 4, 0, 0, point_amount * 12, point_amount * 4);

  float(*positions)[3] = MEM_malloc_arrayN(point_amount, sizeof(float[3]), __func__);
  BParticles_state_get_positions(state, positions);

  MLoopCol *loop_colors = CustomData_add_layer_named(
      &mesh->ldata, CD_MLOOPCOL, CD_DEFAULT, NULL, mesh->totloop, "test");

  for (uint i = 0; i < mesh->totloop; i++) {
    MLoopCol color = {255, 255, 0, 255};
    memcpy(loop_colors + i, &color, sizeof(MLoopCol));
  }

  for (uint i = 0; i < point_amount; i++) {
    float offset0[3] = {1, -1, -1};
    float offset1[3] = {1, 1, 1};
    float offset2[3] = {-1, -1, 1};
    float offset3[3] = {-1, 1, -1};

    madd_v3_v3v3fl(mesh->mvert[i * 4 + 0].co, positions[i], offset0, scale);
    madd_v3_v3v3fl(mesh->mvert[i * 4 + 1].co, positions[i], offset1, scale);
    madd_v3_v3v3fl(mesh->mvert[i * 4 + 2].co, positions[i], offset2, scale);
    madd_v3_v3v3fl(mesh->mvert[i * 4 + 3].co, positions[i], offset3, scale);

    for (uint j = 0; j < 4; j++) {
      mesh->mpoly[i * 4 + j].loopstart = (i * 4 + j) * 3;
      mesh->mpoly[i * 4 + j].totloop = 3;
    }

    mesh->mloop[i * 12 + 0].v = i * 4 + 0;
    mesh->mloop[i * 12 + 1].v = i * 4 + 1;
    mesh->mloop[i * 12 + 2].v = i * 4 + 2;

    mesh->mloop[i * 12 + 3].v = i * 4 + 0;
    mesh->mloop[i * 12 + 4].v = i * 4 + 3;
    mesh->mloop[i * 12 + 5].v = i * 4 + 1;

    mesh->mloop[i * 12 + 6].v = i * 4 + 0;
    mesh->mloop[i * 12 + 7].v = i * 4 + 2;
    mesh->mloop[i * 12 + 8].v = i * 4 + 3;

    mesh->mloop[i * 12 + 9].v = i * 4 + 1;
    mesh->mloop[i * 12 + 10].v = i * 4 + 2;
    mesh->mloop[i * 12 + 11].v = i * 4 + 3;
  }
  BKE_mesh_calc_edges(mesh, false, false);

  MEM_freeN(positions);

  return mesh;
}

static Mesh *applyModifier(ModifierData *md,
                           const struct ModifierEvalContext *ctx,
                           Mesh *UNUSED(mesh))
{
  NodeParticlesModifierData *npmd = (NodeParticlesModifierData *)md;
  RuntimeData *runtime = get_runtime_struct(npmd);

  if (runtime->state == NULL) {
    runtime->state = BParticles_new_empty_state();
  }

  Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
  float current_frame = BKE_scene_frame_get(scene);

  if (current_frame == runtime->last_simulated_frame) {
    /* do nothing */
  }
  else if (current_frame == runtime->last_simulated_frame + 1.0f) {
    BParticles_simulate_modifier(npmd, ctx->depsgraph, runtime->state);
    runtime->last_simulated_frame = current_frame;
  }
  else {
    free_modifier_runtime_data(npmd);
    runtime = get_runtime_struct(npmd);
    runtime->state = BParticles_new_empty_state();
    runtime->last_simulated_frame = current_frame;
  }

  return point_mesh_from_particle_state(runtime->state);
}

static void initData(ModifierData *UNUSED(md))
{
}

static void freeData(ModifierData *md)
{
  NodeParticlesModifierData *npmd = (NodeParticlesModifierData *)md;
  free_modifier_runtime_data(npmd);
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
  NodeParticlesModifierData *npmd = (NodeParticlesModifierData *)md;
  walk(userData, ob, (ID **)&npmd->bparticles_tree, IDWALK_CB_NOP);

  foreachObjectLink(md, ob, (ObjectWalkFunc)walk, userData);
}

ModifierTypeInfo modifierType_NodeParticles = {
    /* name */ "Node Particles",
    /* structName */ "NodeParticlesModifierData",
    /* structSize */ sizeof(NodeParticlesModifierData),
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
