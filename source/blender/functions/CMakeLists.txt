set(INC
  .
  ../blenlib
  ../makesdna
  ../makesrna
  ../blenkernel
  ../depsgraph
  ../windowmanager
  ../imbuf
  ../../../intern/guardedalloc
)

set(INC_SYS
  ${LLVM_INCLUDE_DIRS}
  ${PYTHON_INCLUDE_DIRS}
)

if(WITH_PYTHON)
  add_definitions(-DWITH_PYTHON)
  list(APPEND INC
    ../python
  )
endif()

set(SRC
  intern/multi_functions/lists.cc
  intern/multi_functions/mixed.cc
  intern/multi_functions/network.cc
  intern/multi_functions/particles.cc
  intern/multi_functions/sampling_util.cc
  intern/multi_functions/surface_hook.cc
  intern/multi_functions/vectorize.cc
  intern/inlined_tree_multi_function_network/builder.cc
  intern/inlined_tree_multi_function_network/generate.cc
  intern/inlined_tree_multi_function_network/mappings_nodes.cc
  intern/inlined_tree_multi_function_network/mappings_sockets.cc
  intern/inlined_tree_multi_function_network/mappings.cc
  intern/attributes_block_container.cc
  intern/attributes_ref.cc
  intern/cpp_type.cc
  intern/cpp_types.cc
  intern/generic_array_ref.cc
  intern/generic_tuple.cc
  intern/initialize.cc
  intern/multi_function_common_contexts.cc
  intern/multi_function_context.cc
  intern/multi_function_network.cc
  intern/node_tree.cc

  FN_attributes_block_container.h
  FN_attributes_ref.h
  FN_cpp_type.h
  FN_generic_array_ref.h
  FN_generic_tuple.h
  FN_generic_vector_array.h
  FN_generic_virtual_list_list_ref.h
  FN_generic_virtual_list_ref.h
  FN_initialize.h
  FN_multi_function_common_contexts.h
  FN_multi_function_context.h
  FN_multi_function_data_type.h
  FN_multi_function_dependencies.h
  FN_multi_function_network.h
  FN_multi_function_param_type.h
  FN_multi_function.h
  FN_multi_functions.h
  FN_inlined_tree_multi_function_network_generation.h
  FN_inlined_tree_multi_function_network.h
  FN_node_tree.h

  intern/multi_functions/customizable.h
  intern/multi_functions/lists.h
  intern/multi_functions/mixed.h
  intern/multi_functions/network.h
  intern/multi_functions/particles.h
  intern/multi_functions/sampling_util.h
  intern/multi_functions/surface_hook.h
  intern/multi_functions/util.h
  intern/multi_functions/vectorize.h
  intern/inlined_tree_multi_function_network/builder.h
  intern/inlined_tree_multi_function_network/mappings.h
  intern/cpp_types.h
)

set(LIB
  bf_blenlib
)

blender_add_lib(bf_functions "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
