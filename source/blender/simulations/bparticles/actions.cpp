#include "actions.hpp"

namespace BParticles {

class KillAction : public Action {
  void execute(ActionInterface &interface) override
  {
    ParticleSet &particles = interface.particles();

    auto kill_states = particles.attributes().get_byte("Kill State");
    for (uint pindex : particles.indices()) {
      kill_states[pindex] = 1;
    }
  }
};

class MoveAction : public BParticles::Action {
 private:
  float3 m_offset;

 public:
  MoveAction(float3 offset) : m_offset(offset)
  {
  }

  void execute(ActionInterface &interface) override
  {
    ParticleSet &particles = interface.particles();

    auto positions = particles.attributes().get_float3("Position");
    for (uint pindex : particles.indices()) {
      positions[pindex] += m_offset;
    }
  }
};

std::unique_ptr<Action> ACTION_kill()
{
  Action *action = new KillAction();
  return std::unique_ptr<Action>(action);
}

std::unique_ptr<Action> ACTION_move(float3 offset)
{
  Action *action = new MoveAction(offset);
  return std::unique_ptr<Action>(action);
}

}  // namespace BParticles
