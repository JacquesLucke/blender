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

	FN_all.hpp
	FN_core.hpp
	FN_types.hpp

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

	nodes/nodes.hpp
	nodes/nodes.cpp
	nodes/graph_generation.hpp
	nodes/graph_generation.cpp
	nodes/socket_inputs.cpp
	nodes/test_nodes.cpp
)

blender_add_lib(bf_functions "${SRC}" "${INC}" "${INC_SYS}")
