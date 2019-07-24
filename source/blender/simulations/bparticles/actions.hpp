#pragma once

#include "action_interface.hpp"
#include "particle_function.hpp"

namespace BParticles {

class NoneAction : public Action {
  void execute(ActionInterface &UNUSED(interface)) override;
};

class KillAction : public Action {
  void execute(ActionInterface &interface) override;
};

class ChangeDirectionAction : public Action {
 private:
  ParticleFunction m_compute_inputs;
  std::unique_ptr<Action> m_post_action;

 public:
  ChangeDirectionAction(ParticleFunction &compute_inputs, std::unique_ptr<Action> post_action)
      : m_compute_inputs(compute_inputs), m_post_action(std::move(post_action))
  {
  }

  void execute(ActionInterface &interface) override;
};

class ExplodeAction : public Action {
 private:
  std::string m_new_particle_name;
  ParticleFunction m_compute_inputs;
  std::unique_ptr<Action> m_post_action;

 public:
  ExplodeAction(StringRef new_particle_name,
                ParticleFunction &compute_inputs,
                std::unique_ptr<Action> post_action)
      : m_new_particle_name(new_particle_name.to_std_string()),
        m_compute_inputs(compute_inputs),
        m_post_action(std::move(post_action))
  {
  }

  void execute(ActionInterface &interface) override;
};

class ConditionAction : public Action {
 private:
  ParticleFunction m_compute_inputs;
  std::unique_ptr<Action> m_true_action, m_false_action;

 public:
  ConditionAction(ParticleFunction &compute_inputs,
                  std::unique_ptr<Action> true_action,
                  std::unique_ptr<Action> false_action)
      : m_compute_inputs(compute_inputs),
        m_true_action(std::move(true_action)),
        m_false_action(std::move(false_action))
  {
  }

  void execute(ActionInterface &interface) override;
};

}  // namespace BParticles
