set(INC
  .
  ../blenlib
  ../makesdna
  ../makesrna
  ../blenkernel
  ../depsgraph
  ../functions
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
  bparticles/simulate.hpp
  bparticles/simulate.cpp
  bparticles/emitters.hpp
  bparticles/emitters.cpp
  bparticles/forces.hpp
  bparticles/forces.cpp
  bparticles/actions.hpp
  bparticles/actions.cpp
  bparticles/action_interface.hpp
  bparticles/action_interface.cpp
  bparticles/events.hpp
  bparticles/events.cpp
  bparticles/attributes.hpp
  bparticles/attributes.cpp
  bparticles/c_wrapper.cpp
)

set(LIB
  bf_blenlib
  bf_blenkernel
)

blender_add_lib(bf_simulations "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
