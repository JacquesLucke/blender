#include "DNA_modifier_types.h"

#include "BKE_virtual_node_tree_cxx.h"
#include "BKE_multi_functions.h"
#include "BKE_tuple.h"
#include "BKE_multi_function_network.h"

#include "BLI_math_cxx.h"
#include "BLI_string_map.h"
#include "BLI_owned_resources.h"
#include "BLI_stack_cxx.h"

#include "DEG_depsgraph_query.h"

using BKE::CPPType;
using BKE::GenericArrayRef;
using BKE::GenericMutableArrayRef;
using BKE::GenericVectorArray;
using BKE::GenericVirtualListListRef;
using BKE::GenericVirtualListRef;
using BKE::MFBuilderDummyNode;
using BKE::MFBuilderFunctionNode;
using BKE::MFBuilderInputSocket;
using BKE::MFBuilderNode;
using BKE::MFBuilderOutputSocket;
using BKE::MFBuilderSocket;
using BKE::MFContext;
using BKE::MFDataType;
using BKE::MFDummyNode;
using BKE::MFFunctionNode;
using BKE::MFInputSocket;
using BKE::MFMask;
using BKE::MFNetwork;
using BKE::MFNetworkBuilder;
using BKE::MFNode;
using BKE::MFOutputSocket;
using BKE::MFParams;
using BKE::MFParamsBuilder;
using BKE::MFParamType;
using BKE::MFSignature;
using BKE::MFSignatureBuilder;
using BKE::MFSocket;
using BKE::MultiFunction;
using BKE::TupleRef;
using BKE::VirtualLink;
using BKE::VirtualNode;
using BKE::VirtualNodeTree;
using BKE::VirtualSocket;
using BLI::Array;
using BLI::ArrayRef;
using BLI::float3;
using BLI::IndexRange;
using BLI::Map;
using BLI::OwnedResources;
using BLI::Stack;
using BLI::StringMap;
using BLI::StringRef;
using BLI::TemporaryVector;
using BLI::Vector;

extern "C" {
void MOD_functiondeform_do(FunctionDeformModifierData *fdmd, float (*vertexCos)[3], int numVerts);
}

static MFDataType get_type_by_socket(const VirtualSocket &vsocket)
{
  StringRef idname = vsocket.idname();

  if (idname == "fn_FloatSocket") {
    return MFDataType::ForSingle<float>();
  }
  else if (idname == "fn_VectorSocket") {
    return MFDataType::ForSingle<float3>();
  }
  else if (idname == "fn_IntegerSocket") {
    return MFDataType::ForSingle<int32_t>();
  }
  else if (idname == "fn_BooleanSocket") {
    return MFDataType::ForSingle<bool>();
  }
  else if (idname == "fn_ObjectSocket") {
    return MFDataType::ForSingle<Object *>();
  }
  else if (idname == "fn_TextSocket") {
    return MFDataType::ForSingle<std::string>();
  }
  else if (idname == "fn_FloatListSocket") {
    return MFDataType::ForVector<float>();
  }
  else if (idname == "fn_VectorListSocket") {
    return MFDataType::ForVector<float3>();
  }
  else if (idname == "fn_IntegerListSocket") {
    return MFDataType::ForVector<int32_t>();
  }
  else if (idname == "fn_BooleanListSocket") {
    return MFDataType::ForVector<bool>();
  }
  else if (idname == "fn_ObjectListSocket") {
    return MFDataType::ForVector<Object *>();
  }
  else if (idname == "fn_TextListSocket") {
    return MFDataType::ForVector<std::string>();
  }

  return MFDataType();
}

static const CPPType &get_cpp_type_by_name(StringRef name)
{
  if (name == "Float") {
    return BKE::GET_TYPE<float>();
  }
  else if (name == "Vector") {
    return BKE::GET_TYPE<float3>();
  }
  else if (name == "Integer") {
    return BKE::GET_TYPE<int32_t>();
  }
  else if (name == "Boolean") {
    return BKE::GET_TYPE<bool>();
  }
  else if (name == "Object") {
    return BKE::GET_TYPE<Object *>();
  }
  else if (name == "Text") {
    return BKE::GET_TYPE<std::string>();
  }

  BLI_assert(false);
  return BKE::GET_TYPE<float>();
}

class VTreeMFNetwork {
 private:
  const VirtualNodeTree &m_vtree;
  std::unique_ptr<MFNetwork> m_network;
  Array<const MFSocket *> m_socket_map;

 public:
  VTreeMFNetwork(const VirtualNodeTree &vtree,
                 std::unique_ptr<MFNetwork> network,
                 Array<const MFSocket *> socket_map)
      : m_vtree(vtree), m_network(std::move(network)), m_socket_map(std::move(socket_map))
  {
  }

  const VirtualNodeTree &vtree()
  {
    return m_vtree;
  }

  const MFNetwork &network()
  {
    return *m_network;
  }

  const MFSocket &lookup_socket(const VirtualSocket &vsocket)
  {
    return *m_socket_map[vsocket.id()];
  }
};

class VTreeMFNetworkBuilder {
 private:
  const VirtualNodeTree &m_vtree;
  Vector<MFBuilderSocket *> m_socket_map;
  Vector<MFDataType> m_type_by_vsocket;
  std::unique_ptr<MFNetworkBuilder> m_builder;

 public:
  VTreeMFNetworkBuilder(const VirtualNodeTree &vtree)
      : m_vtree(vtree),
        m_socket_map(vtree.socket_count(), nullptr),
        m_builder(BLI::make_unique<MFNetworkBuilder>())
  {
    m_type_by_vsocket = Vector<MFDataType>(m_vtree.socket_count());
    for (const VirtualNode *vnode : m_vtree.nodes()) {
      for (const VirtualSocket *vsocket : vnode->inputs()) {
        MFDataType data_type = get_type_by_socket(*vsocket);
        m_type_by_vsocket[vsocket->id()] = data_type;
      }
      for (const VirtualSocket *vsocket : vnode->outputs()) {
        MFDataType data_type = get_type_by_socket(*vsocket);
        m_type_by_vsocket[vsocket->id()] = data_type;
      }
    }
  }

