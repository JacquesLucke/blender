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

/** \file blender/modifiers/intern/MOD_bparticles_output.c
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

#include "BParticles.h"

static Mesh *applyModifier(ModifierData *md,
                           const struct ModifierEvalContext *UNUSED(ctx),
                           Mesh *mesh)
{
  BParticlesOutputModifierData *bpmd = (BParticlesOutputModifierData *)md;
  if (bpmd->source_object == NULL) {
    return mesh;
  }

  BParticlesSimulationState simulation_state = MOD_bparticles_find_simulation_state(
      bpmd->source_object);
  if (simulation_state == NULL) {
    return BKE_mesh_new_nomain(0, 0, 0, 0, 0);
  }

  if (bpmd->output_type == MOD_BPARTICLES_OUTPUT_TETRAHEDONS) {
    return BParticles_state_extract_type__tetrahedons(simulation_state,
                                                      bpmd->source_particle_system);
  }
  else if (bpmd->output_type == MOD_BPARTICLES_OUTPUT_POINTS) {
    return BParticles_state_extract_type__points(simulation_state, bpmd->source_particle_system);
  }
  else {
    return BKE_mesh_new_nomain(0, 0, 0, 0, 0);
  }
}

static void initData(ModifierData *UNUSED(md))
{
}

static void freeData(ModifierData *UNUSED(md))
{
}

static void copyData(const ModifierData *md, ModifierData *target, const int flag)
{
  modifier_copyData_generic(md, target, flag);
}

static void updateDepsgraph(ModifierData *md, const ModifierUpdateDepsgraphContext *ctx)
{
  BParticlesOutputModifierData *bpmd = (BParticlesOutputModifierData *)md;
  if (bpmd->source_object) {
    DEG_add_object_relation(
        ctx->node, bpmd->source_object, DEG_OB_COMP_GEOMETRY, "BParticles Output Modifier");
  }
}

static void foreachObjectLink(ModifierData *md, Object *ob, ObjectWalkFunc walk, void *userData)
{
  BParticlesOutputModifierData *bpmd = (BParticlesOutputModifierData *)md;
  walk(userData, ob, &bpmd->source_object, IDWALK_CB_NOP);
}

ModifierTypeInfo modifierType_BParticlesOutput = {
    /* name */ "BParticles Output",
    /* structName */ "BParticlesOutputModifierData",
    /* structSize */ sizeof(BParticlesOutputModifierData),
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
    /* dependsOnTime */ NULL,
    /* dependsOnNormals */ NULL,
    /* foreachObjectLink */ foreachObjectLink,
    /* foreachIDLink */ NULL,
    /* foreachTexLink */ NULL,
    /* freeRuntimeData */ NULL,
};
