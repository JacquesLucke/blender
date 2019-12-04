#include "BKE_inlined_node_tree.h"
#include "BKE_deform.h"
#include "BKE_surface_location.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_timeit.h"
#include "BLI_multi_map.h"
#include "BLI_set.h"
#include "BLI_lazy_init_cxx.h"

#include "FN_multi_functions.h"
#include "FN_generic_tuple.h"
#include "FN_inlined_tree_multi_function_network_generation.h"
#include "FN_multi_function_common_contexts.h"

#include "node_frontend.hpp"
#include "integrator.hpp"
#include "emitters.hpp"
#include "events.hpp"
#include "offset_handlers.hpp"
#include "simulate.hpp"

namespace BParticles {

using BKE::XGroupInput;
using BKE::XInputSocket;
using BKE::XNode;
using BKE::XOutputSocket;
using BKE::XSocket;
using BLI::destruct_ptr;
using BLI::MultiMap;
using BLI::ResourceCollector;
using BLI::rgba_f;
using BLI::Set;
using FN::AttributesInfoBuilder;
using FN::CPPType;
using FN::MFInputSocket;
using FN::MFOutputSocket;
using FN::MultiFunction;
using FN::NamedGenericTupleRef;
using FN::VTreeMFNetwork;

static StringRef particle_system_idname = "fn_ParticleSystemNode";
static StringRef combine_influences_idname = "fn_CombineInfluencesNode";

class VTreeData;
class InfluencesCollector;

using ActionParserCallback = std::function<std::unique_ptr<Action>(
    InfluencesCollector &collector, VTreeData &inlined_tree_data, const XSocket &execute_xsocket)>;
StringMap<ActionParserCallback> &get_action_parsers();

class InfluencesCollector {
 public:
  Vector<Emitter *> &m_emitters;
  MultiMap<std::string, Force *> &m_forces;
  MultiMap<std::string, Event *> &m_events;
  MultiMap<std::string, OffsetHandler *> &m_offset_handlers;
  StringMap<AttributesInfoBuilder *> &m_attributes;
};

static Set<Object *> get_used_objects(const InlinedNodeTree &inlined_tree)
{
  Set<Object *> objects;
  for (const XInputSocket *xsocket : inlined_tree.all_input_sockets()) {
    if (xsocket->idname() == "fn_ObjectSocket") {
      Object *object = (Object *)RNA_pointer_get(xsocket->rna(), "value").data;
      if (object != nullptr) {
        objects.add(object);
      }
    }
  }
  for (const XGroupInput *group_input : inlined_tree.all_group_inputs()) {
    if (group_input->vsocket().idname() == "fn_ObjectSocket") {
      Object *object = (Object *)RNA_pointer_get(group_input->vsocket().rna(), "value").data;
      if (object != nullptr) {
        objects.add(object);
      }
    }
  }

  return objects;
}

class VTreeData {
 private:
  /* Keep this at the beginning, so that it is destructed last. */
  ResourceCollector m_resources;
  VTreeMFNetwork &m_inlined_tree_data_graph;
  FN::ExternalDataCacheContext m_data_cache;
  FN::PersistentSurfacesLookupContext m_persistent_surface_lookup;

 public:
  VTreeData(VTreeMFNetwork &inlined_tree_data)
      : m_inlined_tree_data_graph(inlined_tree_data), m_persistent_surface_lookup({})
  {
    Set<Object *> objects = get_used_objects(inlined_tree_data.inlined_tree());
    Map<int32_t, Object *> object_by_id;
    for (Object *ob : objects) {
      int32_t surface_id = BKE::SurfaceLocation::ComputeObjectSurfaceID(ob);
      object_by_id.add_new(surface_id, ob);
    }
    m_persistent_surface_lookup.~PersistentSurfacesLookupContext();
    new (&m_persistent_surface_lookup) FN::PersistentSurfacesLookupContext(object_by_id);
  }

  const InlinedNodeTree &inlined_tree()
  {
    return m_inlined_tree_data_graph.inlined_tree();
  }

  const FN::MFNetwork &data_graph()
  {
    return m_inlined_tree_data_graph.network();
  }

  const VTreeMFNetwork &inlined_tree_data_graph()
  {
    return m_inlined_tree_data_graph;
  }

