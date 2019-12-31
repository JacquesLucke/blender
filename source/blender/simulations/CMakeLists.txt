set(INC
  .
  ../blenlib
  ../makesdna
  ../makesrna
  ../blenkernel
  ../depsgraph
  ../functions
  ../imbuf
  ../../../intern/guardedalloc
)

set(INC_SYS
  ${LLVM_INCLUDE_DIRS}
)


set(SRC
  BParticles.h

  bparticles/simulate.hpp
  bparticles/simulate.cpp
  bparticles/emitters.hpp
  bparticles/emitters.cpp
  bparticles/forces.hpp
  bparticles/forces.cpp
  bparticles/actions.hpp
  bparticles/actions.cpp
  bparticles/particle_action.hpp
  bparticles/particle_action.cpp
  bparticles/events.hpp
  bparticles/events.cpp
  bparticles/emitter_interface.hpp
  bparticles/block_step_data.hpp
  bparticles/event_interface.hpp
  bparticles/integrator_interface.hpp
  bparticles/offset_handler_interface.hpp
  bparticles/c_wrapper.cpp
  bparticles/step_simulator.hpp
  bparticles/simulation_state.hpp
  bparticles/world_state.hpp
  bparticles/integrator.hpp
  bparticles/integrator.cpp
  bparticles/node_frontend.hpp
  bparticles/node_frontend.cpp
  bparticles/particles_state.hpp
  bparticles/particles_state.cpp
  bparticles/particle_allocator.hpp
  bparticles/particle_allocator.cpp
  bparticles/offset_handlers.hpp
  bparticles/offset_handlers.cpp
  bparticles/particle_function.hpp
  bparticles/particle_function.cpp
  bparticles/particle_set.hpp
  bparticles/particle_set.cpp
  bparticles/force_interface.hpp
  bparticles/force_interface.cpp
)

set(LIB
  bf_blenlib
  bf_blenkernel
)

if(WITH_TBB)
  add_definitions(-DWITH_TBB)

  list(APPEND INC_SYS
    ${TBB_INCLUDE_DIRS}
  )
endif()

blender_add_lib(bf_simulations "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
