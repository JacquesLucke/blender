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
  ParticleFunction *m_inputs_fn;

 public:
  SetVelocityAction(ParticleFunction *inputs_fn) : m_inputs_fn(inputs_fn)
  {
  }

  void execute(ActionInterface &interface) override;
};

class RandomizeVelocityAction : public Action {
 private:
  ParticleFunction *m_inputs_fn;

 public:
  RandomizeVelocityAction(ParticleFunction *inputs_fn) : m_inputs_fn(inputs_fn)
  {
  }

  void execute(ActionInterface &interface) override;
};

class ChangeColorAction : public Action {
 private:
  ParticleFunction *m_inputs_fn;

 public:
  ChangeColorAction(ParticleFunction *inputs_fn) : m_inputs_fn(inputs_fn)
  {
  }

  void execute(ActionInterface &interface) override;
};

class ChangeSizeAction : public Action {
 private:
  ParticleFunction *m_inputs_fn;

 public:
  ChangeSizeAction(ParticleFunction *inputs_fn) : m_inputs_fn(inputs_fn)
  {
  }

  void execute(ActionInterface &interface) override;
};

class ChangePositionAction : public Action {
 private:
  ParticleFunction *m_inputs_fn;

 public:
  ChangePositionAction(ParticleFunction *inputs_fn) : m_inputs_fn(inputs_fn)
  {
  }

  void execute(ActionInterface &interface) override;
};

class ExplodeAction : public Action {
 private:
  ArrayRef<std::string> m_systems_to_emit;
  ParticleFunction *m_inputs_fn;
  std::unique_ptr<Action> m_on_birth_action;

 public:
  ExplodeAction(ArrayRef<std::string> systems_to_emit,
                ParticleFunction *inputs_fn,
                std::unique_ptr<Action> on_birth_action)
      : m_systems_to_emit(systems_to_emit),
        m_inputs_fn(inputs_fn),
        m_on_birth_action(std::move(on_birth_action))
  {
  }

  void execute(ActionInterface &interface) override;
};

class ConditionAction : public Action {
 private:
  ParticleFunction *m_inputs_fn;
  std::unique_ptr<Action> m_true_action, m_false_action;

 public:
  ConditionAction(ParticleFunction *inputs_fn,
                  std::unique_ptr<Action> true_action,
                  std::unique_ptr<Action> false_action)
      : m_inputs_fn(inputs_fn),
        m_true_action(std::move(true_action)),
        m_false_action(std::move(false_action))
  {
  }

  void execute(ActionInterface &interface) override;
};

}  // namespace BParticles
