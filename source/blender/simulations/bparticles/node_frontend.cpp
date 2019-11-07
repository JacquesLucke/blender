#include "BKE_virtual_node_tree.h"
#include "BKE_deform.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_timeit.h"
#include "BLI_multi_map.h"
#include "BLI_lazy_init_cxx.h"

#include "FN_multi_functions.h"
#include "FN_generic_tuple.h"
#include "FN_vtree_multi_function_network_generation.h"

#include "node_frontend.hpp"
#include "integrator.hpp"
#include "particle_function_builder.hpp"
#include "emitters.hpp"
#include "events.hpp"
#include "offset_handlers.hpp"
#include "simulate.hpp"

namespace BParticles {

using BKE::VNode;
using BKE::VOutputSocket;
using BKE::VSocket;
using BLI::destruct_ptr;
using BLI::MultiMap;
using BLI::ResourceCollector;
using BLI::rgba_f;
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
    InfluencesCollector &collector, VTreeData &vtree_data, const VSocket &execute_vsocket)>;
StringMap<ActionParserCallback> &get_action_parsers();

class InfluencesCollector {
 public:
  Vector<Emitter *> &m_emitters;
  MultiMap<std::string, Force *> &m_forces;
  MultiMap<std::string, Event *> &m_events;
  MultiMap<std::string, OffsetHandler *> &m_offset_handlers;
  StringMap<AttributesInfoBuilder> &m_attributes;
  StringMap<AttributesDefaults *> &m_attributes_defaults;
};

class VTreeData {
 private:
  /* Keep this at the beginning, so that it is destructed last. */
  ResourceCollector m_resources;
  VTreeMFNetwork &m_vtree_data_graph;

 public:
  VTreeData(VTreeMFNetwork &vtree_data) : m_vtree_data_graph(vtree_data)
  {
  }

  const VirtualNodeTree &vtree()
  {
    return m_vtree_data_graph.vtree();
  }

  const FN::MFNetwork &data_graph()
  {
    return m_vtree_data_graph.network();
  }

  const VTreeMFNetwork &vtree_data_graph()
  {
    return m_vtree_data_graph;
  }

  template<typename T, typename... Args> T &construct(const char *name, Args &&... args)
  {
    void *buffer = m_resources.allocate(sizeof(T), alignof(T));
    T *value = new (buffer) T(std::forward<Args>(args)...);
    m_resources.add(BLI::destruct_ptr<T>(value), name);
    return *value;
  }

  ParticleFunction *particle_function_for_all_inputs(const VNode &vnode)
  {
    Optional<std::unique_ptr<ParticleFunction>> fn = create_particle_function(vnode,
                                                                              m_vtree_data_graph);
    if (!fn.has_value()) {
      return nullptr;
    }
    ParticleFunction *fn_ptr = fn->get();
    BLI_assert(fn_ptr != nullptr);
    m_resources.add(std::move(fn.extract()), __func__);
    return fn_ptr;
  }

