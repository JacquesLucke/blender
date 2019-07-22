
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

using FN::SharedFunction;

static bool is_particle_data_input(VirtualNode *vnode)
{
  bNode *bnode = vnode->bnode();
  return STREQ(bnode->idname, "bp_ParticleInfoNode") ||
         STREQ(bnode->idname, "bp_MeshCollisionEventNode");
}

static SmallVector<FN::DFGraphSocket> insert_inputs(FN::FunctionBuilder &fn_builder,
                                                    BTreeDataGraph &data_graph,
                                                    ArrayRef<VirtualSocket *> output_vsockets)
{
  SmallSet<VirtualSocket *> to_be_checked = output_vsockets;
  SmallSet<VirtualSocket *> found_inputs;
  SmallVector<FN::DFGraphSocket> inputs;

  while (to_be_checked.size() > 0) {
    VirtualSocket *vsocket = to_be_checked.pop();
    if (vsocket->is_input()) {
      ArrayRef<VirtualSocket *> linked = vsocket->links();
      BLI_assert(linked.size() <= 1);
      if (linked.size() == 1) {
        VirtualSocket *origin = linked[0];
        if (is_particle_data_input(origin->vnode()) && !found_inputs.contains(origin)) {
          FN::DFGraphSocket socket = data_graph.lookup_socket(origin);
          FN::SharedType &type = data_graph.graph()->type_of_socket(socket);
          std::string name_prefix;
          if (STREQ(origin->vnode()->bnode()->idname, "bp_ParticleInfoNode")) {
            name_prefix = "Attribute: ";
          }
          else if (STREQ(origin->vnode()->bnode()->idname, "bp_MeshCollisionEventNode")) {
            name_prefix = "Event: ";
          }
          fn_builder.add_input(name_prefix + origin->bsocket()->name, type);
          found_inputs.add(origin);
          inputs.append(socket);
        }
        else {
          to_be_checked.add(origin);
        }
      }
    }
    else {
      VirtualNode *vnode = vsocket->vnode();
      for (VirtualSocket &input : vnode->inputs()) {
        to_be_checked.add(&input);
      }
    }
  }
  return inputs;
}

static SharedFunction create_function(BTreeDataGraph &data_graph,
                                      ArrayRef<VirtualSocket *> output_vsockets,
                                      StringRef name)
{
  FN::FunctionBuilder fn_builder;
  auto inputs = insert_inputs(fn_builder, data_graph, output_vsockets);

  SmallVector<FN::DFGraphSocket> outputs;
  for (VirtualSocket *vsocket : output_vsockets) {
    FN::DFGraphSocket socket = data_graph.lookup_socket(vsocket);
    fn_builder.add_output(vsocket->bsocket()->name, data_graph.graph()->type_of_socket(socket));
    outputs.append(socket);
  }

  FN::FunctionGraph function_graph(data_graph.graph(), inputs, outputs);
  SharedFunction fn = fn_builder.build(name);
  FN::fgraph_add_DependenciesBody(fn, function_graph);
  FN::fgraph_add_TupleCallBody(fn, function_graph);
  return fn;
}

static SharedFunction create_function_for_data_inputs(VirtualNode *vnode,
                                                      BTreeDataGraph &data_graph)
{
  SmallVector<VirtualSocket *> bsockets_to_compute;
  for (VirtualSocket &vsocket : vnode->inputs()) {
    if (data_graph.uses_socket(&vsocket)) {
      bsockets_to_compute.append(&vsocket);
    }
  }
  return create_function(data_graph, bsockets_to_compute, vnode->name());
}

static std::unique_ptr<Action> build_action(BuildContext &ctx, VirtualSocket *start);
using ActionFromNodeCallback =
    std::function<std::unique_ptr<Action>(BuildContext &ctx, VirtualNode *vnode)>;

static std::unique_ptr<Action> BUILD_ACTION_kill(BuildContext &UNUSED(ctx),
                                                 VirtualNode *UNUSED(vnode))
{
  return std::unique_ptr<Action>(new KillAction());
}

static std::unique_ptr<Action> BUILD_ACTION_change_direction(BuildContext &ctx, VirtualNode *vnode)
{
  SharedFunction fn = create_function_for_data_inputs(vnode, ctx.data_graph);
  ParticleFunction particle_fn(fn);
  auto post_action = build_action(ctx, vnode->output(0));

  return std::unique_ptr<ChangeDirectionAction>(
      new ChangeDirectionAction(particle_fn, std::move(post_action)));
}

