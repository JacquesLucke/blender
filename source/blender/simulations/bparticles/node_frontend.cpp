#include "BKE_inlined_node_tree.h"
#include "BKE_deform.h"
#include "BKE_surface_hook.h"

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
#include "FN_multi_function_dependencies.h"

#include "node_frontend.hpp"
#include "integrator.hpp"
#include "emitters.hpp"
#include "events.hpp"
#include "offset_handlers.hpp"
#include "simulate.hpp"

namespace BParticles {

using BKE::IDHandleLookup;
using BKE::ObjectIDHandle;
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
using FN::InlinedTreeMFNetwork;
using FN::MFInputSocket;
using FN::MFOutputSocket;
using FN::MultiFunction;
using FN::NamedGenericTupleRef;

static StringRef particle_system_idname = "fn_ParticleSystemNode";
static StringRef combine_influences_idname = "fn_CombineInfluencesNode";

class InlinedTreeData;
class InfluencesCollector;
class XSocketActionBuilder;

using ActionParserCallback = std::function<void(XSocketActionBuilder &builder)>;
StringMap<ActionParserCallback> &get_action_parsers();

class InfluencesCollector {
 public:
  Vector<Emitter *> &m_emitters;
  MultiMap<std::string, Force *> &m_forces;
  MultiMap<std::string, Event *> &m_events;
  MultiMap<std::string, OffsetHandler *> &m_offset_handlers;
  StringMap<AttributesInfoBuilder *> &m_attributes;
};

class InlinedTreeData {
 private:
  /* Keep this at the beginning, so that it is destructed last. */
  ResourceCollector m_resources;
  InlinedTreeMFNetwork &m_inlined_tree_data_graph;
  FN::ExternalDataCacheContext m_data_cache;
  BKE::IDHandleLookup m_id_handle_lookup;

 public:
  InlinedTreeData(InlinedTreeMFNetwork &inlined_tree_data)
      : m_inlined_tree_data_graph(inlined_tree_data)
  {
    FN::add_objects_used_by_inputs(m_id_handle_lookup, inlined_tree_data.inlined_tree());
  }

  const InlinedNodeTree &inlined_tree()
  {
    return m_inlined_tree_data_graph.inlined_tree();
  }

  const FN::MFNetwork &data_graph()
  {
    return m_inlined_tree_data_graph.network();
  }

  const InlinedTreeMFNetwork &inlined_tree_data_graph()
  {
    return m_inlined_tree_data_graph;
  }

  IDHandleLookup &id_handle_lookup()
  {
    return m_id_handle_lookup;
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
        "Particle Function", fn, m_data_cache, m_id_handle_lookup);

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

  Action *build_action(InfluencesCollector &collector, const XInputSocket &start);

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
};  // namespace BParticles

class XSocketActionBuilder {
 private:
  InfluencesCollector &m_influences_collector;
  InlinedTreeData &m_inlined_tree_data;
  const XSocket &m_execute_xsocket;
  Action *m_built_action = nullptr;

 public:
  XSocketActionBuilder(InfluencesCollector &influences_collector,
                       InlinedTreeData &inlined_tree_data,
                       const XSocket &execute_xsocket)
      : m_influences_collector(influences_collector),
        m_inlined_tree_data(inlined_tree_data),
        m_execute_xsocket(execute_xsocket)
  {
  }

  Action *built_action()
  {
    return m_built_action;
  }

  const XSocket &xsocket() const
  {
    return m_execute_xsocket;
  }

  const CPPType &base_type_of(const XInputSocket &xsocket) const
  {
    return m_inlined_tree_data.inlined_tree_data_graph()
        .lookup_dummy_socket(xsocket)
        .data_type()
        .single__cpp_type();
  }

  template<typename T, typename... Args> T &construct(Args &&... args)
  {
    return m_inlined_tree_data.construct<T>("construct action", std::forward<Args>(args)...);
  }

  template<typename T, typename... Args> T &set_constructed(Args &&... args)
  {
    BLI_STATIC_ASSERT((std::is_base_of<Action, T>::value), "");
    T &action = this->construct<T>(std::forward<Args>(args)...);
    m_built_action = &action;
    return action;
  }

