set(INC
	.
	../blenlib
	../makesdna
	../makesrna
	../blenkernel
	../depsgraph
)

set(INC_SYS
)

set(SRC
	FN_all.hpp
	FN_core.hpp
	FN_types.hpp
	FN_functions.hpp
	FN_data_flow_nodes.hpp
	FN-C.h

	c_wrapper.cpp

	core/core.hpp
	core/core.cpp
	core/data_flow_graph.hpp
	core/data_flow_graph.cpp
	core/dot_export.cpp
	core/type_inferencing.hpp
	core/type_inferencing.cpp
	core/type_relations.hpp
	core/type_relations.cpp

	backends/tuple_call/cpp_types.hpp
	backends/tuple_call/cpp_types.cpp
	backends/tuple_call/tuple_call.hpp
	backends/tuple_call/tuple_call.cpp
	backends/tuple_call/fgraph_to_tuple_call.hpp
	backends/tuple_call/fgraph_to_tuple_call.cpp
	backends/tuple_call/tuple.hpp

	backends/dependencies/dependencies.hpp
	backends/dependencies/dependencies.cpp
	backends/dependencies/fgraph_dependencies.hpp
	backends/dependencies/fgraph_dependencies.cpp

	types/numeric.cpp
	types/numeric.hpp

	functions/object_input.hpp
	functions/object_input.cpp
	functions/random.hpp
	functions/random.cpp
	functions/scalar_math.hpp
	functions/scalar_math.cpp
	functions/vectors.hpp
	functions/vectors.cpp
	functions/socket_input.hpp
	functions/socket_input.cpp

	frontends/data_flow_nodes/builder.hpp
	frontends/data_flow_nodes/builder.cpp
	frontends/data_flow_nodes/function_generation.hpp
	frontends/data_flow_nodes/function_generation.cpp
	frontends/data_flow_nodes/graph_generation.hpp
	frontends/data_flow_nodes/graph_generation.cpp
	frontends/data_flow_nodes/inserters.hpp
	frontends/data_flow_nodes/inserters.cpp
	frontends/data_flow_nodes/registry.hpp
	frontends/data_flow_nodes/test_nodes.cpp
	frontends/data_flow_nodes/test_sockets.cpp
	frontends/data_flow_nodes/util_wrappers.hpp
)

blender_add_lib(bf_functions "${SRC}" "${INC}" "${INC_SYS}")
