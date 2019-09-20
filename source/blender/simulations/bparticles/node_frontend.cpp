#include "BKE_node_tree.hpp"
#include "BKE_deform.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_timeit.h"
#include "BLI_multi_map.h"
#include "BLI_lazy_init_cxx.h"

#include "node_frontend.hpp"
#include "integrator.hpp"
#include "particle_function_builder.hpp"
#include "emitters.hpp"
#include "events.hpp"
#include "offset_handlers.hpp"
#include "simulate.hpp"

namespace BParticles {

using BKE::VirtualNode;
using BKE::VirtualSocket;
using BLI::MultiMap;
using BLI::rgba_f;
using BLI::ValueOrError;
using FN::Function;
using FN::FunctionBuilder;
using FN::FunctionGraph;
using FN::NamedTupleRef;
using FN::SharedDataGraph;
using FN::DataFlowNodes::VTreeDataGraph;
using FN::Types::FalloffW;
using FN::Types::ObjectW;
using FN::Types::StringW;

static StringRef particle_system_idname = "bp_ParticleSystemNode";
static StringRef combine_influences_idname = "bp_CombineInfluencesNode";

class VTreeData;
using ActionParserCallback =
    std::function<std::unique_ptr<Action>(VTreeData &vtree_data, VirtualSocket *execute_vsocket)>;
StringMap<ActionParserCallback> &get_action_parsers();

class InfluencesCollector {
 public:
  Vector<Emitter *> &m_emitters;
  MultiMap<std::string, Force *> &m_forces;
  MultiMap<std::string, Event *> &m_events;
  MultiMap<std::string, OffsetHandler *> &m_offset_handlers;
};

class VTreeData {
 private:
  VTreeDataGraph &m_vtree_data_graph;
  Vector<std::unique_ptr<ParticleFunction>> m_particle_functions;
  Vector<SharedFunction> m_functions;
  Vector<std::unique_ptr<Tuple>> m_tuples;
  Vector<std::unique_ptr<FN::FunctionOutputNamesProvider>> m_name_providers;
  Vector<std::unique_ptr<Vector<std::string>>> m_string_vectors;
  Vector<std::unique_ptr<Action>> m_actions;

 public:
  VTreeData(VTreeDataGraph &vtree_data) : m_vtree_data_graph(vtree_data)
  {
  }

  VirtualNodeTree &vtree()
  {
    return m_vtree_data_graph.vtree();
  }

  SharedDataGraph &data_graph()
  {
    return m_vtree_data_graph.graph();
  }

  VTreeDataGraph &vtree_data_graph()
  {
    return m_vtree_data_graph;
  }

  ParticleFunction *particle_function_for_all_inputs(VirtualNode *vnode)
  {
    auto fn_or_error = create_particle_function(vnode, m_vtree_data_graph);
    if (fn_or_error.is_error()) {
      return {};
    }
    std::unique_ptr<ParticleFunction> fn = fn_or_error.extract_value();
    ParticleFunction *fn_ptr = fn.get();
    BLI_assert(fn_ptr != nullptr);
    m_particle_functions.append(std::move(fn));
    return fn_ptr;
  }

  Optional<NamedTupleRef> compute_inputs(VirtualNode *vnode, ArrayRef<uint> input_indices)
  {
    TupleCallBody *body_ptr = this->function_body_for_inputs(vnode, input_indices);
    if (body_ptr == nullptr) {
      return {};
    }
    TupleCallBody &body = *body_ptr;

    FN_TUPLE_STACK_ALLOC(fn_in, body.meta_in().ref());
    FN::Tuple *fn_out = new FN::Tuple(body.meta_out());

    body.call__setup_execution_context(fn_in, *fn_out);
    auto *name_provider = new FN::FunctionOutputNamesProvider(body.owner());

    m_tuples.append(std::unique_ptr<FN::Tuple>(fn_out));
    m_name_providers.append(std::unique_ptr<FN::FunctionOutputNamesProvider>(name_provider));

    return NamedTupleRef(fn_out, name_provider);
  }

