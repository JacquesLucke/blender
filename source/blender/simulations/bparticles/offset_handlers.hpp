#pragma once

#include "offset_handler_interface.hpp"
#include "particle_function.hpp"

namespace BParticles {

class CreateTrailHandler : public OffsetHandler {
 private:
  ArrayRef<std::string> m_systems_to_emit;
  const ParticleFunction &m_inputs_fn;
  ParticleAction &m_on_birth_action;

 public:
  CreateTrailHandler(ArrayRef<std::string> systems_to_emit,
                     const ParticleFunction &inputs_fn,
                     ParticleAction &on_birth_action)
      : m_systems_to_emit(systems_to_emit),
        m_inputs_fn(inputs_fn),
        m_on_birth_action(on_birth_action)
  {
  }

  void execute(OffsetHandlerInterface &interface) override;
};

class SizeOverTimeHandler : public OffsetHandler {
 private:
  const ParticleFunction &m_inputs_fn;

 public:
  SizeOverTimeHandler(const ParticleFunction &inputs_fn) : m_inputs_fn(inputs_fn)
  {
  }

  void execute(OffsetHandlerInterface &interface) override;
};

class AlwaysExecuteHandler : public OffsetHandler {
 private:
  ParticleAction &m_action;

 public:
  AlwaysExecuteHandler(ParticleAction &action) : m_action(action)
  {
  }

  void execute(OffsetHandlerInterface &interface) override;
};

}  // namespace BParticles
