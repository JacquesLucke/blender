#pragma once

#include "particle_function.hpp"
#include "offset_handler_interface.hpp"

namespace BParticles {

class CreateTrailHandler : public OffsetHandler {
 private:
  ArrayRef<std::string> m_systems_to_emit;
  ParticleFunction *m_inputs_fn;
  Action &m_on_birth_action;

 public:
  CreateTrailHandler(ArrayRef<std::string> systems_to_emit,
                     ParticleFunction *inputs_fn,
                     Action &on_birth_action)
      : m_systems_to_emit(systems_to_emit),
        m_inputs_fn(inputs_fn),
        m_on_birth_action(on_birth_action)
  {
  }

  void execute(OffsetHandlerInterface &interface) override;
};

class SizeOverTimeHandler : public OffsetHandler {
 private:
  ParticleFunction *m_inputs_fn;

 public:
  SizeOverTimeHandler(ParticleFunction *inputs_fn) : m_inputs_fn(inputs_fn)
  {
  }

  void execute(OffsetHandlerInterface &interface) override;
};

class AlwaysExecuteHandler : public OffsetHandler {
 private:
  Action &m_action;

 public:
  AlwaysExecuteHandler(Action &action) : m_action(action)
  {
  }

  void execute(OffsetHandlerInterface &interface) override;
};

}  // namespace BParticles
