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
#include "BKE_node_tree.hpp"

#include "DEG_depsgraph_query.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_curve_types.h"

#include "RNA_access.h"

#include "FN_tuple_call.hpp"
#include "FN_data_flow_nodes.hpp"

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

class EventFilterWithAction : public Event {
 private:
  EventFilter *m_filter;
  Action *m_action;

 public:
  EventFilterWithAction(EventFilter *filter, Action *action) : m_filter(filter), m_action(action)
  {
  }

  ~EventFilterWithAction()
  {
    delete m_filter;
    delete m_action;
  }

  void filter(EventFilterInterface &interface) override
  {
    m_filter->filter(interface);
  }

  void execute(EventExecuteInterface &interface) override
  {
    m_filter->triggered(interface);
    m_action->execute(interface);
  }

  EventFilter *get_filter() const
  {
    return m_filter;
  }
};

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

    auto last_velocities = block.attributes().get_float3("Velocity");

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

    for (Event *event : m_events) {
      EventFilterWithAction *event_action = dynamic_cast<EventFilterWithAction *>(event);
      BLI_assert(event_action);
      event_action->get_filter()->attributes(interface);
    }
  }
};

class ModifierStepDescription : public StepDescription {
 public:
  float m_duration;
  SmallMap<std::string, ModifierParticleType *> m_types;
  SmallVector<Emitter *> m_emitters;
  SmallVector<std::string> m_particle_type_names;

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

  ArrayRef<std::string> particle_type_names() override
  {
    return m_particle_type_names;
  }

  ParticleType &particle_type(StringRef type_name) override
  {
    return *m_types.lookup(type_name.to_std_string());
  }
};

using EmitterInserter = std::function<void(bNode *bnode,
                                           IndexedNodeTree &indexed_tree,
                                           FN::DataFlowNodes::GeneratedGraph &data_graph,
                                           ModifierStepDescription &step_description)>;
using EventInserter = EmitterInserter;

static bool is_particle_type_node(bNode *bnode)
{
  return STREQ(bnode->idname, "bp_ParticleTypeNode");
}

static ArrayRef<bNode *> get_particle_type_nodes(IndexedNodeTree &indexed_tree)
{
  return indexed_tree.nodes_with_idname("bp_ParticleTypeNode");
}

static Action *build_action(SocketWithNode start,
                            IndexedNodeTree &indexed_tree,
                            FN::DataFlowNodes::GeneratedGraph &data_graph,
                            ModifierStepDescription &step_description)
{
  if (start.socket->in_out == SOCK_OUT) {
    auto linked = indexed_tree.linked(start.socket);
    if (linked.size() == 0) {
      return ACTION_none();
    }
    else if (linked.size() == 1) {
      return build_action(linked[0], indexed_tree, data_graph, step_description);
    }
    else {
      return nullptr;
    }
  }

  BLI_assert(start.socket->in_out == SOCK_IN);
  bNode *bnode = start.node;
  if (STREQ(bnode->idname, "bp_KillParticleNode")) {
    return ACTION_kill();
  }
  else if (STREQ(bnode->idname, "bp_ChangeParticleDirectionNode")) {
    bNodeSocket *direction_socket = bSocketList(bnode->inputs).get(1);

    FN::DFGraphSocket direction_input = data_graph.lookup_socket(direction_socket);
    FN::FunctionGraph function_graph(data_graph.graph(), {}, {direction_input});
    SharedFunction compute_direction_fn = function_graph.new_function("Compute Direction");
    FN::fgraph_add_TupleCallBody(compute_direction_fn, function_graph);

    return ACTION_change_direction(compute_direction_fn,
                                   build_action({bSocketList(bnode->outputs).get(0), bnode},
                                                indexed_tree,
                                                data_graph,
                                                step_description));
  }
  else if (STREQ(bnode->idname, "bp_ExplodeParticleNode")) {
    FN::DFGraphSocket amount_input = data_graph.lookup_socket(bSocketList(bnode->inputs).get(1));
    FN::DFGraphSocket speed_input = data_graph.lookup_socket(bSocketList(bnode->inputs).get(2));
    FN::FunctionGraph function_graph(data_graph.graph(), {}, {amount_input, speed_input});
    SharedFunction compute_amount_fn = function_graph.new_function("Compute Amount");
    FN::fgraph_add_TupleCallBody(compute_amount_fn, function_graph);

    PointerRNA rna = indexed_tree.get_rna(bnode);
    char name[65];
    RNA_string_get(&rna, "particle_type_name", name);

    Action *post_action = build_action(
        {bSocketList(bnode->outputs).get(0), bnode}, indexed_tree, data_graph, step_description);

    if (step_description.m_types.contains(name)) {
      return ACTION_explode(name, compute_amount_fn, post_action);
    }
    else {
      return post_action;
    }
  }
  else {
    return nullptr;
  }
}

static void INSERT_EMITTER_mesh_surface(bNode *emitter_node,
                                        IndexedNodeTree &indexed_tree,
                                        FN::DataFlowNodes::GeneratedGraph &UNUSED(data_graph),
                                        ModifierStepDescription &step_description)
{
  BLI_assert(STREQ(emitter_node->idname, "bp_MeshEmitterNode"));
  bNodeSocket *emitter_output = (bNodeSocket *)emitter_node->outputs.first;
  for (SocketWithNode linked : indexed_tree.linked(emitter_output)) {
    if (!is_particle_type_node(linked.node)) {
      continue;
    }

    bNode *type_node = linked.node;

    PointerRNA rna = indexed_tree.get_rna(emitter_node);

    Object *object = (Object *)RNA_pointer_get(&rna, "object").id.data;
    if (object == nullptr) {
      continue;
    }

    Emitter *emitter = EMITTER_mesh_surface(
        type_node->name, (Mesh *)object->data, object->obmat, object->obmat, 1.0f);
    step_description.m_emitters.append(emitter);
  }
}