  Optional<NamedTupleRef> compute_all_data_inputs(VirtualNode *vnode)
  {
    Vector<uint> data_input_indices;
    for (uint i = 0; i < vnode->inputs().size(); i++) {
      if (m_vtree_data_graph.uses_socket(vnode->input(i))) {
        data_input_indices.append(i);
      }
    }

    return this->compute_inputs(vnode, data_input_indices);
  }

  ArrayRef<std::string> find_target_system_names(VirtualSocket *output_vsocket)
  {
    Vector<std::string> *system_names = new Vector<std::string>();
    for (VirtualNode *vnode : find_target_system_nodes(output_vsocket)) {
      system_names->append(vnode->name());
    }
    m_string_vectors.append(std::unique_ptr<Vector<std::string>>(system_names));
    return *system_names;
  }

  Action *build_action(VirtualSocket *start)
  {
    BLI_assert(start->is_input());
    if (start->links().size() != 1) {
      return nullptr;
    }

    VirtualSocket *execute_socket = start->links()[0];
    if (execute_socket->idname() != "bp_ExecuteSocket") {
      return nullptr;
    }

    StringMap<ActionParserCallback> &parsers = get_action_parsers();
    ActionParserCallback &parser = parsers.lookup(execute_socket->vnode()->idname());
    std::unique_ptr<Action> action = parser(*this, execute_socket);
    Action *action_ptr = action.get();
    if (action_ptr == nullptr) {
      return nullptr;
    }
    m_actions.append(std::move(action));
    return action_ptr;
  }

  Action &build_action_list(VirtualNode *start_vnode, StringRef name)
  {
    Vector<VirtualSocket *> execute_sockets = this->find_execute_sockets(start_vnode, name);
    Vector<Action *> actions;
    for (VirtualSocket *socket : execute_sockets) {
      Action *action = this->build_action(socket);
      if (action != nullptr) {
        actions.append(action);
      }
    }
    Action *sequence = new ActionSequence(std::move(actions));
    m_actions.append(std::unique_ptr<Action>(sequence));
    return *sequence;
  }

 private:
  Vector<VirtualNode *> find_target_system_nodes(VirtualSocket *vsocket)
  {
    VectorSet<VirtualNode *> type_nodes;
    find_target_system_nodes__recursive(vsocket, type_nodes);
    return Vector<VirtualNode *>(type_nodes);
  }

  void find_target_system_nodes__recursive(VirtualSocket *output_vsocket,
                                           VectorSet<VirtualNode *> &r_nodes)
  {
    BLI_assert(output_vsocket->is_output());
    for (VirtualSocket *connected : output_vsocket->links()) {
      VirtualNode *connected_vnode = connected->vnode();
      if (connected_vnode->idname() == particle_system_idname) {
        r_nodes.add(connected_vnode);
      }
      else if (connected_vnode->idname() == combine_influences_idname) {
        find_target_system_nodes__recursive(connected_vnode->output(0), r_nodes);
      }
    }
  }

  TupleCallBody *function_body_for_inputs(VirtualNode *vnode, ArrayRef<uint> input_indices)
  {
    VectorSet<DataSocket> sockets_to_compute;
    for (uint index : input_indices) {
      sockets_to_compute.add_new(m_vtree_data_graph.lookup_socket(vnode->input(index)));
    }

    Vector<VirtualSocket *> dependencies = m_vtree_data_graph.find_placeholder_dependencies(
        sockets_to_compute);
    if (dependencies.size() > 0) {
      return nullptr;
    }

    FunctionGraph fgraph(m_vtree_data_graph.graph(), {}, sockets_to_compute);
    auto fn = fgraph.new_function(vnode->name());
    FN::fgraph_add_TupleCallBody(fn, fgraph);
    m_functions.append(fn);
    return &fn->body<TupleCallBody>();
  }