  template<typename T, typename... Args> T &construct(const char *name, Args &&... args)
  {
    void *buffer = m_resources.allocate(sizeof(T), alignof(T));
    T *value = new (buffer) T(std::forward<Args>(args)...);
    m_resources.add(BLI::destruct_ptr<T>(value), name);
    return *value;
  }

  ParticleFunction *particle_function_for_all_inputs(const XNode &xnode)
  {
    Vector<const MFInputSocket *> sockets_to_compute;
    for (const XInputSocket *xsocket : xnode.inputs()) {
      if (m_inlined_tree_data_graph.is_mapped(*xsocket)) {
        sockets_to_compute.append(&m_inlined_tree_data_graph.lookup_dummy_socket(*xsocket));
      }
    }

    const MultiFunction &fn = this->construct<FN::MF_EvaluateNetwork>(
        "Evaluate Network", Vector<const MFOutputSocket *>(), std::move(sockets_to_compute));
    ParticleFunction &particle_fn = this->construct<ParticleFunction>(
        "Particle Function", fn, m_data_cache, m_persistent_surface_lookup);

    return &particle_fn;
  }

  Optional<NamedGenericTupleRef> compute_inputs(const XNode &xnode, ArrayRef<uint> input_indices)
  {
    const MultiFunction *fn = this->function_for_inputs(xnode, input_indices);
    if (fn == nullptr) {
      return {};
    }

    Vector<const CPPType *> computed_types;
    for (uint i : input_indices) {
      FN::MFDataType data_type =
          m_inlined_tree_data_graph.lookup_dummy_socket(xnode.input(i)).data_type();
      BLI_assert(data_type.is_single());
      computed_types.append(&data_type.single__cpp_type());
    }

    auto &tuple_info = this->construct<FN::GenericTupleInfo>(__func__, std::move(computed_types));
    void *tuple_buffer = m_resources.allocate(tuple_info.size_of_data_and_init(),
                                              tuple_info.alignment());
    FN::GenericTupleRef tuple = FN::GenericTupleRef::FromAlignedBuffer(tuple_info, tuple_buffer);
    tuple.set_all_uninitialized();

    FN::MFParamsBuilder params_builder(*fn, 1);
    FN::MFContextBuilder context_builder;

    for (uint i = 0; i < input_indices.size(); i++) {
      params_builder.add_single_output(
          FN::GenericMutableArrayRef(tuple.info().type_at_index(i), tuple.element_ptr(i), 1));
    }
    fn->call({0}, params_builder, context_builder);
    tuple.set_all_initialized();

    Vector<std::string> computed_names;
    for (uint i : input_indices) {
      computed_names.append(xnode.input(i).name());
    }

    auto &name_provider = this->construct<FN::CustomGenericTupleNameProvider>(
        __func__, std::move(computed_names));
    NamedGenericTupleRef named_tuple_ref{tuple, name_provider};

    return named_tuple_ref;
  }

  Optional<NamedGenericTupleRef> compute_all_data_inputs(const XNode &xnode)
  {
    Vector<uint> data_input_indices;
    for (uint i : xnode.inputs().index_iterator()) {
      if (m_inlined_tree_data_graph.is_mapped(xnode.input(i))) {
        data_input_indices.append(i);
      }
    }

    return this->compute_inputs(xnode, data_input_indices);
  }

  ArrayRef<std::string> find_target_system_names(const XOutputSocket &output_xsocket)
  {
    VectorSet<const XNode *> system_xnodes;
    this->find_target_system_nodes__recursive(output_xsocket, system_xnodes);

    auto &system_names = this->construct<Vector<std::string>>(__func__);
    for (const XNode *xnode : system_xnodes) {
      system_names.append(xnode->name());
    }

    return system_names;
  }

  Action *build_action(InfluencesCollector &collector, const XInputSocket &start)
  {
    if (start.linked_sockets().size() != 1) {
      return nullptr;
    }

    const XSocket &execute_socket = *start.linked_sockets()[0];
    if (execute_socket.idname() != "fn_ExecuteSocket") {
      return nullptr;
    }

    StringMap<ActionParserCallback> &parsers = get_action_parsers();
    ActionParserCallback &parser = parsers.lookup(execute_socket.node().idname());
    std::unique_ptr<Action> action = parser(collector, *this, execute_socket);
    Action *action_ptr = action.get();
    if (action_ptr == nullptr) {
      return nullptr;
    }
    m_resources.add(std::move(action), __func__);
    return action_ptr;
  }