  ParticleFunction *particle_function_for_inputs()
  {
    return m_inlined_tree_data.particle_function_for_all_inputs(m_execute_xsocket.node());
  }

  PointerRNA *node_rna()
  {
    return m_execute_xsocket.node().rna();
  }

  Action &build_input_action_list(StringRef name)
  {
    return m_inlined_tree_data.build_action_list(
        m_influences_collector, m_execute_xsocket.node(), name);
  }

  ArrayRef<std::string> find_system_target_names(uint output_index, StringRef expected_name)
  {
    const XOutputSocket &xsocket = m_execute_xsocket.node().output(output_index, expected_name);
    return m_inlined_tree_data.find_target_system_names(xsocket);
  }

  Optional<NamedGenericTupleRef> compute_all_data_inputs()
  {
    return m_inlined_tree_data.compute_all_data_inputs(m_execute_xsocket.node());
  }

  template<typename T> void add_attribute_to_affected_particles(StringRef name, T default_value)
  {
    this->add_attribute_to_affected_particles(
        name, FN::CPP_TYPE<T>(), (const void *)&default_value);
  }

  void add_attribute_to_affected_particles(StringRef name,
                                           const CPPType &type,
                                           const void *default_value = nullptr)
  {
    /* Add attribute to all particle systems for now. */
    m_influences_collector.m_attributes.foreach_value(
        [&](AttributesInfoBuilder *builder) { builder->add(name, type, default_value); });
  }
};

Action *InlinedTreeData::build_action(InfluencesCollector &collector, const XInputSocket &start)
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

  XSocketActionBuilder builder{collector, *this, execute_socket};
  parser(builder);

  return builder.built_action();
}

static void ACTION_kill(XSocketActionBuilder &builder)
{
  builder.set_constructed<KillAction>();
}

static void ACTION_change_velocity(XSocketActionBuilder &builder)
{
  ParticleFunction *inputs_fn = builder.particle_function_for_inputs();
  if (inputs_fn == nullptr) {
    return;
  }

  int mode = RNA_enum_get(builder.node_rna(), "mode");
  switch (mode) {
    case 0:
      builder.set_constructed<SetVelocityAction>(inputs_fn);
      break;
    case 1:
      builder.set_constructed<RandomizeVelocityAction>(inputs_fn);
      break;
    default:
      BLI_assert(false);
      break;
  }
}

static void ACTION_explode(XSocketActionBuilder &builder)
{
  ParticleFunction *inputs_fn = builder.particle_function_for_inputs();
  if (inputs_fn == nullptr) {
    return;
  }

  Action &on_birth_action = builder.build_input_action_list("Execute on Birth");
  ArrayRef<std::string> system_names = builder.find_system_target_names(1, "Explode System");
  builder.set_constructed<ExplodeAction>(system_names, inputs_fn, on_birth_action);
}

static void ACTION_condition(XSocketActionBuilder &builder)
{
  ParticleFunction *inputs_fn = builder.particle_function_for_inputs();
  if (inputs_fn == nullptr) {
    return;
  }

  Action &action_true = builder.build_input_action_list("Execute If True");
  Action &action_false = builder.build_input_action_list("Execute If False");
  builder.set_constructed<ConditionAction>(inputs_fn, action_true, action_false);
}

static void ACTION_change_color(XSocketActionBuilder &builder)
{
  ParticleFunction *inputs_fn = builder.particle_function_for_inputs();
  if (inputs_fn == nullptr) {
    return;
  }

  builder.set_constructed<ChangeColorAction>(inputs_fn);
}

static void ACTION_change_size(XSocketActionBuilder &builder)
{
  ParticleFunction *inputs_fn = builder.particle_function_for_inputs();
  if (inputs_fn == nullptr) {
    return;
  }

  builder.set_constructed<ChangeSizeAction>(inputs_fn);
}

static void ACTION_change_position(XSocketActionBuilder &builder)
{
  ParticleFunction *inputs_fn = builder.particle_function_for_inputs();
  if (inputs_fn == nullptr) {
    return;
  }

  builder.set_constructed<ChangePositionAction>(inputs_fn);
}

