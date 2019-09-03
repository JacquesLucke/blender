#pragma once

#include "action_interface.hpp"
#include "particle_function.hpp"

namespace BParticles {

class NoneAction : public Action {
  void execute(ActionInterface &UNUSED(interface)) override;
};

class ActionSequence : public Action {
 private:
  Vector<std::unique_ptr<Action>> m_actions;

 public:
  ActionSequence(Vector<std::unique_ptr<Action>> actions) : m_actions(std::move(actions))
  {
  }

  void execute(ActionInterface &interface) override;
};

class KillAction : public Action {
  void execute(ActionInterface &interface) override;
};

class SetVelocityAction : public Action {
 private:
  std::unique_ptr<ParticleFunction> m_compute_inputs;

 public:
  SetVelocityAction(std::unique_ptr<ParticleFunction> compute_inputs)
      : m_compute_inputs(std::move(compute_inputs))
  {
  }

  void execute(ActionInterface &interface) override;
};

class RandomizeVelocityAction : public Action {
 private:
  std::unique_ptr<ParticleFunction> m_compute_inputs;

 public:
  RandomizeVelocityAction(std::unique_ptr<ParticleFunction> compute_inputs)
      : m_compute_inputs(std::move(compute_inputs))
  {
  }

  void execute(ActionInterface &interface) override;
};

class ChangeColorAction : public Action {
 private:
  std::unique_ptr<ParticleFunction> m_compute_inputs;

 public:
  ChangeColorAction(std::unique_ptr<ParticleFunction> compute_inputs)
      : m_compute_inputs(std::move(compute_inputs))
  {
  }

  void execute(ActionInterface &interface) override;
};

class ChangeSizeAction : public Action {
 private:
  std::unique_ptr<ParticleFunction> m_compute_inputs;

 public:
  ChangeSizeAction(std::unique_ptr<ParticleFunction> compute_inputs)
      : m_compute_inputs(std::move(compute_inputs))
  {
  }

  void execute(ActionInterface &interface) override;
};

class ExplodeAction : public Action {
 private:
  Vector<std::string> m_types_to_emit;
  std::unique_ptr<ParticleFunction> m_compute_inputs;
  std::unique_ptr<Action> m_on_birth_action;

 public:
  ExplodeAction(Vector<std::string> types_to_emit,
                std::unique_ptr<ParticleFunction> compute_inputs,
                std::unique_ptr<Action> on_birth_action)
      : m_types_to_emit(std::move(types_to_emit)),
        m_compute_inputs(std::move(compute_inputs)),
        m_on_birth_action(std::move(on_birth_action))
  {
  }

  void execute(ActionInterface &interface) override;
};

class ConditionAction : public Action {
 private:
  std::unique_ptr<ParticleFunction> m_compute_inputs;
  std::unique_ptr<Action> m_true_action, m_false_action;

 public:
  ConditionAction(std::unique_ptr<ParticleFunction> compute_inputs,
                  std::unique_ptr<Action> true_action,
                  std::unique_ptr<Action> false_action)
      : m_compute_inputs(std::move(compute_inputs)),
        m_true_action(std::move(true_action)),
        m_false_action(std::move(false_action))
  {
  }

  void execute(ActionInterface &interface) override;
};

}  // namespace BParticles