  Optional<NamedGenericTupleRef> compute_inputs(const VNode &vnode, ArrayRef<uint> input_indices)
  {
    const MultiFunction *fn = this->function_for_inputs(vnode, input_indices);
    if (fn == nullptr) {
      return {};
    }

    Vector<const CPPType *> computed_types;
    for (uint i : input_indices) {
      FN::MFDataType data_type = m_vtree_data_graph.lookup_socket(vnode.input(i)).type();
      BLI_assert(data_type.is_single());
      computed_types.append(&data_type.type());
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
    fn->call(FN::MFMask({0}), params_builder.build(), context_builder.build());
    tuple.set_all_initialized();

    Vector<std::string> computed_names;
    for (uint i : input_indices) {
      computed_names.append(vnode.input(i).name());
    }

    auto &name_provider = this->construct<FN::CustomGenericTupleNameProvider>(
        __func__, std::move(computed_names));
    NamedGenericTupleRef named_tuple_ref{tuple, name_provider};

    return named_tuple_ref;
  }

  Optional<NamedGenericTupleRef> compute_all_data_inputs(const VNode &vnode)
  {
    Vector<uint> data_input_indices;
    for (uint i = 0; i < vnode.inputs().size(); i++) {
      if (m_vtree_data_graph.is_mapped(vnode.input(i))) {
        data_input_indices.append(i);
      }
    }

    return this->compute_inputs(vnode, data_input_indices);
  }

  ArrayRef<std::string> find_target_system_names(const VOutputSocket &output_vsocket)
  {
    VectorSet<const VNode *> system_vnodes;
    this->find_target_system_nodes__recursive(output_vsocket, system_vnodes);

    auto &system_names = this->construct<Vector<std::string>>(__func__);
    for (const VNode *vnode : system_vnodes) {
      system_names.append(vnode->name());
    }

    return system_names;
  }

  Action *build_action(InfluencesCollector &collector, const VSocket &start)
  {
    BLI_assert(start.is_input());
    if (start.linked_sockets().size() != 1) {
      return nullptr;
    }

    const VSocket &execute_socket = *start.linked_sockets()[0];
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
                            const VNode &start_vnode,
                            StringRef name)
  {
    Vector<const VSocket *> execute_sockets = this->find_execute_sockets(start_vnode, name);
    Vector<Action *> actions;
    for (const VSocket *socket : execute_sockets) {
      Action *action = this->build_action(collector, *socket);
      if (action != nullptr) {
        actions.append(action);
      }
    }
    Action &sequence = this->construct<ActionSequence>(__func__, std::move(actions));
    return sequence;
  }

 private:
  Vector<const VNode *> find_target_system_nodes(const VSocket &vsocket)
  {
    VectorSet<const VNode *> type_nodes;
    find_target_system_nodes__recursive(vsocket, type_nodes);
    return Vector<const VNode *>(type_nodes);
  }

  void find_target_system_nodes__recursive(const VSocket &output_vsocket,
                                           VectorSet<const VNode *> &r_nodes)
  {
    BLI_assert(output_vsocket.is_output());
    for (const VSocket *connected : output_vsocket.linked_sockets()) {
      const VNode &connected_vnode = connected->node();
      if (connected_vnode.idname() == particle_system_idname) {
        r_nodes.add(&connected_vnode);
      }
      else if (connected_vnode.idname() == combine_influences_idname) {
        find_target_system_nodes__recursive(connected_vnode.output(0), r_nodes);
      }
    }
  }

  const FN::MultiFunction *function_for_inputs(const VNode &vnode, ArrayRef<uint> input_indices)
  {
    Vector<const MFInputSocket *> sockets_to_compute;
    for (uint index : input_indices) {
      sockets_to_compute.append(&m_vtree_data_graph.lookup_socket(vnode.input(index)));
    }

    if (m_vtree_data_graph.network().find_dummy_dependencies(sockets_to_compute).size() > 0) {
      return nullptr;
    }

    auto fn = BLI::make_unique<FN::MF_EvaluateNetwork>(ArrayRef<const MFOutputSocket *>(),
                                                       sockets_to_compute);
    const FN::MultiFunction *fn_ptr = fn.get();
    m_resources.add(std::move(fn), __func__);
    return fn_ptr;
  }

  Vector<const VSocket *> find_execute_sockets(const VNode &vnode, StringRef name_prefix)
  {
    bool found_name = false;
    Vector<const VSocket *> execute_sockets;
    for (const VSocket *vsocket : vnode.inputs()) {
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

static std::unique_ptr<Action> ACTION_kill(InfluencesCollector &UNUSED(collector),
                                           VTreeData &UNUSED(vtree_data),
                                           const VSocket &UNUSED(execute_vsocket))
{
  return std::unique_ptr<Action>(new KillAction());
}

static std::unique_ptr<Action> ACTION_change_velocity(InfluencesCollector &UNUSED(collector),
                                                      VTreeData &vtree_data,
                                                      const VSocket &execute_vsocket)
{
  const VNode &vnode = execute_vsocket.node();
  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);

  if (inputs_fn == nullptr) {
    return {};
  }

  int mode = RNA_enum_get(vnode.rna(), "mode");

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
                                              VTreeData &vtree_data,
                                              const VSocket &execute_vsocket)
{
  const VNode &vnode = execute_vsocket.node();
  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);

  if (inputs_fn == nullptr) {
    return {};
  }

  Action &on_birth_action = vtree_data.build_action_list(collector, vnode, "Execute on Birth");
  ArrayRef<std::string> system_names = vtree_data.find_target_system_names(
      vnode.output(1, "Explode System"));

  Action *action = new ExplodeAction(system_names, inputs_fn, on_birth_action);
  return std::unique_ptr<Action>(action);
}

static std::unique_ptr<Action> ACTION_condition(InfluencesCollector &collector,
                                                VTreeData &vtree_data,
                                                const VSocket &execute_vsocket)
{
  const VNode &vnode = execute_vsocket.node();
  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);

  if (inputs_fn == nullptr) {
    return {};
  }

  Action &action_true = vtree_data.build_action_list(collector, vnode, "Execute If True");
  Action &action_false = vtree_data.build_action_list(collector, vnode, "Execute If False");

  Action *action = new ConditionAction(inputs_fn, action_true, action_false);
  return std::unique_ptr<Action>(action);
}

static std::unique_ptr<Action> ACTION_change_color(InfluencesCollector &UNUSED(collector),
                                                   VTreeData &vtree_data,
                                                   const VSocket &execute_vsocket)
{
  const VNode &vnode = execute_vsocket.node();
  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);

