set(INC
	.
	../blenlib
	../makesdna
	../makesrna
	../blenkernel
	../depsgraph
	../windowmanager
)

set(INC_SYS
	${LLVM_INCLUDE_DIRS}
)

set(SRC
	FN_all.hpp
	FN_core.hpp
	FN_types.hpp
	FN_functions.hpp
	FN_data_flow_nodes.hpp
	FN-C.h

	c_wrapper.cpp

	core/type.hpp
	core/parameter.hpp
	core/parameter.cpp
	core/signature.hpp
	core/signature.cpp
	core/function.hpp
	core/function.cpp
	core/data_flow_graph.hpp
	core/data_flow_graph.cpp
	core/dot_export.cpp

	backends/tuple_call/cpp_types.hpp
	backends/tuple_call/cpp_types.cpp
	backends/tuple_call/tuple_call.hpp
	backends/tuple_call/tuple_call.cpp
	backends/tuple_call/fgraph_tuple_call.hpp
	backends/tuple_call/fgraph_tuple_call.cpp
	backends/tuple_call/tuple.hpp
	backends/tuple_call/tuple.cpp
	backends/tuple_call/lazy_to_normal.hpp
	backends/tuple_call/lazy_to_normal.cpp
	backends/tuple_call/execution_context.hpp
	backends/tuple_call/execution_context.cpp

	backends/dependencies/dependencies.hpp
	backends/dependencies/dependencies.cpp
	backends/dependencies/fgraph_dependencies.hpp
	backends/dependencies/fgraph_dependencies.cpp

	backends/llvm/initialize.hpp
	backends/llvm/initialize.cpp
	backends/llvm/llvm_types.hpp
	backends/llvm/llvm_types.cpp
	backends/llvm/build_ir_body.hpp
	backends/llvm/build_ir_body.cpp
	backends/llvm/ir_to_tuple_call.hpp
	backends/llvm/ir_to_tuple_call.cpp
	backends/llvm/ir_for_tuple_call.hpp
	backends/llvm/ir_for_tuple_call.cpp
	backends/llvm/compiled_body.hpp
	backends/llvm/compiled_body.cpp
	backends/llvm/compile.hpp
	backends/llvm/compile.cpp
	backends/llvm/ir_utils.hpp
	backends/llvm/ir_utils.cpp
	backends/llvm/fgraph_ir_generation.hpp
	backends/llvm/fgraph_ir_generation.cpp

	types/lists.hpp
	types/numeric_lists.hpp
	types/numeric_lists.cpp
	types/numeric.hpp
	types/numeric.cpp
	types/boolean.hpp
	types/boolean.cpp

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

	frontends/data_flow_nodes/builder.hpp
	frontends/data_flow_nodes/builder.cpp
	frontends/data_flow_nodes/function_generation.hpp
	frontends/data_flow_nodes/function_generation.cpp
	frontends/data_flow_nodes/graph_generation.hpp
	frontends/data_flow_nodes/graph_generation.cpp
	frontends/data_flow_nodes/inserters.hpp
	frontends/data_flow_nodes/inserters.cpp
	frontends/data_flow_nodes/registry.hpp
	frontends/data_flow_nodes/inserters/nodes.cpp
	frontends/data_flow_nodes/inserters/sockets.cpp
	frontends/data_flow_nodes/inserters/conversions.cpp
	frontends/data_flow_nodes/util_wrappers.hpp
)

blender_add_lib(bf_functions "${SRC}" "${INC}" "${INC_SYS}")