  Action &build_action_list(InfluencesCollector &collector,
                            const XNode &start_xnode,
                            StringRef name)
  {
    Vector<const XInputSocket *> execute_sockets = this->find_execute_sockets(start_xnode, name);
    Vector<Action *> actions;
    for (const XInputSocket *socket : execute_sockets) {
      Action *action = this->build_action(collector, *socket);
      if (action != nullptr) {
        actions.append(action);
      }
    }
    Action &sequence = this->construct<ActionSequence>(__func__, std::move(actions));
    return sequence;
  }

 private:
  Vector<const XNode *> find_target_system_nodes(const XOutputSocket &xsocket)
  {
    VectorSet<const XNode *> type_nodes;
    find_target_system_nodes__recursive(xsocket, type_nodes);
    return Vector<const XNode *>(type_nodes);
  }

  void find_target_system_nodes__recursive(const XOutputSocket &output_xsocket,
                                           VectorSet<const XNode *> &r_nodes)
  {
    for (const XInputSocket *connected : output_xsocket.linked_sockets()) {
      const XNode &connected_xnode = connected->node();
      if (connected_xnode.idname() == particle_system_idname) {
        r_nodes.add(&connected_xnode);
      }
      else if (connected_xnode.idname() == combine_influences_idname) {
        find_target_system_nodes__recursive(connected_xnode.output(0), r_nodes);
      }
    }
  }

  const FN::MultiFunction *function_for_inputs(const XNode &xnode, ArrayRef<uint> input_indices)
  {
    Vector<const MFInputSocket *> sockets_to_compute;
    for (uint index : input_indices) {
      sockets_to_compute.append(
          &m_inlined_tree_data_graph.lookup_dummy_socket(xnode.input(index)));
    }

    if (m_inlined_tree_data_graph.network().find_dummy_dependencies(sockets_to_compute).size() >
        0) {
      return nullptr;
    }

    auto fn = BLI::make_unique<FN::MF_EvaluateNetwork>(ArrayRef<const MFOutputSocket *>(),
                                                       sockets_to_compute);
    const FN::MultiFunction *fn_ptr = fn.get();
    m_resources.add(std::move(fn), __func__);
    return fn_ptr;
  }

