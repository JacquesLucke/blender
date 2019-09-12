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
using FN::SharedDataGraph;
using FN::DataFlowNodes::VTreeDataGraph;
using FN::Types::ObjectW;
using FN::Types::StringW;

class BehaviorCollector {
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

  TupleCallBody &function_body_for_inputs(VirtualNode *vnode, ArrayRef<uint> input_indices)
  {
    SetVector<DataSocket> sockets_to_compute;
    for (uint index : input_indices) {
      sockets_to_compute.add_new(m_vtree_data_graph.lookup_socket(vnode->input(index)));
    }

    FunctionGraph fgraph(m_vtree_data_graph.graph(), {}, sockets_to_compute);
    auto fn = fgraph.new_function(vnode->name());
    FN::fgraph_add_TupleCallBody(fn, fgraph);
    m_functions.append(fn);
    return fn->body<TupleCallBody>();
  }

  TupleCallBody &function_body_for_all_inputs(VirtualNode *vnode)
  {
    SetVector<DataSocket> sockets_to_compute;
    for (VirtualSocket *vsocket : vnode->inputs()) {
      if (m_vtree_data_graph.uses_socket(vsocket)) {
        sockets_to_compute.add_new(m_vtree_data_graph.lookup_socket(vsocket));
      }
    }

    FunctionGraph fgraph(m_vtree_data_graph.graph(), {}, sockets_to_compute);
    auto fn = fgraph.new_function(vnode->name());
    FN::fgraph_add_TupleCallBody(fn, fgraph);
    m_functions.append(fn);
    return fn->body<TupleCallBody>();
  }

  FN::OutputTupleRef compute_all_inputs(VirtualNode *vnode)
  {
    TupleCallBody &body = this->function_body_for_all_inputs(vnode);
    FN_TUPLE_STACK_ALLOC(fn_in, body.meta_in().ref());
    FN::Tuple *fn_out = new FN::Tuple(body.meta_out());

    body.call__setup_execution_context(fn_in, *fn_out);

    m_tuples.append(std::unique_ptr<FN::Tuple>(fn_out));
    return FN::OutputTupleRef(fn_out, body.owner());
  }
};

static StringRef particle_type_idname = "bp_ParticleTypeNode";
static StringRef combine_behaviors_idname = "bp_CombineBehaviorsNode";

static std::unique_ptr<Action> build_action_list(VTreeData &vtree_data,
                                                 VirtualNode *start_vnode,
                                                 StringRef name);

static void find_connected_particle_type_nodes__recursive(VirtualSocket *output_vsocket,
                                                          SetVector<VirtualNode *> &r_nodes)
{
  BLI_assert(output_vsocket->is_output());
  for (VirtualSocket *connected : output_vsocket->links()) {
    VirtualNode *connected_vnode = connected->vnode();
    if (connected_vnode->idname() == particle_type_idname) {
      r_nodes.add(connected_vnode);
    }
    else if (connected_vnode->idname() == combine_behaviors_idname) {
      find_connected_particle_type_nodes__recursive(connected_vnode->output(0), r_nodes);
    }
  }
}

static Vector<VirtualNode *> find_connected_particle_type_nodes(VirtualSocket *output_vsocket)
{
  SetVector<VirtualNode *> type_nodes;
  find_connected_particle_type_nodes__recursive(output_vsocket, type_nodes);
  return Vector<VirtualNode *>(type_nodes);
}

static Vector<std::string> find_connected_particle_type_names(VirtualSocket *output_vsocket)
{
  Vector<std::string> type_names;
  for (VirtualNode *vnode : find_connected_particle_type_nodes(output_vsocket)) {
    type_names.append(vnode->name());
  }
  return type_names;
}

static Vector<VirtualSocket *> find_execute_sockets(VirtualNode *vnode, StringRef name_prefix)
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

