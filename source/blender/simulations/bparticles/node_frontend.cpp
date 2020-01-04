#include "BKE_deform.h"
#include "BKE_surface_hook.h"
#include "BKE_id_data_cache.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_timeit.h"
#include "BLI_multi_map.h"
#include "BLI_set.h"
#include "BLI_lazy_init_cxx.h"

#include "FN_node_tree.h"
#include "FN_multi_functions.h"
#include "FN_generic_tuple.h"
#include "FN_node_tree_multi_function_network_generation.h"
#include "FN_multi_function_common_contexts.h"
#include "FN_multi_function_dependencies.h"

#include "node_frontend.hpp"
#include "integrator.hpp"
#include "emitters.hpp"
#include "events.hpp"
#include "offset_handlers.hpp"
#include "simulate.hpp"

namespace BParticles {

using BKE::IDDataCache;
using BKE::IDHandleLookup;
using BKE::ObjectIDHandle;
using BLI::destruct_ptr;
using BLI::MultiMap;
using BLI::ResourceCollector;
using BLI::rgba_f;
using BLI::ScopedVector;
using BLI::Set;
using BLI::StringMultiMap;
using FN::AttributesInfoBuilder;
using FN::CPPType;
using FN::FGroupInput;
using FN::FInputSocket;
using FN::FNode;
using FN::FOutputSocket;
using FN::FSocket;
using FN::FunctionTreeMFNetwork;
using FN::MFInputSocket;
using FN::MFOutputSocket;
using FN::MultiFunction;
using FN::NamedGenericTupleRef;

static StringRef particle_system_idname = "fn_ParticleSystemNode";
static StringRef combine_influences_idname = "fn_CombineInfluencesNode";

class FunctionTreeData;
class InfluencesCollector;
class FSocketActionBuilder;

using ActionParserCallback = std::function<void(FSocketActionBuilder &builder)>;
StringMap<ActionParserCallback> &get_action_parsers();

class InfluencesCollector {
 public:
  Vector<Emitter *> m_emitters;
  StringMultiMap<Force *> m_forces;
  StringMultiMap<Event *> m_events;
  StringMultiMap<OffsetHandler *> m_offset_handlers;
  StringMap<AttributesInfoBuilder *> m_attributes;
};

class FunctionTreeData {
 private:
  /* Keep this at the beginning, so that it is destructed last. */
  ResourceCollector m_resources;
  FunctionTreeMFNetwork &m_function_tree_data_graph;
  IDDataCache m_id_data_cache;
  IDHandleLookup m_id_handle_lookup;

 public:
  FunctionTreeData(FunctionTreeMFNetwork &function_tree_data)
      : m_function_tree_data_graph(function_tree_data)
  {
    FN::add_ids_used_by_nodes(m_id_handle_lookup, function_tree_data.function_tree());
  }

  const FunctionNodeTree &function_tree()
  {
    return m_function_tree_data_graph.function_tree();
  }

  const FN::MFNetwork &data_graph()
  {
    return m_function_tree_data_graph.network();
  }

  const FunctionTreeMFNetwork &function_tree_data_graph()
  {
    return m_function_tree_data_graph;
  }

  IDHandleLookup &id_handle_lookup()
  {
    return m_id_handle_lookup;
  }

  IDDataCache &id_data_cache()
  {
    return m_id_data_cache;
  }

  template<typename T, typename... Args> T &construct(const char *name, Args &&... args)
  {
    void *buffer = m_resources.allocate(sizeof(T), alignof(T));
    T *value = new (buffer) T(std::forward<Args>(args)...);
    m_resources.add(BLI::destruct_ptr<T>(value), name);
    return *value;
  }

  ParticleFunction *particle_function_for_all_inputs(const FNode &fnode)
  {
    Vector<const MFInputSocket *> sockets_to_compute;
    Vector<std::string> names_to_compute;
    for (const FInputSocket *fsocket : fnode.inputs()) {
      if (m_function_tree_data_graph.is_mapped(*fsocket)) {
        sockets_to_compute.append(&m_function_tree_data_graph.lookup_dummy_socket(*fsocket));
        names_to_compute.append(fsocket->name());
      }
    }

    return this->particle_function_for_sockets(std::move(sockets_to_compute),
                                               std::move(names_to_compute));
  }

