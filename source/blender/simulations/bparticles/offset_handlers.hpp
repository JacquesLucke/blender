#pragma once

#include "particle_function.hpp"
#include "offset_handler_interface.hpp"

namespace BParticles {

class CreateTrailHandler : public OffsetHandler {
 private:
  Vector<std::string> m_types_to_emit;
  ParticleFunction *m_inputs_fn;
  std::unique_ptr<Action> m_on_birth_action;

 public:
  CreateTrailHandler(Vector<std::string> types_to_emit,
                     ParticleFunction *inputs_fn,
                     std::unique_ptr<Action> on_birth_action)
      : m_types_to_emit(std::move(types_to_emit)),
        m_inputs_fn(inputs_fn),
        m_on_birth_action(std::move(on_birth_action))
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
  std::unique_ptr<Action> m_action;

 public:
  AlwaysExecuteHandler(std::unique_ptr<Action> action) : m_action(std::move(action))
  {
  }

  void execute(OffsetHandlerInterface &interface) override;
};

}  // namespace BParticles
