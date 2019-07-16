
#include "FN_data_flow_nodes.hpp"
#include "FN_tuple_call.hpp"
#include "FN_dependencies.hpp"
#include "FN_types.hpp"

#include "BLI_timeit.hpp"
#include "BLI_lazy_init.hpp"

#include "inserters.hpp"
#include "core.hpp"
#include "actions.hpp"
#include "emitters.hpp"
#include "events.hpp"
#include "forces.hpp"
#include "integrator.hpp"

namespace BParticles {

using FN::SharedFunction;

static bool is_particle_data_input(bNode *bnode)
{
  return STREQ(bnode->idname, "bp_ParticleInfoNode") ||
         STREQ(bnode->idname, "bp_MeshCollisionEventNode");
}

static SmallVector<FN::DFGraphSocket> insert_inputs(FN::FunctionBuilder &fn_builder,
                                                    IndexedNodeTree &indexed_tree,
                                                    BTreeDataGraph &data_graph,
                                                    ArrayRef<bNodeSocket *> output_sockets)
{
  SmallSet<bNodeSocket *> to_be_checked = output_sockets;
  SmallSet<bNodeSocket *> found_inputs;
  SmallVector<FN::DFGraphSocket> inputs;

  while (to_be_checked.size() > 0) {
    bNodeSocket *bsocket = to_be_checked.pop();
    if (bsocket->in_out == SOCK_IN) {
      auto linked = indexed_tree.linked(bsocket);
      BLI_assert(linked.size() <= 1);
      if (linked.size() == 1) {
        SocketWithNode origin = linked[0];
        if (is_particle_data_input(origin.node) && !found_inputs.contains(origin.socket)) {
          FN::DFGraphSocket socket = data_graph.lookup_socket(origin.socket);
          FN::SharedType &type = data_graph.graph()->type_of_socket(socket);
          std::string name_prefix;
          if (STREQ(origin.node->idname, "bp_ParticleInfoNode")) {
            name_prefix = "Attribute: ";
          }
          else if (STREQ(origin.node->idname, "bp_MeshCollisionEventNode")) {
            name_prefix = "Event: ";
          }
          fn_builder.add_input(name_prefix + origin.socket->name, type);
          found_inputs.add(origin.socket);
          inputs.append(socket);
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
  return inputs;
}

static SharedFunction create_function(IndexedNodeTree &indexed_tree,
                                      BTreeDataGraph &data_graph,
                                      ArrayRef<bNodeSocket *> output_bsockets,
                                      StringRef name)
{
  FN::FunctionBuilder fn_builder;
  auto inputs = insert_inputs(fn_builder, indexed_tree, data_graph, output_bsockets);

  SmallVector<FN::DFGraphSocket> outputs;
  for (bNodeSocket *bsocket : output_bsockets) {
    FN::DFGraphSocket socket = data_graph.lookup_socket(bsocket);
    fn_builder.add_output(bsocket->name, data_graph.graph()->type_of_socket(socket));
    outputs.append(socket);
  }

  FN::FunctionGraph function_graph(data_graph.graph(), inputs, outputs);
  SharedFunction fn = fn_builder.build(name);
  FN::fgraph_add_DependenciesBody(fn, function_graph);
  FN::fgraph_add_TupleCallBody(fn, function_graph);
  return fn;
}

static SharedFunction create_function_for_data_inputs(bNode *bnode,
                                                      IndexedNodeTree &indexed_tree,
                                                      BTreeDataGraph &data_graph)
{
  SmallVector<bNodeSocket *> bsockets_to_compute;
  for (bNodeSocket *bsocket : bSocketList(bnode->inputs)) {
    if (data_graph.uses_socket(bsocket)) {
      bsockets_to_compute.append(bsocket);
    }
  }
  return create_function(indexed_tree, data_graph, bsockets_to_compute, bnode->name);
}

static std::unique_ptr<Action> build_action(BuildContext &ctx, SocketWithNode start);
using ActionFromNodeCallback =
    std::function<std::unique_ptr<Action>(BuildContext &ctx, bNode *bnode)>;

static std::unique_ptr<Action> BUILD_ACTION_kill(BuildContext &UNUSED(ctx), bNode *UNUSED(bnode))
{
  return std::unique_ptr<Action>(new KillAction());
}

static std::unique_ptr<Action> BUILD_ACTION_change_direction(BuildContext &ctx, bNode *bnode)
{
  bSocketList node_inputs(bnode->inputs);
  bSocketList node_outputs(bnode->outputs);

  SharedFunction fn = create_function_for_data_inputs(bnode, ctx.indexed_tree, ctx.data_graph);
  ParticleFunction particle_fn(fn);
  auto post_action = build_action(ctx, {node_outputs.get(0), bnode});

  return std::unique_ptr<ChangeDirectionAction>(
      new ChangeDirectionAction(particle_fn, std::move(post_action)));
}

static std::unique_ptr<Action> BUILD_ACTION_explode(BuildContext &ctx, bNode *bnode)
{
  bSocketList node_inputs(bnode->inputs);
  bSocketList node_outputs(bnode->outputs);

  SharedFunction fn = create_function_for_data_inputs(bnode, ctx.indexed_tree, ctx.data_graph);
  ParticleFunction particle_fn(fn);

  PointerRNA rna = ctx.indexed_tree.get_rna(bnode);
  char name[65];
  RNA_string_get(&rna, "particle_type_name", name);

  auto post_action = build_action(ctx, {node_outputs.get(0), bnode});

  if (ctx.step_description.m_types.contains(name)) {
    return std::unique_ptr<Action>(new ExplodeAction(name, particle_fn, std::move(post_action)));
  }
  else {
    return post_action;
  }
}

static std::unique_ptr<Action> BUILD_ACTION_condition(BuildContext &ctx, bNode *bnode)
{
  bSocketList node_inputs(bnode->inputs);
  bSocketList node_outputs(bnode->outputs);

  SharedFunction fn = create_function_for_data_inputs(bnode, ctx.indexed_tree, ctx.data_graph);
  ParticleFunction particle_fn(fn);

  auto true_action = build_action(ctx, {node_outputs.get(0), bnode});
  auto false_action = build_action(ctx, {node_outputs.get(1), bnode});

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

static std::unique_ptr<Action> build_action(BuildContext &ctx, SocketWithNode start)
{
  if (start.socket->in_out == SOCK_OUT) {
    auto linked = ctx.indexed_tree.linked(start.socket);
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

  BLI_assert(start.socket->in_out == SOCK_IN);
  bNode *bnode = start.node;

  auto builders = get_action_builders();
  return builders.lookup(bnode->idname)(ctx, bnode);
}

static std::unique_ptr<Force> BUILD_FORCE_gravity(BuildContext &ctx, bNode *bnode)
{
  SharedFunction fn = create_function_for_data_inputs(bnode, ctx.indexed_tree, ctx.data_graph);
  return std::unique_ptr<Force>(new GravityForce(fn));
}

static std::unique_ptr<Force> BUILD_FORCE_turbulence(BuildContext &ctx, bNode *bnode)
{
  SharedFunction fn = create_function_for_data_inputs(bnode, ctx.indexed_tree, ctx.data_graph);
  return std::unique_ptr<Force>(new TurbulenceForce(fn));
}

static std::unique_ptr<Event> BUILD_EVENT_mesh_collision(BuildContext &ctx, bNode *bnode)
{
  PointerRNA rna = ctx.indexed_tree.get_rna(bnode);
  Object *object = (Object *)RNA_pointer_get(&rna, "object").id.data;
  if (object == nullptr || object->type != OB_MESH) {
    return {};
  }

  auto action = build_action(ctx, {bSocketList(bnode->outputs).get(0), bnode});
  return std::unique_ptr<Event>(new MeshCollisionEvent(bnode->name, object, std::move(action)));
}

static std::unique_ptr<Event> BUILD_EVENT_age_reached(BuildContext &ctx, bNode *bnode)
{
  FN::SharedFunction fn = create_function_for_data_inputs(bnode, ctx.indexed_tree, ctx.data_graph);
  auto action = build_action(ctx, {bSocketList(bnode->outputs).get(0), bnode});
  return std::unique_ptr<Event>(new AgeReachedEvent(bnode->name, fn, std::move(action)));
}

static std::unique_ptr<Emitter> BUILD_EMITTER_mesh_surface(BuildContext &ctx,
                                                           bNode *bnode,
                                                           StringRef particle_type_name)
{
  SharedFunction fn = create_function_for_data_inputs(bnode, ctx.indexed_tree, ctx.data_graph);
  BLI_assert(fn->input_amount() == 0);

  auto body = fn->body<TupleCallBody>();
  FN_TUPLE_CALL_ALLOC_TUPLES(body, fn_in, fn_out);
  body->call__setup_execution_context(fn_in, fn_out);

  auto on_birth_action = build_action(ctx, {bSocketList(bnode->outputs).get(0), bnode});

  Object *object = body->get_output<Object *>(fn_out, 0, "Object");
  InterpolatedFloat4x4 transform = ctx.world_state.get_interpolated_value(bnode->name,
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
                                                           bNode *bnode,
                                                           StringRef particle_type_name)
{
  SharedFunction fn = create_function_for_data_inputs(bnode, ctx.indexed_tree, ctx.data_graph);
  BLI_assert(fn->input_amount() == 0);

  auto body = fn->body<TupleCallBody>();
  FN_TUPLE_CALL_ALLOC_TUPLES(body, fn_in, fn_out);
  body->call__setup_execution_context(fn_in, fn_out);

  auto point = ctx.world_state.get_interpolated_value(
      bnode->name, body->get_output<float3>(fn_out, 0, "Position"));
  return std::unique_ptr<PointEmitter>(new PointEmitter(particle_type_name, point, 10));
}

static FN::FunctionGraph link_inputs_to_function(SharedFunction &main_fn,
                                                 SharedFunction &inputs_fn,
                                                 SharedFunction &reserved_fn)
{
  FN::DataFlowGraphBuilder builder;
  auto *main_node = builder.insert_function(main_fn);
  auto *inputs_node = builder.insert_function(inputs_fn);
  auto *reserved_node = builder.insert_function(reserved_fn);

  uint offset = 0;
  for (uint i = 0; i < main_fn->input_amount(); i++) {
    StringRef input_name = main_fn->input_name(i);
    FN::SharedType &input_type = main_fn->input_type(i);

    bool is_reserved_input = false;
    for (uint j = 0; j < reserved_fn->output_amount(); j++) {
      if (reserved_fn->output_name(j) == input_name && reserved_fn->output_type(j) == input_type) {
        builder.insert_link(reserved_node->output(j), main_node->input(i));
        is_reserved_input = true;
      }
    }

    if (!is_reserved_input) {
      builder.insert_link(inputs_node->output(offset), main_node->input(i));
      offset++;
    }
  }

  auto build_result = FN::DataFlowGraph::FromBuilder(builder);
  auto final_inputs = build_result.mapping.map_sockets(reserved_node->outputs());
  auto final_outputs = build_result.mapping.map_sockets(main_node->outputs());
  return FN::FunctionGraph(build_result.graph, final_inputs, final_outputs);
}

static std::unique_ptr<Emitter> BUILD_EMITTER_custom_function(BuildContext &ctx,
                                                              bNode *bnode,
                                                              StringRef particle_type_name)
{
  PointerRNA rna = ctx.indexed_tree.get_rna(bnode);
  bNodeTree *btree = (bNodeTree *)RNA_pointer_get(&rna, "function_tree").id.data;
  if (btree == nullptr) {
    return {};
  }

  Optional<SharedFunction> fn_emitter_ = FN::DataFlowNodes::generate_function(btree);
  if (!fn_emitter_.has_value()) {
    return {};
  }
  SharedFunction fn_emitter = fn_emitter_.value();

  SharedFunction fn_inputs = create_function_for_data_inputs(
      bnode, ctx.indexed_tree, ctx.data_graph);

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
  return map;
}

BLI_LAZY_INIT(StringMap<EmitterFromNodeCallback>, get_emitter_builders)
{
  StringMap<EmitterFromNodeCallback> map;
  map.add_new("bp_PointEmitterNode", BUILD_EMITTER_moving_point);
  map.add_new("bp_MeshEmitterNode", BUILD_EMITTER_mesh_surface);
  map.add_new("bp_CustomEmitterNode", BUILD_EMITTER_custom_function);
  return map;
}

}  // namespace BParticles