using ActionParserCallback =
    std::function<std::unique_ptr<Action>(VTreeData &vtree_data, VirtualSocket *execute_vsocket)>;

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

  std::unique_ptr<Action> on_birth_action = build_action_list(
      vtree_data, vnode, "Execute on Birth");
  Vector<std::string> type_names = find_connected_particle_type_names(vnode->output(1, "Type"));

  Action *action = new ExplodeAction(type_names, inputs_fn, std::move(on_birth_action));
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

  auto action_true = build_action_list(vtree_data, vnode, "Execute If True");
  auto action_false = build_action_list(vtree_data, vnode, "Execute If False");

  Action *action = new ConditionAction(inputs_fn, std::move(action_true), std::move(action_false));
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

BLI_LAZY_INIT_STATIC(StringMap<ActionParserCallback>, get_action_parsers)
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

static std::unique_ptr<Action> build_action(VTreeData &vtree_data, VirtualSocket *start)
{
  BLI_assert(start->is_input());
  if (start->links().size() != 1) {
    return std::unique_ptr<Action>(new NoneAction());
  }

  VirtualSocket *execute_socket = start->links()[0];
  if (execute_socket->idname() != "bp_ExecuteSocket") {
    return std::unique_ptr<Action>(new NoneAction());
  }

  StringMap<ActionParserCallback> &parsers = get_action_parsers();
  ActionParserCallback &parser = parsers.lookup(execute_socket->vnode()->idname());
  std::unique_ptr<Action> action = parser(vtree_data, execute_socket);
  if (action.get() == nullptr) {
    return std::unique_ptr<Action>(new NoneAction());
  }
  return action;
}

static std::unique_ptr<Action> build_action_list(VTreeData &vtree_data,
                                                 VirtualNode *start_vnode,
                                                 StringRef name)
{
  Vector<VirtualSocket *> execute_sockets = find_execute_sockets(start_vnode, name);
  Vector<std::unique_ptr<Action>> actions;
  for (VirtualSocket *socket : execute_sockets) {
    actions.append(build_action(vtree_data, socket));
  }
  Action *sequence = new ActionSequence(std::move(actions));
  return std::unique_ptr<Action>(sequence);
}

using ParseNodeCallback = std::function<void(BehaviorCollector &collector,
                                             VTreeData &vtree_data,
                                             WorldTransition &world_transition,
                                             VirtualNode *vnode)>;

static SharedFunction get_compute_data_inputs_function(VTreeData &vtree_data, VirtualNode *vnode)
{
  SharedDataGraph &data_graph = vtree_data.data_graph();

  SetVector<DataSocket> function_outputs;
  for (VirtualSocket *vsocket : vnode->inputs()) {
    if (vtree_data.vtree_data_graph().uses_socket(vsocket)) {
      DataSocket socket = vtree_data.vtree_data_graph().lookup_socket(vsocket);
      function_outputs.add(socket);
    }
  }

  FunctionGraph fgraph(data_graph, {}, function_outputs);
  SharedFunction fn = fgraph.new_function(vnode->name());
  FN::fgraph_add_TupleCallBody(fn, fgraph);
  FN::fgraph_add_LLVMBuildIRBody(fn, fgraph);
  return fn;
}

static void PARSE_point_emitter(BehaviorCollector &collector,
                                VTreeData &vtree_data,
                                WorldTransition &world_transition,
                                VirtualNode *vnode)
{
  FN::OutputTupleRef inputs = vtree_data.compute_all_inputs(vnode);
  Vector<std::string> type_names = find_connected_particle_type_names(vnode->output(0, "Emitter"));
  std::string name = vnode->name();

  VaryingFloat3 position = world_transition.update_float3(
      name, "Position", inputs.get<float3>(0, "Position"));
  VaryingFloat3 velocity = world_transition.update_float3(
      name, "Velocity", inputs.get<float3>(1, "Velocity"));
  VaryingFloat size = world_transition.update_float(name, "Size", inputs.get<float>(2, "Size"));

  Emitter *emitter = new PointEmitter(std::move(type_names), position, velocity, size);
  collector.m_emitters.append(emitter);
}

