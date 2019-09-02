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
  bparticles/action_contexts.hpp
  bparticles/events.hpp
  bparticles/events.cpp
  bparticles/attributes.hpp
  bparticles/attributes.cpp
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
  bparticles/particle_set.hpp
  bparticles/particle_set.cpp
  bparticles/particles_state.hpp
  bparticles/particles_state.cpp
  bparticles/particle_allocator.hpp
  bparticles/particle_allocator.cpp
  bparticles/offset_handlers.hpp
  bparticles/offset_handlers.cpp
  bparticles/particle_function.hpp
  bparticles/particle_function.cpp
  bparticles/force_interface.hpp
  bparticles/force_interface.cpp
  bparticles/particle_function_builder.hpp
  bparticles/particle_function_builder.cpp
)

set(LIB
  bf_blenlib
  bf_blenkernel
)

blender_add_lib(bf_simulations "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
