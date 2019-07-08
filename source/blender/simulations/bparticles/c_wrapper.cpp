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

    ArrayAllocator::Array<float3> combined_force(interface.array_allocator());
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
using ModifierInserter = EmitterInserter;

static bool is_particle_type_node(bNode *bnode)
{
  return STREQ(bnode->idname, "bp_ParticleTypeNode");
}

static bool is_particle_info_node(bNode *bnode)
{
  return STREQ(bnode->idname, "bp_ParticleInfoNode");
}

static ArrayRef<bNode *> get_particle_type_nodes(IndexedNodeTree &indexed_tree)
{
  return indexed_tree.nodes_with_idname("bp_ParticleTypeNode");
}

static SmallVector<bNodeSocket *> find_input_sockets(IndexedNodeTree &indexed_tree,
                                                     ArrayRef<bNodeSocket *> output_sockets)
{
  /* TODO: this has bad time complexity currently */
  SmallSet<bNodeSocket *> to_be_checked = output_sockets;
  SmallSetVector<bNodeSocket *> found_inputs;

  while (to_be_checked.size() > 0) {
    bNodeSocket *bsocket = to_be_checked.pop();
    if (bsocket->in_out == SOCK_IN) {
      auto linked = indexed_tree.linked(bsocket);
      BLI_assert(linked.size() <= 1);
      if (linked.size() == 1) {
        SocketWithNode origin = linked[0];
        if (is_particle_info_node(origin.node)) {
          found_inputs.add(origin.socket);
        }
        else {
          to_be_checked.add(origin.socket);
        }
      }
    }
    else {
      bNode *bnode = indexed_tree.node_of_socket(bsocket);
      for (bNodeSocket *input : bSocketList(bnode->inputs)) {
        to_be_checked.add(input);
      }
    }
  }

  return found_inputs.to_small_vector();
}

