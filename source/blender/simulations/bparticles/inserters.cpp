
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

  SharedFunction fn = create_function(
      indexed_tree, data_graph, {node_inputs.get(1)}, "Compute Direction");
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

  SharedFunction fn = create_function(
      indexed_tree, data_graph, {node_inputs.get(1), node_inputs.get(2)}, bnode->name);
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

  SharedFunction fn = create_function(indexed_tree, data_graph, {node_inputs.get(1)}, bnode->name);
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

    bSocketList inputs = interface.inputs();
    SharedFunction fn = create_function(
        interface.indexed_tree(),
        interface.data_graph(),
        {inputs.get(0), inputs.get(1), inputs.get(2), inputs.get(3), inputs.get(4)},
        interface.bnode()->name);

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

static void INSERT_EVENT_age_reached(ProcessNodeInterface &interface)
{
  FN::SharedFunction fn = create_function(interface.indexed_tree(),
                                          interface.data_graph(),
                                          {interface.inputs().get(1)},
                                          interface.bnode()->name);

  for (SocketWithNode linked : interface.linked_with_input(0)) {
    if (!is_particle_type_node(linked.node)) {
      continue;
    }

    auto action = build_action({interface.outputs().get(0), interface.bnode()},
                               interface.indexed_tree(),
                               interface.data_graph(),
                               interface.step_description());
    auto event = EVENT_age_reached(interface.bnode()->name, fn, std::move(action));

    bNode *type_node = linked.node;
    interface.step_description()
        .m_types.lookup_ref(type_node->name)
        ->m_events.append(event.release());
  }
}

static void INSERT_EVENT_mesh_collision(ProcessNodeInterface &interface)
{
  for (SocketWithNode linked : interface.linked_with_input(0)) {
    if (!is_particle_type_node(linked.node)) {
      continue;
    }

    PointerRNA rna = interface.node_rna();
    Object *object = (Object *)RNA_pointer_get(&rna, "object").id.data;
    if (object == nullptr || object->type != OB_MESH) {
      continue;
    }

    auto action = build_action({interface.outputs().get(0), interface.bnode()},
                               interface.indexed_tree(),
                               interface.data_graph(),
                               interface.step_description());
    auto event = EVENT_mesh_collision(interface.bnode()->name, object, std::move(action));

    bNode *type_node = linked.node;
    interface.step_description()
        .m_types.lookup_ref(type_node->name)
        ->m_events.append(event.release());
  }
}

static void INSERT_FORCE_gravity(ProcessNodeInterface &interface)
{
  for (SocketWithNode linked : interface.linked_with_output(0)) {
    if (!is_particle_type_node(linked.node)) {
      continue;
    }

    SharedFunction fn = create_function(interface.indexed_tree(),
                                        interface.data_graph(),
                                        {interface.inputs().get(0)},
                                        interface.bnode()->name);

    Force *force = FORCE_gravity(fn);

    bNode *type_node = linked.node;
    EulerIntegrator *integrator = reinterpret_cast<EulerIntegrator *>(
        interface.step_description().m_types.lookup_ref(type_node->name)->m_integrator);
    integrator->add_force(std::unique_ptr<Force>(force));
  }
}

static void INSERT_FORCE_turbulence(ProcessNodeInterface &interface)
{
  for (SocketWithNode linked : interface.linked_with_output(0)) {
    if (!is_particle_type_node(linked.node)) {
      continue;
    }

    SharedFunction fn = create_function(interface.indexed_tree(),
                                        interface.data_graph(),
                                        {interface.inputs().get(0)},
                                        interface.bnode()->name);

    Force *force = FORCE_turbulence(fn);

    bNode *type_node = linked.node;
    EulerIntegrator *integrator = reinterpret_cast<EulerIntegrator *>(
        interface.step_description().m_types.lookup_ref(type_node->name)->m_integrator);
    integrator->add_force(std::unique_ptr<Force>(force));
  }
}

BLI_LAZY_INIT(ProcessFunctionsMap, get_node_processors)
{
  ProcessFunctionsMap processors;
  processors.add_new("bp_MeshEmitterNode", INSERT_EMITTER_mesh_surface);
  processors.add_new("bp_PointEmitterNode", INSERT_EMITTER_point);
  processors.add_new("bp_AgeReachedEventNode", INSERT_EVENT_age_reached);
  processors.add_new("bp_MeshCollisionEventNode", INSERT_EVENT_mesh_collision);
  processors.add_new("bp_GravityForceNode", INSERT_FORCE_gravity);
  processors.add_new("bp_TurbulenceForceNode", INSERT_FORCE_turbulence);
  return processors;
}

}  // namespace BParticles
