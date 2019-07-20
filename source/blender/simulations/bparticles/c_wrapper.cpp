#include "BParticles.h"
#include "step_description.hpp"
#include "simulate.hpp"
#include "world_state.hpp"
#include "node_frontend.hpp"

#include "BLI_timeit.hpp"
#include "BLI_task.hpp"
#include "BLI_string.h"

#include "BKE_mesh.h"
#include "BKE_customdata.h"
#include "BKE_node_tree.hpp"

#include "DEG_depsgraph_query.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#define WRAPPERS(T1, T2) \
  inline T1 unwrap(T2 value) \
  { \
    return (T1)value; \
  } \
  inline T2 wrap(T1 value) \
  { \
    return (T2)value; \
  }

using namespace BParticles;

using BKE::bSocketList;
using BKE::IndexedNodeTree;
using BKE::SocketWithNode;
using BLI::ArrayRef;
using BLI::float3;
using BLI::SmallVector;
using BLI::StringRef;

WRAPPERS(ParticlesState *, BParticlesState);

BParticlesState BParticles_new_empty_state()
{
  ParticlesState *state = new ParticlesState();
  return wrap(state);
}

void BParticles_state_free(BParticlesState state_c)
{
  delete unwrap(state_c);
}

WRAPPERS(WorldState *, BParticlesWorldState);

BParticlesWorldState BParticles_new_world_state()
{
  WorldState *world_state = new WorldState();
  return wrap(world_state);
}

void BParticles_world_state_free(BParticlesWorldState world_state_c)
{
  delete unwrap(world_state_c);
}

void BParticles_simulate_modifier(BParticlesModifierData *bpmd,
                                  Depsgraph *UNUSED(depsgraph),
                                  BParticlesState particles_state_c,
                                  BParticlesWorldState world_state_c,
                                  float time_step)
{
  SCOPED_TIMER(__func__);

  if (bpmd->bparticles_tree == NULL) {
    return;
  }

  WorldState &world_state = *unwrap(world_state_c);

  bNodeTree *btree = (bNodeTree *)DEG_get_original_id((ID *)bpmd->bparticles_tree);
  IndexedNodeTree indexed_tree(btree);

  auto step_description = step_description_from_node_tree(indexed_tree, world_state, time_step);

  ParticlesState &particles_state = *unwrap(particles_state_c);
  simulate_step(particles_state, *step_description);
  world_state.current_step_is_over();

  auto &containers = particles_state.particle_containers();
  for (auto item : containers.items()) {
    std::cout << "Particle Type: " << item.key << "\n";
    std::cout << "  Particles: " << item.value->count_active() << "\n";
    std::cout << "  Blocks: " << item.value->active_blocks().size() << "\n";
  }
}

uint BParticles_state_particle_count(BParticlesState state_c)
{
  ParticlesState &state = *unwrap(state_c);

  uint count = 0;
  for (ParticlesContainer *container : state.particle_containers().values()) {
    count += container->count_active();
  }
  return count;
}

void BParticles_state_get_positions(BParticlesState state_c, float (*dst_c)[3])
{
  SCOPED_TIMER(__func__);
  ParticlesState &state = *unwrap(state_c);

  uint index = 0;
  for (ParticlesContainer *container : state.particle_containers().values()) {
    container->flatten_attribute_data("Position", dst_c + index);
    index += container->count_active();
  }
}

static float3 tetrahedon_vertices[4] = {
    {1, -1, -1},
    {1, 1, 1},
    {-1, -1, 1},
    {-1, 1, -1},
};

static uint tetrahedon_loop_starts[4] = {0, 3, 6, 9};
static uint tetrahedon_loop_lengths[4] = {3, 3, 3, 3};
static uint tetrahedon_loop_vertices[12] = {0, 1, 2, 0, 3, 1, 0, 2, 3, 1, 2, 3};
static uint tetrahedon_edges[6][2] = {{0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}};

