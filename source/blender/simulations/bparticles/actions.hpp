#pragma once

#include "action_interface.hpp"

namespace BParticles {

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

std::unique_ptr<Action> ACTION_none();
std::unique_ptr<Action> ACTION_kill();
std::unique_ptr<Action> ACTION_explode(StringRef new_particle_name,
                                       ParticleFunction &compute_inputs,
                                       std::unique_ptr<Action> post_action);
std::unique_ptr<Action> ACTION_condition(ParticleFunction &compute_inputs,
                                         std::unique_ptr<Action> true_action,
                                         std::unique_ptr<Action> false_action);

}  // namespace BParticles
