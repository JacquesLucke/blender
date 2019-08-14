set(INC
  .
  ../blenlib
  ../makesdna
  ../makesrna
  ../blenkernel
  ../depsgraph
  ../windowmanager
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
  FN_all.hpp
  FN_core.hpp
  FN_types.hpp
  FN_functions.hpp
  FN_data_flow_nodes.hpp

  FN_all-c.h
  FN_core-c.h
  FN_types-c.h
  FN_dependencies-c.h
  FN_data_flow_nodes-c.h

  initialize.cpp

  core/type.hpp
  core/type.cpp
  core/function.hpp
  core/function.cpp
  core/function_builder.hpp
  core/function_builder.cpp
  core/data_graph.hpp
  core/data_graph.cpp
  core/data_graph_builder.hpp
  core/data_graph_builder.cpp
  core/function_graph.hpp
  core/function_graph.cpp
  core/source_info.hpp
  core/source_info.cpp
  core/dot_export.cpp
  core/core-c.h
  core/core-c.cpp

  backends/cpp/cpp_type_info.hpp
  backends/cpp/cpp_type_info.cpp
  backends/cpp/tuple.hpp
  backends/cpp/tuple.cpp
  backends/cpp/list.hpp
  backends/cpp/list.cpp
  backends/cpp/cpp-c.h
  backends/cpp/cpp-c.cpp

  backends/tuple_call/tuple_call.hpp
  backends/tuple_call/tuple_call.cpp
  backends/tuple_call/fgraph_tuple_call.hpp
  backends/tuple_call/fgraph_tuple_call.cpp
  backends/tuple_call/lazy_to_normal.hpp
  backends/tuple_call/lazy_to_normal.cpp
  backends/tuple_call/execution_context.hpp
  backends/tuple_call/execution_context.cpp
  backends/tuple_call/tuple_call-c.h
  backends/tuple_call/tuple_call-c.cpp

  backends/dependencies/dependencies.hpp
  backends/dependencies/dependencies.cpp
  backends/dependencies/fgraph_dependencies.hpp
  backends/dependencies/fgraph_dependencies.cpp
  backends/dependencies/dependencies-c.h
  backends/dependencies/dependencies-c.cpp

  backends/llvm/initialize.hpp
  backends/llvm/initialize.cpp
  backends/llvm/llvm_type_info.hpp
  backends/llvm/llvm_type_info.cpp
  backends/llvm/build_ir_body.hpp
  backends/llvm/build_ir_body.cpp
  backends/llvm/ir_to_tuple_call.hpp
  backends/llvm/ir_to_tuple_call.cpp
  backends/llvm/ir_for_tuple_call.hpp
  backends/llvm/ir_for_tuple_call.cpp
  backends/llvm/compile.hpp
  backends/llvm/compile.cpp
  backends/llvm/fgraph_ir_generation.hpp
  backends/llvm/fgraph_ir_generation.cpp
  backends/llvm/builder.hpp
  backends/llvm/builder.cpp
  backends/llvm/context_pool.hpp
  backends/llvm/context_pool.cpp

  types/initialization.hpp
  types/initialization.cpp
  types/lists.hpp
  types/lists.cpp
  types/numeric.hpp
  types/numeric.cpp
  types/boolean.hpp
  types/boolean.cpp
  types/external.hpp
  types/external.cpp
  types/types-c.h
  types/types-c.cpp
  types/tuple_access-c.h
  types/tuple_access-c.cpp

  functions/object_input.hpp
  functions/object_input.cpp
  functions/random.hpp
  functions/random.cpp
  functions/scalar_math.hpp
  functions/scalar_math.cpp
  functions/vectors.hpp
  functions/vectors.cpp
  functions/lists.hpp
  functions/lists.cpp
  functions/simple_conversions.hpp
  functions/simple_conversions.cpp
  functions/switch.hpp
  functions/switch.cpp
  functions/auto_vectorization.hpp
  functions/auto_vectorization.cpp
  functions/array_execution.hpp
  functions/array_execution.cpp
  functions/ranges.hpp
  functions/ranges.cpp
  functions/comparisons.hpp
  functions/comparisons.cpp
  functions/constants.hpp
  functions/constants.cpp
  functions/color.hpp
  functions/color.cpp

  frontends/data_flow_nodes/vtree_data_graph_builder.hpp
  frontends/data_flow_nodes/vtree_data_graph_builder.cpp
  frontends/data_flow_nodes/function_generation.hpp
  frontends/data_flow_nodes/function_generation.cpp
  frontends/data_flow_nodes/graph_generation.hpp
  frontends/data_flow_nodes/graph_generation.cpp
  frontends/data_flow_nodes/vtree_data_graph.hpp
  frontends/data_flow_nodes/vtree_data_graph.cpp
  frontends/data_flow_nodes/unlinked_input_inserters.hpp
  frontends/data_flow_nodes/unlinked_input_inserters.cpp
  frontends/data_flow_nodes/unlinked_input_groupers.hpp
  frontends/data_flow_nodes/unlinked_input_groupers.cpp
  frontends/data_flow_nodes/mappings.hpp
  frontends/data_flow_nodes/mappings.cpp
  frontends/data_flow_nodes/mappings/registry.hpp
  frontends/data_flow_nodes/mappings/node_inserters.cpp
  frontends/data_flow_nodes/mappings/socket_loaders.cpp
  frontends/data_flow_nodes/mappings/conversion_inserters.cpp
  frontends/data_flow_nodes/mappings/type_mappings.cpp
  frontends/data_flow_nodes/data_flow_nodes-c.h
  frontends/data_flow_nodes/data_flow_nodes-c.cpp
)

set(LIB
  bf_blenlib
)

blender_add_lib(bf_functions "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
