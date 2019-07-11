
#include "FN_data_flow_nodes.hpp"
#include "FN_tuple_call.hpp"

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

static bool is_particle_type_node(bNode *bnode)
{
  return STREQ(bnode->idname, "bp_ParticleTypeNode");
}

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

static std::unique_ptr<Action> build_action(SocketWithNode start,
                                            IndexedNodeTree &indexed_tree,
                                            BTreeDataGraph &data_graph,
                                            ModifierStepDescription &step_description);

static std::unique_ptr<Action> BUILD_ACTION_kill()
{
  return ACTION_kill();
}

static std::unique_ptr<Action> BUILD_ACTION_change_direction(
    IndexedNodeTree &indexed_tree,
    BTreeDataGraph &data_graph,
    bNode *bnode,
    ModifierStepDescription &step_description)
{
  bSocketList node_inputs(bnode->inputs);
  bSocketList node_outputs(bnode->outputs);

  SharedFunction fn = create_function_for_data_inputs(bnode, indexed_tree, data_graph);
  ParticleFunction particle_fn(fn);
  return ACTION_change_direction(
      particle_fn,
      build_action({node_outputs.get(0), bnode}, indexed_tree, data_graph, step_description));
}

static std::unique_ptr<Action> BUILD_ACTION_explode(IndexedNodeTree &indexed_tree,
                                                    BTreeDataGraph &data_graph,
                                                    bNode *bnode,
                                                    ModifierStepDescription &step_description)
{
  bSocketList node_inputs(bnode->inputs);
  bSocketList node_outputs(bnode->outputs);

  SharedFunction fn = create_function_for_data_inputs(bnode, indexed_tree, data_graph);
  ParticleFunction particle_fn(fn);

  PointerRNA rna = indexed_tree.get_rna(bnode);
  char name[65];
  RNA_string_get(&rna, "particle_type_name", name);

  auto post_action = build_action(
      {node_outputs.get(0), bnode}, indexed_tree, data_graph, step_description);

  if (step_description.m_types.contains(name)) {
    return ACTION_explode(name, particle_fn, std::move(post_action));
  }
  else {
    return post_action;
  }
}

static std::unique_ptr<Action> BUILD_ACTION_condition(IndexedNodeTree &indexed_tree,
                                                      BTreeDataGraph &data_graph,
                                                      bNode *bnode,
                                                      ModifierStepDescription &step_description)
{
  bSocketList node_inputs(bnode->inputs);
  bSocketList node_outputs(bnode->outputs);

  SharedFunction fn = create_function_for_data_inputs(bnode, indexed_tree, data_graph);
  ParticleFunction particle_fn(fn);

  auto true_action = build_action(
      {node_outputs.get(0), bnode}, indexed_tree, data_graph, step_description);
  auto false_action = build_action(
      {node_outputs.get(1), bnode}, indexed_tree, data_graph, step_description);

  return ACTION_condition(particle_fn, std::move(true_action), std::move(false_action));
}

static std::unique_ptr<Action> build_action(SocketWithNode start,
                                            IndexedNodeTree &indexed_tree,
                                            BTreeDataGraph &data_graph,
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
    return BUILD_ACTION_kill();
  }
  else if (STREQ(bnode->idname, "bp_ChangeParticleDirectionNode")) {
    return BUILD_ACTION_change_direction(indexed_tree, data_graph, bnode, step_description);
  }
  else if (STREQ(bnode->idname, "bp_ExplodeParticleNode")) {
    return BUILD_ACTION_explode(indexed_tree, data_graph, bnode, step_description);
  }
  else if (STREQ(bnode->idname, "bp_ParticleConditionNode")) {
    return BUILD_ACTION_condition(indexed_tree, data_graph, bnode, step_description);
  }
  else {
    return nullptr;
  }
}