  Vector<VirtualSocket *> find_execute_sockets(VirtualNode *vnode, StringRef name_prefix)
  {
    bool found_name = false;
    Vector<VirtualSocket *> execute_sockets;
    for (VirtualSocket *vsocket : vnode->inputs()) {
      if (StringRef(vsocket->name()).startswith(name_prefix)) {
        if (vsocket->idname() == "fn_OperatorSocket") {
          found_name = true;
          break;
        }
        else {
          execute_sockets.append(vsocket);
        }
      }
    }
    BLI_assert(found_name);
    UNUSED_VARS_NDEBUG(found_name);
    return execute_sockets;
  }
};

static std::unique_ptr<Action> ACTION_kill(VTreeData &UNUSED(vtree_data),
                                           VirtualSocket *UNUSED(execute_vsocket))
{
  return std::unique_ptr<Action>(new KillAction());
}

static std::unique_ptr<Action> ACTION_change_velocity(VTreeData &vtree_data,
                                                      VirtualSocket *execute_vsocket)
{
  VirtualNode *vnode = execute_vsocket->vnode();
  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);

  if (inputs_fn == nullptr) {
    return {};
  }

  PointerRNA rna = vnode->rna();
  int mode = RNA_enum_get(&rna, "mode");

  Action *action = nullptr;
  if (mode == 0) {
    action = new SetVelocityAction(inputs_fn);
  }
  else if (mode == 1) {
    action = new RandomizeVelocityAction(inputs_fn);
  }

  return std::unique_ptr<Action>(action);
}

static std::unique_ptr<Action> ACTION_explode(VTreeData &vtree_data,
                                              VirtualSocket *execute_vsocket)
{
  VirtualNode *vnode = execute_vsocket->vnode();
  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);

  if (inputs_fn == nullptr) {
    return {};
  }

  Action &on_birth_action = vtree_data.build_action_list(vnode, "Execute on Birth");
  ArrayRef<std::string> system_names = vtree_data.find_target_system_names(
      vnode->output(1, "Explode System"));

  Action *action = new ExplodeAction(system_names, inputs_fn, on_birth_action);
  return std::unique_ptr<Action>(action);
}

static std::unique_ptr<Action> ACTION_condition(VTreeData &vtree_data,
                                                VirtualSocket *execute_vsocket)
{
  VirtualNode *vnode = execute_vsocket->vnode();
  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);

  if (inputs_fn == nullptr) {
    return {};
  }

  Action &action_true = vtree_data.build_action_list(vnode, "Execute If True");
  Action &action_false = vtree_data.build_action_list(vnode, "Execute If False");

  Action *action = new ConditionAction(inputs_fn, action_true, action_false);
  return std::unique_ptr<Action>(action);
}

static std::unique_ptr<Action> ACTION_change_color(VTreeData &vtree_data,
                                                   VirtualSocket *execute_vsocket)
{
  VirtualNode *vnode = execute_vsocket->vnode();
  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);

  if (inputs_fn == nullptr) {
    return {};
  }

  Action *action = new ChangeColorAction(inputs_fn);
  return std::unique_ptr<Action>(action);
}

static std::unique_ptr<Action> ACTION_change_size(VTreeData &vtree_data,
                                                  VirtualSocket *execute_vsocket)
{
  VirtualNode *vnode = execute_vsocket->vnode();
  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);

  if (inputs_fn == nullptr) {
    return {};
  }

  Action *action = new ChangeSizeAction(inputs_fn);
  return std::unique_ptr<Action>(action);
}

static std::unique_ptr<Action> ACTION_change_position(VTreeData &vtree_data,
                                                      VirtualSocket *execute_vsocket)
{
  VirtualNode *vnode = execute_vsocket->vnode();
  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);

  if (inputs_fn == nullptr) {
    return {};
  }

  Action *action = new ChangePositionAction(inputs_fn);
  return std::unique_ptr<Action>(action);
}

