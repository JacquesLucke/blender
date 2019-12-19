#pragma once

#include "particle_action.hpp"
#include "particle_function.hpp"

namespace BParticles {

using FN::CPPType;

class ActionSequence : public ParticleAction {
 private:
  Vector<ParticleAction *> m_actions;

 public:
  ActionSequence(Vector<ParticleAction *> actions) : m_actions(std::move(actions))
  {
  }

  void execute(ParticleActionContext &context) override;
};

class ConditionAction : public ParticleAction {
 private:
  ParticleFunction *m_inputs_fn;
  ParticleAction &m_true_action;
  ParticleAction &m_false_action;

 public:
  ConditionAction(ParticleFunction *inputs_fn,
                  ParticleAction &true_action,
                  ParticleAction &false_action)
      : m_inputs_fn(inputs_fn), m_true_action(true_action), m_false_action(false_action)
  {
  }

  void execute(ParticleActionContext &context) override;
};

class SetAttributeAction : public ParticleAction {
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

  void execute(ParticleActionContext &context) override;
};

class SpawnParticlesAction : public ParticleAction {
 private:
  ArrayRef<std::string> m_systems_to_emit;
  const ParticleFunction &m_spawn_function;
  Vector<std::string> m_attribute_names;
  ParticleAction &m_action;

 public:
  SpawnParticlesAction(ArrayRef<std::string> systems_to_emit,
                       const ParticleFunction &spawn_function,
                       Vector<std::string> attribute_names,
                       ParticleAction &action)
      : m_systems_to_emit(systems_to_emit),
        m_spawn_function(spawn_function),
        m_attribute_names(std::move(attribute_names)),
        m_action(action)
  {
  }

  void execute(ParticleActionContext &context) override;
};

}  // namespace BParticles