  if (inputs_fn == nullptr) {
    return {};
  }

  Action *action = new ChangeColorAction(inputs_fn);
  return std::unique_ptr<Action>(action);
}

static std::unique_ptr<Action> ACTION_change_size(InfluencesCollector &UNUSED(collector),
                                                  VTreeData &vtree_data,
                                                  const VSocket &execute_vsocket)
{
  const VNode &vnode = execute_vsocket.node();
  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);

  if (inputs_fn == nullptr) {
    return {};
  }

  Action *action = new ChangeSizeAction(inputs_fn);
  return std::unique_ptr<Action>(action);
}

static std::unique_ptr<Action> ACTION_change_position(InfluencesCollector &UNUSED(collector),
                                                      VTreeData &vtree_data,
                                                      const VSocket &execute_vsocket)
{
  const VNode &vnode = execute_vsocket.node();
  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);

  if (inputs_fn == nullptr) {
    return {};
  }

  Action *action = new ChangePositionAction(inputs_fn);
  return std::unique_ptr<Action>(action);
}

static std::unique_ptr<Action> ACTION_add_to_group(InfluencesCollector &collector,
                                                   VTreeData &vtree_data,
                                                   const VSocket &execute_vsocket)
{
  const VNode &vnode = execute_vsocket.node();
  auto inputs = vtree_data.compute_all_data_inputs(vnode);
  if (!inputs.has_value()) {
    return {};
  }

  std::string group_name = inputs->relocate_out<std::string>(0, "Group");

  /* Add group to all particle systems for now. */
  collector.m_attributes_defaults.foreach_value([&](AttributesDefaults *attributes_defaults) {
    attributes_defaults->add<uint8_t>(group_name, 0);
  });
  collector.m_attributes.foreach_value(
      [&](AttributesInfoBuilder &builder) { builder.add<uint8_t>(group_name); });

  Action *action = new AddToGroupAction(group_name);
  return std::unique_ptr<Action>(action);
}

static std::unique_ptr<Action> ACTION_remove_from_group(InfluencesCollector &UNUSED(collector),
                                                        VTreeData &vtree_data,
                                                        const VSocket &execute_vsocket)
{
  const VNode &vnode = execute_vsocket.node();
  auto inputs = vtree_data.compute_all_data_inputs(vnode);
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
                                             VTreeData &vtree_data,
                                             WorldTransition &world_transition,
                                             const VNode &vnode)>;