  const VirtualNodeTree &vtree() const
  {
    return m_vtree;
  }

  MFBuilderFunctionNode &add_function(const MultiFunction &function,
                                      ArrayRef<uint> input_param_indices,
                                      ArrayRef<uint> output_param_indices)
  {
    return m_builder->add_function(function, input_param_indices, output_param_indices);
  }

  MFBuilderFunctionNode &add_function(const MultiFunction &function,
                                      ArrayRef<uint> input_param_indices,
                                      ArrayRef<uint> output_param_indices,
                                      const VirtualNode &vnode)
  {
    MFBuilderFunctionNode &node = m_builder->add_function(
        function, input_param_indices, output_param_indices);
    this->map_sockets_exactly(vnode, node);
    return node;
  }

  MFBuilderDummyNode &add_dummy(const VirtualNode &vnode)
  {
    Vector<MFDataType> input_types;
    for (const VirtualSocket *vsocket : vnode.inputs()) {
      MFDataType data_type = this->try_get_data_type(*vsocket);
      if (!data_type.is_none()) {
        input_types.append(data_type);
      }
    }

    Vector<MFDataType> output_types;
    for (const VirtualSocket *vsocket : vnode.outputs()) {
      MFDataType data_type = this->try_get_data_type(*vsocket);
      if (!data_type.is_none()) {
        output_types.append(data_type);
      }
    }

    MFBuilderDummyNode &node = m_builder->add_dummy(input_types, output_types);
    this->map_data_sockets(vnode, node);
    return node;
  }

  MFBuilderDummyNode &add_dummy(ArrayRef<MFDataType> input_types,
                                ArrayRef<MFDataType> output_types)
  {
    return m_builder->add_dummy(input_types, output_types);
  }

  void add_link(MFBuilderOutputSocket &from, MFBuilderInputSocket &to)
  {
    m_builder->add_link(from, to);
  }

  MFDataType try_get_data_type(const VirtualSocket &vsocket) const
  {
    return m_type_by_vsocket[vsocket.id()];
  }

  bool is_data_socket(const VirtualSocket &vsocket) const
  {
    return !m_type_by_vsocket[vsocket.id()].is_none();
  }

  void map_sockets_exactly(const VirtualNode &vnode, MFBuilderNode &node)
  {
    BLI_assert(vnode.inputs().size() == node.inputs().size());
    BLI_assert(vnode.outputs().size() == node.outputs().size());

    for (uint i = 0; i < vnode.inputs().size(); i++) {
      m_socket_map[vnode.inputs()[i]->id()] = node.inputs()[i];
    }
    for (uint i = 0; i < vnode.outputs().size(); i++) {
      m_socket_map[vnode.outputs()[i]->id()] = node.outputs()[i];
    }
  }

  void map_data_sockets(const VirtualNode &vnode, MFBuilderNode &node)
  {
    uint data_inputs = 0;
    for (const VirtualSocket *vsocket : vnode.inputs()) {
      if (this->is_data_socket(*vsocket)) {
        this->map_sockets(*vsocket, *node.inputs()[data_inputs]);
        data_inputs++;
      }
    }

    uint data_outputs = 0;
    for (const VirtualSocket *vsocket : vnode.outputs()) {
      if (this->is_data_socket(*vsocket)) {
        this->map_sockets(*vsocket, *node.outputs()[data_outputs]);
        data_outputs++;
      }
    }
  }

  void map_sockets(const VirtualSocket &vsocket, MFBuilderSocket &socket)
  {
    BLI_assert(m_socket_map[vsocket.id()] == nullptr);
    m_socket_map[vsocket.id()] = &socket;
  }

  bool vsocket_is_mapped(const VirtualSocket &vsocket) const
  {
    return m_socket_map[vsocket.id()] != nullptr;
  }

  bool data_sockets_are_mapped(ArrayRef<const VirtualSocket *> vsockets) const
  {
    for (const VirtualSocket *vsocket : vsockets) {
      if (this->is_data_socket(*vsocket)) {
        if (!this->vsocket_is_mapped(*vsocket)) {
          return false;
        }
      }
    }
    return true;
  }

  bool data_sockets_of_vnode_are_mapped(const VirtualNode &vnode) const
  {
    if (!this->data_sockets_are_mapped(vnode.inputs())) {
      return false;
    }
    if (!this->data_sockets_are_mapped(vnode.outputs())) {
      return false;
    }
    return true;
  }

  bool has_data_sockets(const VirtualNode &vnode) const
  {
    for (const VirtualSocket *vsocket : vnode.inputs()) {
      if (this->is_data_socket(*vsocket)) {
        return true;
      }
    }
    for (const VirtualSocket *vsocket : vnode.outputs()) {
      if (this->is_data_socket(*vsocket)) {
        return true;
      }
    }
    return false;
  }

  bool is_input_linked(const VirtualSocket &vsocket) const
  {
    auto &socket = this->lookup_input_socket(vsocket);
    return socket.as_input().origin() != nullptr;
  }

  MFBuilderOutputSocket &lookup_output_socket(const VirtualSocket &vsocket) const
  {
    BLI_assert(vsocket.is_output());
    MFBuilderSocket *socket = m_socket_map[vsocket.id()];
    BLI_assert(socket != nullptr);
    return socket->as_output();
  }

  MFBuilderInputSocket &lookup_input_socket(const VirtualSocket &vsocket) const
  {
    BLI_assert(vsocket.is_input());
    MFBuilderSocket *socket = m_socket_map[vsocket.id()];
    BLI_assert(socket != nullptr);
    return socket->as_input();
  }

