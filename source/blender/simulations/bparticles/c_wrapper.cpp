#include "BParticles.h"
#include "core.hpp"
#include "particles_container.hpp"
#include "emitters.hpp"
#include "forces.hpp"
#include "events.hpp"
#include "actions.hpp"
#include "simulate.hpp"

#include "BLI_timeit.hpp"
#include "BLI_listbase.h"

#include "BKE_curve.h"
#include "BKE_bvhutils.h"
#include "BKE_mesh.h"
#include "BKE_customdata.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_curve_types.h"

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

using BLI::ArrayRef;
using BLI::float3;
using BLI::SmallVector;
using BLI::StringRef;

WRAPPERS(ParticlesState *, BParticlesState);

/* New Functions
 *********************************************************/

BParticlesState BParticles_new_empty_state()
{
  ParticlesState *state = new ParticlesState();
  return wrap(state);
}

void BParticles_state_free(BParticlesState state)
{
  delete unwrap(state);
}

class EulerIntegrator : public Integrator {
 private:
  AttributesInfo m_offset_attributes_info;

 public:
  SmallVector<Force *> m_forces;

  EulerIntegrator() : m_offset_attributes_info({}, {}, {"Position", "Velocity"})
  {
  }

  ~EulerIntegrator()
  {
    for (Force *force : m_forces) {
      delete force;
    }
  }

  AttributesInfo &offset_attributes_info() override
  {
    return m_offset_attributes_info;
  }

  void integrate(IntegratorInterface &interface) override
  {
    ParticlesBlock &block = interface.block();
    AttributeArrays r_offsets = interface.offset_targets();
    ArrayRef<float> durations = interface.durations();

    uint amount = block.active_amount();
    BLI_assert(amount == r_offsets.size());

    SmallVector<float3> combined_force(amount);
    this->compute_combined_force(block, combined_force);

    auto last_velocities = block.slice_active().get_float3("Velocity");

    auto position_offsets = r_offsets.get_float3("Position");
    auto velocity_offsets = r_offsets.get_float3("Velocity");
    this->compute_offsets(
        durations, last_velocities, combined_force, position_offsets, velocity_offsets);
  }

  BLI_NOINLINE void compute_combined_force(ParticlesBlock &block, ArrayRef<float3> r_force)
  {
    r_force.fill({0, 0, 0});

    for (Force *force : m_forces) {
      force->add_force(block, r_force);
    }
  }

  BLI_NOINLINE void compute_offsets(ArrayRef<float> durations,
                                    ArrayRef<float3> last_velocities,
                                    ArrayRef<float3> combined_force,
                                    ArrayRef<float3> r_position_offsets,
                                    ArrayRef<float3> r_velocity_offsets)
  {
    uint amount = durations.size();
    for (uint pindex = 0; pindex < amount; pindex++) {
      float mass = 1.0f;
      float duration = durations[pindex];

      r_velocity_offsets[pindex] = duration * combined_force[pindex] / mass;
      r_position_offsets[pindex] = duration *
                                   (last_velocities[pindex] + r_velocity_offsets[pindex] * 0.5f);
    }
  }
};

class EventActionTest : public Event {
 public:
  EventFilter *m_event;
  Action *m_action;

  EventActionTest(EventFilter *event, Action *action) : m_event(event), m_action(action)
  {
  }

  ~EventActionTest()
  {
    delete m_event;
    delete m_action;
  }

  void filter(EventFilterInterface &interface) override
  {
    m_event->filter(interface);
  }

  void execute(EventExecuteInterface &interface) override
  {
    m_action->execute(interface);
  }
};

class ModifierParticleType : public ParticleType {
 public:
  SmallVector<Event *> m_events;
  EulerIntegrator *m_integrator;

  ~ModifierParticleType()
  {
    delete m_integrator;

    for (Event *event : m_events) {
      delete event;
    }
  }

  ArrayRef<Event *> events() override
  {
    return m_events;
  }

  Integrator &integrator() override
  {
    return *m_integrator;
  }

  void attributes(TypeAttributeInterface &interface) override
  {
    interface.use(AttributeType::Float3, "Position");
    interface.use(AttributeType::Float3, "Velocity");
  }
};

class ModifierStepDescription : public StepDescription {
 public:
  float m_duration;
  SmallMap<uint, ModifierParticleType *> m_types;
  SmallVector<Emitter *> m_emitters;

  ~ModifierStepDescription()
  {
    for (auto *type : m_types.values()) {
      delete type;
    }
    for (Emitter *emitter : m_emitters) {
      delete emitter;
    }
  }

  float step_duration() override
  {
    return m_duration;
  }

  ArrayRef<Emitter *> emitters() override
  {
    return m_emitters;
  }

  ArrayRef<uint> particle_type_ids() override
  {
    return {0, 1};
  }

  ParticleType &particle_type(uint type_id) override
  {
    return *m_types.lookup(type_id);
  }
};

