
#include "FN_data_flow_nodes.hpp"
#include "FN_tuple_call.hpp"
#include "FN_dependencies.hpp"
#include "FN_types.hpp"

#include "BLI_timeit.hpp"
#include "BLI_lazy_init.hpp"

#include "inserters.hpp"
#include "step_description.hpp"
#include "actions.hpp"
#include "emitters.hpp"
#include "events.hpp"
#include "forces.hpp"
#include "integrator.hpp"
#include "offset_handlers.hpp"

namespace BParticles {

using BLI::ValueOrError;
using FN::DFGraphSocket;
using FN::FunctionBuilder;
using FN::FunctionGraph;
using FN::SharedFunction;
using FN::SharedType;

static Vector<DFGraphSocket> find_input_data_sockets(VirtualNode *vnode,
                                                     VTreeDataGraph &data_graph)
{
  Vector<DFGraphSocket> inputs;
  for (VirtualSocket *vsocket : vnode->inputs()) {
    DFGraphSocket *socket = data_graph.lookup_socket_ptr(vsocket);
    if (socket != nullptr) {
      inputs.append(*socket);
    }
  }
  return inputs;
}

static ValueOrError<SharedFunction> create_function__emitter_inputs(VirtualNode *emitter_vnode,
                                                                    VTreeDataGraph &data_graph)
{
  Vector<DFGraphSocket> sockets_to_compute = find_input_data_sockets(emitter_vnode, data_graph);
  auto dependencies = data_graph.find_placeholder_dependencies(sockets_to_compute);

  if (dependencies.size() > 0) {
    return BLI_ERROR_CREATE("Emitter inputs cannot have dependencies currently.");
  }

  FunctionGraph fgraph(data_graph.graph(), {}, sockets_to_compute);
  SharedFunction fn = fgraph.new_function(emitter_vnode->name());
  FN::fgraph_add_TupleCallBody(fn, fgraph);
  return fn;
}

static ValueOrError<SharedFunction> create_function__force_inputs(VirtualNode *force_vnode,
                                                                  VTreeDataGraph &data_graph)
{
  Vector<DFGraphSocket> sockets_to_compute = find_input_data_sockets(force_vnode, data_graph);
  auto dependencies = data_graph.find_placeholder_dependencies(sockets_to_compute);

  if (dependencies.size() > 0) {
    return BLI_ERROR_CREATE("Force inputs cannot have dependencies currently.");
  }

  FunctionGraph fgraph(data_graph.graph(), {}, sockets_to_compute);
  SharedFunction fn = fgraph.new_function(force_vnode->name());
  FN::fgraph_add_TupleCallBody(fn, fgraph);
  return fn;
}

static ValueOrError<SharedFunction> create_function__event_inputs(VirtualNode *event_vnode,
                                                                  VTreeDataGraph &data_graph)
{
  Vector<DFGraphSocket> sockets_to_compute = find_input_data_sockets(event_vnode, data_graph);
  auto dependencies = data_graph.find_placeholder_dependencies(sockets_to_compute);

  if (dependencies.size() > 0) {
    return BLI_ERROR_CREATE("Event inputs cannot have dependencies currently.");
  }

  FunctionGraph fgraph(data_graph.graph(), {}, sockets_to_compute);
  SharedFunction fn = fgraph.new_function(event_vnode->name());
  FN::fgraph_add_TupleCallBody(fn, fgraph);
  return fn;
}

static ValueOrError<SharedFunction> create_function__offset_handler_inputs(
    VirtualNode *offset_handler_vnode, VTreeDataGraph &data_graph)
{
  Vector<DFGraphSocket> sockets_to_compute = find_input_data_sockets(offset_handler_vnode,
                                                                     data_graph);
  auto dependencies = data_graph.find_placeholder_dependencies(sockets_to_compute);

  if (dependencies.size() > 0) {
    return BLI_ERROR_CREATE("Offset handler inputs cannot have dependencies currently.");
  }

  FunctionGraph fgraph(data_graph.graph(), {}, sockets_to_compute);
  SharedFunction fn = fgraph.new_function(offset_handler_vnode->name());
  FN::fgraph_add_TupleCallBody(fn, fgraph);
  return fn;
}

static ValueOrError<SharedFunction> create_function__action_inputs(VirtualNode *action_vnode,
                                                                   VTreeDataGraph &data_graph)
{
  Vector<DFGraphSocket> sockets_to_compute = find_input_data_sockets(action_vnode, data_graph);
  auto dependencies = data_graph.find_placeholder_dependencies(sockets_to_compute);

  FunctionBuilder fn_builder;
  fn_builder.add_outputs(data_graph.graph(), sockets_to_compute);

  for (uint i = 0; i < dependencies.size(); i++) {
    VirtualSocket *vsocket = dependencies.vsockets[i];
    DFGraphSocket socket = dependencies.sockets[i];
    VirtualNode *vnode = vsocket->vnode();

    SharedType &type = data_graph.graph()->type_of_output(socket);
    std::string name_prefix;
    if (STREQ(vnode->idname(), "bp_ParticleInfoNode")) {
      name_prefix = "Attribute: ";
    }
    else if (STREQ(vnode->idname(), "bp_CollisionInfoNode")) {
      name_prefix = "Event: ";
    }
    else {
      BLI_assert(false);
    }
    fn_builder.add_input(name_prefix + vsocket->name(), type);
  }

  SharedFunction fn = fn_builder.build(action_vnode->name());

  FunctionGraph fgraph(data_graph.graph(), dependencies.sockets, sockets_to_compute);
  FN::fgraph_add_TupleCallBody(fn, fgraph);
  return fn;
}

static std::unique_ptr<Action> build_action(BuildContext &ctx,
                                            VirtualSocket *start,
                                            VirtualSocket *trigger);
using ActionFromNodeCallback = std::function<std::unique_ptr<Action>(
    BuildContext &ctx, VirtualSocket *start, VirtualSocket *trigger)>;

static std::unique_ptr<Action> BUILD_ACTION_kill(BuildContext &UNUSED(ctx),
                                                 VirtualSocket *UNUSED(start),
                                                 VirtualSocket *UNUSED(trigger))
{
  return std::unique_ptr<Action>(new KillAction());
}

static std::unique_ptr<Action> BUILD_ACTION_change_direction(BuildContext &ctx,
                                                             VirtualSocket *start,
                                                             VirtualSocket *trigger)
{
  VirtualNode *vnode = start->vnode();
  auto fn_or_error = create_function__action_inputs(vnode, ctx.data_graph);
  if (fn_or_error.is_error()) {
    return {};
  }

  SharedFunction fn = fn_or_error.extract_value();
  ParticleFunction particle_fn(fn);
  auto post_action = build_action(ctx, vnode->output(0), trigger);

  return std::unique_ptr<ChangeDirectionAction>(
      new ChangeDirectionAction(particle_fn, std::move(post_action)));
}

static std::unique_ptr<Action> BUILD_ACTION_explode(BuildContext &ctx,
                                                    VirtualSocket *start,
                                                    VirtualSocket *trigger)
{
  VirtualNode *vnode = start->vnode();
  auto fn_or_error = create_function__action_inputs(vnode, ctx.data_graph);
  if (fn_or_error.is_error()) {
    return {};
  }

  SharedFunction fn = fn_or_error.extract_value();
  ParticleFunction particle_fn(fn);

  PointerRNA rna = vnode->rna();
  char name[65];
  RNA_string_get(&rna, "particle_type_name", name);

  auto post_action = build_action(ctx, vnode->output(0), trigger);

  if (ctx.type_name_exists(name)) {
    return std::unique_ptr<Action>(new ExplodeAction(name, particle_fn, std::move(post_action)));
  }
  else {
    return post_action;
  }
}

static std::unique_ptr<Action> BUILD_ACTION_condition(BuildContext &ctx,
                                                      VirtualSocket *start,
                                                      VirtualSocket *trigger)
{
  VirtualNode *vnode = start->vnode();
  auto fn_or_error = create_function__action_inputs(vnode, ctx.data_graph);
  if (fn_or_error.is_error()) {
    return {};
  }

  SharedFunction fn = fn_or_error.extract_value();
  ParticleFunction particle_fn(fn);

  auto true_action = build_action(ctx, vnode->output(0), trigger);
  auto false_action = build_action(ctx, vnode->output(1), trigger);

  return std::unique_ptr<Action>(
      new ConditionAction(particle_fn, std::move(true_action), std::move(false_action)));
}

BLI_LAZY_INIT_STATIC(StringMap<ActionFromNodeCallback>, get_action_builders)
{
  StringMap<ActionFromNodeCallback> map;
  map.add_new("bp_KillParticleNode", BUILD_ACTION_kill);
  map.add_new("bp_ChangeParticleDirectionNode", BUILD_ACTION_change_direction);
  map.add_new("bp_ExplodeParticleNode", BUILD_ACTION_explode);
  map.add_new("bp_ParticleConditionNode", BUILD_ACTION_condition);
  return map;
}

static std::unique_ptr<Action> build_action(BuildContext &ctx,
                                            VirtualSocket *start,
                                            VirtualSocket *trigger)
{
  if (start->is_output()) {
    ArrayRef<VirtualSocket *> linked = start->links();
    if (linked.size() == 0) {
      return std::unique_ptr<Action>(new NoneAction());
    }
    else if (linked.size() == 1) {
      return build_action(ctx, linked[0], trigger);
    }
    else {
      return nullptr;
    }
  }

  BLI_assert(start->is_input());
  StringRef idname = start->vnode()->idname();

  auto builders = get_action_builders();
  return builders.lookup(idname)(ctx, start, trigger);
}

static std::unique_ptr<Action> build_action_for_trigger(BuildContext &ctx, VirtualSocket *start)
{
  return build_action(ctx, start, start);
}

static std::unique_ptr<Force> BUILD_FORCE_gravity(BuildContext &ctx, VirtualNode *vnode)
{
  auto fn_or_error = create_function__force_inputs(vnode, ctx.data_graph);
  if (fn_or_error.is_error()) {
    return {};
  }

  SharedFunction fn = fn_or_error.extract_value();
  return std::unique_ptr<Force>(new GravityForce(fn));
}

static std::unique_ptr<Force> BUILD_FORCE_turbulence(BuildContext &ctx, VirtualNode *vnode)
{
  auto fn_or_error = create_function__force_inputs(vnode, ctx.data_graph);
  if (fn_or_error.is_error()) {
    return {};
  }

  SharedFunction fn = fn_or_error.extract_value();
  return std::unique_ptr<Force>(new TurbulenceForce(fn));
}

static std::unique_ptr<Event> BUILD_EVENT_mesh_collision(BuildContext &ctx, VirtualNode *vnode)
{
  PointerRNA rna = vnode->rna();
  Object *object = (Object *)RNA_pointer_get(&rna, "object").id.data;
  if (object == nullptr || object->type != OB_MESH) {
    return {};
  }

  auto action = build_action_for_trigger(ctx, vnode->output(0));
  return std::unique_ptr<Event>(new MeshCollisionEvent(vnode->name(), object, std::move(action)));
}

static std::unique_ptr<Event> BUILD_EVENT_age_reached(BuildContext &ctx, VirtualNode *vnode)
{
  auto fn_or_error = create_function__event_inputs(vnode, ctx.data_graph);
  if (fn_or_error.is_error()) {
    return {};
  }

  SharedFunction fn = fn_or_error.extract_value();
  auto action = build_action_for_trigger(ctx, vnode->output(0));
  return std::unique_ptr<Event>(new AgeReachedEvent(vnode->name(), fn, std::move(action)));
}

static std::unique_ptr<Event> BUILD_EVENT_close_by_points(BuildContext &ctx, VirtualNode *vnode)
{
  auto fn_or_error = create_function__event_inputs(vnode, ctx.data_graph);
  if (fn_or_error.is_error()) {
    return {};
  }

  SharedFunction fn = fn_or_error.extract_value();
  auto action = build_action_for_trigger(ctx, vnode->output(0));

  TupleCallBody *body = fn->body<TupleCallBody>();
  FN_TUPLE_CALL_ALLOC_TUPLES(body, fn_in, fn_out);
  body->call__setup_execution_context(fn_in, fn_out);

  auto vectors = fn_out.relocate_out<FN::Types::SharedFloat3List>(0);
  float distance = body->get_output<float>(fn_out, 1, "Distance");

  KDTree_3d *kdtree = BLI_kdtree_3d_new(vectors->size());
  for (float3 vector : *vectors.ptr()) {
    BLI_kdtree_3d_insert(kdtree, 0, vector);
  }
  BLI_kdtree_3d_balance(kdtree);

  return std::unique_ptr<Event>(
      new CloseByPointsEvent(vnode->name(), kdtree, distance, std::move(action)));
}

static std::unique_ptr<Emitter> BUILD_EMITTER_mesh_surface(BuildContext &ctx,
                                                           VirtualNode *vnode,
                                                           StringRef particle_type_name)
{
  auto fn_or_error = create_function__emitter_inputs(vnode, ctx.data_graph);
  if (fn_or_error.is_error()) {
    return {};
  }
  SharedFunction fn = fn_or_error.extract_value();

  auto body = fn->body<TupleCallBody>();
  FN_TUPLE_CALL_ALLOC_TUPLES(body, fn_in, fn_out);
  body->call__setup_execution_context(fn_in, fn_out);

  auto on_birth_action = build_action_for_trigger(ctx, vnode->output(0));

  Object *object = body->get_output<Object *>(fn_out, 0, "Object");
  if (object == nullptr) {
    return {};
  }
  InterpolatedFloat4x4 transform = ctx.world_state.get_interpolated_value(vnode->name(),
                                                                          object->obmat);

  return std::unique_ptr<SurfaceEmitter>(
      new SurfaceEmitter(particle_type_name,
                         std::move(on_birth_action),
                         object,
                         transform,
                         body->get_output<float>(fn_out, 1, "Rate"),
                         body->get_output<float>(fn_out, 2, "Normal Velocity"),
                         body->get_output<float>(fn_out, 3, "Emitter Velocity"),
                         body->get_output<float>(fn_out, 4, "Size")));
}

static std::unique_ptr<Emitter> BUILD_EMITTER_moving_point(BuildContext &ctx,
                                                           VirtualNode *vnode,
                                                           StringRef particle_type_name)
{
  auto fn_or_error = create_function__emitter_inputs(vnode, ctx.data_graph);
  if (fn_or_error.is_error()) {
    return {};
  }
  SharedFunction fn = fn_or_error.extract_value();

  auto body = fn->body<TupleCallBody>();
  FN_TUPLE_CALL_ALLOC_TUPLES(body, fn_in, fn_out);
  body->call__setup_execution_context(fn_in, fn_out);

  StringRef bnode_name = vnode->name();

  auto point = ctx.world_state.get_interpolated_value(
      bnode_name + StringRef("Position"), body->get_output<float3>(fn_out, 0, "Position"));
  auto velocity = ctx.world_state.get_interpolated_value(
      bnode_name + StringRef("Velocity"), body->get_output<float3>(fn_out, 1, "Velocity"));
  auto size = ctx.world_state.get_interpolated_value(bnode_name + StringRef("Size"),
                                                     body->get_output<float>(fn_out, 2, "Size"));
  return std::unique_ptr<PointEmitter>(
      new PointEmitter(particle_type_name, 10, point, velocity, size));
}

static void match_inputs_to_node_outputs(FN::DataFlowGraphBuilder &builder,
                                         FN::DFGB_Node *target_node,
                                         FN::DFGB_Node *origin_node_1,
                                         FN::DFGB_Node *origin_node_2)
{
  SharedFunction &target_fn = target_node->function();
  SharedFunction &origin_fn_1 = origin_node_1->function();

  uint offset = 0;
  for (uint i = 0; i < target_fn->input_amount(); i++) {
    StringRef input_name = target_fn->input_name(i);
    FN::SharedType &input_type = target_fn->input_type(i);

    bool is_reserved_input = false;
    for (uint j = 0; j < origin_fn_1->output_amount(); j++) {
      if (origin_fn_1->output_name(j) == input_name && origin_fn_1->output_type(j) == input_type) {
        builder.insert_link(origin_node_1->output(j), target_node->input(i));
        is_reserved_input = true;
      }
    }

    if (!is_reserved_input) {
      builder.insert_link(origin_node_2->output(offset), target_node->input(i));
      offset++;
    }
  }
}

static FN::FunctionGraph link_inputs_to_function(SharedFunction &main_fn,
                                                 SharedFunction &inputs_fn,
                                                 SharedFunction &reserved_fn)
{
  FN::DataFlowGraphBuilder builder;
  auto *main_node = builder.insert_function(main_fn);
  auto *inputs_node = builder.insert_function(inputs_fn);
  auto *reserved_node = builder.insert_function(reserved_fn);

  match_inputs_to_node_outputs(builder, main_node, reserved_node, inputs_node);

  auto build_result = FN::DataFlowGraph::FromBuilder(builder);
  auto final_inputs = build_result.mapping.map_sockets(reserved_node->outputs());
  auto final_outputs = build_result.mapping.map_sockets(main_node->outputs());
  return FN::FunctionGraph(build_result.graph, final_inputs, final_outputs);
}

static std::unique_ptr<Emitter> BUILD_EMITTER_custom_function(BuildContext &ctx,
                                                              VirtualNode *vnode,
                                                              StringRef particle_type_name)
{
  PointerRNA rna = vnode->rna();
  bNodeTree *btree = (bNodeTree *)RNA_pointer_get(&rna, "function_tree").id.data;
  if (btree == nullptr) {
    return {};
  }

  auto fn_emitter_or_error = FN::DataFlowNodes::generate_function(btree);
  auto fn_inputs_or_error = create_function__emitter_inputs(vnode, ctx.data_graph);
  if (fn_emitter_or_error.is_error()) {
    return {};
  }
  if (fn_inputs_or_error.is_error()) {
    return {};
  }
  SharedFunction fn_emitter = fn_emitter_or_error.extract_value();
  SharedFunction fn_inputs = fn_inputs_or_error.extract_value();

  FN::FunctionBuilder fn_builder;
  fn_builder.add_output("Start Time", FN::Types::GET_TYPE_float());
  fn_builder.add_output("Time Step", FN::Types::GET_TYPE_float());
  SharedFunction fn_reserved_inputs = fn_builder.build("Reserved Inputs");

  FN::FunctionGraph fgraph = link_inputs_to_function(fn_emitter, fn_inputs, fn_reserved_inputs);
  auto fn = fgraph.new_function("Emitter");
  FN::fgraph_add_DependenciesBody(fn, fgraph);
  FN::fgraph_add_TupleCallBody(fn, fgraph);

  return std::unique_ptr<Emitter>(new CustomFunctionEmitter(particle_type_name, fn));
}

static std::unique_ptr<Emitter> BUILD_EMITTER_initial_grid(BuildContext &ctx,
                                                           VirtualNode *vnode,
                                                           StringRef particle_type_name)
{
  auto fn_or_error = create_function__emitter_inputs(vnode, ctx.data_graph);
  if (fn_or_error.is_error()) {
    return {};
  }

  SharedFunction fn = fn_or_error.extract_value();
  TupleCallBody *body = fn->body<TupleCallBody>();
  FN_TUPLE_CALL_ALLOC_TUPLES(body, fn_in, fn_out);
  body->call__setup_execution_context(fn_in, fn_out);

  return std::unique_ptr<Emitter>(
      new InitialGridEmitter(particle_type_name,
                             body->get_output<uint>(fn_out, 0, "Amount X"),
                             body->get_output<uint>(fn_out, 1, "Amount Y"),
                             body->get_output<float>(fn_out, 2, "Step X"),
                             body->get_output<float>(fn_out, 3, "Step Y"),
                             body->get_output<float>(fn_out, 4, "Size")));
}

static std::unique_ptr<OffsetHandler> BUILD_OFFSET_HANDLER_trails(BuildContext &ctx,
                                                                  VirtualNode *vnode)
{
  PointerRNA rna = vnode->rna();
  char name[65];
  RNA_string_get(&rna, "particle_type_name", name);

  auto fn_or_error = create_function__offset_handler_inputs(vnode, ctx.data_graph);
  if (fn_or_error.is_error()) {
    return {};
  }

  SharedFunction fn = fn_or_error.extract_value();
  TupleCallBody *body = fn->body<TupleCallBody>();
  FN_TUPLE_CALL_ALLOC_TUPLES(body, fn_in, fn_out);
  body->call__setup_execution_context(fn_in, fn_out);
  float rate = body->get_output<float>(fn_out, 0, "Rate");
  rate = std::max(rate, 0.0f);

  if (ctx.type_name_exists(name)) {
    return std::unique_ptr<OffsetHandler>(new CreateTrailHandler(name, rate));
  }
  else {
    return {};
  }
}

BLI_LAZY_INIT(StringMap<ForceFromNodeCallback>, get_force_builders)
{
  StringMap<ForceFromNodeCallback> map;
  map.add_new("bp_GravityForceNode", BUILD_FORCE_gravity);
  map.add_new("bp_TurbulenceForceNode", BUILD_FORCE_turbulence);
  return map;
}

BLI_LAZY_INIT(StringMap<EventFromNodeCallback>, get_event_builders)
{
  StringMap<EventFromNodeCallback> map;
  map.add_new("bp_MeshCollisionEventNode", BUILD_EVENT_mesh_collision);
  map.add_new("bp_AgeReachedEventNode", BUILD_EVENT_age_reached);
  map.add_new("bp_CloseByPointsEventNode", BUILD_EVENT_close_by_points);
  return map;
}

BLI_LAZY_INIT(StringMap<EmitterFromNodeCallback>, get_emitter_builders)
{
  StringMap<EmitterFromNodeCallback> map;
  map.add_new("bp_PointEmitterNode", BUILD_EMITTER_moving_point);
  map.add_new("bp_MeshEmitterNode", BUILD_EMITTER_mesh_surface);
  map.add_new("bp_CustomEmitterNode", BUILD_EMITTER_custom_function);
  map.add_new("bp_InitialGridEmitterNode", BUILD_EMITTER_initial_grid);
  return map;
}

BLI_LAZY_INIT(StringMap<OffsetHandlerFromNodeCallback>, get_offset_handler_builders)
{
  StringMap<OffsetHandlerFromNodeCallback> map;
  map.add_new("bp_ParticleTrailsNode", BUILD_OFFSET_HANDLER_trails);
  return map;
}

}  // namespace BParticles