static void PARSE_point_emitter(InfluencesCollector &collector,
                                VTreeData &vtree_data,
                                WorldTransition &world_transition,
                                const VNode &vnode)
{
  Optional<NamedGenericTupleRef> inputs = vtree_data.compute_all_data_inputs(vnode);
  if (!inputs.has_value()) {
    return;
  }

  Action &action = vtree_data.build_action_list(collector, vnode, "Execute on Birth");

  ArrayRef<std::string> system_names = vtree_data.find_target_system_names(
      vnode.output(0, "Emitter"));
  std::string name = vnode.name();

  VaryingFloat3 position = world_transition.update_float3(
      name, "Position", inputs->get<float3>(0, "Position"));
  VaryingFloat3 velocity = world_transition.update_float3(
      name, "Velocity", inputs->get<float3>(1, "Velocity"));
  VaryingFloat size = world_transition.update_float(name, "Size", inputs->get<float>(2, "Size"));

  Emitter *emitter = new PointEmitter(std::move(system_names), position, velocity, size, action);
  collector.m_emitters.append(emitter);
}

static Vector<float> compute_emitter_vertex_weights(const VNode &vnode,
                                                    NamedGenericTupleRef inputs,
                                                    Object *object)
{
  uint density_mode = RNA_enum_get(vnode.rna(), "density_mode");

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
                               VTreeData &vtree_data,
                               WorldTransition &world_transition,
                               const VNode &vnode)
{
  Optional<NamedGenericTupleRef> inputs = vtree_data.compute_all_data_inputs(vnode);
  if (!inputs.has_value()) {
    return;
  }

  Action &on_birth_action = vtree_data.build_action_list(collector, vnode, "Execute on Birth");

  Object *object = inputs->relocate_out<Object *>(0, "Object");
  if (object == nullptr || object->type != OB_MESH) {
    return;
  }

  auto vertex_weights = compute_emitter_vertex_weights(vnode, *inputs, object);

  VaryingFloat4x4 transform = world_transition.update_float4x4(
      vnode.name(), "Transform", object->obmat);
  ArrayRef<std::string> system_names = vtree_data.find_target_system_names(
      vnode.output(0, "Emitter"));
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
                                const VNode &vnode)
{
  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);
  if (inputs_fn == nullptr) {
    return;
  }

  ArrayRef<std::string> system_names = vtree_data.find_target_system_names(
      vnode.output(0, "Force"));

  for (const std::string &system_name : system_names) {
    GravityForce *force = new GravityForce(inputs_fn);
    collector.m_forces.add(system_name, force);
  }
}

static void PARSE_age_reached_event(InfluencesCollector &collector,
                                    VTreeData &vtree_data,
                                    WorldTransition &UNUSED(world_transition),
                                    const VNode &vnode)
{
  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);
  if (inputs_fn == nullptr) {
    return;
  }

  ArrayRef<std::string> system_names = vtree_data.find_target_system_names(
      vnode.output(0, "Event"));
  Action &action = vtree_data.build_action_list(collector, vnode, "Execute on Event");

  std::string is_triggered_attribute = vnode.name();

  for (const std::string &system_name : system_names) {
    collector.m_attributes.lookup(system_name).add<uint8_t>(is_triggered_attribute);
    collector.m_attributes_defaults.lookup(system_name)->add<uint8_t>(is_triggered_attribute, 0);
    Event *event = new AgeReachedEvent(is_triggered_attribute, inputs_fn, action);
    collector.m_events.add(system_name, event);
  }
}

static void PARSE_trails(InfluencesCollector &collector,
                         VTreeData &vtree_data,
                         WorldTransition &UNUSED(world_transition),
                         const VNode &vnode)
{
  ArrayRef<std::string> main_system_names = vtree_data.find_target_system_names(
      vnode.output(0, "Main System"));
  ArrayRef<std::string> trail_system_names = vtree_data.find_target_system_names(
      vnode.output(1, "Trail System"));

  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);
  if (inputs_fn == nullptr) {
    return;
  }

  Action &action = vtree_data.build_action_list(collector, vnode, "Execute on Birth");
  for (const std::string &main_type : main_system_names) {

    OffsetHandler *offset_handler = new CreateTrailHandler(trail_system_names, inputs_fn, action);
    collector.m_offset_handlers.add(main_type, offset_handler);
  }
}