BLI_LAZY_INIT(StringMap<ActionParserCallback>, get_action_parsers)
{
  StringMap<ActionParserCallback> map;
  map.add_new("bp_KillParticleNode", ACTION_kill);
  map.add_new("bp_ChangeParticleVelocityNode", ACTION_change_velocity);
  map.add_new("bp_ExplodeParticleNode", ACTION_explode);
  map.add_new("bp_ParticleConditionNode", ACTION_condition);
  map.add_new("bp_ChangeParticleColorNode", ACTION_change_color);
  map.add_new("bp_ChangeParticleSizeNode", ACTION_change_size);
  map.add_new("bp_ChangeParticlePositionNode", ACTION_change_position);
  return map;
}

using ParseNodeCallback = std::function<void(InfluencesCollector &collector,
                                             VTreeData &vtree_data,
                                             WorldTransition &world_transition,
                                             VirtualNode *vnode)>;

static void PARSE_point_emitter(InfluencesCollector &collector,
                                VTreeData &vtree_data,
                                WorldTransition &world_transition,
                                VirtualNode *vnode)
{
  Optional<NamedTupleRef> inputs = vtree_data.compute_all_data_inputs(vnode);
  if (!inputs.has_value()) {
    return;
  }

  Action &action = vtree_data.build_action_list(vnode, "Execute on Birth");

  ArrayRef<std::string> system_names = vtree_data.find_target_system_names(
      vnode->output(0, "Emitter"));
  std::string name = vnode->name();

  VaryingFloat3 position = world_transition.update_float3(
      name, "Position", inputs->get<float3>(0, "Position"));
  VaryingFloat3 velocity = world_transition.update_float3(
      name, "Velocity", inputs->get<float3>(1, "Velocity"));
  VaryingFloat size = world_transition.update_float(name, "Size", inputs->get<float>(2, "Size"));

  Emitter *emitter = new PointEmitter(std::move(system_names), position, velocity, size, action);
  collector.m_emitters.append(emitter);
}

static Vector<float> compute_emitter_vertex_weights(VirtualNode *vnode,
                                                    NamedTupleRef inputs,
                                                    Object *object)
{
  PointerRNA rna = vnode->rna();
  uint density_mode = RNA_enum_get(&rna, "density_mode");

  Mesh *mesh = (Mesh *)object->data;
  Vector<float> vertex_weights(mesh->totvert);

  if (density_mode == 0) {
    /* Mode: 'UNIFORM' */
    vertex_weights.fill(1.0f);
  }
  else if (density_mode == 1) {
    /* Mode: 'VERTEX_WEIGHTS' */
    auto group_name = inputs.relocate_out<FN::Types::StringW>(2, "Density Group");

    MDeformVert *vertices = mesh->dvert;
    int group_index = defgroup_name_index(object, group_name->data());
    if (group_index == -1 || vertices == nullptr) {
      vertex_weights.fill(0);
    }
    else {
      for (uint i = 0; i < mesh->totvert; i++) {
        vertex_weights[i] = defvert_find_weight(vertices + i, group_index);
      }
    }
  }
  else if (density_mode == 2) {
    /* Mode: 'FALLOFF' */
    auto falloff = inputs.relocate_out<FN::Types::FalloffW>(2, "Density Falloff");

    float4x4 transform = object->obmat;

    TemporaryArray<float3> vertex_positions(mesh->totvert);
    for (uint i = 0; i < mesh->totvert; i++) {
      vertex_positions[i] = transform.transform_position(mesh->mvert[i].co);
    }
    AttributesDeclaration info_declaration;
    info_declaration.add<float3>("Position", {0, 0, 0});
    AttributesInfo info(info_declaration);

    std::array<void *, 1> buffers = {(void *)vertex_positions.begin()};
    AttributesRef attributes{info, buffers, (uint)mesh->totvert};
    falloff->compute(attributes, IndexRange(mesh->totvert).as_array_ref(), vertex_weights);
  }

  return vertex_weights;
}

