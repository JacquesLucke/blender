
#include "FN_data_flow_nodes.hpp"
#include "FN_tuple_call.hpp"

#include "BLI_timeit.hpp"

#include "inserters.hpp"
#include "core.hpp"
#include "actions.hpp"
#include "emitters.hpp"
#include "events.hpp"
#include "forces.hpp"
#include "integrator.hpp"

namespace BParticles {

using BKE::bSocketList;
using BKE::SocketWithNode;
using FN::SharedFunction;

using EmitterInserter = std::function<void(bNode *bnode,
                                           IndexedNodeTree &indexed_tree,
                                           FN::DataFlowNodes::GeneratedGraph &data_graph,
                                           ModifierStepDescription &step_description,
                                           WorldState &world_state)>;
using EventInserter = EmitterInserter;
using ModifierInserter = EmitterInserter;

static bool is_particle_type_node(bNode *bnode)
{
  return STREQ(bnode->idname, "bp_ParticleTypeNode");
}

static bool is_particle_data_input(bNode *bnode)
{
  return STREQ(bnode->idname, "bp_ParticleInfoNode") ||
         STREQ(bnode->idname, "bp_MeshCollisionEventNode");
}

static ArrayRef<bNode *> get_particle_type_nodes(IndexedNodeTree &indexed_tree)
{
  return indexed_tree.nodes_with_idname("bp_ParticleTypeNode");
}

static SmallVector<FN::DFGraphSocket> insert_inputs(FN::FunctionBuilder &fn_builder,
                                                    IndexedNodeTree &indexed_tree,
                                                    FN::DataFlowNodes::GeneratedGraph &data_graph,
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
                                      FN::DataFlowNodes::GeneratedGraph &data_graph,
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
  bSocketList node_outputs(bnode->outputs);

  if (STREQ(bnode->idname, "bp_KillParticleNode")) {
    return ACTION_kill();
  }
  else if (STREQ(bnode->idname, "bp_ChangeParticleDirectionNode")) {
    SharedFunction fn = create_function(
        indexed_tree, data_graph, {node_inputs.get(1)}, "Compute Direction");
    ParticleFunction particle_fn(fn);
    return ACTION_change_direction(
        particle_fn,
        build_action({node_outputs.get(0), bnode}, indexed_tree, data_graph, step_description));
  }
  else if (STREQ(bnode->idname, "bp_ExplodeParticleNode")) {
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
  else if (STREQ(bnode->idname, "bp_ParticleConditionNode")) {
    SharedFunction fn = create_function(
        indexed_tree, data_graph, {node_inputs.get(1)}, bnode->name);
    ParticleFunction particle_fn(fn);

    auto true_action = build_action(
        {node_outputs.get(0), bnode}, indexed_tree, data_graph, step_description);
    auto false_action = build_action(
        {node_outputs.get(1), bnode}, indexed_tree, data_graph, step_description);

    return ACTION_condition(particle_fn, std::move(true_action), std::move(false_action));
  }
  else {
    return nullptr;
  }
}

static void INSERT_EMITTER_mesh_surface(bNode *emitter_node,
                                        IndexedNodeTree &indexed_tree,
                                        FN::DataFlowNodes::GeneratedGraph &UNUSED(data_graph),
                                        ModifierStepDescription &step_description,
                                        WorldState &world_state)
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

    float4x4 last_transformation = world_state.update(emitter_node->name, object->obmat);
    float4x4 current_transformation = object->obmat;

    Emitter *emitter = EMITTER_mesh_surface(
        type_node->name, (Mesh *)object->data, last_transformation, current_transformation, 1.0f);
    step_description.m_emitters.append(emitter);
  }
}

static void INSERT_EMITTER_point(bNode *emitter_node,
                                 IndexedNodeTree &indexed_tree,
                                 FN::DataFlowNodes::GeneratedGraph &UNUSED(data_graph),
                                 ModifierStepDescription &step_description,
                                 WorldState &UNUSED(world_state))
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
                                     ModifierStepDescription &step_description,
                                     WorldState &UNUSED(world_state))
{
  BLI_assert(STREQ(event_node->idname, "bp_AgeReachedEventNode"));
  bSocketList node_inputs(event_node->inputs);
  FN::SharedFunction fn = create_function(
      indexed_tree, data_graph, {node_inputs.get(1)}, event_node->name);

  for (SocketWithNode linked : indexed_tree.linked(node_inputs.get(0))) {
    if (!is_particle_type_node(linked.node)) {
      continue;
    }

    auto action = build_action({(bNodeSocket *)event_node->outputs.first, event_node},
                               indexed_tree,
                               data_graph,
                               step_description);
    auto event = EVENT_age_reached(event_node->name, fn, std::move(action));

    bNode *type_node = linked.node;
    step_description.m_types.lookup_ref(type_node->name)->m_events.append(event.release());
  }
}

static void INSERT_EVENT_mesh_collision(bNode *event_node,
                                        IndexedNodeTree &indexed_tree,
                                        FN::DataFlowNodes::GeneratedGraph &data_graph,
                                        ModifierStepDescription &step_description,
                                        WorldState &UNUSED(world_state))
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

    auto action = build_action({(bNodeSocket *)event_node->outputs.first, event_node},
                               indexed_tree,
                               data_graph,
                               step_description);
    auto event = EVENT_mesh_collision(event_node->name, object, std::move(action));

    bNode *type_node = linked.node;
    step_description.m_types.lookup_ref(type_node->name)->m_events.append(event.release());
  }
}

static void INSERT_FORCE_gravity(bNode *force_node,
                                 IndexedNodeTree &indexed_tree,
                                 FN::DataFlowNodes::GeneratedGraph &data_graph,
                                 ModifierStepDescription &step_description,
                                 WorldState &UNUSED(world_state))
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
    EulerIntegrator *integrator = reinterpret_cast<EulerIntegrator *>(
        step_description.m_types.lookup_ref(type_node->name)->m_integrator);
    integrator->add_force(std::unique_ptr<Force>(force));
  }
}

static void INSERT_FORCE_turbulence(bNode *force_node,
                                    IndexedNodeTree &indexed_tree,
                                    FN::DataFlowNodes::GeneratedGraph &data_graph,
                                    ModifierStepDescription &step_description,
                                    WorldState &UNUSED(world_state))
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
    EulerIntegrator *integrator = reinterpret_cast<EulerIntegrator *>(
        step_description.m_types.lookup_ref(type_node->name)->m_integrator);
    integrator->add_force(std::unique_ptr<Force>(force));
  }
}

ModifierStepDescription *step_description_from_node_tree(IndexedNodeTree &indexed_tree,
                                                         WorldState &world_state)
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
      item.value(emitter_node, indexed_tree, generated_graph, *step_description, world_state);
    }
  }

  for (auto item : event_inserters.items()) {
    for (bNode *event_node : indexed_tree.nodes_with_idname(item.key)) {
      item.value(event_node, indexed_tree, generated_graph, *step_description, world_state);
    }
  }

  for (auto item : modifier_inserters.items()) {
    for (bNode *modifier_node : indexed_tree.nodes_with_idname(item.key)) {
      item.value(modifier_node, indexed_tree, generated_graph, *step_description, world_state);
    }
  }

  return step_description;
}

}  // namespace BParticles
