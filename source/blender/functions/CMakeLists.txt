set(INC
	.
	../blenlib
)

set(INC_SYS
)

set(SRC
	intern/c_wrapper.cpp
	intern/function.cpp

	FN_functions.h
	FN_functions.hpp

	types/numeric.hpp
	types/types.hpp
)

blender_add_lib(bf_functions "${SRC}" "${INC}" "${INC_SYS}")