  std::unique_ptr<VTreeMFNetwork> build()
  {
    // m_builder->to_dot__clipboard();

    Array<int> socket_ids(m_vtree.socket_count(), -1);
    for (uint vsocket_id = 0; vsocket_id < m_vtree.socket_count(); vsocket_id++) {
      MFBuilderSocket *builder_socket = m_socket_map[vsocket_id];
      if (builder_socket != nullptr) {
        socket_ids[vsocket_id] = builder_socket->id();
      }
    }

    auto network = BLI::make_unique<MFNetwork>(std::move(m_builder));

    Array<const MFSocket *> socket_map(m_vtree.socket_count(), nullptr);
    for (uint vsocket_id = 0; vsocket_id < m_vtree.socket_count(); vsocket_id++) {
      int id = socket_ids[vsocket_id];
      if (id != -1) {
        socket_map[vsocket_id] = &network->socket_by_id(socket_ids[vsocket_id]);
      }
    }

    return BLI::make_unique<VTreeMFNetwork>(m_vtree, std::move(network), std::move(socket_map));
  }
};

using InsertVNodeFunction = std::function<void(
    VTreeMFNetworkBuilder &builder, OwnedResources &resources, const VirtualNode &vnode)>;
using InsertUnlinkedInputFunction = std::function<MFBuilderOutputSocket &(
    VTreeMFNetworkBuilder &builder, OwnedResources &resources, const VirtualSocket &vsocket)>;
using InsertImplicitConversionFunction =
    std::function<std::pair<MFBuilderInputSocket *, MFBuilderOutputSocket *>(
        VTreeMFNetworkBuilder &builder, OwnedResources &resources)>;

template<typename T, typename... Args>
T &allocate_resource(const char *name, OwnedResources &resources, Args &&... args)
{
  std::unique_ptr<T> value = BLI::make_unique<T>(std::forward<Args>(args)...);
  T &value_ref = *value;
  resources.add(std::move(value), name);
  return value_ref;
}

static void INSERT_vector_math(VTreeMFNetworkBuilder &builder,
                               OwnedResources &resources,
                               const VirtualNode &vnode)
{
  const MultiFunction &fn = allocate_resource<BKE::MultiFunction_AddFloat3s>(
      "vector math function", resources);
  builder.add_function(fn, {0, 1}, {2}, vnode);
}

static const MultiFunction &get_vectorized_function(
    const MultiFunction &base_function,
    OwnedResources &resources,
    PointerRNA rna,
    ArrayRef<const char *> is_vectorized_prop_names)
{
  Vector<bool> input_is_vectorized;
  for (const char *prop_name : is_vectorized_prop_names) {
    char state[5];
    RNA_string_get(&rna, prop_name, state);
    BLI_assert(STREQ(state, "BASE") || STREQ(state, "LIST"));

    bool is_vectorized = STREQ(state, "LIST");
    input_is_vectorized.append(is_vectorized);
  }

  if (input_is_vectorized.contains(true)) {
    return allocate_resource<BKE::MultiFunction_SimpleVectorize>(
        "vectorized function", resources, base_function, input_is_vectorized);
  }
  else {
    return base_function;
  }
}

static void INSERT_float_math(VTreeMFNetworkBuilder &builder,
                              OwnedResources &resources,
                              const VirtualNode &vnode)
{
  const MultiFunction &base_fn = allocate_resource<BKE::MultiFunction_AddFloats>(
      "float math function", resources);
  const MultiFunction &fn = get_vectorized_function(
      base_fn, resources, vnode.rna(), {"use_list__a", "use_list__b"});

  builder.add_function(fn, {0, 1}, {2}, vnode);
}

static void INSERT_combine_vector(VTreeMFNetworkBuilder &builder,
                                  OwnedResources &resources,
                                  const VirtualNode &vnode)
{
  const MultiFunction &base_fn = allocate_resource<BKE::MultiFunction_CombineVector>(
      "combine vector function", resources);
  const MultiFunction &fn = get_vectorized_function(
      base_fn, resources, vnode.rna(), {"use_list__x", "use_list__y", "use_list__z"});
  builder.add_function(fn, {0, 1, 2}, {3}, vnode);
}

static void INSERT_separate_vector(VTreeMFNetworkBuilder &builder,
                                   OwnedResources &resources,
                                   const VirtualNode &vnode)
{
  const MultiFunction &base_fn = allocate_resource<BKE::MultiFunction_SeparateVector>(
      "separate vector function", resources);
  const MultiFunction &fn = get_vectorized_function(
      base_fn, resources, vnode.rna(), {"use_list__vector"});
  builder.add_function(fn, {0}, {1, 2, 3}, vnode);
}

static void INSERT_list_length(VTreeMFNetworkBuilder &builder,
                               OwnedResources &resources,
                               const VirtualNode &vnode)
{
  PointerRNA rna = vnode.rna();
  char *type_name = RNA_string_get_alloc(&rna, "active_type", nullptr, 0);
  const CPPType &type = get_cpp_type_by_name(type_name);
  MEM_freeN(type_name);

  const MultiFunction &fn = allocate_resource<BKE::MultiFunction_ListLength>(
      "list length function", resources, type);
  builder.add_function(fn, {0}, {1}, vnode);
}

static void INSERT_get_list_element(VTreeMFNetworkBuilder &builder,
                                    OwnedResources &resources,
                                    const VirtualNode &vnode)
{
  PointerRNA rna = vnode.rna();
  char *type_name = RNA_string_get_alloc(&rna, "active_type", nullptr, 0);
  const CPPType &type = get_cpp_type_by_name(type_name);
  MEM_freeN(type_name);

  const MultiFunction &fn = allocate_resource<BKE::MultiFunction_GetListElement>(
      "get list element function", resources, type);
  builder.add_function(fn, {0, 1, 2}, {3}, vnode);
}