  Vector<const XInputSocket *> find_execute_sockets(const XNode &xnode, StringRef name_prefix)
  {
    bool found_name = false;
    Vector<const XInputSocket *> execute_sockets;
    for (const XInputSocket *xsocket : xnode.inputs()) {
      if (StringRef(xsocket->name()).startswith(name_prefix)) {
        if (xsocket->idname() == "fn_OperatorSocket") {
          found_name = true;
          break;
        }
        else {
          execute_sockets.append(xsocket);
        }
      }
    }
    BLI_assert(found_name);
    UNUSED_VARS_NDEBUG(found_name);
    return execute_sockets;
  }
};

static std::unique_ptr<Action> ACTION_kill(InfluencesCollector &UNUSED(collector),
                                           VTreeData &UNUSED(inlined_tree_data),
                                           const XSocket &UNUSED(execute_xsocket))
{
  return std::unique_ptr<Action>(new KillAction());
}

static std::unique_ptr<Action> ACTION_change_velocity(InfluencesCollector &UNUSED(collector),
                                                      VTreeData &inlined_tree_data,
                                                      const XSocket &execute_xsocket)
{
  const XNode &xnode = execute_xsocket.node();
  ParticleFunction *inputs_fn = inlined_tree_data.particle_function_for_all_inputs(xnode);

  if (inputs_fn == nullptr) {
    return {};
  }

  int mode = RNA_enum_get(xnode.rna(), "mode");

  Action *action = nullptr;
  if (mode == 0) {
    action = new SetVelocityAction(inputs_fn);
  }
  else if (mode == 1) {
    action = new RandomizeVelocityAction(inputs_fn);
  }

  return std::unique_ptr<Action>(action);
}

static std::unique_ptr<Action> ACTION_explode(InfluencesCollector &collector,
                                              VTreeData &inlined_tree_data,
                                              const XSocket &execute_xsocket)
{
  const XNode &xnode = execute_xsocket.node();
  ParticleFunction *inputs_fn = inlined_tree_data.particle_function_for_all_inputs(xnode);

  if (inputs_fn == nullptr) {
    return {};
  }

  Action &on_birth_action = inlined_tree_data.build_action_list(
      collector, xnode, "Execute on Birth");
  ArrayRef<std::string> system_names = inlined_tree_data.find_target_system_names(
      xnode.output(1, "Explode System"));

  Action *action = new ExplodeAction(system_names, inputs_fn, on_birth_action);
  return std::unique_ptr<Action>(action);
}

static std::unique_ptr<Action> ACTION_condition(InfluencesCollector &collector,
                                                VTreeData &inlined_tree_data,
                                                const XSocket &execute_xsocket)
{
  const XNode &xnode = execute_xsocket.node();
  ParticleFunction *inputs_fn = inlined_tree_data.particle_function_for_all_inputs(xnode);

  if (inputs_fn == nullptr) {
    return {};
  }

  Action &action_true = inlined_tree_data.build_action_list(collector, xnode, "Execute If True");
  Action &action_false = inlined_tree_data.build_action_list(collector, xnode, "Execute If False");

  Action *action = new ConditionAction(inputs_fn, action_true, action_false);
  return std::unique_ptr<Action>(action);
}

static std::unique_ptr<Action> ACTION_change_color(InfluencesCollector &UNUSED(collector),
                                                   VTreeData &inlined_tree_data,
                                                   const XSocket &execute_xsocket)
{
  const XNode &xnode = execute_xsocket.node();
  ParticleFunction *inputs_fn = inlined_tree_data.particle_function_for_all_inputs(xnode);

  if (inputs_fn == nullptr) {
    return {};
  }

  Action *action = new ChangeColorAction(inputs_fn);
  return std::unique_ptr<Action>(action);
}

static std::unique_ptr<Action> ACTION_change_size(InfluencesCollector &UNUSED(collector),
                                                  VTreeData &inlined_tree_data,
                                                  const XSocket &execute_xsocket)
{
  const XNode &xnode = execute_xsocket.node();
  ParticleFunction *inputs_fn = inlined_tree_data.particle_function_for_all_inputs(xnode);

  if (inputs_fn == nullptr) {
    return {};
  }

  Action *action = new ChangeSizeAction(inputs_fn);
  return std::unique_ptr<Action>(action);
}

static std::unique_ptr<Action> ACTION_change_position(InfluencesCollector &UNUSED(collector),
                                                      VTreeData &inlined_tree_data,
                                                      const XSocket &execute_xsocket)
{
  const XNode &xnode = execute_xsocket.node();
  ParticleFunction *inputs_fn = inlined_tree_data.particle_function_for_all_inputs(xnode);

  if (inputs_fn == nullptr) {
    return {};
  }

  Action *action = new ChangePositionAction(inputs_fn);
  return std::unique_ptr<Action>(action);
}

static std::unique_ptr<Action> ACTION_add_to_group(InfluencesCollector &collector,
                                                   VTreeData &inlined_tree_data,
                                                   const XSocket &execute_xsocket)
{
  const XNode &xnode = execute_xsocket.node();
  auto inputs = inlined_tree_data.compute_all_data_inputs(xnode);
  if (!inputs.has_value()) {
    return {};
  }

  std::string group_name = inputs->relocate_out<std::string>(0, "Group");

  /* Add group to all particle systems for now. */
  collector.m_attributes.foreach_value(
      [&](AttributesInfoBuilder *builder) { builder->add<bool>(group_name, 0); });

  Action *action = new AddToGroupAction(group_name);
  return std::unique_ptr<Action>(action);
}

static std::unique_ptr<Action> ACTION_remove_from_group(InfluencesCollector &UNUSED(collector),
                                                        VTreeData &inlined_tree_data,
                                                        const XSocket &execute_xsocket)
{
  const XNode &xnode = execute_xsocket.node();
  auto inputs = inlined_tree_data.compute_all_data_inputs(xnode);
  if (!inputs.has_value()) {
    return {};
  }

  std::string group_name = inputs->relocate_out<std::string>(0, "Group");
  Action *action = new RemoveFromGroupAction(group_name);
  return std::unique_ptr<Action>(action);
}

BLI_LAZY_INIT(StringMap<ActionParserCallback>, get_action_parsers)
{
  StringMap<ActionParserCallback> map;
  map.add_new("fn_KillParticleNode", ACTION_kill);
  map.add_new("fn_ChangeParticleVelocityNode", ACTION_change_velocity);
  map.add_new("fn_ExplodeParticleNode", ACTION_explode);
  map.add_new("fn_ParticleConditionNode", ACTION_condition);
  map.add_new("fn_ChangeParticleColorNode", ACTION_change_color);
  map.add_new("fn_ChangeParticleSizeNode", ACTION_change_size);
  map.add_new("fn_ChangeParticlePositionNode", ACTION_change_position);
  map.add_new("fn_AddToGroupNode", ACTION_add_to_group);
  map.add_new("fn_RemoveFromGroupNode", ACTION_remove_from_group);
  return map;
}

using ParseNodeCallback = std::function<void(InfluencesCollector &collector,
                                             VTreeData &inlined_tree_data,
                                             WorldTransition &world_transition,
                                             const XNode &xnode)>;

static void PARSE_point_emitter(InfluencesCollector &collector,
                                VTreeData &inlined_tree_data,
                                WorldTransition &world_transition,
                                const XNode &xnode)
{
  Optional<NamedGenericTupleRef> inputs = inlined_tree_data.compute_all_data_inputs(xnode);
  if (!inputs.has_value()) {
    return;
  }

  Action &action = inlined_tree_data.build_action_list(collector, xnode, "Execute on Birth");

  ArrayRef<std::string> system_names = inlined_tree_data.find_target_system_names(
      xnode.output(0, "Emitter"));
  std::string name = xnode.name();

  VaryingFloat3 position = world_transition.update_float3(
      name, "Position", inputs->get<float3>(0, "Position"));
  VaryingFloat3 velocity = world_transition.update_float3(
      name, "Velocity", inputs->get<float3>(1, "Velocity"));
  VaryingFloat size = world_transition.update_float(name, "Size", inputs->get<float>(2, "Size"));

  Emitter *emitter = new PointEmitter(std::move(system_names), position, velocity, size, action);
  collector.m_emitters.append(emitter);
}

static Vector<float> compute_emitter_vertex_weights(const XNode &xnode,
                                                    NamedGenericTupleRef inputs,
                                                    Object *object)
{
  uint density_mode = RNA_enum_get(xnode.rna(), "density_mode");

  Mesh *mesh = (Mesh *)object->data;
  Vector<float> vertex_weights(mesh->totvert);

  if (density_mode == 0) {
    /* Mode: 'UNIFORM' */
    vertex_weights.fill(1.0f);
  }
  else if (density_mode == 1) {
    /* Mode: 'VERTEX_WEIGHTS' */
    std::string group_name = inputs.relocate_out<std::string>(2, "Density Group");

    MDeformVert *vertices = mesh->dvert;
    int group_index = defgroup_name_index(object, group_name.c_str());
    if (group_index == -1 || vertices == nullptr) {
      vertex_weights.fill(0);
    }
    else {
      for (uint i = 0; i < mesh->totvert; i++) {
        vertex_weights[i] = defvert_find_weight(vertices + i, group_index);
      }
    }
  }

  return vertex_weights;
}

static void PARSE_mesh_emitter(InfluencesCollector &collector,
                               VTreeData &inlined_tree_data,
                               WorldTransition &world_transition,
                               const XNode &xnode)
{
  Optional<NamedGenericTupleRef> inputs = inlined_tree_data.compute_all_data_inputs(xnode);
  if (!inputs.has_value()) {
    return;
  }

  Action &on_birth_action = inlined_tree_data.build_action_list(
      collector, xnode, "Execute on Birth");

  Object *object = inputs->relocate_out<Object *>(0, "Object");
  if (object == nullptr || object->type != OB_MESH) {
    return;
  }

  auto vertex_weights = compute_emitter_vertex_weights(xnode, *inputs, object);

  VaryingFloat4x4 transform = world_transition.update_float4x4(
      object->id.name, "obmat", object->obmat);
  ArrayRef<std::string> system_names = inlined_tree_data.find_target_system_names(
      xnode.output(0, "Emitter"));
  Emitter *emitter = new SurfaceEmitter(system_names,
                                        on_birth_action,
                                        object,
                                        transform,
                                        inputs->get<float>(1, "Rate"),
                                        std::move(vertex_weights));
  collector.m_emitters.append(emitter);
}

static void PARSE_custom_force(InfluencesCollector &collector,
                               VTreeData &inlined_tree_data,
                               WorldTransition &UNUSED(world_transition),
                               const XNode &xnode)
{
  ParticleFunction *inputs_fn = inlined_tree_data.particle_function_for_all_inputs(xnode);
  if (inputs_fn == nullptr) {
    return;
  }

  ArrayRef<std::string> system_names = inlined_tree_data.find_target_system_names(
      xnode.output(0, "Force"));

  for (const std::string &system_name : system_names) {
    CustomForce *force = new CustomForce(inputs_fn);
    collector.m_forces.add(system_name, force);
  }
}

static void PARSE_age_reached_event(InfluencesCollector &collector,
                                    VTreeData &inlined_tree_data,
                                    WorldTransition &UNUSED(world_transition),
                                    const XNode &xnode)
{
  ParticleFunction *inputs_fn = inlined_tree_data.particle_function_for_all_inputs(xnode);
  if (inputs_fn == nullptr) {
    return;
  }

  ArrayRef<std::string> system_names = inlined_tree_data.find_target_system_names(
      xnode.output(0, "Event"));
  Action &action = inlined_tree_data.build_action_list(collector, xnode, "Execute on Event");

  std::string is_triggered_attribute = xnode.name();

  for (const std::string &system_name : system_names) {
    collector.m_attributes.lookup(system_name)->add<bool>(is_triggered_attribute, 0);
    Event *event = new AgeReachedEvent(is_triggered_attribute, inputs_fn, action);
    collector.m_events.add(system_name, event);
  }
}

static void PARSE_trails(InfluencesCollector &collector,
                         VTreeData &inlined_tree_data,
                         WorldTransition &UNUSED(world_transition),
                         const XNode &xnode)
{
  ArrayRef<std::string> main_system_names = inlined_tree_data.find_target_system_names(
      xnode.output(0, "Main System"));
  ArrayRef<std::string> trail_system_names = inlined_tree_data.find_target_system_names(
      xnode.output(1, "Trail System"));

  ParticleFunction *inputs_fn = inlined_tree_data.particle_function_for_all_inputs(xnode);
  if (inputs_fn == nullptr) {
    return;
  }

  Action &action = inlined_tree_data.build_action_list(collector, xnode, "Execute on Birth");
  for (const std::string &main_type : main_system_names) {

    OffsetHandler *offset_handler = new CreateTrailHandler(trail_system_names, inputs_fn, action);
    collector.m_offset_handlers.add(main_type, offset_handler);
  }
}

static void PARSE_initial_grid_emitter(InfluencesCollector &collector,
                                       VTreeData &inlined_tree_data,
                                       WorldTransition &UNUSED(world_transition),
                                       const XNode &xnode)
{
  Optional<NamedGenericTupleRef> inputs = inlined_tree_data.compute_all_data_inputs(xnode);
  if (!inputs.has_value()) {
    return;
  }

  Action &action = inlined_tree_data.build_action_list(collector, xnode, "Execute on Birth");

  ArrayRef<std::string> system_names = inlined_tree_data.find_target_system_names(
      xnode.output(0, "Emitter"));
  Emitter *emitter = new InitialGridEmitter(std::move(system_names),
                                            std::max(0, inputs->get<int>(0, "Amount X")),
                                            std::max(0, inputs->get<int>(1, "Amount Y")),
                                            inputs->get<float>(2, "Step X"),
                                            inputs->get<float>(3, "Step Y"),
                                            inputs->get<float>(4, "Size"),
                                            action);
  collector.m_emitters.append(emitter);
}

static void PARSE_mesh_collision(InfluencesCollector &collector,
                                 VTreeData &inlined_tree_data,
                                 WorldTransition &world_transition,
                                 const XNode &xnode)
{
  ParticleFunction *inputs_fn = inlined_tree_data.particle_function_for_all_inputs(xnode);
  if (inputs_fn == nullptr) {
    return;
  }

  Optional<NamedGenericTupleRef> inputs = inlined_tree_data.compute_inputs(xnode, {0});
  if (!inputs.has_value()) {
    return;
  }

  Object *object = inputs->relocate_out<Object *>(0, "Object");
  if (object == nullptr || object->type != OB_MESH) {
    return;
  }

  ArrayRef<std::string> system_names = inlined_tree_data.find_target_system_names(
      xnode.output(0, "Event"));
  Action &action = inlined_tree_data.build_action_list(collector, xnode, "Execute on Event");

  float4x4 local_to_world_end = object->obmat;
  float4x4 local_to_world_begin =
      world_transition.update_float4x4(object->id.name, "obmat", object->obmat).start;

  std::string last_collision_attribute = xnode.name();

  for (const std::string &system_name : system_names) {
    Event *event = new MeshCollisionEvent(
        last_collision_attribute, object, action, local_to_world_begin, local_to_world_end);
    collector.m_attributes.lookup(system_name)->add<int32_t>(last_collision_attribute, -1);
    collector.m_events.add(system_name, event);
  }
}

static void PARSE_size_over_time(InfluencesCollector &collector,
                                 VTreeData &inlined_tree_data,
                                 WorldTransition &UNUSED(world_transition),
                                 const XNode &xnode)
{
  ParticleFunction *inputs_fn = inlined_tree_data.particle_function_for_all_inputs(xnode);
  if (inputs_fn == nullptr) {
    return;
  }

  ArrayRef<std::string> system_names = inlined_tree_data.find_target_system_names(
      xnode.output(0, "Influence"));
  for (const std::string &system_name : system_names) {
    OffsetHandler *handler = new SizeOverTimeHandler(inputs_fn);
    collector.m_offset_handlers.add(system_name, handler);
  }
}

static void PARSE_custom_event(InfluencesCollector &collector,
                               VTreeData &inlined_tree_data,
                               WorldTransition &UNUSED(world_transition),
                               const XNode &xnode)
{
  ParticleFunction *inputs_fn = inlined_tree_data.particle_function_for_all_inputs(xnode);
  if (inputs_fn == nullptr) {
    return;
  }

  ArrayRef<std::string> system_names = inlined_tree_data.find_target_system_names(
      xnode.output(0, "Event"));
  Action &action = inlined_tree_data.build_action_list(collector, xnode, "Execute on Event");

  std::string is_triggered_attribute = xnode.name();

  for (const std::string &system_name : system_names) {
    Event *event = new CustomEvent(is_triggered_attribute, inputs_fn, action);
    collector.m_attributes.lookup(system_name)->add<bool>(system_name, 0);
    collector.m_events.add(system_name, event);
  }
}

static void PARSE_always_execute(InfluencesCollector &collector,
                                 VTreeData &inlined_tree_data,
                                 WorldTransition &UNUSED(world_transition),
                                 const XNode &xnode)
{
  ArrayRef<std::string> system_names = inlined_tree_data.find_target_system_names(
      xnode.output(0, "Influence"));
  Action &action = inlined_tree_data.build_action_list(collector, xnode, "Execute");

  for (const std::string &system_name : system_names) {
    OffsetHandler *handler = new AlwaysExecuteHandler(action);
    collector.m_offset_handlers.add(system_name, handler);
  }
}

BLI_LAZY_INIT_STATIC(StringMap<ParseNodeCallback>, get_node_parsers)
{
  StringMap<ParseNodeCallback> map;
  map.add_new("fn_PointEmitterNode", PARSE_point_emitter);
  map.add_new("fn_MeshEmitterNode", PARSE_mesh_emitter);
  map.add_new("fn_AgeReachedEventNode", PARSE_age_reached_event);
  map.add_new("fn_ParticleTrailsNode", PARSE_trails);
  map.add_new("fn_InitialGridEmitterNode", PARSE_initial_grid_emitter);
  map.add_new("fn_MeshCollisionEventNode", PARSE_mesh_collision);
  map.add_new("fn_SizeOverTimeNode", PARSE_size_over_time);
  map.add_new("fn_CustomEventNode", PARSE_custom_event);
  map.add_new("fn_AlwaysExecuteNode", PARSE_always_execute);
  map.add_new("fn_ForceNode", PARSE_custom_force);
  return map;
}

static void collect_influences(VTreeData &inlined_tree_data,
                               WorldTransition &world_transition,
                               Vector<std::string> &r_system_names,
                               Vector<Emitter *> &r_emitters,
                               MultiMap<std::string, Event *> &r_events_per_type,
                               MultiMap<std::string, OffsetHandler *> &r_offset_handler_per_type,
                               StringMap<AttributesInfoBuilder *> &r_attributes_per_type,
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
      r_attributes_per_type,
  };

