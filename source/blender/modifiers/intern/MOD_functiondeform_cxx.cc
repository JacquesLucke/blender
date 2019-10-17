#include "DNA_modifier_types.h"

#include "BKE_virtual_node_tree_cxx.h"
#include "BKE_multi_functions.h"
#include "BKE_tuple.h"
#include "BKE_multi_function_network.h"

#include "BLI_math_cxx.h"

#include "DEG_depsgraph_query.h"

using BKE::CPPType;
using BKE::TupleRef;
using BKE::VirtualLink;
using BKE::VirtualNode;
using BKE::VirtualNodeTree;
using BKE::VirtualSocket;
using BLI::ArrayRef;
using BLI::float3;
using BLI::IndexRange;
using BLI::StringRef;
using BLI::TemporaryVector;
using BLI::Vector;

extern "C" {
void MOD_functiondeform_do(FunctionDeformModifierData *fdmd, float (*vertexCos)[3], int numVerts);
}

static CPPType &get_type_by_socket(VirtualSocket *vsocket)
{
  StringRef idname = vsocket->idname();

  if (idname == "fn_FloatSocket") {
    return BKE::GET_TYPE<float>();
  }
  else if (idname == "fn_IntegerSocket") {
    return BKE::GET_TYPE<int>();
  }
  else if (idname == "fn_VectorSocket") {
    return BKE::GET_TYPE<float3>();
  }
  BLI_assert(false);
  return BKE::GET_TYPE<float>();
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
  Vector<BKE::MultiFunctionNetwork::OutputSocket *> m_inputs;
  Vector<BKE::MultiFunctionNetwork::InputSocket *> m_outputs;

 public:
  MultiFunction_FunctionTree(Vector<BKE::MultiFunctionNetwork::OutputSocket *> inputs,
                             Vector<BKE::MultiFunctionNetwork::InputSocket *> outputs)
      : m_inputs(std::move(inputs)), m_outputs(std::move(outputs))
  {
    SignatureBuilder signature;
    for (auto socket : m_inputs) {
      signature.readonly_single_input("Input", socket->type().type());
    }
    for (auto socket : m_outputs) {
      signature.single_output("Output", socket->type().type());
    }
    this->set_signature(signature);
  }

  void call(ArrayRef<uint> mask_indices, Params &params, Context &context) const override
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
                      Params &global_params,
                      Context &context,
                      BKE::MultiFunctionNetwork::OutputSocket &socket_to_compute,
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

    ParamsBuilder params;
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

  auto network_builder = BLI::make_unique<BKE::MultiFunctionNetwork::NetworkBuilder>();
  auto &input_node = network_builder->add_placeholder(
      {},
      {BKE::MultiFunctionDataType{BKE::MultiFunctionDataType::Single, BKE::GET_TYPE<float3>()},
       BKE::MultiFunctionDataType{BKE::MultiFunctionDataType::Single, BKE::GET_TYPE<float>()}});

  auto &output_node = network_builder->add_placeholder(
      {BKE::MultiFunctionDataType{BKE::MultiFunctionDataType::Single, BKE::GET_TYPE<float3>()}},
      {});

  BKE::MultiFunction_AddFloat3s add_function;
  auto &add_node = network_builder->add_function(add_function, {0, 1}, {2});

  uint input_node_id = input_node.id();
  uint output_node_id = output_node.id();

  network_builder->add_link(*input_node.outputs()[0], *add_node.inputs()[0]);
  network_builder->add_link(*input_node.outputs()[0], *add_node.inputs()[1]);
  network_builder->add_link(*add_node.outputs()[0], *output_node.inputs()[0]);

  BKE::MultiFunctionNetwork::Network network(std::move(network_builder));

  auto &final_input_node = network.node_by_id(input_node_id);
  auto &final_output_node = network.node_by_id(output_node_id);

  MultiFunction_FunctionTree function{final_input_node.outputs(), final_output_node.inputs()};

  BKE::MultiFunction::ParamsBuilder params;
  params.start_new(function.signature(), numVerts);
  params.add_readonly_array_ref(ArrayRef<float3>((float3 *)vertexCos, numVerts));
  params.add_readonly_single_ref(&fdmd->control1);

  TemporaryVector<float3> output_vectors(numVerts);
  params.add_mutable_array_ref<float3>(output_vectors);

  BKE::MultiFunction::Context context;
  function.call(IndexRange(numVerts).as_array_ref(), params.build(), context);

  memcpy(vertexCos, output_vectors.begin(), output_vectors.size() * sizeof(float3));
}
