#pragma once

#include "action_interface.hpp"

namespace BParticles {

Action *ACTION_none();
Action *ACTION_change_direction(ParticleFunction &compute_inputs, Action *post_action);
Action *ACTION_kill();
Action *ACTION_explode(StringRef new_particle_name,
                       ParticleFunction &compute_inputs,
                       Action *post_action);
Action *ACTION_condition(ParticleFunction &compute_inputs,
                         Action *true_action,
                         Action *false_action);

}  // namespace BParticles