static void PARSE_initial_grid_emitter(InfluencesCollector &collector,
                                       VTreeData &vtree_data,
                                       WorldTransition &UNUSED(world_transition),
                                       const VNode &vnode)
{
  Optional<NamedGenericTupleRef> inputs = vtree_data.compute_all_data_inputs(vnode);
  if (!inputs.has_value()) {
    return;
  }

  Action &action = vtree_data.build_action_list(collector, vnode, "Execute on Birth");

  ArrayRef<std::string> system_names = vtree_data.find_target_system_names(
      vnode.output(0, "Emitter"));
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
                                   const VNode &vnode)
{
  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);
  if (inputs_fn == nullptr) {
    return;
  }

  ArrayRef<std::string> system_names = vtree_data.find_target_system_names(
      vnode.output(0, "Force"));

  for (const std::string &system_name : system_names) {
    Force *force = new TurbulenceForce(inputs_fn);
    collector.m_forces.add(system_name, force);
  }
}

static void PARSE_drag_force(InfluencesCollector &collector,
                             VTreeData &vtree_data,
                             WorldTransition &UNUSED(world_transition),
                             const VNode &vnode)
{
  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);
  if (inputs_fn == nullptr) {
    return;
  }

  ArrayRef<std::string> system_names = vtree_data.find_target_system_names(
      vnode.output(0, "Force"));

  for (const std::string &system_name : system_names) {
    Force *force = new DragForce(inputs_fn);
    collector.m_forces.add(system_name, force);
  }
}

static void PARSE_mesh_collision(InfluencesCollector &collector,
                                 VTreeData &vtree_data,
                                 WorldTransition &world_transition,
                                 const VNode &vnode)
{
  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);
  if (inputs_fn == nullptr) {
    return;
  }

  Optional<NamedGenericTupleRef> inputs = vtree_data.compute_inputs(vnode, {0});
  if (!inputs.has_value()) {
    return;
  }

  Object *object = inputs->relocate_out<Object *>(0, "Object");
  if (object == nullptr || object->type != OB_MESH) {
    return;
  }

  ArrayRef<std::string> system_names = vtree_data.find_target_system_names(
      vnode.output(0, "Event"));
  Action &action = vtree_data.build_action_list(collector, vnode, "Execute on Event");

  float4x4 local_to_world_end = object->obmat;
  float4x4 local_to_world_begin =
      world_transition.update_float4x4(object->id.name, "obmat", object->obmat).start;

  std::string last_collision_attribute = vnode.name();

  for (const std::string &system_name : system_names) {
    Event *event = new MeshCollisionEvent(
        last_collision_attribute, object, action, local_to_world_begin, local_to_world_end);
    collector.m_attributes.lookup(system_name).add<int32_t>(last_collision_attribute);
    collector.m_attributes_defaults.lookup(system_name)
        ->add<int32_t>(last_collision_attribute, -1);
    collector.m_events.add(system_name, event);
  }
}

static void PARSE_size_over_time(InfluencesCollector &collector,
                                 VTreeData &vtree_data,
                                 WorldTransition &UNUSED(world_transition),
                                 const VNode &vnode)
{
  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);
  if (inputs_fn == nullptr) {
    return;
  }

  ArrayRef<std::string> system_names = vtree_data.find_target_system_names(
      vnode.output(0, "Influence"));
  for (const std::string &system_name : system_names) {
    OffsetHandler *handler = new SizeOverTimeHandler(inputs_fn);
    collector.m_offset_handlers.add(system_name, handler);
  }
}

