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
	core/cpu.hpp

	FN_functions.hpp

	types/numeric.hpp
	types/types.hpp
)

blender_add_lib(bf_functions "${SRC}" "${INC}" "${INC_SYS}")
