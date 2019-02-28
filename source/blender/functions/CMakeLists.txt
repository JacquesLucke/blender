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
	core/cpu.hpp
	core/cpu.cpp
	core/data_flow_graph.hpp
	core/data_flow_graph.cpp
	core/dependencies.hpp
	core/dependencies.cpp
	core/dot_export.cpp
	core/graph_to_function.hpp
	core/graph_to_function.cpp
	core/type_inferencing.hpp
	core/type_inferencing.cpp
	core/type_relations.hpp
	core/type_relations.cpp

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

	frontends/data_flow_nodes/nodes.hpp
	frontends/data_flow_nodes/nodes.cpp
	frontends/data_flow_nodes/graph_generation.hpp
	frontends/data_flow_nodes/graph_generation.cpp
	frontends/data_flow_nodes/socket_inputs.cpp
	frontends/data_flow_nodes/test_nodes.cpp
)

blender_add_lib(bf_functions "${SRC}" "${INC}" "${INC_SYS}")