static void INSERT_EMITTER_mesh_surface(ProcessNodeInterface &interface)
{
  for (SocketWithNode linked : interface.linked_with_output(1)) {
    if (!is_particle_type_node(linked.node)) {
      continue;
    }

    SharedFunction fn = create_function_for_data_inputs(
        interface.bnode(), interface.indexed_tree(), interface.data_graph());

    auto action = build_action({interface.outputs().get(0), interface.bnode()},
                               interface.indexed_tree(),
                               interface.data_graph(),
                               interface.step_description());

    bNode *type_node = linked.node;
    Emitter *emitter = EMITTER_mesh_surface(
        type_node->name, fn, interface.world_state(), std::move(action));
    interface.step_description().m_emitters.append(emitter);
  }
}

static void INSERT_EMITTER_point(ProcessNodeInterface &interface)
{
  for (SocketWithNode linked : interface.linked_with_output(0)) {
    if (!is_particle_type_node(linked.node)) {
      continue;
    }

    float3 position;
    PointerRNA rna = interface.node_rna();
    RNA_float_get_array(&rna, "position", position);

    bNode *type_node = linked.node;
    Emitter *emitter = EMITTER_point(type_node->name, position);

    interface.step_description().m_emitters.append(emitter);
  }
}

BLI_LAZY_INIT(ProcessFunctionsMap, get_node_processors)
{
  ProcessFunctionsMap processors;
  processors.add_new("bp_MeshEmitterNode", INSERT_EMITTER_mesh_surface);
  processors.add_new("bp_PointEmitterNode", INSERT_EMITTER_point);
  return processors;
}

static std::unique_ptr<Force> BUILD_FORCE_gravity(BuildContext &ctx, bNode *bnode)
{
  SharedFunction fn = create_function_for_data_inputs(bnode, ctx.indexed_tree, ctx.data_graph);
  return FORCE_gravity(fn);
}

static std::unique_ptr<Force> BUILD_FORCE_turbulence(BuildContext &ctx, bNode *bnode)
{
  SharedFunction fn = create_function_for_data_inputs(bnode, ctx.indexed_tree, ctx.data_graph);
  return FORCE_turbulence(fn);
}

BLI_LAZY_INIT(ForceFromNodeCallbackMap, get_force_builders)
{
  ForceFromNodeCallbackMap map;
  map.add_new("bp_GravityForceNode", BUILD_FORCE_gravity);
  map.add_new("bp_TurbulenceForceNode", BUILD_FORCE_turbulence);
  return map;
}

static std::unique_ptr<Event> Build_EVENT_mesh_collision(BuildContext &ctx, bNode *bnode)
{
  PointerRNA rna = ctx.indexed_tree.get_rna(bnode);
  Object *object = (Object *)RNA_pointer_get(&rna, "object").id.data;
  if (object == nullptr || object->type != OB_MESH) {
    return {};
  }

  auto action = build_action({bSocketList(bnode->outputs).get(0), bnode},
                             ctx.indexed_tree,
                             ctx.data_graph,
                             ctx.step_description);
  return EVENT_mesh_collision(bnode->name, object, std::move(action));
}

static std::unique_ptr<Event> BUILD_EVENT_age_reached(BuildContext &ctx, bNode *bnode)
{
  FN::SharedFunction fn = create_function_for_data_inputs(bnode, ctx.indexed_tree, ctx.data_graph);

  auto action = build_action({bSocketList(bnode->outputs).get(0), bnode},
                             ctx.indexed_tree,
                             ctx.data_graph,
                             ctx.step_description);
  return EVENT_age_reached(bnode->name, fn, std::move(action));
}

BLI_LAZY_INIT(EventFromNodeCallbackMap, get_event_builders)
{
  EventFromNodeCallbackMap map;
  map.add_new("bp_MeshCollisionEventNode", Build_EVENT_mesh_collision);
  map.add_new("bp_AgeReachedEventNode", BUILD_EVENT_age_reached);
  return map;
}

}  // namespace BParticles