  ParticleFunction *particle_function_for_inputs(const FNode &fnode, ArrayRef<uint> input_indices)
  {
    Vector<const MFInputSocket *> sockets_to_compute;
    Vector<std::string> names_to_compute;
    for (uint i : input_indices) {
      const MFInputSocket &socket = m_function_tree_data_graph.lookup_dummy_socket(fnode.input(i));
      sockets_to_compute.append(&socket);
      names_to_compute.append(fnode.input(i).name());
    }

    return this->particle_function_for_sockets(std::move(sockets_to_compute),
                                               std::move(names_to_compute));
  }

  ParticleFunction *particle_function_for_sockets(Vector<const MFInputSocket *> sockets_to_compute,
                                                  Vector<std::string> names_to_compute)
  {

    const MultiFunction &fn = this->construct<FN::MF_EvaluateNetwork>(
        "Evaluate Network", Vector<const MFOutputSocket *>(), std::move(sockets_to_compute));
    ParticleFunction &particle_fn = this->construct<ParticleFunction>(
        "Particle Function", fn, std::move(names_to_compute), m_id_data_cache, m_id_handle_lookup);

    return &particle_fn;
  }

  Optional<NamedGenericTupleRef> compute_inputs(const FNode &fnode, ArrayRef<uint> input_indices)
  {
    const MultiFunction *fn = this->function_for_inputs(fnode, input_indices);
    if (fn == nullptr) {
      return {};
    }
    if (fn->uses_element_context<FN::ParticleAttributesContext>()) {
      std::cout << "Inputs may not depend on particle attributes: " << fnode.name() << "\n";
      return {};
    }

    Vector<const CPPType *> computed_types;
    for (uint i : input_indices) {
      FN::MFDataType data_type =
          m_function_tree_data_graph.lookup_dummy_socket(fnode.input(i)).data_type();
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
    context_builder.add_global_context(m_id_handle_lookup);
    context_builder.add_global_context(m_id_data_cache);

    for (uint i = 0; i < input_indices.size(); i++) {
      params_builder.add_single_output(
          FN::GenericMutableArrayRef(tuple.info().type_at_index(i), tuple.element_ptr(i), 1));
    }
    fn->call(BLI::IndexMask(1), params_builder, context_builder);
    tuple.set_all_initialized();

    Vector<std::string> computed_names;
    for (uint i : input_indices) {
      computed_names.append(fnode.input(i).name());
    }

    auto &name_provider = this->construct<FN::CustomGenericTupleNameProvider>(
        __func__, std::move(computed_names));
    NamedGenericTupleRef named_tuple_ref{tuple, name_provider};

    return named_tuple_ref;
  }

  Optional<NamedGenericTupleRef> compute_all_data_inputs(const FNode &fnode)
  {
    ScopedVector<uint> data_input_indices;
    for (uint i : fnode.inputs().index_range()) {
      if (m_function_tree_data_graph.is_mapped(fnode.input(i))) {
        data_input_indices.append(i);
      }
    }

    return this->compute_inputs(fnode, data_input_indices);
  }

  ArrayRef<std::string> find_target_system_names(const FOutputSocket &output_fsocket)
  {
    VectorSet<const FNode *> system_fnodes;
    this->find_target_system_nodes__recursive(output_fsocket, system_fnodes);

    auto &system_names = this->construct<Vector<std::string>>(__func__);
    for (const FNode *fnode : system_fnodes) {
      system_names.append(fnode->name());
    }

    return system_names;
  }

  ParticleAction *build_action(InfluencesCollector &collector,
                               const FInputSocket &start,
                               ArrayRef<std::string> system_names);

  ParticleAction &build_action_list(InfluencesCollector &collector,
                                    const FNode &start_fnode,
                                    StringRef name,
                                    ArrayRef<std::string> system_names)
  {
    Vector<const FInputSocket *> execute_sockets = this->find_execute_sockets(start_fnode, name);
    Vector<ParticleAction *> actions;
    for (const FInputSocket *socket : execute_sockets) {
      ParticleAction *action = this->build_action(collector, *socket, system_names);
      if (action != nullptr) {
        actions.append(action);
      }
    }
    ParticleAction &sequence = this->construct<ActionSequence>(__func__, std::move(actions));
    return sequence;
  }

  const FN::MultiFunction *function_for_inputs(const FNode &fnode, ArrayRef<uint> input_indices)
  {
    Vector<const MFInputSocket *> sockets_to_compute;
    for (uint index : input_indices) {
      sockets_to_compute.append(
          &m_function_tree_data_graph.lookup_dummy_socket(fnode.input(index)));
    }

    if (m_function_tree_data_graph.network().find_dummy_dependencies(sockets_to_compute).size() >
        0) {
      return nullptr;
    }

    auto fn = BLI::make_unique<FN::MF_EvaluateNetwork>(ArrayRef<const MFOutputSocket *>(),
                                                       sockets_to_compute);
    const FN::MultiFunction *fn_ptr = fn.get();
    m_resources.add(std::move(fn), __func__);
    return fn_ptr;
  }

  bool try_add_attribute(InfluencesCollector &collector,
                         ArrayRef<std::string> system_names,
                         StringRef name,
                         const CPPType &type,
                         const void *default_value)
  {
    bool collides_with_existing = false;
    for (StringRef system_name : system_names) {
      AttributesInfoBuilder *attributes = collector.m_attributes.lookup(system_name);
      collides_with_existing = collides_with_existing ||
                               attributes->name_and_type_collide_with_existing(name, type);
    }

    if (collides_with_existing) {
      return false;
    }

    for (StringRef system_name : system_names) {
      collector.m_attributes.lookup(system_name)->add(name, type, default_value);
    }

    return true;
  }

 private:
  Vector<const FNode *> find_target_system_nodes(const FOutputSocket &fsocket)
  {
    VectorSet<const FNode *> type_nodes;
    find_target_system_nodes__recursive(fsocket, type_nodes);
    return Vector<const FNode *>(type_nodes);
  }

  void find_target_system_nodes__recursive(const FOutputSocket &output_fsocket,
                                           VectorSet<const FNode *> &r_nodes)
  {
    for (const FInputSocket *connected : output_fsocket.linked_sockets()) {
      const FNode &connected_fnode = connected->node();
      if (connected_fnode.idname() == particle_system_idname) {
        r_nodes.add(&connected_fnode);
      }
      else if (connected_fnode.idname() == combine_influences_idname) {
        find_target_system_nodes__recursive(connected_fnode.output(0), r_nodes);
      }
    }
  }

  Vector<const FInputSocket *> find_execute_sockets(const FNode &fnode, StringRef name_prefix)
  {
    int first_index = -1;
    for (const FInputSocket *fsocket : fnode.inputs()) {
      if (fsocket->name() == name_prefix) {
        first_index = fsocket->index();
        break;
      }
    }
    BLI_assert(first_index >= 0);

    Vector<const FInputSocket *> execute_sockets;
    for (const FInputSocket *fsocket : fnode.inputs().drop_front(first_index)) {
      if (fsocket->idname() == "fn_OperatorSocket") {
        break;
      }
      else {
        execute_sockets.append(fsocket);
      }
    }

    return execute_sockets;
  }
};

class FSocketActionBuilder {
 private:
  InfluencesCollector &m_influences_collector;
  FunctionTreeData &m_function_tree_data;
  const FSocket &m_execute_fsocket;
  ArrayRef<std::string> m_system_names;
  ParticleAction *m_built_action = nullptr;