static void PARSE_mesh_emitter(InfluencesCollector &collector,
                               VTreeData &vtree_data,
                               WorldTransition &world_transition,
                               VirtualNode *vnode)
{
  Optional<NamedTupleRef> inputs = vtree_data.compute_all_data_inputs(vnode);
  if (!inputs.has_value()) {
    return;
  }

  Action &on_birth_action = vtree_data.build_action_list(vnode, "Execute on Birth");

  Object *object = inputs->relocate_out<ObjectW>(0, "Object").ptr();
  if (object == nullptr || object->type != OB_MESH) {
    return;
  }

  auto vertex_weights = compute_emitter_vertex_weights(vnode, *inputs, object);

  VaryingFloat4x4 transform = world_transition.update_float4x4(
      vnode->name(), "Transform", object->obmat);
  ArrayRef<std::string> system_names = vtree_data.find_target_system_names(
      vnode->output(0, "Emitter"));
  Emitter *emitter = new SurfaceEmitter(system_names,
                                        on_birth_action,
                                        object,
                                        transform,
                                        inputs->get<float>(1, "Rate"),
                                        std::move(vertex_weights));
  collector.m_emitters.append(emitter);
}

static void PARSE_gravity_force(InfluencesCollector &collector,
                                VTreeData &vtree_data,
                                WorldTransition &UNUSED(world_transition),
                                VirtualNode *vnode)
{
  Optional<NamedTupleRef> inputs = vtree_data.compute_inputs(vnode, {1});
  if (!inputs.has_value()) {
    return;
  }

  auto falloff = inputs->relocate_out<FN::Types::FalloffW>(0, "Falloff");

  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);
  if (inputs_fn == nullptr) {
    return;
  }

  ArrayRef<std::string> system_names = vtree_data.find_target_system_names(
      vnode->output(0, "Force"));
  for (const std::string &system_name : system_names) {
    GravityForce *force = new GravityForce(inputs_fn, falloff.get_unique_copy());
    collector.m_forces.add(system_name, force);
  }
}

static void PARSE_age_reached_event(InfluencesCollector &collector,
                                    VTreeData &vtree_data,
                                    WorldTransition &UNUSED(world_transition),
                                    VirtualNode *vnode)
{
  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);
  if (inputs_fn == nullptr) {
    return;
  }

  ArrayRef<std::string> system_names = vtree_data.find_target_system_names(
      vnode->output(0, "Event"));
  Action &action = vtree_data.build_action_list(vnode, "Execute on Event");

  for (const std::string &system_name : system_names) {
    Event *event = new AgeReachedEvent(vnode->name(), inputs_fn, action);
    collector.m_events.add(system_name, event);
  }
}

static void PARSE_trails(InfluencesCollector &collector,
                         VTreeData &vtree_data,
                         WorldTransition &UNUSED(world_transition),
                         VirtualNode *vnode)
{
  ArrayRef<std::string> main_system_names = vtree_data.find_target_system_names(
      vnode->output(0, "Main System"));
  ArrayRef<std::string> trail_system_names = vtree_data.find_target_system_names(
      vnode->output(1, "Trail System"));

  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);
  if (inputs_fn == nullptr) {
    return;
  }

  Action &action = vtree_data.build_action_list(vnode, "Execute on Birth");
  for (const std::string &main_type : main_system_names) {

    OffsetHandler *offset_handler = new CreateTrailHandler(trail_system_names, inputs_fn, action);
    collector.m_offset_handlers.add(main_type, offset_handler);
  }
}

static void PARSE_initial_grid_emitter(InfluencesCollector &collector,
                                       VTreeData &vtree_data,
                                       WorldTransition &UNUSED(world_transition),
                                       VirtualNode *vnode)
{
  Optional<NamedTupleRef> inputs = vtree_data.compute_all_data_inputs(vnode);
  if (!inputs.has_value()) {
    return;
  }

  Action &action = vtree_data.build_action_list(vnode, "Execute on Birth");

  ArrayRef<std::string> system_names = vtree_data.find_target_system_names(
      vnode->output(0, "Emitter"));
  Emitter *emitter = new InitialGridEmitter(std::move(system_names),
                                            std::max(0, inputs->get<int>(0, "Amount X")),
                                            std::max(0, inputs->get<int>(1, "Amount Y")),
                                            inputs->get<float>(2, "Step X"),
                                            inputs->get<float>(3, "Step Y"),
                                            inputs->get<float>(4, "Size"),
                                            action);
  collector.m_emitters.append(emitter);
}