static void ACTION_add_to_group(XSocketActionBuilder &builder)
{
  auto inputs = builder.compute_all_data_inputs();
  if (!inputs.has_value()) {
    return;
  }

  std::string group_name = inputs->relocate_out<std::string>(0, "Group");
  builder.add_attribute_to_affected_particles<bool>(group_name, false);
  builder.set_constructed<AddToGroupAction>(group_name);
}

static void ACTION_remove_from_group(XSocketActionBuilder &builder)
{
  auto inputs = builder.compute_all_data_inputs();
  if (!inputs.has_value()) {
    return;
  }

  std::string group_name = inputs->relocate_out<std::string>(0, "Group");
  builder.set_constructed<RemoveFromGroupAction>(group_name);
}

static std::string RNA_get_string_std(PointerRNA *rna, StringRefNull prop_name)
{
  char *str;
  str = RNA_string_get_alloc(rna, prop_name.data(), nullptr, 0);
  std::string std_string = str;
  MEM_freeN(str);
  return std_string;
}

static void ACTION_set_attribute(XSocketActionBuilder &builder)
{
  ParticleFunction *inputs_fn = builder.particle_function_for_inputs();
  if (inputs_fn == nullptr) {
    return;
  }

  std::string attribute_name = RNA_get_string_std(builder.node_rna(), "attribute_name");
  const CPPType &type = builder.base_type_of(builder.xsocket().node().input(0));

  builder.add_attribute_to_affected_particles(attribute_name, type);
  builder.set_constructed<SetAttributeAction>(attribute_name, type, *inputs_fn);
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
  map.add_new("fn_SetParticleAttributeNode", ACTION_set_attribute);
  return map;
}

class XNodeInfluencesBuilder {
 private:
  InfluencesCollector &m_influences_collector;
  InlinedTreeData &m_inlined_tree_data;
  WorldTransition &m_world_transition;
  const XNode &m_xnode;

 public:
  XNodeInfluencesBuilder(InfluencesCollector &influences_collector,
                         InlinedTreeData &inlined_tree_data,
                         WorldTransition &world_transition,
                         const XNode &xnode)
      : m_influences_collector(influences_collector),
        m_inlined_tree_data(inlined_tree_data),
        m_world_transition(world_transition),
        m_xnode(xnode)
  {
  }

  const XNode &xnode() const
  {
    return m_xnode;
  }

  Optional<NamedGenericTupleRef> compute_all_data_inputs()
  {
    return m_inlined_tree_data.compute_all_data_inputs(m_xnode);
  }

  Optional<NamedGenericTupleRef> compute_inputs(ArrayRef<uint> input_indices)
  {
    return m_inlined_tree_data.compute_inputs(m_xnode, input_indices);
  }

  const MultiFunction *function_for_inputs(ArrayRef<uint> input_indices)
  {
    return m_inlined_tree_data.function_for_inputs(m_xnode, input_indices);
  }

  Action &build_action_list(StringRef name)
  {
    return m_inlined_tree_data.build_action_list(m_influences_collector, m_xnode, name);
  }

  ArrayRef<std::string> find_target_system_names(uint output_index, StringRef expected_name)
  {
    return m_inlined_tree_data.find_target_system_names(
        m_xnode.output(output_index, expected_name));
  }

  WorldTransition &world_transition()
  {
    return m_world_transition;
  }

  template<typename T, typename... Args> T &construct(Args &&... args)
  {
    return m_inlined_tree_data.construct<T>(__func__, std::forward<Args>(args)...);
  }

  void add_emitter(Emitter &emitter)
  {
    m_influences_collector.m_emitters.append(&emitter);
  }

  void add_force(ArrayRef<std::string> system_names, Force &force)
  {
    for (StringRef system_name : system_names) {
      m_influences_collector.m_forces.add(system_name, &force);
    }
  }

  void add_event(ArrayRef<std::string> system_names, Event &event)
  {
    for (StringRef system_name : system_names) {
      m_influences_collector.m_events.add(system_name, &event);
    }
  }

