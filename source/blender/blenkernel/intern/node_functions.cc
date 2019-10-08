#include "BKE_node_functions.h"
#include "BKE_generic_array_ref.h"

#include "BLI_math_cxx.h"
#include "BLI_lazy_init_cxx.h"
#include "BLI_string_map.h"

namespace BKE {

using BLI::float3;
using BLI::StringMap;
using CreateFunctionCB = std::unique_ptr<CPPFunction> (*)(VirtualNode *vnode);

BLI_LAZY_INIT_STATIC_VAR(StringMap<std::unique_ptr<CPPFunction>>, get_cached_functions)
BLI_LAZY_INIT_STATIC_VAR(StringMap<CreateFunctionCB>, get_function_builders)

class ArrayRefFunction_AddFloats : public CPPFunction {
  void signature(SignatureBuilderCPP &signature) override
  {
    signature.add_input("Indices", GET_TYPE_array_ref<uint>());
    signature.add_input("A", GET_TYPE_array_ref<float>());
    signature.add_input("B", GET_TYPE_array_ref<float>());
    signature.add_input("Result", GET_TYPE_mutable_array_ref<float>());
  }

  void call(TupleRef &fn_in, TupleRef &UNUSED(fn_out)) const override
  {
    auto indices = fn_in.get<GenericArrayRef>(0).get_ref<uint>();
    auto a = fn_in.get<GenericArrayRef>(1).get_ref<float>();
    auto b = fn_in.get<GenericArrayRef>(2).get_ref<float>();
    auto result = fn_in.get<GenericMutableArrayRef>(3).get_ref<float>();

    for (uint i : indices) {
      result[i] = a[i] + b[i];
    }
  }
};

class ArrayRefFunction_VectorDistance : public CPPFunction {
  void signature(SignatureBuilderCPP &signature) override
  {
    signature.add_input("Indices", GET_TYPE_array_ref<uint>());
    signature.add_input("A", GET_TYPE_array_ref<float3>());
    signature.add_input("B", GET_TYPE_array_ref<float3>());
    signature.add_input("Result", GET_TYPE_mutable_array_ref<float>());
  }

  void call(TupleRef &fn_in, TupleRef &UNUSED(fn_out)) const override
  {
    auto indices = fn_in.get<GenericArrayRef>(0).get_ref<uint>();
    auto a = fn_in.get<GenericArrayRef>(1).get_ref<float3>();
    auto b = fn_in.get<GenericArrayRef>(2).get_ref<float3>();
    auto result = fn_in.get<GenericMutableArrayRef>(3).get_ref<float>();

    for (uint i : indices) {
      result[i] = float3::distance(a[i], b[i]);
    }
  }
};

void init_vnode_array_functions()
{
  auto &cached_functions = get_cached_functions();
  auto &callbacks = get_function_builders();
}

Optional<FunctionForNode> get_vnode_array_function(VirtualNode *vnode)
{
  {
    auto &cached_functions = get_cached_functions();
    std::unique_ptr<CPPFunction> *function = cached_functions.lookup_ptr(vnode->idname());
    if (function != nullptr) {
      return FunctionForNode{(*function).get(), false};
    }
  }
  return {};
}

}  // namespace BKE