static void PARSE_turbulence_force(InfluencesCollector &collector,
                                   VTreeData &vtree_data,
                                   WorldTransition &UNUSED(world_transition),
                                   VirtualNode *vnode)
{
  Optional<NamedTupleRef> inputs = vtree_data.compute_inputs(vnode, {2});
  if (!inputs.has_value()) {
    return;
  }

  auto falloff = inputs->relocate_out<FN::Types::FalloffW>(0, "Falloff");

  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);
  if (inputs_fn == nullptr) {
    return;
  }

  ArrayRef<std::string> system_names = vtree_data.find_target_system_names(
      vnode->output(0, "Force"));
  for (const std::string &system_name : system_names) {

    Force *force = new TurbulenceForce(inputs_fn, falloff.get_unique_copy());
    collector.m_forces.add(system_name, force);
  }
}

static void PARSE_drag_force(InfluencesCollector &collector,
                             VTreeData &vtree_data,
                             WorldTransition &UNUSED(world_transition),
                             VirtualNode *vnode)
{
  Optional<NamedTupleRef> inputs = vtree_data.compute_inputs(vnode, {1});
  if (!inputs.has_value()) {
    return;
  }

  auto falloff = inputs->relocate_out<FN::Types::FalloffW>(0, "Falloff");

  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);
  if (inputs_fn == nullptr) {
    return;
  }

  ArrayRef<std::string> system_names = vtree_data.find_target_system_names(
      vnode->output(0, "Force"));

  for (const std::string &system_name : system_names) {
    Force *force = new DragForce(inputs_fn, falloff.get_unique_copy());
    collector.m_forces.add(system_name, force);
  }
}

static void PARSE_mesh_collision(InfluencesCollector &collector,
                                 VTreeData &vtree_data,
                                 WorldTransition &UNUSED(world_transition),
                                 VirtualNode *vnode)
{
  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);
  if (inputs_fn == nullptr) {
    return;
  }

  Optional<NamedTupleRef> inputs = vtree_data.compute_inputs(vnode, {0});
  if (!inputs.has_value()) {
    return;
  }

  Object *object = inputs->relocate_out<ObjectW>(0, "Object").ptr();
  if (object == nullptr || object->type != OB_MESH) {
    return;
  }

  ArrayRef<std::string> system_names = vtree_data.find_target_system_names(
      vnode->output(0, "Event"));
  Action &action = vtree_data.build_action_list(vnode, "Execute on Event");

  for (const std::string &system_name : system_names) {
    Event *event = new MeshCollisionEvent(vnode->name(), object, action);
    collector.m_events.add(system_name, event);
  }
}

static void PARSE_size_over_time(InfluencesCollector &collector,
                                 VTreeData &vtree_data,
                                 WorldTransition &UNUSED(world_transition),
                                 VirtualNode *vnode)
{
  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);
  if (inputs_fn == nullptr) {
    return;
  }

  ArrayRef<std::string> system_names = vtree_data.find_target_system_names(
      vnode->output(0, "Influence"));
  for (const std::string &system_name : system_names) {
    OffsetHandler *handler = new SizeOverTimeHandler(inputs_fn);
    collector.m_offset_handlers.add(system_name, handler);
  }
}