static void INSERT_EMITTER_point(bNode *emitter_node,
                                 IndexedNodeTree &indexed_tree,
                                 FN::DataFlowNodes::GeneratedGraph &UNUSED(data_graph),
                                 ModifierStepDescription &step_description)
{
  BLI_assert(STREQ(emitter_node->idname, "bp_PointEmitterNode"));
  bNodeSocket *emitter_output = (bNodeSocket *)emitter_node->outputs.first;

  for (SocketWithNode linked : indexed_tree.linked(emitter_output)) {
    if (!is_particle_type_node(linked.node)) {
      continue;
    }

    bNode *type_node = linked.node;

    PointerRNA rna = indexed_tree.get_rna(emitter_node);

    float3 position;
    RNA_float_get_array(&rna, "position", position);

    Emitter *emitter = EMITTER_point(type_node->name, position);
    step_description.m_emitters.append(emitter);
  }
}

static void INSERT_EVENT_age_reached(bNode *event_node,
                                     IndexedNodeTree &indexed_tree,
                                     FN::DataFlowNodes::GeneratedGraph &data_graph,
                                     ModifierStepDescription &step_description)
{
  BLI_assert(STREQ(event_node->idname, "bp_AgeReachedEventNode"));
  bNodeSocket *event_input = (bNodeSocket *)event_node->inputs.first;

  FN::DFGraphSocket age_input_socket = data_graph.lookup_socket(event_input->next);
  FN::FunctionGraph function_graph(data_graph.graph(), {}, {age_input_socket});
  FN::SharedFunction compute_age_fn = function_graph.new_function("Compute Age");
  FN::fgraph_add_TupleCallBody(compute_age_fn, function_graph);

  for (SocketWithNode linked : indexed_tree.linked(event_input)) {
    if (!is_particle_type_node(linked.node)) {
      continue;
    }

    bNode *type_node = linked.node;

    EventFilter *event_filter = EVENT_age_reached(event_node->name, compute_age_fn);
    Action *action = build_action({(bNodeSocket *)event_node->outputs.first, event_node},
                                  indexed_tree,
                                  data_graph,
                                  step_description);
    Event *event = new EventFilterWithAction(event_filter, action);
    step_description.m_types.lookup_ref(type_node->name)->m_events.append(event);
  }
}

static ModifierStepDescription *step_description_from_node_tree(bNodeTree *btree)
{
  SCOPED_TIMER(__func__);

  SmallMap<std::string, EmitterInserter> emitter_inserters;
  emitter_inserters.add_new("bp_MeshEmitterNode", INSERT_EMITTER_mesh_surface);
  emitter_inserters.add_new("bp_PointEmitterNode", INSERT_EMITTER_point);

  SmallMap<std::string, EventInserter> event_inserters;
  event_inserters.add_new("bp_AgeReachedEventNode", INSERT_EVENT_age_reached);

  ModifierStepDescription *step_description = new ModifierStepDescription();

  IndexedNodeTree indexed_tree(btree);

  auto generated_graph = FN::DataFlowNodes::generate_graph(indexed_tree).value();

  for (bNode *particle_type_node : get_particle_type_nodes(indexed_tree)) {
    ModifierParticleType *type = new ModifierParticleType();
    type->m_integrator = new EulerIntegrator();

    std::string type_name = particle_type_node->name;
    step_description->m_types.add_new(type_name, type);
    step_description->m_particle_type_names.append(type_name);
  }

  for (auto item : emitter_inserters.items()) {
    for (bNode *emitter_node : indexed_tree.nodes_with_idname(item.key)) {
      item.value(emitter_node, indexed_tree, generated_graph, *step_description);
    }
  }

  for (auto item : event_inserters.items()) {
    for (bNode *event_node : indexed_tree.nodes_with_idname(item.key)) {
      item.value(event_node, indexed_tree, generated_graph, *step_description);
    }
  }

  return step_description;
}

void BParticles_simulate_modifier(NodeParticlesModifierData *npmd,
                                  Depsgraph *UNUSED(depsgraph),
                                  BParticlesState state_c)
{
  SCOPED_TIMER(__func__);

  if (npmd->bparticles_tree == NULL) {
    return;
  }

  ModifierStepDescription *step_description = step_description_from_node_tree(
      (bNodeTree *)DEG_get_original_id((ID *)npmd->bparticles_tree));
  step_description->m_duration = 1.0f / 24.0f;

  ParticlesState &state = *unwrap(state_c);
  simulate_step(state, *step_description);

  auto &containers = state.particle_containers();
  for (auto item : containers.items()) {
    std::cout << "Particle Type: " << item.key << "\n";
    std::cout << "  Particles: " << item.value->count_active() << "\n";
    std::cout << "  Blocks: " << item.value->active_blocks().size() << "\n";
  }

  delete step_description;
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
      AttributeArrays attributes = block->attributes();
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