static void distribute_tetrahedons_range(Mesh *mesh,
                                         ArrayRef<MLoopCol> loop_colors,
                                         Range<uint> range,
                                         ArrayRef<float3> centers,
                                         ArrayRef<float> scales,
                                         ArrayRef<float3> colors)
{
  for (uint instance : range) {
    uint vertex_offset = instance * ARRAY_SIZE(tetrahedon_vertices);
    uint face_offset = instance * ARRAY_SIZE(tetrahedon_loop_starts);
    uint loop_offset = instance * ARRAY_SIZE(tetrahedon_loop_vertices);
    uint edge_offset = instance * ARRAY_SIZE(tetrahedon_edges);

    float3 center = centers[instance];
    for (uint i = 0; i < ARRAY_SIZE(tetrahedon_vertices); i++) {
      copy_v3_v3(mesh->mvert[vertex_offset + i].co,
                 center + tetrahedon_vertices[i] * scales[instance]);
    }

    for (uint i = 0; i < ARRAY_SIZE(tetrahedon_loop_starts); i++) {
      mesh->mpoly[face_offset + i].loopstart = loop_offset + tetrahedon_loop_starts[i];
      mesh->mpoly[face_offset + i].totloop = tetrahedon_loop_lengths[i];
    }

    float3 color_f = colors[instance];
    MLoopCol color_b = {
        (uchar)(color_f.x * 255.0f), (uchar)(color_f.y * 255.0f), (uchar)(color_f.z * 255.0f)};
    for (uint i = 0; i < ARRAY_SIZE(tetrahedon_loop_vertices); i++) {
      mesh->mloop[loop_offset + i].v = vertex_offset + tetrahedon_loop_vertices[i];
      loop_colors[loop_offset + i] = color_b;
    }

    for (uint i = 0; i < ARRAY_SIZE(tetrahedon_edges); i++) {
      mesh->medge[edge_offset + i].v1 = vertex_offset + tetrahedon_edges[i][0];
      mesh->medge[edge_offset + i].v2 = vertex_offset + tetrahedon_edges[i][1];
    }
  }
}

static Mesh *distribute_tetrahedons(ArrayRef<float3> centers,
                                    ArrayRef<float> scales,
                                    ArrayRef<float3> colors)
{
  SCOPED_TIMER(__func__);

  uint amount = centers.size();
  Mesh *mesh = BKE_mesh_new_nomain(amount * ARRAY_SIZE(tetrahedon_vertices),
                                   amount * ARRAY_SIZE(tetrahedon_edges),
                                   0,
                                   amount * ARRAY_SIZE(tetrahedon_loop_vertices),
                                   amount * ARRAY_SIZE(tetrahedon_loop_starts));

  auto loop_colors = ArrayRef<MLoopCol>(
      (MLoopCol *)CustomData_add_layer_named(
          &mesh->ldata, CD_MLOOPCOL, CD_DEFAULT, nullptr, mesh->totloop, "Color"),
      mesh->totloop);

  BLI::Task::parallel_range(Range<uint>(0, amount),
                            1000,
                            [mesh, centers, scales, colors, loop_colors](Range<uint> range) {
                              distribute_tetrahedons_range(
                                  mesh, loop_colors, range, centers, scales, colors);
                            });

  return mesh;
}

void BParticles_modifier_free_cache(BParticlesModifierData *bpmd)
{
  if (bpmd->cached_frames == nullptr) {
    BLI_assert(bpmd->num_cached_frames == 0);
    return;
  }

  for (auto &cached_frame : BLI::ref_c_array(bpmd->cached_frames, bpmd->num_cached_frames)) {
    for (auto &cached_type :
         BLI::ref_c_array(cached_frame.particle_types, cached_frame.num_particle_types)) {
      for (auto &cached_attribute :
           BLI::ref_c_array(cached_type.attributes_float, cached_type.num_attributes_float)) {
        if (cached_attribute.values != nullptr) {
          MEM_freeN(cached_attribute.values);
        }
      }
      if (cached_type.attributes_float != nullptr) {
        MEM_freeN(cached_type.attributes_float);
      }
    }
    if (cached_frame.particle_types != nullptr) {
      MEM_freeN(cached_frame.particle_types);
    }
  }
  MEM_freeN(bpmd->cached_frames);
  bpmd->cached_frames = nullptr;
  bpmd->num_cached_frames = 0;
}

Mesh *BParticles_modifier_point_mesh_from_state(BParticlesState state_c)
{
  SCOPED_TIMER(__func__);

  ParticlesState &state = *unwrap(state_c);

  SmallVector<float3> positions;
  for (ParticlesContainer *container : state.particle_containers().values()) {
    positions.extend(container->flatten_attribute_float3("Position"));
  }

  Mesh *mesh = BKE_mesh_new_nomain(positions.size(), 0, 0, 0, 0);

  for (uint i = 0; i < mesh->totvert; i++) {
    copy_v3_v3(mesh->mvert[i].co, positions[i]);
  }

  return mesh;
}