static Vector<float> compute_emitter_vertex_weights(VirtualNode *vnode,
                                                    FN::OutputTupleRef &inputs,
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

static void PARSE_mesh_emitter(BehaviorCollector &collector,
                               VTreeData &vtree_data,
                               WorldTransition &world_transition,
                               VirtualNode *vnode)
{
  FN::OutputTupleRef inputs = vtree_data.compute_all_inputs(vnode);

  std::unique_ptr<Action> on_birth_action = build_action_list(
      vtree_data, vnode, "Execute on Birth");

  Object *object = inputs.relocate_out<ObjectW>(0, "Object").ptr();
  if (object == nullptr || object->type != OB_MESH) {
    return;
  }

  auto vertex_weights = compute_emitter_vertex_weights(vnode, inputs, object);

  VaryingFloat4x4 transform = world_transition.update_float4x4(
      vnode->name(), "Transform", object->obmat);
  Vector<std::string> type_names = find_connected_particle_type_names(vnode->output(0, "Emitter"));
  Emitter *emitter = new SurfaceEmitter(std::move(type_names),
                                        std::move(on_birth_action),
                                        object,
                                        transform,
                                        inputs.get<float>(1, "Rate"),
                                        std::move(vertex_weights));
  collector.m_emitters.append(emitter);
}

static void PARSE_gravity_force(BehaviorCollector &collector,
                                VTreeData &vtree_data,
                                WorldTransition &UNUSED(world_transition),
                                VirtualNode *vnode)
{
  FN::TupleCallBody &body = vtree_data.function_body_for_inputs(vnode, {1});
  FN_TUPLE_CALL_ALLOC_TUPLES(body, fn_in, fn_out);
  body.call__setup_execution_context(fn_in, fn_out);

  auto falloff = fn_out.relocate_out<FN::Types::FalloffW>(0);

  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);
  if (inputs_fn == nullptr) {
    return;
  }

  Vector<std::string> type_names = find_connected_particle_type_names(vnode->output(0, "Force"));
  for (std::string &type_name : type_names) {
    GravityForce *force = new GravityForce(inputs_fn, falloff.get_unique_copy());
    collector.m_forces.add(type_name, force);
  }
}

static void PARSE_age_reached_event(BehaviorCollector &collector,
                                    VTreeData &vtree_data,
                                    WorldTransition &UNUSED(world_transition),
                                    VirtualNode *vnode)
{
  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);
  if (inputs_fn == nullptr) {
    return;
  }

  Vector<std::string> type_names = find_connected_particle_type_names(vnode->output(0, "Event"));
  for (std::string &type_name : type_names) {
    auto action = build_action_list(vtree_data, vnode, "Execute on Event");

    Event *event = new AgeReachedEvent(vnode->name(), inputs_fn, std::move(action));
    collector.m_events.add(type_name, event);
  }
}

static void PARSE_trails(BehaviorCollector &collector,
                         VTreeData &vtree_data,
                         WorldTransition &UNUSED(world_transition),
                         VirtualNode *vnode)
{
  Vector<std::string> main_type_names = find_connected_particle_type_names(
      vnode->output(0, "Main Type"));
  Vector<std::string> trail_type_names = find_connected_particle_type_names(
      vnode->output(1, "Trail Type"));

  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);
  if (inputs_fn == nullptr) {
    return;
  }

  for (std::string &main_type : main_type_names) {
    auto action = build_action_list(vtree_data, vnode, "Execute on Birth");

    OffsetHandler *offset_handler = new CreateTrailHandler(
        trail_type_names, inputs_fn, std::move(action));
    collector.m_offset_handlers.add(main_type, offset_handler);
  }
}

static void PARSE_initial_grid_emitter(BehaviorCollector &collector,
                                       VTreeData &vtree_data,
                                       WorldTransition &UNUSED(world_transition),
                                       VirtualNode *vnode)
{
  FN::OutputTupleRef inputs = vtree_data.compute_all_inputs(vnode);

  Vector<std::string> type_names = find_connected_particle_type_names(vnode->output(0, "Emitter"));
  Emitter *emitter = new InitialGridEmitter(std::move(type_names),
                                            std::max(0, inputs.get<int>(0, "Amount X")),
                                            std::max(0, inputs.get<int>(1, "Amount Y")),
                                            inputs.get<float>(2, "Step X"),
                                            inputs.get<float>(3, "Step Y"),
                                            inputs.get<float>(4, "Size"));
  collector.m_emitters.append(emitter);
}

