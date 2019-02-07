set(INC
	.
	../blenlib
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

	FN_functions.hpp

	types/numeric.cpp
	types/numeric.hpp
	types/types.hpp
)

blender_add_lib(bf_functions "${SRC}" "${INC}" "${INC_SYS}")