static void PARSE_mesh_force(InfluencesCollector &collector,
                             VTreeData &vtree_data,
                             WorldTransition &UNUSED(world_transition),
                             VirtualNode *vnode)
{
  Optional<NamedTupleRef> inputs = vtree_data.compute_inputs(vnode, {0});
  if (!inputs.has_value()) {
    return;
  }

  Object *object = inputs->relocate_out<ObjectW>(0, "Object").ptr();
  if (object == nullptr || object->type != OB_MESH) {
    return;
  }

  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);
  if (inputs_fn == nullptr) {
    return;
  }

  ArrayRef<std::string> system_names = vtree_data.find_target_system_names(
      vnode->output(0, "Force"));
  for (const std::string &system_name : system_names) {
    Force *force = new MeshForce(inputs_fn, object);
    collector.m_forces.add(system_name, force);
  }
}

static void PARSE_custom_event(InfluencesCollector &collector,
                               VTreeData &vtree_data,
                               WorldTransition &UNUSED(world_transition),
                               VirtualNode *vnode)
{
  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);
  if (inputs_fn == nullptr) {
    return;
  }

  ArrayRef<std::string> system_names = vtree_data.find_target_system_names(
      vnode->output(0, "Event"));
  Action &action = vtree_data.build_action_list(vnode, "Execute on Event");

  for (const std::string &system_name : system_names) {
    Event *event = new CustomEvent(vnode->name(), inputs_fn, action);
    collector.m_events.add(system_name, event);
  }
}

static void PARSE_always_execute(InfluencesCollector &collector,
                                 VTreeData &vtree_data,
                                 WorldTransition &UNUSED(world_transition),
                                 VirtualNode *vnode)
{
  ArrayRef<std::string> system_names = vtree_data.find_target_system_names(
      vnode->output(0, "Influence"));
  Action &action = vtree_data.build_action_list(vnode, "Execute");

  for (const std::string &system_name : system_names) {
    OffsetHandler *handler = new AlwaysExecuteHandler(action);
    collector.m_offset_handlers.add(system_name, handler);
  }
}

BLI_LAZY_INIT_STATIC(StringMap<ParseNodeCallback>, get_node_parsers)
{
  StringMap<ParseNodeCallback> map;
  map.add_new("bp_PointEmitterNode", PARSE_point_emitter);
  map.add_new("bp_MeshEmitterNode", PARSE_mesh_emitter);
  map.add_new("bp_GravityForceNode", PARSE_gravity_force);
  map.add_new("bp_AgeReachedEventNode", PARSE_age_reached_event);
  map.add_new("bp_ParticleTrailsNode", PARSE_trails);
  map.add_new("bp_InitialGridEmitterNode", PARSE_initial_grid_emitter);
  map.add_new("bp_TurbulenceForceNode", PARSE_turbulence_force);
  map.add_new("bp_MeshCollisionEventNode", PARSE_mesh_collision);
  map.add_new("bp_SizeOverTimeNode", PARSE_size_over_time);
  map.add_new("bp_DragForceNode", PARSE_drag_force);
  map.add_new("bp_MeshForceNode", PARSE_mesh_force);
  map.add_new("bp_CustomEventNode", PARSE_custom_event);
  map.add_new("bp_AlwaysExecuteNode", PARSE_always_execute);
  return map;
}

