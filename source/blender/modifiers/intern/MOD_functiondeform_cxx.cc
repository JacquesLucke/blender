#include "DNA_modifier_types.h"

#include "BKE_virtual_node_tree_cxx.h"
#include "BKE_multi_functions.h"
#include "BKE_tuple.h"

#include "BLI_math_cxx.h"

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
  VirtualNode &m_input_vnode;
  VirtualNode &m_output_vnode;

 public:
  MultiFunction_FunctionTree(VirtualNode &input_vnode, VirtualNode &output_vnode)
      : m_input_vnode(input_vnode), m_output_vnode(output_vnode)
  {
    SignatureBuilder signature;
    for (VirtualSocket *vsocket : input_vnode.outputs().drop_back(1)) {
      CPPType &type = get_type_by_socket(vsocket);
      signature.readonly_single_input(vsocket->name(), type);
    }
    for (VirtualSocket *vsocket : output_vnode.inputs().drop_back(1)) {
      CPPType &type = get_type_by_socket(vsocket);
      signature.single_output(vsocket->name(), type);
    }
    this->set_signature(signature);
  }

  void call(ArrayRef<uint> mask_indices, Params &params) const override
  {
    if (mask_indices.size() == 0) {
      return;
    }

    for (uint output_index = 0; output_index < m_output_vnode.inputs().size() - 1;
         output_index++) {
      uint param_index = output_index + m_input_vnode.outputs().size() - 1;
      VirtualSocket *vsocket = m_output_vnode.input(output_index);
      BKE::GenericMutableArrayRef output_array = params.single_output(param_index,
                                                                      vsocket->name());

      if (vsocket->is_linked()) {
        this->compute_output(mask_indices, params, vsocket->links()[0], output_array);
      }
      else {
        CPPType &type = get_type_by_socket(vsocket);
        BKE::TupleInfo value_info{{&type}};
        BKE_TUPLE_STACK_ALLOC(value_tuple, value_info);
        load_socket_value(vsocket, value_tuple, 0);
        for (uint i : mask_indices) {
          output_array.copy_in__uninitialized(i, value_tuple->element_ptr(0));
        }
      }
    }
  }

  void compute_output(ArrayRef<uint> mask_indices,
                      Params &global_params,
                      VirtualSocket *vsocket,
                      BKE::GenericMutableArrayRef result) const
  {
    VirtualNode *vnode = vsocket->vnode();

    uint output_index = 0;
    while (vnode->outputs()[output_index] != vsocket) {
      output_index++;
    }

    if (vnode == &m_input_vnode) {
      auto input_values = global_params.readonly_single_input(output_index, vsocket->name());

      for (uint i : mask_indices) {
        result.copy_in__uninitialized(i, input_values[i]);
      }

      return;
    }

    auto node_function = get_multi_function_by_node(vnode);

    ParamsBuilder params;
    uint array_size = result.size();
    params.start_new(node_function->signature(), array_size);

    Vector<void *> linked_buffers;
    Vector<CPPType *> unlinked_types;

    for (VirtualSocket *input_vsocket : vnode->inputs()) {
      CPPType &type = get_type_by_socket(input_vsocket);
      if (input_vsocket->is_linked()) {
        void *buffer = MEM_mallocN_aligned(array_size * type.size(), type.alignment(), __func__);
        linked_buffers.append(buffer);
      }
      else {
        unlinked_types.append(&type);
      }
    }

    BKE::TupleInfo unlinked_info{unlinked_types};
    BKE_TUPLE_STACK_ALLOC(unlinked_values, unlinked_info);

    {
      uint linked_index = 0;
      uint unlinked_index = 0;
      for (VirtualSocket *input_vsocket : vnode->inputs()) {
        if (input_vsocket->is_linked()) {
          CPPType &type = get_type_by_socket(input_vsocket);
          BKE::GenericMutableArrayRef array_ref{&type, linked_buffers[linked_index], array_size};
          VirtualSocket *origin = input_vsocket->links()[0];
          this->compute_output(mask_indices, global_params, origin, array_ref);
          params.add_readonly_array_ref(array_ref);
          linked_index++;
        }
        else {
          load_socket_value(input_vsocket, unlinked_values, unlinked_index);
          params.add_readonly_single_ref(unlinked_values, unlinked_index);
          unlinked_index++;
        }
      }
    }

    Vector<BKE::GenericMutableArrayRef> temporary_output_buffers;
    {
      for (VirtualSocket *output_vsocket : vnode->outputs()) {
        if (output_vsocket == vsocket) {
          params.add_mutable_array_ref(result);
        }
        else {
          CPPType &type = get_type_by_socket(output_vsocket);
          void *buffer = MEM_mallocN_aligned(type.size() * array_size, type.alignment(), __func__);
          BKE::GenericMutableArrayRef array_ref{&type, buffer, array_size};
          params.add_mutable_array_ref(array_ref);
          temporary_output_buffers.append(array_ref);
        }
      }
    }

    node_function->call(mask_indices, params.build());

    {
      uint linked_index = 0;
      for (VirtualSocket *input_vsocket : vnode->inputs()) {
        if (input_vsocket->is_linked()) {
          CPPType &type = get_type_by_socket(input_vsocket);
          void *buffer = linked_buffers[linked_index];
          for (uint i : mask_indices) {
            type.destruct(POINTER_OFFSET(buffer, i * type.size()));
          }
          linked_index++;
        }
      }
      for (void *buffer : linked_buffers) {
        MEM_freeN(buffer);
      }
      for (auto array_ref : temporary_output_buffers) {
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

  VirtualNodeTree vtree;
  vtree.add_all_of_tree(fdmd->function_tree);
  vtree.freeze_and_index();

  MultiFunction_FunctionTree function{*vtree.nodes_with_idname("fn_FunctionInputNode")[0],
                                      *vtree.nodes_with_idname("fn_FunctionOutputNode")[0]};

  BKE::MultiFunction::ParamsBuilder params;
  params.start_new(function.signature(), numVerts);
  params.add_readonly_array_ref(ArrayRef<float3>((float3 *)vertexCos, numVerts));
  params.add_readonly_single_ref(&fdmd->control1);
  params.add_readonly_single_ref(&fdmd->control2);

  TemporaryVector<float3> output_vectors(numVerts);
  params.add_mutable_array_ref<float3>(output_vectors);

  function.call(IndexRange(numVerts).as_array_ref(), params.build());

  memcpy(vertexCos, output_vectors.begin(), output_vectors.size() * sizeof(float3));
}