 public:
  FSocketActionBuilder(InfluencesCollector &influences_collector,
                       FunctionTreeData &function_tree_data,
                       const FSocket &execute_fsocket,
                       ArrayRef<std::string> system_names)
      : m_influences_collector(influences_collector),
        m_function_tree_data(function_tree_data),
        m_execute_fsocket(execute_fsocket),
        m_system_names(system_names)
  {
  }

  ParticleAction *built_action()
  {
    return m_built_action;
  }

  const FSocket &fsocket() const
  {
    return m_execute_fsocket;
  }

  ArrayRef<std::string> system_names() const
  {
    return m_system_names;
  }

  const CPPType &base_type_of(const FInputSocket &fsocket) const
  {
    return m_function_tree_data.function_tree_data_graph()
        .lookup_dummy_socket(fsocket)
        .data_type()
        .single__cpp_type();
  }

  template<typename T, typename... Args> T &construct(Args &&... args)
  {
    return m_function_tree_data.construct<T>("construct action", std::forward<Args>(args)...);
  }

  template<typename T, typename... Args> T &set_constructed(Args &&... args)
  {
    BLI_STATIC_ASSERT((std::is_base_of<ParticleAction, T>::value), "");
    T &action = this->construct<T>(std::forward<Args>(args)...);
    this->set(action);
    return action;
  }

  void set(ParticleAction &action)
  {
    m_built_action = &action;
  }

  ParticleFunction *particle_function_for_all_inputs()
  {
    return m_function_tree_data.particle_function_for_all_inputs(m_execute_fsocket.node());
  }

  ParticleFunction *particle_function_for_inputs(ArrayRef<uint> input_indices)
  {
    return m_function_tree_data.particle_function_for_inputs(m_execute_fsocket.node(),
                                                             input_indices);
  }