void BParticles_simulate_modifier(NodeParticlesModifierData *npmd,
                                  Depsgraph *UNUSED(depsgraph),
                                  BParticlesState state_c)
{
  SCOPED_TIMER_STATS("simulate");

  ParticlesState &state = *unwrap(state_c);
  ModifierStepDescription description;
  description.m_duration = 1.0f / 24.0f;

  auto *type0 = new ModifierParticleType();
  description.m_types.add_new(0, type0);
  type0->m_integrator = new EulerIntegrator();
  type0->m_integrator->m_forces.append(FORCE_directional({0, 0, -2}));

  if (npmd->emitter_object) {
    description.m_emitters.append(EMITTER_mesh_surface(
        0, (Mesh *)npmd->emitter_object->data, npmd->emitter_object->obmat, npmd->control1));
  }
  BVHTreeFromMesh treedata = {0};
  if (npmd->collision_object) {
    BKE_bvhtree_from_mesh_get(
        &treedata, (Mesh *)npmd->collision_object->data, BVHTREE_FROM_LOOPTRI, 4);
    type0->m_events.append(EVENT_mesh_bounce(&treedata, npmd->collision_object->obmat));
  }

  auto *type1 = new ModifierParticleType();
  description.m_types.add_new(1, type1);
  type1->m_integrator = new EulerIntegrator();
  type1->m_events.append(new EventActionTest(EVENT_age_reached(0.3f), ACTION_kill()));

  simulate_step(state, description);

  if (npmd->collision_object) {
    free_bvhtree_from_mesh(&treedata);
  }

  auto &containers = state.particle_containers();
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
  ParticlesState &state = *unwrap(state_c);
  float3 *dst = (float3 *)dst_c;

  uint index = 0;
  for (ParticlesContainer *container : state.particle_containers().values()) {
    for (auto *block : container->active_blocks()) {
      auto positions = block->slice_active().get_float3("Position");
      positions.copy_to(dst + index);
      index += positions.size();
    }
  }
}

static inline void append_tetrahedon_mesh_data(float3 position,
                                               float scale,
                                               MLoopCol color,
                                               SmallVector<float3> &vertex_positions,
                                               SmallVector<uint> &poly_starts,
                                               SmallVector<uint> &poly_lengths,
                                               SmallVector<uint> &loops,
                                               SmallVector<MLoopCol> &loop_colors)
{
  uint vertex_offset = vertex_positions.size();

  vertex_positions.append(position + scale * float3(1, -1, -1));
  vertex_positions.append(position + scale * float3(1, 1, 1));
  vertex_positions.append(position + scale * float3(-1, -1, 1));
  vertex_positions.append(position + scale * float3(-1, 1, -1));

  poly_lengths.append_n_times(3, 4);

  poly_starts.append(loops.size());
  loops.extend({vertex_offset + 0, vertex_offset + 1, vertex_offset + 2});
  poly_starts.append(loops.size());
  loops.extend({vertex_offset + 0, vertex_offset + 3, vertex_offset + 1});
  poly_starts.append(loops.size());
  loops.extend({vertex_offset + 0, vertex_offset + 2, vertex_offset + 3});
  poly_starts.append(loops.size());
  loops.extend({vertex_offset + 1, vertex_offset + 2, vertex_offset + 3});

  loop_colors.append_n_times(color, 12);
}

Mesh *BParticles_test_mesh_from_state(BParticlesState state_c)
{
  ParticlesState &state = *unwrap(state_c);

  SmallVector<float3> vertex_positions;
  SmallVector<uint> poly_starts;
  SmallVector<uint> poly_lengths;
  SmallVector<uint> loops;
  SmallVector<MLoopCol> loop_colors;

  SmallVector<MLoopCol> colors_to_use = {
      {230, 30, 30, 255}, {30, 230, 30, 255}, {30, 30, 230, 255}};

  uint type_index = 0;
  for (ParticlesContainer *container : state.particle_containers().values()) {
    for (ParticlesBlock *block : container->active_blocks()) {
      AttributeArrays attributes = block->slice_active();
      auto positions = attributes.get_float3("Position");

      for (uint pindex = 0; pindex < attributes.size(); pindex++) {
        append_tetrahedon_mesh_data(positions[pindex],
                                    0.03f,
                                    colors_to_use[type_index],
                                    vertex_positions,
                                    poly_starts,
                                    poly_lengths,
                                    loops,
                                    loop_colors);
      }
    }
    type_index++;
  }

  Mesh *mesh = BKE_mesh_new_nomain(
      vertex_positions.size(), 0, 0, loops.size(), poly_starts.size());

  for (uint i = 0; i < vertex_positions.size(); i++) {
    copy_v3_v3(mesh->mvert[i].co, vertex_positions[i]);
  }

  for (uint i = 0; i < poly_starts.size(); i++) {
    mesh->mpoly[i].loopstart = poly_starts[i];
    mesh->mpoly[i].totloop = poly_lengths[i];
  }

  for (uint i = 0; i < loops.size(); i++) {
    mesh->mloop[i].v = loops[i];
  }

  MLoopCol *mesh_loop_colors = (MLoopCol *)CustomData_add_layer_named(
      &mesh->ldata, CD_MLOOPCOL, CD_DEFAULT, nullptr, mesh->totloop, "test");

  for (uint i = 0; i < loop_colors.size(); i++) {
    mesh_loop_colors[i] = loop_colors[i];
  }

  BKE_mesh_calc_edges(mesh, false, false);
  return mesh;
}