static MFBuilderOutputSocket &build_pack_list_node(VTreeMFNetworkBuilder &builder,
                                                   OwnedResources &resources,
                                                   const VirtualNode &vnode,
                                                   const CPPType &base_type,
                                                   const char *prop_name,
                                                   uint start_index)
{
  PointerRNA rna = vnode.rna();

  Vector<bool> input_is_list;
  RNA_BEGIN (&rna, itemptr, prop_name) {
    int state = RNA_enum_get(&itemptr, "state");
    if (state == 0) {
      /* single value case */
      input_is_list.append(false);
    }
    else if (state == 1) {
      /* list case */
      input_is_list.append(true);
    }
    else {
      BLI_assert(false);
    }
  }
  RNA_END;

  uint input_amount = input_is_list.size();
  uint output_param_index = (input_amount > 0 && input_is_list[0]) ? 0 : input_amount;

  const MultiFunction &fn = allocate_resource<BKE::MultiFunction_PackList>(
      "pack list function", resources, base_type, input_is_list);
  MFBuilderFunctionNode &node = builder.add_function(
      fn, IndexRange(input_amount).as_array_ref(), {output_param_index});

  for (uint i = 0; i < input_amount; i++) {
    builder.map_sockets(vnode.input(start_index + i), *node.inputs()[i]);
  }

  return *node.outputs()[0];
}

static void INSERT_pack_list(VTreeMFNetworkBuilder &builder,
                             OwnedResources &resources,
                             const VirtualNode &vnode)
{
  PointerRNA rna = vnode.rna();
  char *type_name = RNA_string_get_alloc(&rna, "active_type", nullptr, 0);
  const CPPType &type = get_cpp_type_by_name(type_name);
  MEM_freeN(type_name);

  MFBuilderOutputSocket &packed_list_socket = build_pack_list_node(
      builder, resources, vnode, type, "variadic", 0);
  builder.map_sockets(vnode.output(0), packed_list_socket);
}

static void INSERT_object_location(VTreeMFNetworkBuilder &builder,
                                   OwnedResources &resources,
                                   const VirtualNode &vnode)
{
  const MultiFunction &fn = allocate_resource<BKE::MultiFunction_ObjectWorldLocation>(
      "object location function", resources);
  builder.add_function(fn, {0}, {1}, vnode);
}

static void INSERT_text_length(VTreeMFNetworkBuilder &builder,
                               OwnedResources &resources,
                               const VirtualNode &vnode)
{
  const MultiFunction &fn = allocate_resource<BKE::MultiFunction_TextLength>(
      "text length function", resources);
  builder.add_function(fn, {0}, {1}, vnode);
}

static StringMap<InsertVNodeFunction> get_node_inserters()
{
  StringMap<InsertVNodeFunction> inserters;
  inserters.add_new("fn_FloatMathNode", INSERT_float_math);
  inserters.add_new("fn_VectorMathNode", INSERT_vector_math);
  inserters.add_new("fn_CombineVectorNode", INSERT_combine_vector);
  inserters.add_new("fn_SeparateVectorNode", INSERT_separate_vector);
  inserters.add_new("fn_ListLengthNode", INSERT_list_length);
  inserters.add_new("fn_PackListNode", INSERT_pack_list);
  inserters.add_new("fn_GetListElementNode", INSERT_get_list_element);
  inserters.add_new("fn_ObjectTransformsNode", INSERT_object_location);
  inserters.add_new("fn_TextLengthNode", INSERT_text_length);
  return inserters;
}

static MFBuilderOutputSocket &INSERT_vector_socket(VTreeMFNetworkBuilder &builder,
                                                   OwnedResources &resources,
                                                   const VirtualSocket &vsocket)
{
  PointerRNA rna = vsocket.rna();
  float3 value;
  RNA_float_get_array(&rna, "value", value);

  const MultiFunction &fn = allocate_resource<BKE::MultiFunction_ConstantValue<float3>>(
      "vector socket", resources, value);
  MFBuilderFunctionNode &node = builder.add_function(fn, {}, {0});
  return *node.outputs()[0];
}

static MFBuilderOutputSocket &INSERT_float_socket(VTreeMFNetworkBuilder &builder,
                                                  OwnedResources &resources,
                                                  const VirtualSocket &vsocket)
{
  PointerRNA rna = vsocket.rna();
  float value = RNA_float_get(&rna, "value");

  const MultiFunction &fn = allocate_resource<BKE::MultiFunction_ConstantValue<float>>(
      "float socket", resources, value);
  MFBuilderFunctionNode &node = builder.add_function(fn, {}, {0});
  return *node.outputs()[0];
}

static MFBuilderOutputSocket &INSERT_int_socket(VTreeMFNetworkBuilder &builder,
                                                OwnedResources &resources,
                                                const VirtualSocket &vsocket)
{
  PointerRNA rna = vsocket.rna();
  int value = RNA_int_get(&rna, "value");

  const MultiFunction &fn = allocate_resource<BKE::MultiFunction_ConstantValue<int>>(
      "int socket", resources, value);
  MFBuilderFunctionNode &node = builder.add_function(fn, {}, {0});
  return *node.outputs()[0];
}

static MFBuilderOutputSocket &INSERT_object_socket(VTreeMFNetworkBuilder &builder,
                                                   OwnedResources &resources,
                                                   const VirtualSocket &vsocket)
{
  PointerRNA rna = vsocket.rna();
  Object *value = (Object *)RNA_pointer_get(&rna, "value").data;

  const MultiFunction &fn = allocate_resource<BKE::MultiFunction_ConstantValue<Object *>>(
      "object socket", resources, value);
  MFBuilderFunctionNode &node = builder.add_function(fn, {}, {0});
  return *node.outputs()[0];
}

static MFBuilderOutputSocket &INSERT_text_socket(VTreeMFNetworkBuilder &builder,
                                                 OwnedResources &resources,
                                                 const VirtualSocket &vsocket)
{
  PointerRNA rna = vsocket.rna();
  char *value = RNA_string_get_alloc(&rna, "value", nullptr, 0);
  std::string text = value;
  MEM_freeN(value);

  const MultiFunction &fn = allocate_resource<BKE::MultiFunction_ConstantValue<std::string>>(
      "text socket", resources, text);
  MFBuilderFunctionNode &node = builder.add_function(fn, {}, {0});
  return *node.outputs()[0];
}

