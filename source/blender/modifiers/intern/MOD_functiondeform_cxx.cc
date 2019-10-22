#include "DNA_modifier_types.h"

#include "BKE_virtual_node_tree_cxx.h"
#include "BKE_multi_functions.h"
#include "BKE_tuple.h"
#include "BKE_multi_function_network.h"

#include "BLI_math_cxx.h"
#include "BLI_string_map.h"
#include "BLI_owned_resources.h"

#include "DEG_depsgraph_query.h"

using BKE::CPPType;
using BKE::MFBuilderFunctionNode;
using BKE::MFBuilderInputSocket;
using BKE::MFBuilderNode;
using BKE::MFBuilderOutputSocket;
using BKE::MFBuilderPlaceholderNode;
using BKE::MFBuilderSocket;
using BKE::MFContext;
using BKE::MFDataType;
using BKE::MFInputSocket;
using BKE::MFNetwork;
using BKE::MFNetworkBuilder;
using BKE::MFNode;
using BKE::MFOutputSocket;
using BKE::MFParams;
using BKE::MFParamsBuilder;
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
using BLI::OwnedResources;
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
  return MFDataType();
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

  MFBuilderPlaceholderNode &add_placeholder(const VirtualNode &vnode)
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

    MFBuilderPlaceholderNode &node = m_builder->add_placeholder(input_types, output_types);
    this->map_data_sockets(vnode, node);
    return node;
  }

  MFBuilderPlaceholderNode &add_placeholder(ArrayRef<MFDataType> input_types,
                                            ArrayRef<MFDataType> output_types)
  {
    return m_builder->add_placeholder(input_types, output_types);
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

static void INSERT_vector_math(VTreeMFNetworkBuilder &builder,
                               OwnedResources &resources,
                               const VirtualNode &vnode)
{
  auto function = BLI::make_unique<BKE::MultiFunction_AddFloat3s>();
  builder.add_function(*function, {0, 1}, {2}, vnode);
  resources.add(std::move(function), "vector math function");
}

static void INSERT_combine_vector(VTreeMFNetworkBuilder &builder,
                                  OwnedResources &resources,
                                  const VirtualNode &vnode)
{
  auto function = BLI::make_unique<BKE::MultiFunction_CombineVector>();
  builder.add_function(*function, {0, 1, 2}, {3}, vnode);
  resources.add(std::move(function), "combine vector function");
}

static void INSERT_separate_vector(VTreeMFNetworkBuilder &builder,
                                   OwnedResources &resources,
                                   const VirtualNode &vnode)
{
  auto function = BLI::make_unique<BKE::MultiFunction_SeparateVector>();
  builder.add_function(*function, {0}, {1, 2, 3}, vnode);
  resources.add(std::move(function), "separate vector function");
}

static StringMap<InsertVNodeFunction> get_node_inserters()
{
  StringMap<InsertVNodeFunction> inserters;
  inserters.add_new("fn_VectorMathNode", INSERT_vector_math);
  inserters.add_new("fn_CombineVectorNode", INSERT_combine_vector);
  inserters.add_new("fn_SeparateVectorNode", INSERT_separate_vector);
  return inserters;
}

static MFBuilderOutputSocket &INSERT_vector_socket(VTreeMFNetworkBuilder &builder,
                                                   OwnedResources &resources,
                                                   const VirtualSocket &vsocket)
{
  PointerRNA rna = vsocket.rna();
  float3 value;
  RNA_float_get_array(&rna, "value", value);

  auto function = BLI::make_unique<BKE::MultiFunction_ConstantValue<float3>>(value);
  auto &node = builder.add_function(*function, {}, {0});

  resources.add(std::move(function), "vector input");
  return *node.outputs()[0];
}

static MFBuilderOutputSocket &INSERT_float_socket(VTreeMFNetworkBuilder &builder,
                                                  OwnedResources &resources,
                                                  const VirtualSocket &vsocket)
{
  PointerRNA rna = vsocket.rna();
  float value = RNA_float_get(&rna, "value");

  auto function = BLI::make_unique<BKE::MultiFunction_ConstantValue<float>>(value);
  auto &node = builder.add_function(*function, {}, {0});

  resources.add(std::move(function), "float input");
  return *node.outputs()[0];
}

static StringMap<InsertUnlinkedInputFunction> get_unlinked_input_inserter()
{
  StringMap<InsertUnlinkedInputFunction> inserters;
  inserters.add_new("fn_VectorSocket", INSERT_vector_socket);
  inserters.add_new("fn_FloatSocket", INSERT_float_socket);
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
      builder.add_placeholder(*vnode);
    }
  }

  return true;
}