  for (const XNode *xnode :
       inlined_tree_data.inlined_tree().nodes_with_idname(particle_system_idname)) {
    StringRef name = xnode->name();
    r_system_names.append(name);
    r_attributes_per_type.add_new(name, new AttributesInfoBuilder());
  }

  for (const XNode *xnode : inlined_tree_data.inlined_tree().all_nodes()) {
    StringRef idname = xnode->idname();
    ParseNodeCallback *callback = parsers.lookup_ptr(idname);
    if (callback != nullptr) {
      (*callback)(collector, inlined_tree_data, world_transition, *xnode);
    }
  }

  for (std::string &system_name : r_system_names) {
    AttributesInfoBuilder &attributes = *r_attributes_per_type.lookup(system_name);

    attributes.add<bool>("Kill State", 0);
    attributes.add<int32_t>("ID", 0);
    attributes.add<float>("Birth Time", 0);
    attributes.add<float3>("Position", float3(0, 0, 0));
    attributes.add<float3>("Velocity", float3(0, 0, 0));
    attributes.add<float>("Size", 0.05f);
    attributes.add<rgba_f>("Color", rgba_f(1, 1, 1, 1));

    ArrayRef<Force *> forces = collector.m_forces.lookup_default(system_name);
    EulerIntegrator *integrator = new EulerIntegrator(forces);

    r_integrators.add_new(system_name, integrator);
  }
}

