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
  BParticles.h

  bparticles/core.hpp
  bparticles/core.cpp
  bparticles/particles_container.hpp
  bparticles/particles_container.cpp
  bparticles/playground_solver.hpp
  bparticles/playground_solver.cpp
  bparticles/emitter.hpp
  bparticles/emitter.cpp
  bparticles/c_wrapper.cpp
)

set(LIB
  bf_blenlib
)

blender_add_lib(bf_simulations "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
