#include "BParticles.h"
#include "core.hpp"
#include "simulate.hpp"
#include "world_state.hpp"
#include "node_frontend.hpp"

#include "BLI_timeit.hpp"
#include "BLI_task.hpp"

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

void BParticles_simulate_modifier(NodeParticlesModifierData *npmd,
                                  Depsgraph *UNUSED(depsgraph),
                                  BParticlesState particles_state_c,
                                  BParticlesWorldState world_state_c)
{
  SCOPED_TIMER(__func__);

  if (npmd->bparticles_tree == NULL) {
    return;
  }

  WorldState &world_state = *unwrap(world_state_c);

  bNodeTree *btree = (bNodeTree *)DEG_get_original_id((ID *)npmd->bparticles_tree);
  IndexedNodeTree indexed_tree(btree);

  auto step_description = step_description_from_node_tree(indexed_tree, world_state, 1.0f / 24.0f);

  ParticlesState &particles_state = *unwrap(particles_state_c);
  simulate_step(particles_state, *step_description);

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
                                         Range<uint> range,
                                         ArrayRef<float3> centers,
                                         float scale)
{
  for (uint instance : range) {
    uint vertex_offset = instance * ARRAY_SIZE(tetrahedon_vertices);
    uint face_offset = instance * ARRAY_SIZE(tetrahedon_loop_starts);
    uint loop_offset = instance * ARRAY_SIZE(tetrahedon_loop_vertices);
    uint edge_offset = instance * ARRAY_SIZE(tetrahedon_edges);

    float3 center = centers[instance];
    for (uint i = 0; i < ARRAY_SIZE(tetrahedon_vertices); i++) {
      copy_v3_v3(mesh->mvert[vertex_offset + i].co, center + tetrahedon_vertices[i] * scale);
    }

    for (uint i = 0; i < ARRAY_SIZE(tetrahedon_loop_starts); i++) {
      mesh->mpoly[face_offset + i].loopstart = loop_offset + tetrahedon_loop_starts[i];
      mesh->mpoly[face_offset + i].totloop = tetrahedon_loop_lengths[i];
    }

    for (uint i = 0; i < ARRAY_SIZE(tetrahedon_loop_vertices); i++) {
      mesh->mloop[loop_offset + i].v = vertex_offset + tetrahedon_loop_vertices[i];
    }

    for (uint i = 0; i < ARRAY_SIZE(tetrahedon_edges); i++) {
      mesh->medge[edge_offset + i].v1 = vertex_offset + tetrahedon_edges[i][0];
      mesh->medge[edge_offset + i].v2 = vertex_offset + tetrahedon_edges[i][1];
    }
  }
}

static Mesh *distribute_tetrahedons(ArrayRef<float3> centers, float scale)
{
  uint amount = centers.size();
  Mesh *mesh = BKE_mesh_new_nomain(amount * ARRAY_SIZE(tetrahedon_vertices),
                                   amount * ARRAY_SIZE(tetrahedon_edges),
                                   0,
                                   amount * ARRAY_SIZE(tetrahedon_loop_vertices),
                                   amount * ARRAY_SIZE(tetrahedon_loop_starts));

  BLI::Task::parallel_range(
      Range<uint>(0, amount), 1000, [mesh, centers, scale](Range<uint> range) {
        distribute_tetrahedons_range(mesh, range, centers, scale);
      });

  return mesh;
}

Mesh *BParticles_test_mesh_from_state(BParticlesState state_c)
{
  SCOPED_TIMER(__func__);

  ParticlesState &state = *unwrap(state_c);

  SmallVector<ParticlesContainer *> containers = state.particle_containers().values();

  SmallVector<float3> positions;
  SmallVector<uint> particle_counts;
  for (ParticlesContainer *container : containers) {
    SmallVector<float3> positions_in_container = container->flatten_attribute_float3("Position");
    particle_counts.append(positions_in_container.size());
    positions.extend(positions_in_container);
  }

  Mesh *mesh = distribute_tetrahedons(positions, 0.025f);
  if (positions.size() == 0) {
    return mesh;
  }

  uint loops_per_particle = mesh->totloop / positions.size();

  SmallVector<MLoopCol> colors_to_use = {
      {230, 30, 30, 255}, {30, 230, 30, 255}, {30, 30, 230, 255}};

  MLoopCol *loop_colors = (MLoopCol *)CustomData_add_layer_named(
      &mesh->ldata, CD_MLOOPCOL, CD_DEFAULT, nullptr, mesh->totloop, "Color");
  uint loop_offset = 0;
  for (uint i = 0; i < containers.size(); i++) {
    uint loop_count = particle_counts[i] * loops_per_particle;
    MLoopCol color = colors_to_use[i];
    for (uint j = 0; j < loop_count; j++) {
      loop_colors[loop_offset + j] = color;
    }
    loop_offset += loop_count;
  }

  return mesh;
}