  const MultiFunction *function_for_inputs(ArrayRef<uint> input_indices)
  {
    return m_function_tree_data.function_for_inputs(m_execute_fsocket.node(), input_indices);
  }

  FN::MFDataType data_type_of_input(const FInputSocket &fsocket)
  {
    return m_function_tree_data.function_tree_data_graph()
        .lookup_dummy_socket(fsocket)
        .data_type();
  }

  PointerRNA *node_rna()
  {
    return m_execute_fsocket.node().rna();
  }

  ParticleAction &build_input_action_list(StringRef name, ArrayRef<std::string> system_names)
  {
    return m_function_tree_data.build_action_list(
        m_influences_collector, m_execute_fsocket.node(), name, system_names);
  }

  ArrayRef<std::string> find_system_target_names(uint output_index, StringRef expected_name)
  {
    const FOutputSocket &fsocket = m_execute_fsocket.node().output(output_index, expected_name);
    return m_function_tree_data.find_target_system_names(fsocket);
  }

  Optional<NamedGenericTupleRef> compute_all_data_inputs()
  {
    return m_function_tree_data.compute_all_data_inputs(m_execute_fsocket.node());
  }

  Optional<NamedGenericTupleRef> compute_inputs(ArrayRef<uint> input_indices)
  {
    return m_function_tree_data.compute_inputs(m_execute_fsocket.node(), input_indices);
  }

  template<typename T>
  bool try_add_attribute_to_affected_particles(StringRef name, T default_value)
  {
    return this->try_add_attribute_to_affected_particles(
        name, FN::CPP_TYPE<T>(), (const void *)&default_value);
  }

  bool try_add_attribute_to_affected_particles(StringRef name,
                                               const CPPType &type,
                                               const void *default_value = nullptr)
  {
    /* Add attribute to all particle systems for now. */
    ScopedVector<std::string> system_names;
    m_influences_collector.m_attributes.foreach_key(
        [&](StringRef name) { system_names.append(name); });

    return m_function_tree_data.try_add_attribute(
        m_influences_collector, system_names, name, type, default_value);
  }

  bool try_add_attribute(ArrayRef<std::string> system_names,
                         StringRef name,
                         const CPPType &type,
                         const void *default_value = nullptr)
  {
    return m_function_tree_data.try_add_attribute(
        m_influences_collector, system_names, name, type, default_value);
  }

  IDHandleLookup &id_handle_lookup()
  {
    return m_function_tree_data.id_handle_lookup();
  }