template<typename T>
static MFBuilderOutputSocket &INSERT_empty_list_socket(VTreeMFNetworkBuilder &builder,
                                                       OwnedResources &resources,
                                                       const VirtualSocket &UNUSED(vsocket))
{
  const MultiFunction &fn = allocate_resource<BKE::MultiFunction_EmptyList<T>>("empty list socket",
                                                                               resources);
  MFBuilderFunctionNode &node = builder.add_function(fn, {}, {0});
  return *node.outputs()[0];
}

static StringMap<InsertUnlinkedInputFunction> get_unlinked_input_inserter()
{
  StringMap<InsertUnlinkedInputFunction> inserters;
  inserters.add_new("fn_VectorSocket", INSERT_vector_socket);
  inserters.add_new("fn_FloatSocket", INSERT_float_socket);
  inserters.add_new("fn_IntegerSocket", INSERT_int_socket);
  inserters.add_new("fn_ObjectSocket", INSERT_object_socket);
  inserters.add_new("fn_TextSocket", INSERT_text_socket);
  inserters.add_new("fn_VectorListSocket", INSERT_empty_list_socket<float3>);
  inserters.add_new("fn_FloatListSocket", INSERT_empty_list_socket<float>);
  inserters.add_new("fn_IntegerListSocket", INSERT_empty_list_socket<int32_t>);
  inserters.add_new("fn_ObjectListSocket", INSERT_empty_list_socket<Object *>);
  inserters.add_new("fn_TextListSocket", INSERT_empty_list_socket<std::string>);
  return inserters;
}

template<typename FromT, typename ToT>
static std::pair<MFBuilderInputSocket *, MFBuilderOutputSocket *> INSERT_convert(
    VTreeMFNetworkBuilder &builder, OwnedResources &resources)
{
  const MultiFunction &fn = allocate_resource<BKE::MultiFunction_Convert<FromT, ToT>>(
      "converter function", resources);
  MFBuilderFunctionNode &node = builder.add_function(fn, {0}, {1});
  return {node.inputs()[0], node.outputs()[0]};
}

template<typename FromT, typename ToT>
static std::pair<MFBuilderInputSocket *, MFBuilderOutputSocket *> INSERT_convert_list(
    VTreeMFNetworkBuilder &builder, OwnedResources &resources)
{
  const MultiFunction &fn = allocate_resource<BKE::MultiFunction_ConvertList<FromT, ToT>>(
      "convert list function", resources);
  MFBuilderFunctionNode &node = builder.add_function(fn, {0}, {1});
  return {node.inputs()[0], node.outputs()[0]};
}

template<typename T>
static std::pair<MFBuilderInputSocket *, MFBuilderOutputSocket *> INSERT_element_to_list(
    VTreeMFNetworkBuilder &builder, OwnedResources &resources)
{
  const MultiFunction &fn = allocate_resource<BKE::MultiFunction_SingleElementList<T>>(
      "single element list function", resources);
  MFBuilderFunctionNode &node = builder.add_function(fn, {0}, {1});
  return {node.inputs()[0], node.outputs()[0]};
}

static Map<std::pair<std::string, std::string>, InsertImplicitConversionFunction>
get_conversion_inserters()
{
  Map<std::pair<std::string, std::string>, InsertImplicitConversionFunction> inserters;

  inserters.add_new({"fn_IntegerSocket", "fn_FloatSocket"}, INSERT_convert<int, float>);
  inserters.add_new({"fn_FloatSocket", "fn_IntegerSocket"}, INSERT_convert<float, int>);

  inserters.add_new({"fn_FloatSocket", "fn_BooleanSocket"}, INSERT_convert<float, bool>);
  inserters.add_new({"fn_BooleanSocket", "fn_FloatSocket"}, INSERT_convert<bool, float>);

  inserters.add_new({"fn_IntegerSocket", "fn_BooleanSocket"}, INSERT_convert<int, bool>);
  inserters.add_new({"fn_BooleanSocket", "fn_IntegerSocket"}, INSERT_convert<bool, int>);

  inserters.add_new({"fn_IntegerListSocket", "fn_FloatListSocket"},
                    INSERT_convert_list<int, float>);
  inserters.add_new({"fn_FloatListSocket", "fn_IntegerListSocket"},
                    INSERT_convert_list<float, int>);

  inserters.add_new({"fn_FloatListSocket", "fn_BooleanListSocket"},
                    INSERT_convert_list<float, bool>);
  inserters.add_new({"fn_BooleanListSocket", "fn_FloatListSocket"},
                    INSERT_convert_list<bool, float>);

  inserters.add_new({"fn_IntegerListSocket", "fn_BooleanListSocket"},
                    INSERT_convert_list<int, bool>);
  inserters.add_new({"fn_BooleanListSocket", "fn_IntegerListSocket"},
                    INSERT_convert_list<bool, int>);

  inserters.add_new({"fn_IntegerSocket", "fn_IntegerListSocket"}, INSERT_element_to_list<int>);
  inserters.add_new({"fn_FloatSocket", "fn_FloatListSocket"}, INSERT_element_to_list<float>);
  inserters.add_new({"fn_BooleanSocket", "fn_BooleanListSocket"}, INSERT_element_to_list<bool>);

  return inserters;
}

static bool insert_nodes(VTreeMFNetworkBuilder &builder, OwnedResources &resources)
{
  const VirtualNodeTree &vtree = builder.vtree();
  auto inserters = get_node_inserters();

  for (const VirtualNode *vnode : vtree.nodes()) {
    StringRef idname = vnode->idname();
    InsertVNodeFunction *inserter = inserters.lookup_ptr(idname);

    if (inserter != nullptr) {
      (*inserter)(builder, resources, *vnode);
      BLI_assert(builder.data_sockets_of_vnode_are_mapped(*vnode));
    }
    else if (builder.has_data_sockets(*vnode)) {
      builder.add_dummy(*vnode);
    }
  }

  return true;
}