static SharedFunction create_function(IndexedNodeTree &indexed_tree,
                                      FN::DataFlowNodes::GeneratedGraph &data_graph,
                                      ArrayRef<bNodeSocket *> output_bsockets,
                                      StringRef name)
{
  SmallVector<FN::DFGraphSocket> inputs;
  for (bNodeSocket *bsocket : find_input_sockets(indexed_tree, output_bsockets)) {
    inputs.append(data_graph.lookup_socket(bsocket));
  }

  SmallVector<FN::DFGraphSocket> outputs;
  for (bNodeSocket *bsocket : output_bsockets) {
    outputs.append(data_graph.lookup_socket(bsocket));
  }

  FN::FunctionGraph function_graph(data_graph.graph(), inputs, outputs);
  SharedFunction fn = function_graph.new_function(name);
  FN::fgraph_add_TupleCallBody(fn, function_graph);
  return fn;
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
  bSocketList node_inputs(bnode->inputs);

  if (STREQ(bnode->idname, "bp_KillParticleNode")) {
    return ACTION_kill();
  }
  else if (STREQ(bnode->idname, "bp_ChangeParticleDirectionNode")) {
    SharedFunction fn = create_function(
        indexed_tree, data_graph, {node_inputs.get(1)}, "Compute Direction");
    ParticleFunction particle_fn(fn);
    return ACTION_change_direction(particle_fn,
                                   build_action({bSocketList(bnode->outputs).get(0), bnode},
                                                indexed_tree,
                                                data_graph,
                                                step_description));
  }
  else if (STREQ(bnode->idname, "bp_ExplodeParticleNode")) {
    SharedFunction fn = create_function(
        indexed_tree, data_graph, {node_inputs.get(1), node_inputs.get(2)}, bnode->name);
    ParticleFunction particle_fn(fn);

    PointerRNA rna = indexed_tree.get_rna(bnode);
    char name[65];
    RNA_string_get(&rna, "particle_type_name", name);

    Action *post_action = build_action(
        {bSocketList(bnode->outputs).get(0), bnode}, indexed_tree, data_graph, step_description);

    if (step_description.m_types.contains(name)) {
      return ACTION_explode(name, particle_fn, post_action);
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
  bSocketList node_inputs(event_node->inputs);
  FN::SharedFunction fn = create_function(
      indexed_tree, data_graph, {node_inputs.get(1)}, event_node->name);

  for (SocketWithNode linked : indexed_tree.linked(node_inputs.get(0))) {
    if (!is_particle_type_node(linked.node)) {
      continue;
    }

    EventFilter *event_filter = EVENT_age_reached(event_node->name, fn);
    Action *action = build_action({(bNodeSocket *)event_node->outputs.first, event_node},
                                  indexed_tree,
                                  data_graph,
                                  step_description);
    Event *event = new EventFilterWithAction(event_filter, action);

    bNode *type_node = linked.node;
    step_description.m_types.lookup_ref(type_node->name)->m_events.append(event);
  }
}

static void INSERT_EVENT_mesh_collision(bNode *event_node,
                                        IndexedNodeTree &indexed_tree,
                                        FN::DataFlowNodes::GeneratedGraph &data_graph,
                                        ModifierStepDescription &step_description)
{
  BLI_assert(STREQ(event_node->idname, "bp_MeshCollisionEventNode"));
  bSocketList node_inputs(event_node->inputs);

  for (SocketWithNode linked : indexed_tree.linked(node_inputs.get(0))) {
    if (!is_particle_type_node(linked.node)) {
      continue;
    }

    PointerRNA rna = indexed_tree.get_rna(event_node);
    Object *object = (Object *)RNA_pointer_get(&rna, "object").id.data;
    if (object == nullptr || object->type != OB_MESH) {
      continue;
    }

    EventFilter *event_filter = EVENT_mesh_collision(event_node->name, object);
    Action *action = build_action({(bNodeSocket *)event_node->outputs.first, event_node},
                                  indexed_tree,
                                  data_graph,
                                  step_description);
    Event *event = new EventFilterWithAction(event_filter, action);

    bNode *type_node = linked.node;
    step_description.m_types.lookup_ref(type_node->name)->m_events.append(event);
  }
}

static void INSERT_FORCE_gravity(bNode *force_node,
                                 IndexedNodeTree &indexed_tree,
                                 FN::DataFlowNodes::GeneratedGraph &data_graph,
                                 ModifierStepDescription &step_description)
{
  BLI_assert(STREQ(force_node->idname, "bp_GravityForceNode"));
  bSocketList node_inputs(force_node->inputs);
  bSocketList node_outputs(force_node->outputs);

  for (SocketWithNode linked : indexed_tree.linked(node_outputs.get(0))) {
    if (!is_particle_type_node(linked.node)) {
      continue;
    }

    SharedFunction fn = create_function(
        indexed_tree, data_graph, {node_inputs.get(0)}, force_node->name);

    Force *force = FORCE_gravity(fn);

    bNode *type_node = linked.node;
    step_description.m_types.lookup_ref(type_node->name)->m_integrator->m_forces.append(force);
  }
}

static void INSERT_FORCE_turbulence(bNode *force_node,
                                    IndexedNodeTree &indexed_tree,
                                    FN::DataFlowNodes::GeneratedGraph &data_graph,
                                    ModifierStepDescription &step_description)
{
  BLI_assert(STREQ(force_node->idname, "bp_TurbulenceForceNode"));
  bSocketList node_inputs(force_node->inputs);
  bSocketList node_outputs(force_node->outputs);

  for (SocketWithNode linked : indexed_tree.linked(node_outputs.get(0))) {
    if (!is_particle_type_node(linked.node)) {
      continue;
    }

    SharedFunction fn = create_function(
        indexed_tree, data_graph, {node_inputs.get(0)}, force_node->name);

    Force *force = FORCE_turbulence(fn);

    bNode *type_node = linked.node;
    step_description.m_types.lookup_ref(type_node->name)->m_integrator->m_forces.append(force);
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
  event_inserters.add_new("bp_MeshCollisionEventNode", INSERT_EVENT_mesh_collision);

  SmallMap<std::string, ModifierInserter> modifier_inserters;
  event_inserters.add_new("bp_GravityForceNode", INSERT_FORCE_gravity);
  event_inserters.add_new("bp_TurbulenceForceNode", INSERT_FORCE_turbulence);

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

  for (auto item : modifier_inserters.items()) {
    for (bNode *modifier_node : indexed_tree.nodes_with_idname(item.key)) {
      item.value(modifier_node, indexed_tree, generated_graph, *step_description);
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

static Mesh *distribute_tetrahedons(ArrayRef<float3> centers, float scale)
{
  uint amount = centers.size();
  Mesh *mesh = BKE_mesh_new_nomain(amount * ARRAY_SIZE(tetrahedon_vertices),
                                   amount * ARRAY_SIZE(tetrahedon_edges),
                                   0,
                                   amount * ARRAY_SIZE(tetrahedon_loop_vertices),
                                   amount * ARRAY_SIZE(tetrahedon_loop_starts));

  for (uint instance = 0; instance < amount; instance++) {
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
