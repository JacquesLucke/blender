
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
#include "particle_function_builder.hpp"

namespace BParticles {

using BLI::ValueOrError;
using FN::DataSocket;
using FN::FunctionBuilder;
using FN::FunctionGraph;
using FN::SharedDataGraph;
using FN::SharedFunction;
using FN::Type;

static ValueOrError<SharedFunction> create_function__emitter_inputs(VirtualNode *emitter_vnode,
                                                                    VTreeDataGraph &data_graph)
{
  Vector<DataSocket> sockets_to_compute = find_input_data_sockets(emitter_vnode, data_graph);
  auto dependencies = data_graph.find_placeholder_dependencies(sockets_to_compute);

  if (dependencies.size() > 0) {
    return BLI_ERROR_CREATE("Emitter inputs cannot have dependencies currently.");
  }

  FunctionGraph fgraph(data_graph.graph(), {}, sockets_to_compute);
  SharedFunction fn = fgraph.new_function(emitter_vnode->name());
  FN::fgraph_add_TupleCallBody(fn, fgraph);
  return fn;
}

static std::unique_ptr<Action> build_action(BuildContext &ctx,
                                            VirtualSocket *start,
                                            VirtualSocket *trigger);
using ActionFromNodeCallback =
    std::function<std::unique_ptr<Action>(BuildContext &ctx,
                                          VirtualSocket *start,
                                          VirtualSocket *trigger,
                                          std::unique_ptr<ParticleFunction> compute_inputs_fn)>;

static std::unique_ptr<Action> BUILD_ACTION_kill(
    BuildContext &UNUSED(ctx),
    VirtualSocket *UNUSED(start),
    VirtualSocket *UNUSED(trigger),
    std::unique_ptr<ParticleFunction> UNUSED(compute_inputs_fn))
{
  return std::unique_ptr<Action>(new KillAction());
}

static std::unique_ptr<Action> BUILD_ACTION_change_direction(
    BuildContext &ctx,
    VirtualSocket *start,
    VirtualSocket *trigger,
    std::unique_ptr<ParticleFunction> compute_inputs_fn)
{
  VirtualNode *vnode = start->vnode();
  auto post_action = build_action(ctx, vnode->output(0), trigger);

  return std::unique_ptr<ChangeDirectionAction>(
      new ChangeDirectionAction(std::move(compute_inputs_fn), std::move(post_action)));
}

static std::unique_ptr<Action> BUILD_ACTION_change_color(
    BuildContext &ctx,
    VirtualSocket *start,
    VirtualSocket *trigger,
    std::unique_ptr<ParticleFunction> compute_inputs_fn)
{
  VirtualNode *vnode = start->vnode();
  auto post_action = build_action(ctx, vnode->output(0), trigger);

  return std::unique_ptr<ChangeColorAction>(
      new ChangeColorAction(std::move(compute_inputs_fn), std::move(post_action)));
}

static std::unique_ptr<Action> BUILD_ACTION_explode(
    BuildContext &ctx,
    VirtualSocket *start,
    VirtualSocket *trigger,
    std::unique_ptr<ParticleFunction> compute_inputs_fn)
{
  VirtualNode *vnode = start->vnode();

  PointerRNA rna = vnode->rna();
  char name[65];
  RNA_string_get(&rna, "particle_type_name", name);

  auto post_action = build_action(ctx, vnode->output(0), trigger);
  auto new_particles_action = build_action(ctx, vnode->output(1), trigger);

  if (ctx.type_name_exists(name)) {
    return std::unique_ptr<Action>(new ExplodeAction(name,
                                                     std::move(compute_inputs_fn),
                                                     std::move(post_action),
                                                     std::move(new_particles_action)));
  }
  else {
    return post_action;
  }
}

static std::unique_ptr<Action> BUILD_ACTION_condition(
    BuildContext &ctx,
    VirtualSocket *start,
    VirtualSocket *trigger,
    std::unique_ptr<ParticleFunction> compute_inputs_fn)
{
  VirtualNode *vnode = start->vnode();
  auto true_action = build_action(ctx, vnode->output(0), trigger);
  auto false_action = build_action(ctx, vnode->output(1), trigger);

  return std::unique_ptr<Action>(new ConditionAction(
      std::move(compute_inputs_fn), std::move(true_action), std::move(false_action)));
}

BLI_LAZY_INIT_STATIC(StringMap<ActionFromNodeCallback>, get_action_builders)
{
  StringMap<ActionFromNodeCallback> map;
  map.add_new("bp_KillParticleNode", BUILD_ACTION_kill);
  map.add_new("bp_ChangeParticleDirectionNode", BUILD_ACTION_change_direction);
  map.add_new("bp_ExplodeParticleNode", BUILD_ACTION_explode);
  map.add_new("bp_ParticleConditionNode", BUILD_ACTION_condition);
  map.add_new("bp_ChangeParticleColorNode", BUILD_ACTION_change_color);
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
  VirtualNode *vnode = start->vnode();

  auto fn_or_error = create_particle_function(vnode, ctx.data_graph);
  if (fn_or_error.is_error()) {
    return std::unique_ptr<Action>(new NoneAction());
  }

  StringRef idname = start->vnode()->idname();
  auto builders = get_action_builders();
  return builders.lookup(idname)(ctx, start, trigger, fn_or_error.extract_value());
}

static std::unique_ptr<Action> build_action_for_trigger(BuildContext &ctx, VirtualSocket *start)
{
  return build_action(ctx, start, start);
}

static std::unique_ptr<Force> BUILD_FORCE_gravity(
    BuildContext &UNUSED(ctx),
    VirtualNode *UNUSED(vnode),
    std::unique_ptr<ParticleFunction> compute_inputs_fn)
{
  return std::unique_ptr<Force>(new GravityForce(std::move(compute_inputs_fn)));
}

static std::unique_ptr<Force> BUILD_FORCE_turbulence(
    BuildContext &UNUSED(ctx),
    VirtualNode *UNUSED(vnode),
    std::unique_ptr<ParticleFunction> compute_inputs_fn)
{
  return std::unique_ptr<Force>(new TurbulenceForce(std::move(compute_inputs_fn)));
}

static std::unique_ptr<Force> BUILD_FORCE_point(
    BuildContext &UNUSED(ctx),
    VirtualNode *UNUSED(vnode),
    std::unique_ptr<ParticleFunction> compute_inputs_fn)
{
  return std::unique_ptr<Force>(new PointForce(std::move(compute_inputs_fn)));
}

static std::unique_ptr<Event> BUILD_EVENT_mesh_collision(
    BuildContext &ctx, VirtualNode *vnode, std::unique_ptr<ParticleFunction> compute_inputs_fn)
{
  if (compute_inputs_fn->parameter_depends_on_particle("Object", 0)) {
    return {};
  }

  SharedFunction &fn = compute_inputs_fn->function_no_deps();
  TupleCallBody &body = fn->body<TupleCallBody>();
  FN_TUPLE_CALL_ALLOC_TUPLES(body, fn_in, fn_out);
  body.call__setup_execution_context(fn_in, fn_out);

  Object *object = body.get_output<Object *>(fn_out, 0, "Object");
  if (object == nullptr || object->type != OB_MESH) {
    return {};
  }

  auto action = build_action_for_trigger(ctx, vnode->output(0));
  return std::unique_ptr<Event>(new MeshCollisionEvent(vnode->name(), object, std::move(action)));
}

static std::unique_ptr<Event> BUILD_EVENT_age_reached(
    BuildContext &ctx, VirtualNode *vnode, std::unique_ptr<ParticleFunction> compute_inputs_fn)
{
  auto action = build_action_for_trigger(ctx, vnode->output(0));
  return std::unique_ptr<Event>(
      new AgeReachedEvent(vnode->name(), std::move(compute_inputs_fn), std::move(action)));
}

static std::unique_ptr<Event> BUILD_EVENT_close_by_points(
    BuildContext &ctx, VirtualNode *vnode, std::unique_ptr<ParticleFunction> compute_inputs)
{
  if (compute_inputs->parameter_depends_on_particle("Points", 0)) {
    return {};
  }

  auto action = build_action_for_trigger(ctx, vnode->output(0));

  SharedFunction &fn = compute_inputs->function_no_deps();
  BLI_assert(fn->input_amount() == 0);
  TupleCallBody &body = fn->body<TupleCallBody>();
  FN_TUPLE_CALL_ALLOC_TUPLES(body, fn_in, fn_out);
  body.call__setup_execution_context(fn_in, fn_out);

  auto vectors = fn_out.relocate_out<FN::SharedList>(0);
  float distance = body.get_output<float>(fn_out, 1, "Distance");

  KDTree_3d *kdtree = BLI_kdtree_3d_new(vectors->size());
  for (float3 vector : vectors->as_array_ref<float3>()) {
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

  auto &body = fn->body<TupleCallBody>();
  FN_TUPLE_CALL_ALLOC_TUPLES(body, fn_in, fn_out);
  body.call__setup_execution_context(fn_in, fn_out);

  auto on_birth_action = build_action_for_trigger(ctx, vnode->output(0));

  Object *object = body.get_output<Object *>(fn_out, 0, "Object");
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
                         body.get_output<float>(fn_out, 1, "Rate"),
                         body.get_output<float>(fn_out, 2, "Normal Velocity"),
                         body.get_output<float>(fn_out, 3, "Emitter Velocity"),
                         body.get_output<float>(fn_out, 4, "Size")));
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

