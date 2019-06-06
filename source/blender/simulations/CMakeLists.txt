set(INC
  .
  ../blenlib
  ../makesdna
  ../makesrna
  ../blenkernel
  ../depsgraph
  ../../../intern/guardedalloc
)

set(INC_SYS
)


set(SRC
   playground.cpp
)

set(LIB
  bf_blenlib
)

blender_add_lib(bf_simulations "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
