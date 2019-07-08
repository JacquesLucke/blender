#pragma once

#include "action_interface.hpp"

namespace BParticles {

std::unique_ptr<Action> ACTION_none();
std::unique_ptr<Action> ACTION_change_direction(ParticleFunction &compute_inputs,
                                                std::unique_ptr<Action> post_action);
std::unique_ptr<Action> ACTION_kill();
std::unique_ptr<Action> ACTION_explode(StringRef new_particle_name,
                                       ParticleFunction &compute_inputs,
                                       std::unique_ptr<Action> post_action);
std::unique_ptr<Action> ACTION_condition(ParticleFunction &compute_inputs,
                                         std::unique_ptr<Action> true_action,
                                         std::unique_ptr<Action> false_action);

}  // namespace BParticles