Mesh *BParticles_modifier_mesh_from_state(BParticlesState state_c)
{
  SCOPED_TIMER(__func__);

  ParticlesState &state = *unwrap(state_c);

  SmallVector<float3> positions;
  SmallVector<float> sizes;
  SmallVector<float3> colors;

  for (ParticlesContainer *container : state.particle_containers().values()) {
    positions.extend(container->flatten_attribute_float3("Position"));
    colors.extend(container->flatten_attribute_float3("Color"));
    sizes.extend(container->flatten_attribute_float("Size"));
  }

  Mesh *mesh = distribute_tetrahedons(positions, sizes, colors);
  return mesh;
}

Mesh *BParticles_modifier_mesh_from_cache(BParticlesFrameCache *cached_frame)
{
  SCOPED_TIMER(__func__);

  SmallVector<float3> positions;
  SmallVector<float> sizes;
  SmallVector<float3> colors;

  for (uint i = 0; i < cached_frame->num_particle_types; i++) {
    BParticlesTypeCache &type = cached_frame->particle_types[i];
    positions.extend(
        ArrayRef<float3>((float3 *)type.attributes_float[0].values, type.particle_amount));
    sizes.extend(ArrayRef<float>(type.attributes_float[1].values, type.particle_amount));
    colors.extend(
        ArrayRef<float3>((float3 *)type.attributes_float[2].values, type.particle_amount));
  }

  Mesh *mesh = distribute_tetrahedons(positions, sizes, colors);
  return mesh;
}

void BParticles_modifier_cache_state(BParticlesModifierData *bpmd,
                                     BParticlesState particles_state_c,
                                     float frame)
{
  ParticlesState &state = *unwrap(particles_state_c);

  SmallVector<std::string> container_names = state.particle_containers().keys();
  SmallVector<ParticlesContainer *> containers = state.particle_containers().values();

  BParticlesFrameCache cached_frame = {0};
  cached_frame.frame = frame;
  cached_frame.num_particle_types = containers.size();
  cached_frame.particle_types = (BParticlesTypeCache *)MEM_calloc_arrayN(
      containers.size(), sizeof(BParticlesTypeCache), __func__);

  for (uint i = 0; i < containers.size(); i++) {
    ParticlesContainer &container = *containers[i];
    BParticlesTypeCache &cached_type = cached_frame.particle_types[i];

    strncpy(cached_type.name, container_names[i].data(), sizeof(cached_type.name));
    cached_type.particle_amount = container.count_active();

    cached_type.num_attributes_float = 3;
    cached_type.attributes_float = (BParticlesAttributeCacheFloat *)MEM_calloc_arrayN(
        cached_type.num_attributes_float, sizeof(BParticlesAttributeCacheFloat), __func__);

    BParticlesAttributeCacheFloat &position_attribute = cached_type.attributes_float[0];
    position_attribute.floats_per_particle = 3;
    strncpy(position_attribute.name, "Position", sizeof(position_attribute.name));
    position_attribute.values = (float *)MEM_malloc_arrayN(
        cached_type.particle_amount, sizeof(float3), __func__);
    container.flatten_attribute_data("Position", position_attribute.values);

    BParticlesAttributeCacheFloat &size_attribute = cached_type.attributes_float[1];
    size_attribute.floats_per_particle = 1;
    strncpy(size_attribute.name, "Size", sizeof(size_attribute.name));
    size_attribute.values = (float *)MEM_malloc_arrayN(
        cached_type.particle_amount, sizeof(float), __func__);
    container.flatten_attribute_data("Size", size_attribute.values);

    BParticlesAttributeCacheFloat &color_attribute = cached_type.attributes_float[2];
    color_attribute.floats_per_particle = 3;
    strncpy(color_attribute.name, "Color", sizeof(color_attribute.name));
    color_attribute.values = (float *)MEM_malloc_arrayN(
        cached_type.particle_amount, sizeof(float3), __func__);
    container.flatten_attribute_data("Color", color_attribute.values);
  }

  bpmd->cached_frames = (BParticlesFrameCache *)MEM_reallocN(
      bpmd->cached_frames, sizeof(BParticlesFrameCache) * (bpmd->num_cached_frames + 1));
  bpmd->cached_frames[bpmd->num_cached_frames] = cached_frame;
  bpmd->num_cached_frames++;
}