static std::unique_ptr<Action> BUILD_ACTION_explode(BuildContext &ctx, VirtualNode *vnode)
{
  SharedFunction fn = create_function_for_data_inputs(vnode, ctx.data_graph);
  ParticleFunction particle_fn(fn);

  PointerRNA rna = vnode->rna();
  char name[65];
  RNA_string_get(&rna, "particle_type_name", name);

  auto post_action = build_action(ctx, vnode->output(0));

  if (ctx.step_builder.has_type(name)) {
    return std::unique_ptr<Action>(new ExplodeAction(name, particle_fn, std::move(post_action)));
  }
  else {
    return post_action;
  }
}

static std::unique_ptr<Action> BUILD_ACTION_condition(BuildContext &ctx, VirtualNode *vnode)
{
  SharedFunction fn = create_function_for_data_inputs(vnode, ctx.data_graph);
  ParticleFunction particle_fn(fn);

  auto true_action = build_action(ctx, vnode->output(0));
  auto false_action = build_action(ctx, vnode->output(1));

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

static std::unique_ptr<Action> build_action(BuildContext &ctx, VirtualSocket *start)
{
  if (start->is_output()) {
    ArrayRef<VirtualSocket *> linked = start->links();
    if (linked.size() == 0) {
      return std::unique_ptr<Action>(new NoneAction());
    }
    else if (linked.size() == 1) {
      return build_action(ctx, linked[0]);
    }
    else {
      return nullptr;
    }
  }

  BLI_assert(start->is_input());
  VirtualNode *vnode = start->vnode();
  StringRef idname = vnode->bnode()->idname;

  auto builders = get_action_builders();
  return builders.lookup(idname)(ctx, vnode);
}

static std::unique_ptr<Force> BUILD_FORCE_gravity(BuildContext &ctx, VirtualNode *vnode)
{
  SharedFunction fn = create_function_for_data_inputs(vnode, ctx.data_graph);
  return std::unique_ptr<Force>(new GravityForce(fn));
}

static std::unique_ptr<Force> BUILD_FORCE_turbulence(BuildContext &ctx, VirtualNode *vnode)
{
  SharedFunction fn = create_function_for_data_inputs(vnode, ctx.data_graph);
  return std::unique_ptr<Force>(new TurbulenceForce(fn));
}

static std::unique_ptr<Event> BUILD_EVENT_mesh_collision(BuildContext &ctx, VirtualNode *vnode)
{
  PointerRNA rna = vnode->rna();
  Object *object = (Object *)RNA_pointer_get(&rna, "object").id.data;
  if (object == nullptr || object->type != OB_MESH) {
    return {};
  }

  auto action = build_action(ctx, vnode->output(0));
  return std::unique_ptr<Event>(new MeshCollisionEvent(vnode->name(), object, std::move(action)));
}

static std::unique_ptr<Event> BUILD_EVENT_age_reached(BuildContext &ctx, VirtualNode *vnode)
{
  SharedFunction fn = create_function_for_data_inputs(vnode, ctx.data_graph);
  auto action = build_action(ctx, vnode->output(0));
  return std::unique_ptr<Event>(new AgeReachedEvent(vnode->name(), fn, std::move(action)));
}

static std::unique_ptr<Event> BUILD_EVENT_close_by_points(BuildContext &ctx, VirtualNode *vnode)
{
  SharedFunction fn = create_function_for_data_inputs(vnode, ctx.data_graph);
  auto action = build_action(ctx, vnode->output(0));

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
  SharedFunction fn = create_function_for_data_inputs(vnode, ctx.data_graph);
  BLI_assert(fn->input_amount() == 0);

  auto body = fn->body<TupleCallBody>();
  FN_TUPLE_CALL_ALLOC_TUPLES(body, fn_in, fn_out);
  body->call__setup_execution_context(fn_in, fn_out);

  auto on_birth_action = build_action(ctx, vnode->output(0));

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
  SharedFunction fn = create_function_for_data_inputs(vnode, ctx.data_graph);
  BLI_assert(fn->input_amount() == 0);

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

  Optional<SharedFunction> fn_emitter_ = FN::DataFlowNodes::generate_function(btree);
  if (!fn_emitter_.has_value()) {
    return {};
  }
  SharedFunction fn_emitter = fn_emitter_.value();

  SharedFunction fn_inputs = create_function_for_data_inputs(vnode, ctx.data_graph);

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
  SharedFunction fn = create_function_for_data_inputs(vnode, ctx.data_graph);

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

  SharedFunction fn = create_function_for_data_inputs(vnode, ctx.data_graph);
  TupleCallBody *body = fn->body<TupleCallBody>();
  FN_TUPLE_CALL_ALLOC_TUPLES(body, fn_in, fn_out);
  body->call__setup_execution_context(fn_in, fn_out);
  float rate = body->get_output<float>(fn_out, 0, "Rate");
  rate = std::max(rate, 0.0f);

  if (ctx.step_builder.has_type(name)) {
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