  void add_offset_handler(ArrayRef<std::string> system_names, OffsetHandler &offset_handler)
  {
    for (StringRef system_name : system_names) {
      m_influences_collector.m_offset_handlers.add(system_name, &offset_handler);
    }
  }

  std::string node_identifier()
  {
    std::stringstream ss;
    for (const BKE::XParentNode *parent = m_xnode.parent(); parent; parent = parent->parent()) {
      ss << "/" << parent->vnode().name();
    }
    ss << "/" << m_xnode.name();

    std::string identifier = ss.str();
    return identifier;
  }

  IDHandleLookup &id_handle_lookup()
  {
    return m_inlined_tree_data.id_handle_lookup();
  }

  PointerRNA *node_rna()
  {
    return m_xnode.rna();
  }

  ParticleFunction *particle_function_for_all_inputs()
  {
    return m_inlined_tree_data.particle_function_for_all_inputs(m_xnode);
  }

  FN::MFDataType data_type_of_input(const XInputSocket &xsocket)
  {
    return m_inlined_tree_data.inlined_tree_data_graph().lookup_dummy_socket(xsocket).data_type();
  }

  template<typename T>
  void add_attribute(ArrayRef<std::string> system_names, StringRef name, T default_value)
  {
    this->add_attribute(system_names, name, (const void *)&default_value);
  }

  void add_attribute(ArrayRef<std::string> system_names,
                     StringRef name,
                     const CPPType &type,
                     const void *default_value = nullptr)
  {
    for (StringRef system_name : system_names) {
      m_influences_collector.m_attributes.lookup(system_name)->add(name, type, default_value);
    }
  }
};

using ParseNodeCallback = std::function<void(XNodeInfluencesBuilder &builder)>;

static void PARSE_point_emitter(XNodeInfluencesBuilder &builder)
{
  Optional<NamedGenericTupleRef> inputs = builder.compute_all_data_inputs();
  if (!inputs.has_value()) {
    return;
  }

  Action &action = builder.build_action_list("Execute on Birth");

  ArrayRef<std::string> system_names = builder.find_target_system_names(0, "Emitter");
  std::string identifier = builder.node_identifier();

  WorldTransition &world_transition = builder.world_transition();
  VaryingFloat3 position = world_transition.update_float3(
      identifier, "Position", inputs->get<float3>(0, "Position"));
  VaryingFloat3 velocity = world_transition.update_float3(
      identifier, "Velocity", inputs->get<float3>(1, "Velocity"));
  VaryingFloat size = world_transition.update_float(
      identifier, "Size", inputs->get<float>(2, "Size"));

  Emitter &emitter = builder.construct<PointEmitter>(
      std::move(system_names), position, velocity, size, action);
  builder.add_emitter(emitter);
}

static void PARSE_custom_emitter(XNodeInfluencesBuilder &builder)
{
  const XNode &xnode = builder.xnode();
  const XInputSocket &first_execute_socket = *xnode.input_with_name_prefix("Execute on Birth");
  ArrayRef<const XInputSocket *> data_inputs = xnode.inputs().take_front(
      first_execute_socket.index());
  ArrayRef<uint> input_indices = IndexRange(data_inputs.size()).as_array_ref();
  const MultiFunction *emitter_function = builder.function_for_inputs(input_indices);
  if (emitter_function == nullptr) {
    return;
  }

  ArrayRef<std::string> system_names = builder.find_target_system_names(0, "Emitter");

  Vector<std::string> attribute_names;
  for (const XInputSocket *socket : data_inputs) {
    StringRef attribute_name = socket->name();
    attribute_names.append(attribute_name);
    const CPPType *attribute_type = nullptr;

    FN::MFDataType data_type = builder.data_type_of_input(*socket);
    if (data_type.is_single()) {
      attribute_type = &data_type.single__cpp_type();
    }
    else if (data_type.is_vector()) {
      attribute_type = &data_type.vector__cpp_base_type();
    }
    else {
      BLI_assert(false);
    }

    builder.add_attribute(system_names, attribute_name, *attribute_type);
  }

  Action &action = builder.build_action_list("Execute on Birth");

  Emitter &emitter = builder.construct<CustomEmitter>(
      system_names, *emitter_function, std::move(attribute_names), action);
  builder.add_emitter(emitter);
}