static void PARSE_mesh_force(InfluencesCollector &collector,
                             VTreeData &vtree_data,
                             WorldTransition &UNUSED(world_transition),
                             const VNode &vnode)
{
  Optional<NamedGenericTupleRef> inputs = vtree_data.compute_inputs(vnode, {0});
  if (!inputs.has_value()) {
    return;
  }

  Object *object = inputs->relocate_out<Object *>(0, "Object");
  if (object == nullptr || object->type != OB_MESH) {
    return;
  }

  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);
  if (inputs_fn == nullptr) {
    return;
  }

  ArrayRef<std::string> system_names = vtree_data.find_target_system_names(
      vnode.output(0, "Force"));
  for (const std::string &system_name : system_names) {
    Force *force = new MeshForce(inputs_fn, object);
    collector.m_forces.add(system_name, force);
  }
}

static void PARSE_custom_event(InfluencesCollector &collector,
                               VTreeData &vtree_data,
                               WorldTransition &UNUSED(world_transition),
                               const VNode &vnode)
{
  ParticleFunction *inputs_fn = vtree_data.particle_function_for_all_inputs(vnode);
  if (inputs_fn == nullptr) {
    return;
  }

  ArrayRef<std::string> system_names = vtree_data.find_target_system_names(
      vnode.output(0, "Event"));
  Action &action = vtree_data.build_action_list(collector, vnode, "Execute on Event");

  std::string is_triggered_attribute = vnode.name();

  for (const std::string &system_name : system_names) {
    Event *event = new CustomEvent(is_triggered_attribute, inputs_fn, action);
    collector.m_attributes.lookup(system_name).add<uint8_t>(system_name);
    collector.m_attributes_defaults.lookup(system_name)->add<uint8_t>(system_name, 0);
    collector.m_events.add(system_name, event);
  }
}

static void PARSE_always_execute(InfluencesCollector &collector,
                                 VTreeData &vtree_data,
                                 WorldTransition &UNUSED(world_transition),
                                 const VNode &vnode)
{
  ArrayRef<std::string> system_names = vtree_data.find_target_system_names(
      vnode.output(0, "Influence"));
  Action &action = vtree_data.build_action_list(collector, vnode, "Execute");

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
  map.add_new("fn_GravityForceNode", PARSE_gravity_force);
  map.add_new("fn_AgeReachedEventNode", PARSE_age_reached_event);
  map.add_new("fn_ParticleTrailsNode", PARSE_trails);
  map.add_new("fn_InitialGridEmitterNode", PARSE_initial_grid_emitter);
  map.add_new("fn_TurbulenceForceNode", PARSE_turbulence_force);
  map.add_new("fn_MeshCollisionEventNode", PARSE_mesh_collision);
  map.add_new("fn_SizeOverTimeNode", PARSE_size_over_time);
  map.add_new("fn_DragForceNode", PARSE_drag_force);
  map.add_new("fn_MeshForceNode", PARSE_mesh_force);
  map.add_new("fn_CustomEventNode", PARSE_custom_event);
  map.add_new("fn_AlwaysExecuteNode", PARSE_always_execute);
  return map;
}

static void collect_influences(VTreeData &vtree_data,
                               WorldTransition &world_transition,
                               Vector<std::string> &r_system_names,
                               Vector<Emitter *> &r_emitters,
                               MultiMap<std::string, Event *> &r_events_per_type,
                               MultiMap<std::string, OffsetHandler *> &r_offset_handler_per_type,
                               StringMap<AttributesInfoBuilder> &r_attributes_per_type,
                               StringMap<AttributesDefaults *> &r_attributes_defaults,
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
      r_attributes_defaults,
  };

  for (const VNode *vnode : vtree_data.vtree().nodes_with_idname(particle_system_idname)) {
    StringRef name = vnode->name();
    r_system_names.append(name);
    r_attributes_per_type.add_new(name, AttributesInfoBuilder());
    r_attributes_defaults.add_new(name, new AttributesDefaults());
  }

  for (const VNode *vnode : vtree_data.vtree().nodes()) {
    StringRef idname = vnode->idname();
    ParseNodeCallback *callback = parsers.lookup_ptr(idname);
    if (callback != nullptr) {
      (*callback)(collector, vtree_data, world_transition, *vnode);
    }
  }

  for (std::string &system_name : r_system_names) {
    AttributesInfoBuilder &attributes = r_attributes_per_type.lookup(system_name);
    AttributesDefaults &defaults = *r_attributes_defaults.lookup(system_name);

    attributes.add<uint8_t>("Kill State");
    defaults.add<uint8_t>("Kill State", 0);

    attributes.add<int32_t>("ID");
    defaults.add<int32_t>("ID", 0);

    attributes.add<float>("Birth Time");
    defaults.add<float>("Birth Time", 0);

    attributes.add<float3>("Position");
    defaults.add<float3>("Position", float3(0, 0, 0));

    attributes.add<float3>("Velocity");
    defaults.add<float3>("Velocity", float3(0, 0, 0));

    attributes.add<float>("Size");
    defaults.add<float>("Size", 0.05f);

    attributes.add<rgba_f>("Color");
    defaults.add<rgba_f>("Color", rgba_f(1, 1, 1, 1));

    ArrayRef<Force *> forces = collector.m_forces.lookup_default(system_name);
    EulerIntegrator *integrator = new EulerIntegrator(forces);

    r_integrators.add_new(system_name, integrator);
  }
}