static bool insert_links(VTreeMFNetworkBuilder &builder, OwnedResources &resources)
{
  auto conversion_inserters = get_conversion_inserters();

  for (const VirtualSocket *to_vsocket : builder.vtree().inputs_with_links()) {
    if (to_vsocket->links().size() > 1) {
      continue;
    }
    BLI_assert(to_vsocket->links().size() == 1);

    if (!builder.is_data_socket(*to_vsocket)) {
      continue;
    }

    const VirtualSocket *from_vsocket = to_vsocket->links()[0];
    if (!builder.is_data_socket(*from_vsocket)) {
      return false;
    }

    auto &from_socket = builder.lookup_output_socket(*from_vsocket);
    auto &to_socket = builder.lookup_input_socket(*to_vsocket);

    if (from_socket.type() == to_socket.type()) {
      builder.add_link(from_socket, to_socket);
    }
    else {
      InsertImplicitConversionFunction *inserter = conversion_inserters.lookup_ptr(
          {from_vsocket->idname(), to_vsocket->idname()});
      if (inserter == nullptr) {
        return false;
      }
      auto new_sockets = (*inserter)(builder, resources);
      builder.add_link(from_socket, *new_sockets.first);
      builder.add_link(*new_sockets.second, to_socket);
    }
  }

  return true;
}

static bool insert_unlinked_inputs(VTreeMFNetworkBuilder &builder, OwnedResources &resources)
{
  Vector<const VirtualSocket *> unlinked_data_inputs;
  for (const VirtualNode *vnode : builder.vtree().nodes()) {
    for (const VirtualSocket *vsocket : vnode->inputs()) {
      if (builder.is_data_socket(*vsocket)) {
        if (!builder.is_input_linked(*vsocket)) {
          unlinked_data_inputs.append(vsocket);
        }
      }
    }
  }

  auto inserters = get_unlinked_input_inserter();

  for (const VirtualSocket *vsocket : unlinked_data_inputs) {
    InsertUnlinkedInputFunction *inserter = inserters.lookup_ptr(vsocket->idname());

    if (inserter == nullptr) {
      return false;
    }
    MFBuilderOutputSocket &from_socket = (*inserter)(builder, resources, *vsocket);
    MFBuilderInputSocket &to_socket = builder.lookup_input_socket(*vsocket);
    builder.add_link(from_socket, to_socket);
  }

  return true;
}

class MultiFunction_FunctionTree : public BKE::MultiFunction {
 private:
  Vector<const MFOutputSocket *> m_inputs;
  Vector<const MFInputSocket *> m_outputs;

 public:
  MultiFunction_FunctionTree(Vector<const MFOutputSocket *> inputs,
                             Vector<const MFInputSocket *> outputs)
      : m_inputs(std::move(inputs)), m_outputs(std::move(outputs))
  {
    MFSignatureBuilder signature("Function Tree");
    for (auto socket : m_inputs) {
      BLI_assert(socket->node().is_dummy());

      MFDataType type = socket->type();
      switch (type.category()) {
        case MFDataType::Single:
          signature.readonly_single_input("Input", type.type());
          break;
        case MFDataType::Vector:
          signature.readonly_vector_input("Input", type.base_type());
          break;
        case MFDataType::None:
          BLI_assert(false);
          break;
      }
    }
    for (auto socket : m_outputs) {
      BLI_assert(socket->node().is_dummy());

      MFDataType type = socket->type();
      switch (type.category()) {
        case MFDataType::Single:
          signature.single_output("Output", type.type());
          break;
        case MFDataType::Vector:
          signature.vector_output("Output", type.base_type());
          break;
        case MFDataType::None:
          BLI_assert(false);
          break;
      }
    }
    this->set_signature(signature);
  }

  class Storage {
   private:
    const MFMask &m_mask;
    Vector<GenericVectorArray *> m_vector_arrays;
    Vector<GenericMutableArrayRef> m_arrays;
    Map<uint, GenericVectorArray *> m_vector_per_socket;
    Map<uint, GenericVirtualListRef> m_virtual_list_for_inputs;
    Map<uint, GenericVirtualListListRef> m_virtual_list_list_for_inputs;

   public:
    Storage(const MFMask &mask) : m_mask(mask)
    {
    }

    ~Storage()
    {
      for (GenericVectorArray *vector_array : m_vector_arrays) {
        delete vector_array;
      }
      for (GenericMutableArrayRef array : m_arrays) {
        array.destruct_indices(m_mask.indices());
        MEM_freeN(array.buffer());
      }
    }

    void take_array_ref_ownership(GenericMutableArrayRef array)
    {
      m_arrays.append(array);
    }

    void take_vector_array_ownership(GenericVectorArray *vector_array)
    {
      m_vector_arrays.append(vector_array);
    }

    void take_vector_array_ownership__not_twice(GenericVectorArray *vector_array)
    {
      if (!m_vector_arrays.contains(vector_array)) {
        m_vector_arrays.append(vector_array);
      }
    }

    void set_virtual_list_for_input__non_owning(const MFInputSocket &socket,
                                                GenericVirtualListRef list)
    {
      m_virtual_list_for_inputs.add_new(socket.id(), list);
    }

    void set_virtual_list_list_for_input__non_owning(const MFInputSocket &socket,
                                                     GenericVirtualListListRef list)
    {
      m_virtual_list_list_for_inputs.add_new(socket.id(), list);
    }

    void set_vector_array_for_input__non_owning(const MFInputSocket &socket,
                                                GenericVectorArray *vector_array)
    {
      m_vector_per_socket.add_new(socket.id(), vector_array);
    }

    GenericVirtualListRef get_virtual_list_for_input(const MFInputSocket &socket) const
    {
      return m_virtual_list_for_inputs.lookup(socket.id());
    }

    GenericVirtualListListRef get_virtual_list_list_for_input(const MFInputSocket &socket) const
    {
      return m_virtual_list_list_for_inputs.lookup(socket.id());
    }

    GenericVectorArray &get_vector_array_for_input(const MFInputSocket &socket) const
    {
      return *m_vector_per_socket.lookup(socket.id());
    }

    bool input_is_computed(const MFInputSocket &socket) const
    {
      switch (socket.type().category()) {
        case MFDataType::Single:
          return m_virtual_list_for_inputs.contains(socket.id());
        case MFDataType::Vector:
          return m_virtual_list_list_for_inputs.contains(socket.id()) ||
                 m_vector_per_socket.contains(socket.id());
        case MFDataType::None:
          break;
      }
      BLI_assert(false);
      return false;
    }
  };