static Vector<float> compute_emitter_vertex_weights(PointerRNA *node_rna,
                                                    NamedGenericTupleRef inputs,
                                                    Object *object)
{
  uint density_mode = RNA_enum_get(node_rna, "density_mode");

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

static void PARSE_mesh_emitter(XNodeInfluencesBuilder &builder)
{
  Optional<NamedGenericTupleRef> inputs = builder.compute_all_data_inputs();
  if (!inputs.has_value()) {
    return;
  }

  Action &on_birth_action = builder.build_action_list("Execute on Birth");

  ObjectIDHandle object_handle = inputs->relocate_out<ObjectIDHandle>(0, "Object");
  Object *object = builder.id_handle_lookup().lookup(object_handle);
  if (object == nullptr || object->type != OB_MESH) {
    return;
  }

  auto vertex_weights = compute_emitter_vertex_weights(builder.node_rna(), *inputs, object);

  VaryingFloat4x4 transform = builder.world_transition().update_float4x4(
      object->id.name, "obmat", object->obmat);

  ArrayRef<std::string> system_names = builder.find_target_system_names(0, "Emitter");
  Emitter &emitter = builder.construct<SurfaceEmitter>(system_names,
                                                       on_birth_action,
                                                       object,
                                                       transform,
                                                       inputs->get<float>(1, "Rate"),
                                                       std::move(vertex_weights));
  builder.add_emitter(emitter);
}

static void PARSE_custom_force(XNodeInfluencesBuilder &builder)
{
  ParticleFunction *inputs_fn = builder.particle_function_for_all_inputs();
  if (inputs_fn == nullptr) {
    return;
  }

  ArrayRef<std::string> system_names = builder.find_target_system_names(0, "Force");
  CustomForce &force = builder.construct<CustomForce>(inputs_fn);
  builder.add_force(system_names, force);
}

static void PARSE_age_reached_event(XNodeInfluencesBuilder &builder)
{
  ParticleFunction *inputs_fn = builder.particle_function_for_all_inputs();
  if (inputs_fn == nullptr) {
    return;
  }

  ArrayRef<std::string> system_names = builder.find_target_system_names(0, "Event");
  Action &action = builder.build_action_list("Execute on Event");

  std::string is_triggered_attribute = builder.node_identifier();

  builder.add_attribute<bool>(system_names, is_triggered_attribute, false);
  Event &event = builder.construct<AgeReachedEvent>(is_triggered_attribute, inputs_fn, action);
  builder.add_event(system_names, event);
}

static void PARSE_trails(XNodeInfluencesBuilder &builder)
{
  ArrayRef<std::string> main_system_names = builder.find_target_system_names(0, "Main System");
  ArrayRef<std::string> trail_system_names = builder.find_target_system_names(1, "Trail System");

  ParticleFunction *inputs_fn = builder.particle_function_for_all_inputs();
  if (inputs_fn == nullptr) {
    return;
  }

  Action &action = builder.build_action_list("Execute on Birth");
  OffsetHandler &offset_handler = builder.construct<CreateTrailHandler>(
      trail_system_names, inputs_fn, action);
  builder.add_offset_handler(main_system_names, offset_handler);
}

static void PARSE_initial_grid_emitter(XNodeInfluencesBuilder &builder)
{
  Optional<NamedGenericTupleRef> inputs = builder.compute_all_data_inputs();
  if (!inputs.has_value()) {
    return;
  }

  Action &action = builder.build_action_list("Execute on Birth");

  ArrayRef<std::string> system_names = builder.find_target_system_names(0, "Emitter");
  Emitter &emitter = builder.construct<InitialGridEmitter>(
      std::move(system_names),
      std::max(0, inputs->get<int>(0, "Amount X")),
      std::max(0, inputs->get<int>(1, "Amount Y")),
      inputs->get<float>(2, "Step X"),
      inputs->get<float>(3, "Step Y"),
      inputs->get<float>(4, "Size"),
      action);
  builder.add_emitter(emitter);
}

static void PARSE_mesh_collision(XNodeInfluencesBuilder &builder)
{
  ParticleFunction *inputs_fn = builder.particle_function_for_all_inputs();
  if (inputs_fn == nullptr) {
    return;
  }

  Optional<NamedGenericTupleRef> inputs = builder.compute_inputs({0});
  if (!inputs.has_value()) {
    return;
  }

  ObjectIDHandle object_handle = inputs->relocate_out<ObjectIDHandle>(0, "Object");
  Object *object = builder.id_handle_lookup().lookup(object_handle);
  if (object == nullptr || object->type != OB_MESH) {
    return;
  }

  ArrayRef<std::string> system_names = builder.find_target_system_names(0, "Event");
  Action &action = builder.build_action_list("Execute on Event");

  float4x4 local_to_world_end = object->obmat;
  float4x4 local_to_world_begin =
      builder.world_transition().update_float4x4(object->id.name, "obmat", object->obmat).start;

  std::string last_collision_attribute = builder.node_identifier();

  Event &event = builder.construct<MeshCollisionEvent>(
      last_collision_attribute, object, action, local_to_world_begin, local_to_world_end);
  builder.add_attribute<int32_t>(system_names, last_collision_attribute, -1);
  builder.add_event(system_names, event);
}

static void PARSE_size_over_time(XNodeInfluencesBuilder &builder)
{
  ParticleFunction *inputs_fn = builder.particle_function_for_all_inputs();
  if (inputs_fn == nullptr) {
    return;
  }

  ArrayRef<std::string> system_names = builder.find_target_system_names(0, "Influence");
  OffsetHandler &offset_handler = builder.construct<SizeOverTimeHandler>(inputs_fn);
  builder.add_offset_handler(system_names, offset_handler);
}

static void PARSE_custom_event(XNodeInfluencesBuilder &builder)
{
  ParticleFunction *inputs_fn = builder.particle_function_for_all_inputs();
  if (inputs_fn == nullptr) {
    return;
  }

  ArrayRef<std::string> system_names = builder.find_target_system_names(0, "Event");
  Action &action = builder.build_action_list("Execute on Event");

  std::string is_triggered_attribute = builder.node_identifier();

  Event &event = builder.construct<CustomEvent>(is_triggered_attribute, inputs_fn, action);
  builder.add_attribute<bool>(system_names, is_triggered_attribute, false);
  builder.add_event(system_names, event);
}

static void PARSE_always_execute(XNodeInfluencesBuilder &builder)
{
  ArrayRef<std::string> system_names = builder.find_target_system_names(0, "Influence");
  Action &action = builder.build_action_list("Execute");

  OffsetHandler &offset_handler = builder.construct<AlwaysExecuteHandler>(action);
  builder.add_offset_handler(system_names, offset_handler);
}

BLI_LAZY_INIT_STATIC(StringMap<ParseNodeCallback>, get_node_parsers)
{
  StringMap<ParseNodeCallback> map;
  map.add_new("fn_PointEmitterNode", PARSE_point_emitter);
  map.add_new("fn_CustomEmitterNode", PARSE_custom_emitter);
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

static void collect_influences(InlinedTreeData &inlined_tree_data,
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
      XNodeInfluencesBuilder builder{collector, inlined_tree_data, world_transition, *xnode};
      (*callback)(builder);
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
    attributes.add<BKE::SurfaceHook>("Emit Hook", {});

    ArrayRef<Force *> forces = collector.m_forces.lookup_default(system_name);
    EulerIntegrator &integrator = inlined_tree_data.construct<EulerIntegrator>("integrator",
                                                                               forces);

    r_integrators.add_new(system_name, &integrator);
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
    std::unique_ptr<InlinedTreeMFNetwork> data_graph =
        FN::generate_inlined_tree_multi_function_network(m_inlined_tree, resources);
    if (data_graph.get() == nullptr) {
      return;
    }
    InlinedTreeData inlined_tree_data(*data_graph);

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
