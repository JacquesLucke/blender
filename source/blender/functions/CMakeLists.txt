set(INC
  .
  ../blenlib
)

set(INC_SYS
)

set(SRC
  intern/core_cpp_types.cc
)

set(LIB
  bf_blenlib
)

blender_add_lib(bf_functions "${SRC}" "${INC}", "${INC_SYS}" "${LIB}")
