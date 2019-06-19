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
  BParticlesDescription description;
  BParticlesSolver solver;
  BParticlesState state;
  float last_simulated_frame;
} RuntimeData;

static RuntimeData *get_runtime_data(NodeParticlesModifierData *npmd)
{
  RuntimeData *data = npmd->modifier.runtime;
  BLI_assert(data);
  return data;
}

static BParticlesDescription create_current_description(Object *UNUSED(self),
                                                        NodeParticlesModifierData *npmd,
                                                        Depsgraph *UNUSED(depsgraph))
{
  return BParticles_playground_description(npmd->control1, npmd->control2, npmd->emitter_object);
}

static void ensure_runtime_data(Object *self,
                                NodeParticlesModifierData *npmd,
                                Depsgraph *depsgraph)
{
  if (npmd->modifier.runtime != NULL) {
    return;
  }

  RuntimeData *runtime = MEM_callocN(sizeof(RuntimeData), __func__);

  runtime->description = create_current_description(self, npmd, depsgraph);
  runtime->solver = BParticles_solver_build(runtime->description);
  runtime->state = BParticles_state_init(runtime->solver);
  runtime->last_simulated_frame = 0.0f;

  npmd->modifier.runtime = runtime;
}

static void free_runtime_data(RuntimeData *runtime)
{
  BParticles_state_free(runtime->state);
  BParticles_solver_free(runtime->solver);
  BParticles_description_free(runtime->description);
  MEM_freeN(runtime);
}

static void clear_runtime_data(NodeParticlesModifierData *npmd)
{
  if (npmd->modifier.runtime != NULL) {
    free_runtime_data(npmd->modifier.runtime);
    npmd->modifier.runtime = NULL;
  }
}

static Mesh *applyModifier(ModifierData *md,
                           const struct ModifierEvalContext *ctx,
                           Mesh *UNUSED(mesh))
{
  NodeParticlesModifierData *npmd = (NodeParticlesModifierData *)md;
  ensure_runtime_data(ctx->object, npmd, ctx->depsgraph);
  RuntimeData *runtime = get_runtime_data(npmd);

  Scene *scene = DEG_get_evaluated_scene(ctx->depsgraph);
  float current_frame = BKE_scene_frame_get(scene);
  float fps = FPS;

  if (current_frame != runtime->last_simulated_frame) {
    BParticlesDescription new_description = create_current_description(
        ctx->object, npmd, ctx->depsgraph);
    BParticlesSolver new_solver = BParticles_solver_build(new_description);

    if (current_frame == runtime->last_simulated_frame + 1) {
      BParticles_state_adapt(new_solver, runtime->state);

      BParticles_solver_free(runtime->solver);
      BParticles_description_free(runtime->description);
      runtime->description = new_description;
      runtime->solver = new_solver;

      BParticles_state_step(runtime->solver, runtime->state, 1.0f / fps);
    }
    else {
      BParticles_state_free(runtime->state);
      BParticles_solver_free(runtime->solver);
      BParticles_description_free(runtime->description);

      runtime->description = new_description;
      runtime->solver = new_solver;
      runtime->state = BParticles_state_init(new_solver);
    }
    runtime->last_simulated_frame = current_frame;
  }

  uint point_amount = BParticles_state_particle_count(runtime->solver, runtime->state);
  Mesh *mesh = BKE_mesh_new_nomain(point_amount, 0, 0, 0, 0);

  float(*positions)[3] = MEM_malloc_arrayN(point_amount, sizeof(float[3]), __func__);
  BParticles_state_get_positions(runtime->solver, runtime->state, positions);

  for (uint i = 0; i < point_amount; i++) {
    copy_v3_v3(mesh->mvert[i].co, positions[i]);
  }

  MEM_freeN(positions);

  return mesh;
}

static void initData(ModifierData *UNUSED(md))
{
}

static void freeData(ModifierData *md)
{
  NodeParticlesModifierData *npmd = (NodeParticlesModifierData *)md;
  clear_runtime_data(npmd);
}

static void freeRuntimeData(void *runtime_data_v)
{
  if (runtime_data_v == NULL) {
    return;
  }
  RuntimeData *data = (RuntimeData *)runtime_data_v;
  free_runtime_data(data);
}

static bool dependsOnTime(ModifierData *UNUSED(md))
{
  return true;
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  NodeParticlesModifierData *npmd = (NodeParticlesModifierData *)md;
  if (npmd->emitter_object) {
    DEG_add_object_relation(
        ctx->node, npmd->emitter_object, DEG_OB_COMP_GEOMETRY, "Node Particles Modifier");
  }
}

static void foreachObjectLink(ModifierData *md, Object *ob, ObjectWalkFunc walk, void *userData)
{
  NodeParticlesModifierData *npmd = (NodeParticlesModifierData *)md;
  walk(userData, ob, &npmd->emitter_object, IDWALK_CB_NOP);
}

static void foreachIDLink(ModifierData *md, Object *ob, IDWalkFunc walk, void *userData)
{
  NodeParticlesModifierData *npmd = (NodeParticlesModifierData *)md;
  walk(userData, ob, (ID **)&npmd->emitter_object, IDWALK_CB_NOP);
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
