/*
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup bke
 */

#include <iostream>

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_defaults.h"
#include "DNA_scene_types.h"
#include "DNA_simulation_types.h"

#include "BLI_array_ref.hh"
#include "BLI_compiler_compat.h"
#include "BLI_float3.hh"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_anim_data.h"
#include "BKE_animsys.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_lib_query.h"
#include "BKE_lib_remap.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_simulation.h"

#include "NOD_simulation.h"

#include "BLT_translation.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

using BLI::ArrayRef;
using BLI::float3;

static void simulation_init_data(ID *id)
{
  Simulation *simulation = (Simulation *)id;
  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(simulation, id));

  MEMCPY_STRUCT_AFTER(simulation, DNA_struct_default_get(Simulation), id);

  bNodeTree *ntree = ntreeAddTree(nullptr, "Simulation Nodetree", ntreeType_Simulation->idname);
  simulation->nodetree = ntree;
}

static void simulation_copy_data(Main *bmain, ID *id_dst, const ID *id_src, const int flag)
{
  Simulation *simulation_dst = (Simulation *)id_dst;
  Simulation *simulation_src = (Simulation *)id_src;

  /* We always need allocation of our private ID data. */
  const int flag_private_id_data = flag & ~LIB_ID_CREATE_NO_ALLOCATE;

  if (simulation_src->nodetree) {
    BKE_id_copy_ex(bmain,
                   (ID *)simulation_src->nodetree,
                   (ID **)&simulation_dst->nodetree,
                   flag_private_id_data);
  }

  simulation_dst->caches = NULL;
  simulation_dst->tot_caches = 0;
}

static void simulation_free_data(ID *id)
{
  Simulation *simulation = (Simulation *)id;

  BKE_animdata_free(&simulation->id, false);

  if (simulation->nodetree) {
    ntreeFreeEmbeddedTree(simulation->nodetree);
    MEM_freeN(simulation->nodetree);
    simulation->nodetree = nullptr;
  }

  for (SimulationCache *cache : BLI::ref_c_array(simulation->caches, simulation->tot_caches)) {
    switch ((eSimulationCacheType)cache->type) {
      case SIM_CACHE_TYPE_PARTICLES: {
        ParticleSimulationCache *particle_cache = (ParticleSimulationCache *)cache;
        for (ParticleSimulationFrameCache *frame_cache :
             BLI::ref_c_array(particle_cache->frames, particle_cache->tot_frames)) {
          for (SimulationAttributeData *attribute :
               BLI::ref_c_array(frame_cache->attributes, frame_cache->tot_attributes)) {
            MEM_freeN(attribute->data);
            MEM_freeN(attribute);
          }
          MEM_SAFE_FREE(frame_cache->attributes);
          MEM_freeN(frame_cache);
        }
        MEM_SAFE_FREE(particle_cache->frames);
        break;
      }
    }
    MEM_freeN(cache);
  }
  MEM_SAFE_FREE(simulation->caches);
}

static void simulation_foreach_id(ID *id, LibraryForeachIDData *data)
{
  Simulation *simulation = (Simulation *)id;
  if (simulation->nodetree) {
    /* nodetree **are owned by IDs**, treat them as mere sub-data and not real ID! */
    BKE_library_foreach_ID_embedded(data, (ID **)&simulation->nodetree);
  }
}

IDTypeInfo IDType_ID_SIM = {
    /* id_code */ ID_SIM,
    /* id_filter */ FILTER_ID_SIM,
    /* main_listbase_index */ INDEX_ID_SIM,
    /* struct_size */ sizeof(Simulation),
    /* name */ "Simulation",
    /* name_plural */ "simulations",
    /* translation_context */ BLT_I18NCONTEXT_ID_SIMULATION,
    /* flags */ 0,

    /* init_data */ simulation_init_data,
    /* copy_data */ simulation_copy_data,
    /* free_data */ simulation_free_data,
    /* make_local */ nullptr,
    /* foreach_id */ simulation_foreach_id,
};

void *BKE_simulation_add(Main *bmain, const char *name)
{
  Simulation *simulation = (Simulation *)BKE_libblock_alloc(bmain, ID_SIM, name, 0);

  simulation_init_data(&simulation->id);

  return simulation;
}

static ParticleSimulationFrameCache *find_particle_frame_cache(
    ParticleSimulationCache *particle_cache, int frame)
{
  for (int i = 0; i < particle_cache->tot_frames; i++) {
    if (particle_cache->frames[i]->frame == frame) {
      return particle_cache->frames[i];
    }
  }
  return nullptr;
}

