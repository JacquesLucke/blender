set(INC
	.
	../blenlib
	../makesdna
	../makesrna
	../blenkernel
)

set(INC_SYS
)

set(SRC
	FN_functions.h
	c_wrapper.cpp

	core/core.hpp
	core/data_flow_graph.hpp
	core/cpu.hpp
	core/cpu.cpp
	core/data_flow_graph.cpp
	core/dot_export.cpp
	core/graph_to_function.hpp
	core/graph_to_function.cpp

	FN_functions.hpp

	types/numeric.cpp
	types/numeric.hpp
	types/types.hpp

	function_nodes/function_nodes.hpp
	function_nodes/function_nodes.cpp
)

blender_add_lib(bf_functions "${SRC}" "${INC}" "${INC_SYS}")
