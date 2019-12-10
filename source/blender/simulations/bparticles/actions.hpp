#pragma once

#include "action_interface.hpp"
#include "particle_function.hpp"

namespace BParticles {

using FN::CPPType;

class NoneAction : public Action {
  void execute(ActionInterface &UNUSED(interface)) override;
};

class ActionSequence : public Action {
 private:
  Vector<Action *> m_actions;

 public:
  ActionSequence(Vector<Action *> actions) : m_actions(std::move(actions))
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
  Action &m_on_birth_action;

 public:
  ExplodeAction(ArrayRef<std::string> systems_to_emit,
                ParticleFunction *inputs_fn,
                Action &on_birth_action)
      : m_systems_to_emit(systems_to_emit),
        m_inputs_fn(inputs_fn),
        m_on_birth_action(on_birth_action)
  {
  }

  void execute(ActionInterface &interface) override;
};

class ConditionAction : public Action {
 private:
  ParticleFunction *m_inputs_fn;
  Action &m_true_action;
  Action &m_false_action;

 public:
  ConditionAction(ParticleFunction *inputs_fn, Action &true_action, Action &false_action)
      : m_inputs_fn(inputs_fn), m_true_action(true_action), m_false_action(false_action)
  {
  }

  void execute(ActionInterface &interface) override;
};

class AddToGroupAction : public Action {
 private:
  std::string m_group_name;

 public:
  AddToGroupAction(std::string group_name) : m_group_name(std::move(group_name))
  {
  }

  void execute(ActionInterface &interface) override;
};

class RemoveFromGroupAction : public Action {
 private:
  std::string m_group_name;

 public:
  RemoveFromGroupAction(std::string group_name) : m_group_name(std::move(group_name))
  {
  }

  void execute(ActionInterface &interface) override;
};

class SetAttributeAction : public Action {
 private:
  std::string m_attribute_name;
  const CPPType &m_attribute_type;
  ParticleFunction &m_inputs_fn;

 public:
  SetAttributeAction(std::string attribute_name,
                     const CPPType &attribute_type,
                     ParticleFunction &inputs_fn)
      : m_attribute_name(std::move(attribute_name)),
        m_attribute_type(attribute_type),
        m_inputs_fn(inputs_fn)
  {
  }

  void execute(ActionInterface &interface) override;
};

}  // namespace BParticles