static void PARSE_turbulence_force(BehaviorCollector &collector,
                                   VTreeData &vtree_data,
                                   WorldTransition &UNUSED(world_transition),
                                   VirtualNode *vnode)
{
  FN::TupleCallBody &body = vtree_data.function_body_for_inputs(vnode, {2});
  FN_TUPLE_CALL_ALLOC_TUPLES(body, fn_in, fn_out);
  body.call__setup_execution_context(fn_in, fn_out);

  auto falloff = fn_out.relocate_out<FN::Types::FalloffW>(0);

  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);
  if (inputs_fn == nullptr) {
    return;
  }

  Vector<std::string> type_names = find_connected_particle_type_names(vnode->output(0, "Force"));
  for (std::string &type_name : type_names) {

    Force *force = new TurbulenceForce(inputs_fn, falloff.get_unique_copy());
    collector.m_forces.add(type_name, force);
  }
}

static void PARSE_drag_force(BehaviorCollector &collector,
                             VTreeData &vtree_data,
                             WorldTransition &UNUSED(world_transition),
                             VirtualNode *vnode)
{
  FN::TupleCallBody &body = vtree_data.function_body_for_inputs(vnode, {1});
  FN_TUPLE_CALL_ALLOC_TUPLES(body, fn_in, fn_out);
  body.call__setup_execution_context(fn_in, fn_out);

  auto falloff = fn_out.relocate_out<FN::Types::FalloffW>(0);

  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);
  if (inputs_fn == nullptr) {
    return;
  }

  Vector<std::string> type_names = find_connected_particle_type_names(vnode->output(0, "Force"));
  for (std::string &type_name : type_names) {

    Force *force = new DragForce(inputs_fn, falloff.get_unique_copy());
    collector.m_forces.add(type_name, force);
  }
}

static void PARSE_mesh_collision(BehaviorCollector &collector,
                                 VTreeData &vtree_data,
                                 WorldTransition &UNUSED(world_transition),
                                 VirtualNode *vnode)
{
  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);
  if (inputs_fn == nullptr) {
    return;
  }

  if (inputs_fn->parameter_depends_on_particle("Object", 0)) {
    return;
  }

  SharedFunction &fn = inputs_fn->function_no_deps();
  TupleCallBody &body = fn->body<TupleCallBody>();
  FN_TUPLE_CALL_ALLOC_TUPLES(body, fn_in, fn_out);
  body.call__setup_execution_context(fn_in, fn_out);

  Object *object = fn_out.relocate_out<ObjectW>(0).ptr();
  if (object == nullptr || object->type != OB_MESH) {
    return;
  }

  Vector<std::string> type_names = find_connected_particle_type_names(vnode->output(0, "Event"));
  for (std::string &type_name : type_names) {
    auto action = build_action_list(vtree_data, vnode, "Execute on Event");
    Event *event = new MeshCollisionEvent(vnode->name(), object, std::move(action));
    collector.m_events.add(type_name, event);
  }
}

static void PARSE_size_over_time(BehaviorCollector &collector,
                                 VTreeData &vtree_data,
                                 WorldTransition &UNUSED(world_transition),
                                 VirtualNode *vnode)
{
  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);
  if (inputs_fn == nullptr) {
    return;
  }

  Vector<std::string> type_names = find_connected_particle_type_names(vnode->output(0, "Type"));
  for (std::string &type_name : type_names) {
    OffsetHandler *handler = new SizeOverTimeHandler(inputs_fn);
    collector.m_offset_handlers.add(type_name, handler);
  }
}