class NodeTreeStepSimulator : public StepSimulator {
 private:
  bNodeTree *m_btree;
  std::unique_ptr<VirtualNodeTree> m_vtree;

 public:
  NodeTreeStepSimulator(bNodeTree *btree) : m_btree(btree)
  {
    BKE::VirtualNodeTreeBuilder vtree_builder;
    vtree_builder.add_all_of_node_tree(btree);
    m_vtree = vtree_builder.build();
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
    StringMap<AttributesInfoBuilder> attributes;
    StringMap<Integrator *> integrators;
    StringMap<AttributesDefaults *> attributes_defaults;

    ResourceCollector resources;
    std::unique_ptr<VTreeMFNetwork> data_graph = FN::generate_vtree_multi_function_network(
        *m_vtree, resources);
    if (data_graph.get() == nullptr) {
      return;
    }
    VTreeData vtree_data(*data_graph);

    collect_influences(vtree_data,
                       world_transition,
                       system_names,
                       emitters,
                       events,
                       offset_handlers,
                       attributes,
                       attributes_defaults,
                       integrators);

    auto &containers = particles_state.particle_containers();

    StringMap<ParticleSystemInfo> systems_to_simulate;
    for (std::string name : system_names) {
      AttributesInfoBuilder &system_attributes = attributes.lookup(name);
      AttributesDefaults &defaults = *attributes_defaults.lookup(name);

      /* Keep old attributes. */
      AttributesBlockContainer *container = containers.lookup_default(name, nullptr);
      if (container != nullptr) {
        system_attributes.add(container->info());
        /* TODO: Remember attribute defaults from before. */
      }

      this->ensure_particle_container_exist_and_has_attributes(
          particles_state, name, system_attributes, defaults);

      ParticleSystemInfo type_info = {
          &defaults,
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

    attributes_defaults.foreach_value([](AttributesDefaults *defaults) { delete defaults; });

    simulation_state.world() = std::move(new_world_state);
  }

 private:
  void ensure_particle_container_exist_and_has_attributes(
      ParticlesState &particles_state,
      StringRef name,
      AttributesInfoBuilder &attributes_info_builder,
      const AttributesDefaults &attributes_defaults)
  {
    auto attributes_info = BLI::make_unique<AttributesInfo>(attributes_info_builder);

    auto &containers = particles_state.particle_containers();
    AttributesBlockContainer *container = containers.lookup_default(name, nullptr);
    if (container == nullptr) {
      AttributesBlockContainer *container = new AttributesBlockContainer(
          std::move(attributes_info), 1000);
      containers.add_new(name, container);
    }
    else {
      container->update_attributes(std::move(attributes_info), attributes_defaults);
    }
  }
};

std::unique_ptr<StepSimulator> simulator_from_node_tree(bNodeTree *btree)
{
  return std::unique_ptr<StepSimulator>(new NodeTreeStepSimulator(btree));
}

}  // namespace BParticles