static bool insert_links(VTreeMFNetworkBuilder &builder, OwnedResources &UNUSED(resources))
{
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

    if (from_socket.type() != to_socket.type()) {
      return false;
    }

    builder.add_link(from_socket, to_socket);
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
    MFSignatureBuilder signature;
    for (auto socket : m_inputs) {
      signature.readonly_single_input("Input", socket->type().type());
    }
    for (auto socket : m_outputs) {
      signature.single_output("Output", socket->type().type());
    }
    this->set_signature(signature);
  }

  void call(ArrayRef<uint> mask_indices, MFParams &params, MFContext &context) const override
  {
    if (mask_indices.size() == 0) {
      return;
    }

    for (uint output_index = 0; output_index < m_outputs.size(); output_index++) {
      uint output_param_index = output_index + m_inputs.size();
      BKE::GenericMutableArrayRef output_array = params.single_output(output_param_index,
                                                                      "Output");

      this->compute_output(
          mask_indices, params, context, m_outputs[output_index]->origin(), output_array);
    }
  }

  void compute_output(ArrayRef<uint> mask_indices,
                      MFParams &global_params,
                      MFContext &context,
                      const BKE::MFOutputSocket &socket_to_compute,
                      BKE::GenericMutableArrayRef result) const
  {
    auto &current_node = socket_to_compute.node().as_function();
    uint output_index = socket_to_compute.index();

    if (m_inputs.contains(&socket_to_compute)) {
      auto input_values = global_params.readonly_single_input(output_index, "Input");

      for (uint i : mask_indices) {
        result.copy_in__uninitialized(i, input_values[i]);
      }

      return;
    }

    auto &node_function = current_node.function();

    MFParamsBuilder params;
    uint array_size = result.size();
    params.start_new(node_function.signature(), array_size);

    Vector<BKE::GenericMutableArrayRef> temporary_input_buffers;

    for (auto input_socket : current_node.inputs()) {
      const CPPType &type = input_socket->type().type();
      void *buffer = MEM_mallocN_aligned(array_size * type.size(), type.alignment(), __func__);

      BKE::GenericMutableArrayRef array_ref{&type, buffer, array_size};
      temporary_input_buffers.append(array_ref);
      auto &origin = input_socket->origin();
      this->compute_output(mask_indices, global_params, context, origin, array_ref);
      params.add_readonly_array_ref(array_ref);
    }

    Vector<BKE::GenericMutableArrayRef> temporary_output_buffers;
    {
      for (auto output_socket : current_node.outputs()) {
        if (output_socket == &socket_to_compute) {
          params.add_mutable_array_ref(result);
        }
        else {
          const CPPType &type = output_socket->type().type();
          void *buffer = MEM_mallocN_aligned(type.size() * array_size, type.alignment(), __func__);
          BKE::GenericMutableArrayRef array_ref{&type, buffer, array_size};
          params.add_mutable_array_ref(array_ref);
          temporary_output_buffers.append(array_ref);
        }
      }
    }

    node_function.call(mask_indices, params.build(), context);

    {
      for (auto array_ref : temporary_input_buffers) {
        array_ref.destruct_all();
        MEM_freeN(array_ref.buffer());
      }
      for (auto array_ref : temporary_output_buffers) {
        array_ref.destruct_all();
        MEM_freeN(array_ref.buffer());
      }
    }
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
      &vtree_network->lookup_socket(input_vnode.output(1)).as_output()};

  Vector<const MFInputSocket *> function_outputs = {
      &vtree_network->lookup_socket(output_vnode.input(0)).as_input()};

  MultiFunction_FunctionTree function{function_inputs, function_outputs};

  MFParamsBuilder params;
  params.start_new(function.signature(), numVerts);
  params.add_readonly_array_ref(ArrayRef<float3>((float3 *)vertexCos, numVerts));
  params.add_readonly_single_ref(&fdmd->control1);

  TemporaryVector<float3> output_vectors(numVerts);
  params.add_mutable_array_ref<float3>(output_vectors);

  MFContext context;
  function.call(IndexRange(numVerts).as_array_ref(), params.build(), context);

  memcpy(vertexCos, output_vectors.begin(), output_vectors.size() * sizeof(float3));
}