static void append_particle_frame_cache(ParticleSimulationCache *particle_cache,
                                        ParticleSimulationFrameCache *frame_cache)
{
  particle_cache->tot_frames++;
  particle_cache->frames = (ParticleSimulationFrameCache **)MEM_reallocN(
      particle_cache->frames, sizeof(ParticleSimulationFrameCache *) * particle_cache->tot_frames);
  particle_cache->frames[particle_cache->tot_frames - 1] = frame_cache;
}

static void append_attribute(ParticleSimulationFrameCache *frame_cache,
                             SimulationAttributeData *attribute)
{
  frame_cache->tot_attributes++;
  frame_cache->attributes = (SimulationAttributeData **)MEM_reallocN(
      frame_cache->attributes, sizeof(SimulationAttributeData *) * frame_cache->tot_attributes);
  frame_cache->attributes[frame_cache->tot_attributes - 1] = attribute;
}

const ParticleSimulationFrameCache *BKE_simulation_try_find_particle_state(Simulation *simulation,
                                                                           int frame)
{
  Simulation *simulation_orig = (Simulation *)DEG_get_original_id(&simulation->id);
  if (simulation_orig->tot_caches == 0) {
    return nullptr;
  }
  ParticleSimulationCache *particle_cache = (ParticleSimulationCache *)simulation_orig->caches[0];
  return find_particle_frame_cache(particle_cache, frame);
}

/**
 * This is not doing anything useful currently. It just fills the cache structure with some
 * particle data that can then be accessed by the simulation point cloud modifier.
 */
void BKE_simulation_data_update(Depsgraph *UNUSED(depsgraph), Scene *scene, Simulation *simulation)
{
  Simulation *simulation_orig = (Simulation *)DEG_get_original_id(&simulation->id);
  int current_frame = scene->r.cfra;

  if (simulation_orig->tot_caches == 0) {
    simulation_orig->tot_caches = 1;
    simulation_orig->caches = (SimulationCache **)MEM_callocN(sizeof(SimulationCache *) * 1,
                                                              __func__);
    simulation_orig->caches[0] = (SimulationCache *)MEM_callocN(sizeof(ParticleSimulationCache),
                                                                __func__);
  }

  ParticleSimulationCache *particle_cache = (ParticleSimulationCache *)simulation_orig->caches[0];
  ParticleSimulationFrameCache *current_frame_cache = find_particle_frame_cache(particle_cache,
                                                                                current_frame);
  if (current_frame_cache != nullptr) {
    return;
  }

  if (current_frame == 1) {
    int initial_particle_count = 100;

    current_frame_cache = (ParticleSimulationFrameCache *)MEM_callocN(
        sizeof(ParticleSimulationFrameCache), __func__);
    current_frame_cache->len = initial_particle_count;
    current_frame_cache->frame = current_frame;

    float3 *positions = (float3 *)MEM_callocN(sizeof(float3) * initial_particle_count, __func__);

    for (int i = 0; i < initial_particle_count; i++) {
      positions[i].x = i / 20.0f;
    }

    SimulationAttributeData *attribute = (SimulationAttributeData *)MEM_callocN(
        sizeof(SimulationAttributeData), __func__);
    strcpy(attribute->name, "Position");
    attribute->type = SIM_ATTRIBUTE_FLOAT3;
    attribute->data = (float *)positions;

    append_attribute(current_frame_cache, attribute);
    append_particle_frame_cache(particle_cache, current_frame_cache);
    return;
  }

  ParticleSimulationFrameCache *prev_frame_cache = find_particle_frame_cache(particle_cache,
                                                                             current_frame - 1);
  if (prev_frame_cache == nullptr) {
    return;
  }

  int particle_count = prev_frame_cache->len;
  current_frame_cache = (ParticleSimulationFrameCache *)MEM_callocN(
      sizeof(ParticleSimulationCache), __func__);
  current_frame_cache->frame = current_frame;
  current_frame_cache->len = particle_count;
  float3 *old_positions = (float3 *)prev_frame_cache->attributes[0]->data;

  float3 *new_positions = (float3 *)MEM_mallocN(sizeof(float3) * particle_count, __func__);
  for (int i = 0; i < particle_count; i++) {
    new_positions[i] = old_positions[i] + float3(0, 0, 0.2f);
  }

  SimulationAttributeData *attribute = (SimulationAttributeData *)MEM_callocN(
      sizeof(SimulationAttributeData), __func__);
  strcpy(attribute->name, "Position");
  attribute->type = SIM_ATTRIBUTE_FLOAT3;
  attribute->data = (float *)new_positions;
  append_attribute(current_frame_cache, attribute);
  append_particle_frame_cache(particle_cache, current_frame_cache);
}