static void collect_influences(VTreeData &vtree_data,
                               WorldTransition &world_transition,
                               Vector<std::string> &r_system_names,
                               Vector<Emitter *> &r_emitters,
                               MultiMap<std::string, Event *> &r_events_per_type,
                               MultiMap<std::string, OffsetHandler *> &r_offset_handler_per_type,
                               StringMap<AttributesDeclaration> &r_attributes_per_type,
                               StringMap<Integrator *> &r_integrators)
{
  SCOPED_TIMER(__func__);

  StringMap<ParseNodeCallback> &parsers = get_node_parsers();

  MultiMap<std::string, Force *> forces;
  InfluencesCollector collector = {
      r_emitters,
      forces,
      r_events_per_type,
      r_offset_handler_per_type,
  };

  for (VirtualNode *vnode : vtree_data.vtree().nodes()) {
    StringRef idname = vnode->idname();
    ParseNodeCallback *callback = parsers.lookup_ptr(idname);
    if (callback != nullptr) {
      (*callback)(collector, vtree_data, world_transition, vnode);
    }
  }

  for (VirtualNode *vnode : vtree_data.vtree().nodes_with_idname(particle_system_idname)) {
    StringRef name = vnode->name();
    r_system_names.append(name);
  }

  for (std::string &system_name : r_system_names) {
    AttributesDeclaration attributes;
    attributes.add<uint8_t>("Kill State", 0);
    attributes.add<int32_t>("ID", 0);
    attributes.add<float>("Birth Time", 0);
    attributes.add<float3>("Position", float3(0, 0, 0));
    attributes.add<float3>("Velocity", float3(0, 0, 0));
    attributes.add<float>("Size", 0.05f);
    attributes.add<rgba_f>("Color", rgba_f(1, 1, 1, 1));

    for (Event *event : r_events_per_type.lookup_default(system_name)) {
      event->attributes(attributes);
    }

    ArrayRef<Force *> forces = collector.m_forces.lookup_default(system_name);
    EulerIntegrator *integrator = new EulerIntegrator(forces);

    r_attributes_per_type.add_new(system_name, attributes);
    r_integrators.add_new(system_name, integrator);
  }
}

class NodeTreeStepSimulator : public StepSimulator {
 private:
  bNodeTree *m_btree;
  VirtualNodeTree m_vtree;

 public:
  NodeTreeStepSimulator(bNodeTree *btree) : m_btree(btree)
  {
    m_vtree.add_all_of_tree(m_btree);
    m_vtree.freeze_and_index();
  }

  void simulate(SimulationState &simulation_state) override
  {
    WorldState &old_world_state = simulation_state.world();
    WorldState new_world_state;
    WorldTransition world_transition = {old_world_state, new_world_state};

    ParticlesState &particles_state = simulation_state.particles();

    Vector<std::string> system_names;
    Vector<Emitter *> emitters;
    MultiMap<std::string, Event *> events;
    MultiMap<std::string, OffsetHandler *> offset_handlers;
    StringMap<AttributesDeclaration> attributes;
    StringMap<Integrator *> integrators;

    auto data_graph_or_error = FN::DataFlowNodes::generate_graph(m_vtree);
    if (data_graph_or_error.is_error()) {
      return;
    }
    std::unique_ptr<VTreeDataGraph> vtree_data_graph = data_graph_or_error.extract_value();
    VTreeData vtree_data(*vtree_data_graph);

    collect_influences(vtree_data,
                       world_transition,
                       system_names,
                       emitters,
                       events,
                       offset_handlers,
                       attributes,
                       integrators);

    auto &containers = particles_state.particle_containers();

    StringMap<ParticleSystemInfo> systems_to_simulate;
    for (std::string name : system_names) {
      AttributesDeclaration &system_attributes = attributes.lookup(name);

      /* Keep old attributes. */
      AttributesBlockContainer *container = containers.lookup_default(name, nullptr);
      if (container != nullptr) {
        system_attributes.join(container->attributes_info());
      }

      ParticleSystemInfo type_info = {
          &system_attributes,
          integrators.lookup(name),
          events.lookup_default(name),
          offset_handlers.lookup_default(name),
      };
      systems_to_simulate.add_new(name, type_info);
    }

    simulate_particles(simulation_state, emitters, systems_to_simulate);

    for (Emitter *emitter : emitters) {
      delete emitter;
    }
    events.foreach_value([](Event *event) { delete event; });
    offset_handlers.foreach_value([](OffsetHandler *handler) { delete handler; });
    integrators.foreach_value([](Integrator *integrator) { delete integrator; });

    simulation_state.world() = std::move(new_world_state);
  }
};

std::unique_ptr<StepSimulator> simulator_from_node_tree(bNodeTree *btree)
{
  return std::unique_ptr<StepSimulator>(new NodeTreeStepSimulator(btree));
}

}  // namespace BParticles