static void PARSE_mesh_force(BehaviorCollector &collector,
                             VTreeData &vtree_data,
                             WorldTransition &UNUSED(world_transition),
                             VirtualNode *vnode)
{
  FN::TupleCallBody &body = vtree_data.function_body_for_inputs(vnode, {0});
  FN_TUPLE_CALL_ALLOC_TUPLES(body, fn_in, fn_out);
  body.call__setup_execution_context(fn_in, fn_out);

  Object *object = fn_out.relocate_out<ObjectW>(0).ptr();
  if (object == nullptr || object->type != OB_MESH) {
    return;
  }

  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);
  if (inputs_fn == nullptr) {
    return;
  }

  Vector<std::string> type_names = find_connected_particle_type_names(vnode->output(0, "Force"));
  for (std::string &type_name : type_names) {
    Force *force = new MeshForce(inputs_fn, object);
    collector.m_forces.add(type_name, force);
  }
}

static void PARSE_custom_event(BehaviorCollector &collector,
                               VTreeData &vtree_data,
                               WorldTransition &UNUSED(world_transition),
                               VirtualNode *vnode)
{
  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);
  if (inputs_fn == nullptr) {
    return;
  }

  Vector<std::string> type_names = find_connected_particle_type_names(vnode->output(0, "Event"));
  for (std::string &type_name : type_names) {
    auto action = build_action_list(vtree_data, vnode, "Execute on Event");

    Event *event = new CustomEvent(vnode->name(), inputs_fn, std::move(action));
    collector.m_events.add(type_name, event);
  }
}

static void PARSE_always_execute(BehaviorCollector &collector,
                                 VTreeData &vtree_data,
                                 WorldTransition &UNUSED(world_transition),
                                 VirtualNode *vnode)
{
  Vector<std::string> type_names = find_connected_particle_type_names(vnode->output(0, "Type"));
  for (std::string &type_name : type_names) {
    auto action = build_action_list(vtree_data, vnode, "Execute");

    OffsetHandler *handler = new AlwaysExecuteHandler(std::move(action));
    collector.m_offset_handlers.add(type_name, handler);
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

static void collect_particle_behaviors(
    VTreeData &vtree_data,
    WorldTransition &world_transition,
    Vector<std::string> &r_type_names,
    Vector<Emitter *> &r_emitters,
    MultiMap<std::string, Event *> &r_events_per_type,
    MultiMap<std::string, OffsetHandler *> &r_offset_handler_per_type,
    StringMap<AttributesDeclaration> &r_attributes_per_type,
    StringMap<Integrator *> &r_integrators)
{
  SCOPED_TIMER(__func__);

  StringMap<ParseNodeCallback> &parsers = get_node_parsers();

  MultiMap<std::string, Force *> forces;
  BehaviorCollector collector = {
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

  for (VirtualNode *vnode : vtree_data.vtree().nodes_with_idname("bp_ParticleTypeNode")) {
    StringRef name = vnode->name();
    r_type_names.append(name);
  }

  for (std::string &type_name : r_type_names) {
    AttributesDeclaration attributes;
    attributes.add<float3>("Position", float3(0, 0, 0));
    attributes.add<float3>("Velocity", float3(0, 0, 0));
    attributes.add<float>("Size", 0.05f);
    attributes.add<rgba_f>("Color", rgba_f(1, 1, 1, 1));

    ArrayRef<Force *> forces = collector.m_forces.lookup_default(type_name);
    EulerIntegrator *integrator = new EulerIntegrator(forces);

    r_attributes_per_type.add_new(type_name, attributes);
    r_integrators.add_new(type_name, integrator);
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

    Vector<std::string> type_names;
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

    collect_particle_behaviors(vtree_data,
                               world_transition,
                               type_names,
                               emitters,
                               events,
                               offset_handlers,
                               attributes,
                               integrators);

    StringMap<ParticleTypeInfo> types_to_simulate;
    for (std::string name : type_names) {
      ParticleTypeInfo type_info = {
          &attributes.lookup(name),
          integrators.lookup(name),
          events.lookup_default(name),
          offset_handlers.lookup_default(name),
      };
      types_to_simulate.add_new(name, type_info);
    }

    simulate_particles(simulation_state, emitters, types_to_simulate);

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