  auto &body = fn->body<TupleCallBody>();
  FN_TUPLE_CALL_ALLOC_TUPLES(body, fn_in, fn_out);
  body.call__setup_execution_context(fn_in, fn_out);

  StringRef bnode_name = vnode->name();

  auto point = ctx.world_state.get_interpolated_value(
      bnode_name + StringRef("Position"), body.get_output<float3>(fn_out, 0, "Position"));
  auto velocity = ctx.world_state.get_interpolated_value(
      bnode_name + StringRef("Velocity"), body.get_output<float3>(fn_out, 1, "Velocity"));
  auto size = ctx.world_state.get_interpolated_value(bnode_name + StringRef("Size"),
                                                     body.get_output<float>(fn_out, 2, "Size"));
  return std::unique_ptr<PointEmitter>(
      new PointEmitter(particle_type_name, 10, point, velocity, size));
}

static void match_inputs_to_node_outputs(FN::DataGraphBuilder &builder,
                                         FN::BuilderNode *target_node,
                                         FN::BuilderNode *origin_node_1,
                                         FN::BuilderNode *origin_node_2)
{
  SharedFunction &target_fn = target_node->function();
  SharedFunction &origin_fn_1 = origin_node_1->function();

  uint offset = 0;
  for (uint i = 0; i < target_fn->input_amount(); i++) {
    StringRef input_name = target_fn->input_name(i);
    FN::Type *input_type = target_fn->input_type(i);

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
  FN::DataGraphBuilder builder;
  FN::BuilderNode *main_node = builder.insert_function(main_fn);
  FN::BuilderNode *inputs_node = builder.insert_function(inputs_fn);
  FN::BuilderNode *reserved_node = builder.insert_function(reserved_fn);

  match_inputs_to_node_outputs(builder, main_node, reserved_node, inputs_node);

  SharedDataGraph data_graph = builder.build();

  auto final_inputs = reserved_node->built_outputs();
  auto final_outputs = main_node->built_outputs();
  return FN::FunctionGraph(data_graph, final_inputs, final_outputs);
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
  fn_builder.add_output("Start Time", FN::Types::TYPE_float);
  fn_builder.add_output("Time Step", FN::Types::TYPE_float);
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
  TupleCallBody &body = fn->body<TupleCallBody>();
  FN_TUPLE_CALL_ALLOC_TUPLES(body, fn_in, fn_out);
  body.call__setup_execution_context(fn_in, fn_out);

  return std::unique_ptr<Emitter>(
      new InitialGridEmitter(particle_type_name,
                             body.get_output<uint>(fn_out, 0, "Amount X"),
                             body.get_output<uint>(fn_out, 1, "Amount Y"),
                             body.get_output<float>(fn_out, 2, "Step X"),
                             body.get_output<float>(fn_out, 3, "Step Y"),
                             body.get_output<float>(fn_out, 4, "Size")));
}

static std::unique_ptr<OffsetHandler> BUILD_OFFSET_HANDLER_trails(
    BuildContext &ctx, VirtualNode *vnode, std::unique_ptr<ParticleFunction> compute_inputs_fn)
{
  PointerRNA rna = vnode->rna();
  char name[65];
  RNA_string_get(&rna, "particle_type_name", name);

  if (ctx.type_name_exists(name)) {
    return std::unique_ptr<OffsetHandler>(
        new CreateTrailHandler(name, std::move(compute_inputs_fn)));
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
  map.add_new("bp_PointForceNode", BUILD_FORCE_point);
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
