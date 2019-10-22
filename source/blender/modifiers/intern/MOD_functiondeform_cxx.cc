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

static MFDataType get_type_by_socket(VirtualSocket *vsocket)
{
  StringRef idname = vsocket->idname();

  if (idname == "fn_FloatSocket") {
    return MFDataType::ForSingle<float>();
  }
  else if (idname == "fn_IntegerSocket") {
    return MFDataType::ForSingle<int>();
  }
  else if (idname == "fn_VectorSocket") {
    return MFDataType::ForSingle<float3>();
  }
  BLI_assert(false);
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

  const MFSocket &lookup_socket(VirtualSocket &vsocket)
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
    for (VirtualNode *vnode : m_vtree.nodes()) {
      for (VirtualSocket *vsocket : vnode->inputs()) {
        MFDataType data_type = get_type_by_socket(vsocket);
        m_type_by_vsocket[vsocket->id()] = data_type;
      }
      for (VirtualSocket *vsocket : vnode->outputs()) {
        MFDataType data_type = get_type_by_socket(vsocket);
        m_type_by_vsocket[vsocket->id()] = data_type;
      }
    }
  }

  const VirtualNodeTree &vtree() const
  {
    return m_vtree;
  }

  MFBuilderFunctionNode &add_function(MultiFunction &function,
                                      ArrayRef<uint> input_param_indices,
                                      ArrayRef<uint> output_param_indices)
  {
    return m_builder->add_function(function, input_param_indices, output_param_indices);
  }

  MFBuilderFunctionNode &add_function(MultiFunction &function,
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
    for (VirtualSocket *vsocket : vnode.inputs()) {
      MFDataType data_type = this->try_get_data_type(*vsocket);
      if (!data_type.is_none()) {
        input_types.append(data_type);
      }
    }

    Vector<MFDataType> output_types;
    for (VirtualSocket *vsocket : vnode.outputs()) {
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
    return m_type_by_vsocket[vsocket.id()].is_none();
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
    for (VirtualSocket *vsocket : vnode.inputs()) {
      if (this->is_data_socket(*vsocket)) {
        this->map_sockets(*vsocket, *node.inputs()[data_inputs]);
        data_inputs++;
      }
    }

    uint data_outputs = 0;
    for (VirtualSocket *vsocket : vnode.outputs()) {
      if (this->is_data_socket(*vsocket)) {
        this->map_sockets(*vsocket, *node.outputs()[data_outputs]);
        data_outputs++;
      }
    }
  }

  void map_sockets(VirtualSocket &vsocket, MFBuilderSocket &socket)
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

  std::unique_ptr<VTreeMFNetwork> build()
  {
    auto network = BLI::make_unique<MFNetwork>(std::move(m_builder));

    Array<const MFSocket *> socket_map(m_vtree.socket_count(), nullptr);
    for (uint vsocket_id = 0; vsocket_id < m_vtree.socket_count(); vsocket_id++) {
      MFBuilderSocket *builder_socket = m_socket_map[vsocket_id];
      if (builder_socket != nullptr) {
        socket_map[vsocket_id] = &network->socket_by_id(builder_socket->id());
      }
    }

    return BLI::make_unique<VTreeMFNetwork>(m_vtree, std::move(network), std::move(socket_map));
  }
};

using InsertVNodeFunction = std::function<void(
    VTreeMFNetworkBuilder &builder, OwnedResources &resources, const VirtualNode &vnode)>;

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

static void insert_nodes(VTreeMFNetworkBuilder &builder, OwnedResources &resources)
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
}

static std::unique_ptr<BKE::MultiFunction> get_multi_function_by_node(VirtualNode *vnode)
{
  StringRef idname = vnode->idname();

  if (idname == "fn_VectorMathNode") {
    return BLI::make_unique<BKE::MultiFunction_AddFloat3s>();
  }
  else if (idname == "fn_CombineVectorNode") {
    return BLI::make_unique<BKE::MultiFunction_CombineVector>();
  }
  else if (idname == "fn_SeparateVectorNode") {
    return BLI::make_unique<BKE::MultiFunction_SeparateVector>();
  }
  else {
    BLI_assert(false);
    return {};
  }
}

static void load_socket_value(VirtualSocket *vsocket, TupleRef tuple, uint index)
{
  StringRef idname = vsocket->idname();
  PointerRNA rna = vsocket->rna();

  if (idname == "fn_FloatSocket") {
    float value = RNA_float_get(&rna, "value");
    tuple.set<float>(index, value);
  }
  else if (idname == "fn_IntegerSocket") {
    int value = RNA_int_get(&rna, "value");
    tuple.set<int>(index, value);
  }
  else if (idname == "fn_VectorSocket") {
    float3 value;
    RNA_float_get_array(&rna, "value", value);
    tuple.set<float3>(index, value);
  }
  else {
    BLI_assert(false);
  }
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
  // vtree.add_all_of_tree(tree);
  vtree.freeze_and_index();

  VTreeMFNetworkBuilder builder(vtree);

  auto &input_node = builder.add_placeholder(
      {}, {MFDataType::ForSingle<float3>(), MFDataType::ForSingle<float>()});

  auto &output_node = builder.add_placeholder({MFDataType::ForSingle<float3>()}, {});

  BKE::MultiFunction_AddFloat3s add_function;
  auto &add_node = builder.add_function(add_function, {0, 1}, {2});

  BKE::MultiFunction_ConstantValue<float3> vector_value{
      {fdmd->control1, fdmd->control1, fdmd->control1}};
  auto &value_node = builder.add_function(vector_value, {}, {0});

  uint input_node_id = input_node.id();
  uint output_node_id = output_node.id();

  builder.add_link(*input_node.outputs()[0], *add_node.inputs()[0]);
  builder.add_link(*value_node.outputs()[0], *add_node.inputs()[1]);
  builder.add_link(*add_node.outputs()[0], *output_node.inputs()[0]);

  auto vtree_network = builder.build();

  auto &final_input_node = vtree_network->network().node_by_id(input_node_id);
  auto &final_output_node = vtree_network->network().node_by_id(output_node_id);

  MultiFunction_FunctionTree function{final_input_node.outputs(), final_output_node.inputs()};

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