  void call(const MFMask &mask, MFParams &params, MFContext &context) const override
  {
    if (mask.indices_amount() == 0) {
      return;
    }

    Storage storage(mask);
    this->copy_inputs_to_storage(params, storage);
    this->evaluate_network_to_compute_outputs(mask, context, storage);
    this->copy_computed_values_to_outputs(mask, params, storage);
  }

 private:
  BLI_NOINLINE void copy_inputs_to_storage(MFParams &params, Storage &storage) const
  {
    for (uint i = 0; i < m_inputs.size(); i++) {
      const MFOutputSocket &socket = *m_inputs[i];
      switch (socket.type().category()) {
        case MFDataType::Single: {
          GenericVirtualListRef input_list = params.readonly_single_input(i, "Input");
          for (const MFInputSocket *target : socket.targets()) {
            storage.set_virtual_list_for_input__non_owning(*target, input_list);
          }
          break;
        }
        case MFDataType::Vector: {
          GenericVirtualListListRef input_list_list = params.readonly_vector_input(i, "Input");
          for (const MFInputSocket *target : socket.targets()) {
            const MFNode &target_node = target->node();
            if (target_node.is_function()) {
              const MFFunctionNode &target_function_node = target_node.as_function();
              uint param_index = target_function_node.input_param_indices()[target->index()];
              MFParamType param_type = target_function_node.function().param_type(param_index);

              if (param_type.is_readonly_vector_input()) {
                storage.set_virtual_list_list_for_input__non_owning(*target, input_list_list);
              }
              else if (param_type.is_mutable_vector()) {
                GenericVectorArray *vector_array = new GenericVectorArray(param_type.base_type(),
                                                                          input_list_list.size());
                for (uint i = 0; i < input_list_list.size(); i++) {
                  vector_array->extend_single__copy(i, input_list_list[i]);
                }
                storage.set_vector_array_for_input__non_owning(*target, vector_array);
                storage.take_vector_array_ownership(vector_array);
              }
              else {
                BLI_assert(false);
              }
            }
            else {
              storage.set_virtual_list_list_for_input__non_owning(*target, input_list_list);
            }
          }
          break;
        }
        case MFDataType::None: {
          BLI_assert(false);
          break;
        }
      }
    }
  }

  BLI_NOINLINE void evaluate_network_to_compute_outputs(const MFMask &mask,
                                                        MFContext &global_context,
                                                        Storage &storage) const
  {
    Stack<const MFSocket *> sockets_to_compute;

    for (const MFInputSocket *input_socket : m_outputs) {
      sockets_to_compute.push(input_socket);
    }

    while (!sockets_to_compute.empty()) {
      const MFSocket &socket = *sockets_to_compute.peek();

      if (socket.is_input()) {
        const MFInputSocket &input_socket = socket.as_input();
        if (storage.input_is_computed(input_socket)) {
          sockets_to_compute.pop();
        }
        else {
          const MFOutputSocket &origin = input_socket.origin();
          sockets_to_compute.push(&origin);
        }
      }
      else {
        const MFOutputSocket &output_socket = socket.as_output();
        const MFFunctionNode &function_node = output_socket.node().as_function();

        uint not_computed_inputs_amount = 0;
        for (const MFInputSocket *input_socket : function_node.inputs()) {
          if (!storage.input_is_computed(*input_socket)) {
            not_computed_inputs_amount++;
            sockets_to_compute.push(input_socket);
          }
        }

        bool all_inputs_are_computed = not_computed_inputs_amount == 0;
        if (all_inputs_are_computed) {
          this->compute_and_forward_outputs(mask, global_context, function_node, storage);
          sockets_to_compute.pop();
        }
      }
    }
  }