  BKE::IDDataCache &id_data_cache()
  {
    return m_function_tree_data.id_data_cache();
  }
};

ParticleAction *FunctionTreeData::build_action(InfluencesCollector &collector,
                                               const FInputSocket &start,
                                               ArrayRef<std::string> system_names)
{
  if (start.linked_sockets().size() != 1) {
    return nullptr;
  }

  const FSocket &execute_socket = *start.linked_sockets()[0];
  if (execute_socket.idname() != "fn_ExecuteSocket") {
    return nullptr;
  }

  StringMap<ActionParserCallback> &parsers = get_action_parsers();
  ActionParserCallback *parser = parsers.lookup_ptr(execute_socket.node().idname());
  if (parser == nullptr) {
    std::cout << "Expected to find parser for: " << execute_socket.node().idname() << "\n";
    return nullptr;
  }

  FSocketActionBuilder builder{collector, *this, execute_socket, system_names};
  (*parser)(builder);

  return builder.built_action();
}

static void ACTION_spawn(FSocketActionBuilder &builder)
{
  const FNode &fnode = builder.fsocket().node();
  const FInputSocket &first_execute_socket = *fnode.input_with_name_prefix("Execute on Birth");
  ArrayRef<const FInputSocket *> data_inputs = fnode.inputs().take_front(
      first_execute_socket.index());
  ArrayRef<uint> input_indices = IndexRange(data_inputs.size()).as_array_ref();
  const ParticleFunction *inputs_fn = builder.particle_function_for_inputs(input_indices);
  if (inputs_fn == nullptr) {
    return;
  }

  ArrayRef<std::string> system_names = builder.find_system_target_names(1, "Spawn System");
  if (builder.system_names().intersects__linear_search(system_names)) {
    std::cout << "Warning: cannot spawn particles in same system yet.\n";
    return;
  }

  Vector<std::string> attribute_names;
  for (const FInputSocket *fsocket : data_inputs) {
    StringRef attribute_name = fsocket->name();
    attribute_names.append(attribute_name);
    const CPPType *attribute_type = nullptr;

    FN::MFDataType data_type = builder.data_type_of_input(*fsocket);
    if (data_type.is_single()) {
      attribute_type = &data_type.single__cpp_type();
    }
    else if (data_type.is_vector()) {
      attribute_type = &data_type.vector__cpp_base_type();
    }
    else {
      BLI_assert(false);
    }

    builder.try_add_attribute(system_names, attribute_name, *attribute_type);
  }

  ParticleAction &action = builder.build_input_action_list("Execute on Birth", system_names);

  builder.set_constructed<SpawnParticlesAction>(
      system_names, *inputs_fn, std::move(attribute_names), action);
}

static void ACTION_condition(FSocketActionBuilder &builder)
{
  ParticleFunction *inputs_fn = builder.particle_function_for_all_inputs();
  if (inputs_fn == nullptr) {
    return;
  }

  ParticleAction &action_true = builder.build_input_action_list("Execute If True",
                                                                builder.system_names());
  ParticleAction &action_false = builder.build_input_action_list("Execute If False",
                                                                 builder.system_names());
  builder.set_constructed<ConditionAction>(*inputs_fn, action_true, action_false);
}

static void ACTION_set_attribute(FSocketActionBuilder &builder)
{
  Optional<NamedGenericTupleRef> values = builder.compute_inputs({0});
  if (!values.has_value()) {
    return;
  }

  ParticleFunction *inputs_fn = builder.particle_function_for_inputs({1});
  if (inputs_fn == nullptr) {
    return;
  }

  const CPPType &attribute_type = builder.base_type_of(builder.fsocket().node().input(1));
  std::string attribute_name = values->relocate_out<std::string>(0, "Name");

  bool attribute_added = builder.try_add_attribute_to_affected_particles(attribute_name,
                                                                         attribute_type);
  if (!attribute_added) {
    return;
  }

  builder.set_constructed<SetAttributeAction>(attribute_name, attribute_type, *inputs_fn);
}

static void ACTION_multi_execute(FSocketActionBuilder &builder)
{
  ParticleAction &action = builder.build_input_action_list("Execute", builder.system_names());
  builder.set(action);
}

BLI_LAZY_INIT(StringMap<ActionParserCallback>, get_action_parsers)
{
  StringMap<ActionParserCallback> map;
  map.add_new("fn_SpawnParticlesNode", ACTION_spawn);
  map.add_new("fn_ParticleConditionNode", ACTION_condition);
  map.add_new("fn_SetParticleAttributeNode", ACTION_set_attribute);
  map.add_new("fn_MultiExecuteNode", ACTION_multi_execute);
  return map;
}

class FNodeInfluencesBuilder {
 private:
  InfluencesCollector &m_influences_collector;
  FunctionTreeData &m_function_tree_data;
  WorldTransition &m_world_transition;
  const FNode &m_fnode;

 public:
  FNodeInfluencesBuilder(InfluencesCollector &influences_collector,
                         FunctionTreeData &function_tree_data,
                         WorldTransition &world_transition,
                         const FNode &fnode)
      : m_influences_collector(influences_collector),
        m_function_tree_data(function_tree_data),
        m_world_transition(world_transition),
        m_fnode(fnode)
  {
  }

  const FNode &fnode() const
  {
    return m_fnode;
  }

  Optional<NamedGenericTupleRef> compute_all_data_inputs()
  {
    return m_function_tree_data.compute_all_data_inputs(m_fnode);
  }

  Optional<NamedGenericTupleRef> compute_inputs(ArrayRef<uint> input_indices)
  {
    return m_function_tree_data.compute_inputs(m_fnode, input_indices);
  }

  const MultiFunction *function_for_inputs(ArrayRef<uint> input_indices)
  {
    return m_function_tree_data.function_for_inputs(m_fnode, input_indices);
  }

  ParticleAction &build_action_list(StringRef name, ArrayRef<std::string> system_names)
  {
    return m_function_tree_data.build_action_list(
        m_influences_collector, m_fnode, name, system_names);
  }

  ArrayRef<std::string> find_target_system_names(uint output_index, StringRef expected_name)
  {
    return m_function_tree_data.find_target_system_names(
        m_fnode.output(output_index, expected_name));
  }

  WorldTransition &world_transition()
  {
    return m_world_transition;
  }

  template<typename T, typename... Args> T &construct(Args &&... args)
  {
    return m_function_tree_data.construct<T>(__func__, std::forward<Args>(args)...);
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
    ss << "private/node";
    for (const FN::FParentNode *parent = m_fnode.parent(); parent; parent = parent->parent()) {
      ss << "/" << parent->vnode().name();
    }
    ss << "/" << m_fnode.name();

    std::string identifier = ss.str();
    return identifier;
  }