class NodeTreeStepSimulator : public StepSimulator {
 private:
  BKE::BTreeVTreeMap m_inlined_trees;
  InlinedNodeTree m_inlined_tree;

 public:
  NodeTreeStepSimulator(bNodeTree *btree) : m_inlined_tree(btree, m_inlined_trees)
  {
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
    StringMap<AttributesInfoBuilder *> attributes;
    StringMap<Integrator *> integrators;

    ResourceCollector resources;
    std::unique_ptr<VTreeMFNetwork> data_graph = FN::generate_inlined_tree_multi_function_network(
        m_inlined_tree, resources);
    if (data_graph.get() == nullptr) {
      return;
    }
    VTreeData inlined_tree_data(*data_graph);

    collect_influences(inlined_tree_data,
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
      AttributesInfoBuilder &system_attributes = *attributes.lookup(name);

      /* Keep old attributes. */
      AttributesBlockContainer *container = containers.lookup_default(name, nullptr);
      if (container != nullptr) {
        system_attributes.add(container->info());
      }

      this->ensure_particle_container_exist_and_has_attributes(
          particles_state, name, system_attributes);

      ParticleSystemInfo type_info = {
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
    attributes.foreach_value([](AttributesInfoBuilder *builder) { delete builder; });

    simulation_state.world() = std::move(new_world_state);
  }

 private:
  void ensure_particle_container_exist_and_has_attributes(
      ParticlesState &particles_state,
      StringRef name,
      const AttributesInfoBuilder &attributes_info_builder)
  {
    auto &containers = particles_state.particle_containers();
    AttributesBlockContainer *container = containers.lookup_default(name, nullptr);
    if (container == nullptr) {
      AttributesBlockContainer *container = new AttributesBlockContainer(attributes_info_builder,
                                                                         1000);
      containers.add_new(name, container);
    }
    else {
      container->update_attributes(attributes_info_builder);
    }
  }
};

std::unique_ptr<StepSimulator> simulator_from_node_tree(bNodeTree *btree)
{
  return std::unique_ptr<StepSimulator>(new NodeTreeStepSimulator(btree));
}

}  // namespace BParticles