  BLI_NOINLINE void compute_and_forward_outputs(const MFMask &mask,
                                                MFContext &global_context,
                                                const MFFunctionNode &function_node,
                                                Storage &storage) const
  {
    uint array_size = mask.min_array_size();

    const MultiFunction &function = function_node.function();
    MFParamsBuilder params_builder(function, array_size);

    Vector<std::pair<const MFOutputSocket *, GenericMutableArrayRef>> single_outputs_to_forward;
    Vector<std::pair<const MFOutputSocket *, GenericVectorArray *>> vector_outputs_to_forward;

    for (uint param_index : function.param_indices()) {
      MFParamType param_type = function.param_type(param_index);
      switch (param_type.category()) {
        case MFParamType::None: {
          BLI_assert(false);
          break;
        }
        case MFParamType::ReadonlySingleInput: {
          uint input_socket_index = function_node.input_param_indices().first_index(param_index);
          const MFInputSocket &input_socket = *function_node.inputs()[input_socket_index];
          GenericVirtualListRef values = storage.get_virtual_list_for_input(input_socket);
          params_builder.add_readonly_single_input(values);
          break;
        }
        case MFParamType::ReadonlyVectorInput: {
          uint input_socket_index = function_node.input_param_indices().first_index(param_index);
          const MFInputSocket &input_socket = *function_node.inputs()[input_socket_index];
          GenericVirtualListListRef values = storage.get_virtual_list_list_for_input(input_socket);
          params_builder.add_readonly_vector_input(values);
          break;
        }
        case MFParamType::SingleOutput: {
          uint output_socket_index = function_node.output_param_indices().first_index(param_index);
          const MFOutputSocket &output_socket = *function_node.outputs()[output_socket_index];
          GenericMutableArrayRef values_destination = this->allocate_array(
              output_socket.type().type(), array_size);
          params_builder.add_single_output(values_destination);
          single_outputs_to_forward.append({&output_socket, values_destination});
          break;
        }
        case MFParamType::VectorOutput: {
          uint output_socket_index = function_node.output_param_indices().first_index(param_index);
          const MFOutputSocket &output_socket = *function_node.outputs()[output_socket_index];
          auto *values_destination = new GenericVectorArray(output_socket.type().base_type(),
                                                            array_size);
          params_builder.add_vector_output(*values_destination);
          vector_outputs_to_forward.append({&output_socket, values_destination});
          break;
        }
        case MFParamType::MutableVector: {
          uint input_socket_index = function_node.input_param_indices().first_index(param_index);
          const MFInputSocket &input_socket = *function_node.inputs()[input_socket_index];

          uint output_socket_index = function_node.output_param_indices().first_index(param_index);
          const MFOutputSocket &output_socket = *function_node.outputs()[output_socket_index];

          GenericVectorArray &values = storage.get_vector_array_for_input(input_socket);
          params_builder.add_mutable_vector(values);
          vector_outputs_to_forward.append({&output_socket, &values});
          break;
        }
      }
    }

    MFParams &params = params_builder.build();
    function.call(mask, params, global_context);

    for (auto single_forward_info : single_outputs_to_forward) {
      const MFOutputSocket &output_socket = *single_forward_info.first;
      GenericMutableArrayRef values = single_forward_info.second;
      storage.take_array_ref_ownership(values);

      for (const MFInputSocket *target : output_socket.targets()) {
        storage.set_virtual_list_for_input__non_owning(*target, values);
      }
    }

    for (auto vector_forward_info : vector_outputs_to_forward) {
      const MFOutputSocket &output_socket = *vector_forward_info.first;
      GenericVectorArray *values = vector_forward_info.second;
      storage.take_vector_array_ownership__not_twice(values);

      for (const MFInputSocket *target : output_socket.targets()) {
        const MFNode &target_node = target->node();
        if (target_node.is_function()) {
          const MFFunctionNode &target_function_node = target_node.as_function();
          uint param_index = target_function_node.input_param_indices()[target->index()];
          MFParamType param_type = target_function_node.function().param_type(param_index);

          if (param_type.is_readonly_vector_input()) {
            storage.set_virtual_list_list_for_input__non_owning(*target, *values);
          }
          else if (param_type.is_mutable_vector()) {
            GenericVectorArray *copied_values = new GenericVectorArray(values->type(),
                                                                       values->size());
            for (uint i = 0; i < values->size(); i++) {
              copied_values->extend_single__copy(i, (*values)[i]);
            }
            storage.take_vector_array_ownership(copied_values);
            storage.set_vector_array_for_input__non_owning(*target, copied_values);
          }
          else {
            BLI_assert(false);
          }
        }
        else if (m_outputs.contains(target)) {
          storage.set_vector_array_for_input__non_owning(*target, values);
        }
      }
    }
  }

  BLI_NOINLINE void copy_computed_values_to_outputs(const MFMask &mask,
                                                    MFParams &params,
                                                    Storage &storage) const
  {
    for (uint output_index = 0; output_index < m_outputs.size(); output_index++) {
      uint global_param_index = m_inputs.size() + output_index;
      const MFInputSocket &socket = *m_outputs[output_index];
      switch (socket.type().category()) {
        case MFDataType::None: {
          BLI_assert(false);
          break;
        }
        case MFDataType::Single: {
          GenericVirtualListRef values = storage.get_virtual_list_for_input(socket);
          GenericMutableArrayRef output_values = params.single_output(global_param_index,
                                                                      "Output");
          for (uint i : mask.indices()) {
            output_values.copy_in__uninitialized(i, values[i]);
          }
          break;
        }
        case MFDataType::Vector: {
          GenericVirtualListListRef values = storage.get_virtual_list_list_for_input(socket);
          GenericVectorArray &output_values = params.vector_output(global_param_index, "Output");
          for (uint i : mask.indices()) {
            output_values.extend_single__copy(i, values[i]);
          }
          break;
        }
      }
    }
  }

  GenericMutableArrayRef allocate_array(const CPPType &type, uint size) const
  {
    void *buffer = MEM_malloc_arrayN(size, type.size(), __func__);
    return GenericMutableArrayRef(type, buffer, size);
  }
};

void MOD_functiondeform_do(FunctionDeformModifierData *fdmd, float (*vertexCos)[3], int numVerts)
{
  if (fdmd->function_tree == nullptr) {
    return;
  }

  bNodeTree *tree = (bNodeTree *)DEG_get_original_id((ID *)fdmd->function_tree);
  VirtualNodeTree vtree;
  vtree.add_all_of_tree(tree);
  vtree.freeze_and_index();

  const VirtualNode &input_vnode = *vtree.nodes_with_idname("fn_FunctionInputNode")[0];
  const VirtualNode &output_vnode = *vtree.nodes_with_idname("fn_FunctionOutputNode")[0];

  OwnedResources resources;
  VTreeMFNetworkBuilder builder(vtree);
  if (!insert_nodes(builder, resources)) {
    BLI_assert(false);
  }
  if (!insert_links(builder, resources)) {
    BLI_assert(false);
  }
  if (!insert_unlinked_inputs(builder, resources)) {
    BLI_assert(false);
  }

  auto vtree_network = builder.build();

  Vector<const MFOutputSocket *> function_inputs = {
      &vtree_network->lookup_socket(input_vnode.output(0)).as_output(),
      &vtree_network->lookup_socket(input_vnode.output(1)).as_output(),
      &vtree_network->lookup_socket(input_vnode.output(2)).as_output()};

  Vector<const MFInputSocket *> function_outputs = {
      &vtree_network->lookup_socket(output_vnode.input(0)).as_input()};

  MultiFunction_FunctionTree function{function_inputs, function_outputs};

  MFParamsBuilder params(function, numVerts);
  params.add_readonly_single_input(ArrayRef<float3>((float3 *)vertexCos, numVerts));
  params.add_readonly_single_input(&fdmd->control1);
  params.add_readonly_single_input(&fdmd->control2);

  TemporaryVector<float3> output_vectors(numVerts);
  params.add_single_output<float3>(output_vectors);

  MFContext context;
  function.call(IndexRange(numVerts).as_array_ref(), params.build(), context);

  memcpy(vertexCos, output_vectors.begin(), output_vectors.size() * sizeof(float3));
}