  IDHandleLookup &id_handle_lookup()
  {
    return m_function_tree_data.id_handle_lookup();
  }

  BKE::IDDataCache &id_data_cache()
  {
    return m_function_tree_data.id_data_cache();
  }

  PointerRNA *node_rna()
  {
    return m_fnode.rna();
  }

  ParticleFunction *particle_function_for_all_inputs()
  {
    return m_function_tree_data.particle_function_for_all_inputs(m_fnode);
  }

  FN::MFDataType data_type_of_input(const FInputSocket &fsocket)
  {
    return m_function_tree_data.function_tree_data_graph()
        .lookup_dummy_socket(fsocket)
        .data_type();
  }

  template<typename T>
  bool try_add_attribute(ArrayRef<std::string> system_names, StringRef name, T default_value)
  {
    return this->try_add_attribute(
        system_names, name, FN::CPP_TYPE<T>(), (const void *)&default_value);
  }

  bool try_add_attribute(ArrayRef<std::string> system_names,
                         StringRef name,
                         const CPPType &type,
                         const void *default_value = nullptr)
  {
    return m_function_tree_data.try_add_attribute(
        m_influences_collector, system_names, name, type, default_value);
  }
};

using ParseNodeCallback = std::function<void(FNodeInfluencesBuilder &builder)>;

static void PARSE_point_emitter(FNodeInfluencesBuilder &builder)
{
  Optional<NamedGenericTupleRef> inputs = builder.compute_all_data_inputs();
  if (!inputs.has_value()) {
    return;
  }

  ArrayRef<std::string> system_names = builder.find_target_system_names(0, "Emitter");
  ParticleAction &action = builder.build_action_list("Execute on Birth", system_names);

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

static void PARSE_custom_emitter(FNodeInfluencesBuilder &builder)
{
  const FNode &fnode = builder.fnode();
  const FInputSocket &first_execute_socket = *fnode.input_with_name_prefix("Execute on Birth");
  ArrayRef<const FInputSocket *> data_inputs = fnode.inputs().take_front(
      first_execute_socket.index());
  ArrayRef<uint> input_indices = IndexRange(data_inputs.size()).as_array_ref();
  const MultiFunction *emitter_function = builder.function_for_inputs(input_indices);
  if (emitter_function == nullptr) {
    return;
  }

  ArrayRef<std::string> system_names = builder.find_target_system_names(0, "Emitter");

  Vector<std::string> attribute_names;
  for (const FInputSocket *socket : data_inputs) {
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

    builder.try_add_attribute(system_names, attribute_name, *attribute_type);
  }

  ParticleAction &action = builder.build_action_list("Execute on Birth", system_names);
  BirthTimeModes::Enum birth_time_mode = (BirthTimeModes::Enum)RNA_enum_get(builder.node_rna(),
                                                                            "birth_time_mode");

  Emitter &emitter = builder.construct<CustomEmitter>(system_names,
                                                      *emitter_function,
                                                      std::move(attribute_names),
                                                      action,
                                                      birth_time_mode,
                                                      builder.id_handle_lookup(),
                                                      builder.id_data_cache());
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

static void PARSE_mesh_emitter(FNodeInfluencesBuilder &builder)
{
  Optional<NamedGenericTupleRef> inputs = builder.compute_all_data_inputs();
  if (!inputs.has_value()) {
    return;
  }

  ObjectIDHandle object_handle = inputs->relocate_out<ObjectIDHandle>(0, "Object");
  Object *object = builder.id_handle_lookup().lookup(object_handle);
  if (object == nullptr || object->type != OB_MESH) {
    return;
  }

  auto vertex_weights = compute_emitter_vertex_weights(builder.node_rna(), *inputs, object);

  VaryingFloat4x4 transform = builder.world_transition().update_float4x4(
      object->id.name, "obmat", object->obmat);

  ArrayRef<std::string> system_names = builder.find_target_system_names(0, "Emitter");
  ParticleAction &on_birth_action = builder.build_action_list("Execute on Birth", system_names);

  Emitter &emitter = builder.construct<SurfaceEmitter>(system_names,
                                                       on_birth_action,
                                                       object,
                                                       transform,
                                                       inputs->get<float>(1, "Rate"),
                                                       std::move(vertex_weights));
  builder.add_emitter(emitter);
}

static void PARSE_custom_force(FNodeInfluencesBuilder &builder)
{
  ParticleFunction *inputs_fn = builder.particle_function_for_all_inputs();
  if (inputs_fn == nullptr) {
    return;
  }

  ArrayRef<std::string> system_names = builder.find_target_system_names(0, "Force");
  CustomForce &force = builder.construct<CustomForce>(*inputs_fn);
  builder.add_force(system_names, force);
}

static void PARSE_age_reached_event(FNodeInfluencesBuilder &builder)
{
  ParticleFunction *inputs_fn = builder.particle_function_for_all_inputs();
  if (inputs_fn == nullptr) {
    return;
  }

  ArrayRef<std::string> system_names = builder.find_target_system_names(0, "Event");
  std::string is_triggered_attribute = builder.node_identifier();

  bool attribute_added = builder.try_add_attribute<bool>(
      system_names, is_triggered_attribute, false);
  if (!attribute_added) {
    return;
  }

  ParticleAction &action = builder.build_action_list("Execute on Event", system_names);
  Event &event = builder.construct<AgeReachedEvent>(is_triggered_attribute, *inputs_fn, action);
  builder.add_event(system_names, event);
}

static void PARSE_trails(FNodeInfluencesBuilder &builder)
{
  ArrayRef<std::string> main_system_names = builder.find_target_system_names(0, "Main System");
  ArrayRef<std::string> trail_system_names = builder.find_target_system_names(1, "Trail System");

  ParticleFunction *inputs_fn = builder.particle_function_for_all_inputs();
  if (inputs_fn == nullptr) {
    return;
  }
  if (main_system_names.intersects__linear_search(trail_system_names)) {
    return;
  }

  ParticleAction &action = builder.build_action_list("Execute on Birth", trail_system_names);
  OffsetHandler &offset_handler = builder.construct<CreateTrailHandler>(
      trail_system_names, *inputs_fn, action);
  builder.add_offset_handler(main_system_names, offset_handler);
}

static void PARSE_initial_grid_emitter(FNodeInfluencesBuilder &builder)
{
  Optional<NamedGenericTupleRef> inputs = builder.compute_all_data_inputs();
  if (!inputs.has_value()) {
    return;
  }

  ArrayRef<std::string> system_names = builder.find_target_system_names(0, "Emitter");
  ParticleAction &action = builder.build_action_list("Execute on Birth", system_names);

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

static void PARSE_mesh_collision(FNodeInfluencesBuilder &builder)
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
  ParticleAction &action = builder.build_action_list("Execute on Event", system_names);

  float4x4 local_to_world_end = object->obmat;
  float4x4 local_to_world_begin =
      builder.world_transition().update_float4x4(object->id.name, "obmat", object->obmat).start;

  std::string last_collision_attribute = builder.node_identifier();
  bool attribute_added = builder.try_add_attribute<int32_t>(
      system_names, last_collision_attribute, -1);
  if (!attribute_added) {
    return;
  }

  Event &event = builder.construct<MeshCollisionEvent>(
      last_collision_attribute, object, action, local_to_world_begin, local_to_world_end);
  builder.add_event(system_names, event);
}

static void PARSE_size_over_time(FNodeInfluencesBuilder &builder)
{
  ParticleFunction *inputs_fn = builder.particle_function_for_all_inputs();
  if (inputs_fn == nullptr) {
    return;
  }

  ArrayRef<std::string> system_names = builder.find_target_system_names(0, "Influence");
  OffsetHandler &offset_handler = builder.construct<SizeOverTimeHandler>(*inputs_fn);
  builder.add_offset_handler(system_names, offset_handler);
}

static void PARSE_custom_event(FNodeInfluencesBuilder &builder)
{
  ParticleFunction *inputs_fn = builder.particle_function_for_all_inputs();
  if (inputs_fn == nullptr) {
    return;
  }

  ArrayRef<std::string> system_names = builder.find_target_system_names(0, "Event");
  ParticleAction &action = builder.build_action_list("Execute on Event", system_names);

  Event &event = builder.construct<CustomEvent>(*inputs_fn, action);
  builder.add_event(system_names, event);
}

static void PARSE_always_execute(FNodeInfluencesBuilder &builder)
{
  ArrayRef<std::string> system_names = builder.find_target_system_names(0, "Influence");
  ParticleAction &action = builder.build_action_list("Execute", system_names);

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

static void collect_influences(FunctionTreeData &function_tree_data,
                               WorldTransition &world_transition,
                               Vector<std::string> &r_system_names,
                               InfluencesCollector &collector,
                               StringMap<Integrator *> &r_integrators)
{
  SCOPED_TIMER(__func__);

  StringMap<ParseNodeCallback> &parsers = get_node_parsers();

  for (const FNode *fnode :
       function_tree_data.function_tree().nodes_with_idname(particle_system_idname)) {
    StringRef name = fnode->name();
    r_system_names.append(name);

    AttributesInfoBuilder *attributes = new AttributesInfoBuilder();
    attributes->add<bool>("Dead", false);
    attributes->add<int32_t>("ID", 0);
    attributes->add<float>("Birth Time", 0);
    attributes->add<float3>("Position", float3(0, 0, 0));
    attributes->add<float3>("Velocity", float3(0, 0, 0));
    attributes->add<float>("Size", 0.05f);
    attributes->add<rgba_f>("Color", rgba_f(1, 1, 1, 1));
    attributes->add<BKE::SurfaceHook>("Emit Hook", {});

    collector.m_attributes.add_new(name, attributes);
  }

  for (const FNode *fnode : function_tree_data.function_tree().all_nodes()) {
    StringRef idname = fnode->idname();
    ParseNodeCallback *callback = parsers.lookup_ptr(idname);
    if (callback != nullptr) {
      FNodeInfluencesBuilder builder{collector, function_tree_data, world_transition, *fnode};
      (*callback)(builder);
    }
  }

  for (std::string &system_name : r_system_names) {
    ArrayRef<Force *> forces = collector.m_forces.lookup_default(system_name);
    EulerIntegrator &integrator = function_tree_data.construct<EulerIntegrator>("integrator",
                                                                                forces);

    r_integrators.add_new(system_name, &integrator);
  }
}

class NodeTreeStepSimulator : public StepSimulator {
 private:
  FN::BTreeVTreeMap m_function_trees;
  FunctionNodeTree m_function_tree;

 public:
  NodeTreeStepSimulator(bNodeTree *btree) : m_function_tree(btree, m_function_trees)
  {
    // m_function_tree.to_dot__clipboard();
  }

  void simulate(SimulationState &simulation_state) override
  {
    WorldState &old_world_state = simulation_state.world();
    WorldState new_world_state;
    WorldTransition world_transition = {old_world_state, new_world_state};

    ParticlesState &particles_state = simulation_state.particles();

    ResourceCollector resources;
    std::unique_ptr<FunctionTreeMFNetwork> data_graph =
        FN::MFGeneration::generate_node_tree_multi_function_network(m_function_tree, resources);
    if (data_graph.get() == nullptr) {
      return;
    }
    FunctionTreeData function_tree_data(*data_graph);

    Vector<std::string> system_names;
    StringMap<Integrator *> integrators;
    InfluencesCollector influences_collector;
    collect_influences(
        function_tree_data, world_transition, system_names, influences_collector, integrators);

    auto &containers = particles_state.particle_containers();

    StringMap<ParticleSystemInfo> systems_to_simulate;
    for (std::string name : system_names) {
      AttributesInfoBuilder &system_attributes = *influences_collector.m_attributes.lookup(name);

      /* Keep old attributes. */
      ParticleSet *particles = containers.lookup_default(name, nullptr);
      if (particles != nullptr) {
        system_attributes.add(particles->attributes_info());
      }

      this->ensure_particle_container_exist_and_has_attributes(
          particles_state, name, system_attributes);

      ParticleSystemInfo type_info = {
          integrators.lookup(name),
          influences_collector.m_events.lookup_default(name),
          influences_collector.m_offset_handlers.lookup_default(name),
      };
      systems_to_simulate.add_new(name, type_info);
    }

    simulate_particles(simulation_state, influences_collector.m_emitters, systems_to_simulate);

    influences_collector.m_attributes.foreach_value(
        [](AttributesInfoBuilder *builder) { delete builder; });

    simulation_state.world() = std::move(new_world_state);
  }

 private:
  void ensure_particle_container_exist_and_has_attributes(
      ParticlesState &particles_state,
      StringRef name,
      const AttributesInfoBuilder &attributes_info_builder)
  {
    auto &containers = particles_state.particle_containers();
    ParticleSet *particles = containers.lookup_default(name, nullptr);
    AttributesInfo *attributes_info = new AttributesInfo(attributes_info_builder);
    if (particles == nullptr) {
      ParticleSet *new_particle_set = new ParticleSet(*attributes_info, true);
      containers.add_new(name, new_particle_set);
    }
    else {
      particles->update_attributes(attributes_info);
    }
  }
};

std::unique_ptr<StepSimulator> simulator_from_node_tree(bNodeTree *btree)
{
  return std::unique_ptr<StepSimulator>(new NodeTreeStepSimulator(btree));
}

}  // namespace BParticles
