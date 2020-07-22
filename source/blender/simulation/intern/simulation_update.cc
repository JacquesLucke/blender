/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "SIM_simulation_update.hh"

#include "BKE_customdata.h"
#include "BKE_simulation.h"

#include "DNA_scene_types.h"
#include "DNA_simulation_types.h"

#include "DEG_depsgraph_query.h"

#include "BLI_array.hh"
#include "BLI_float3.hh"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_rand.h"
#include "BLI_vector.hh"

#include "particle_function.hh"
#include "simulation_collect_influences.hh"
#include "simulation_solver.hh"

namespace blender::sim {

static void copy_states_to_cow(const Simulation *simulation_orig, Simulation *simulation_cow)
{
  BKE_simulation_state_remove_all(simulation_cow);
  simulation_cow->current_frame = simulation_orig->current_frame;

  LISTBASE_FOREACH (const SimulationState *, state_orig, &simulation_orig->states) {
    SimulationState *state_cow = BKE_simulation_state_add(
        simulation_cow, state_orig->type, state_orig->name);
    BKE_simulation_state_copy_data(state_orig, state_cow);
  }
}

static void remove_unused_states(Simulation *simulation, const VectorSet<std::string> &state_names)
{
  LISTBASE_FOREACH_MUTABLE (SimulationState *, state, &simulation->states) {
    if (!state_names.contains(state->name)) {
      BKE_simulation_state_remove(simulation, state);
    }
  }
}

static void add_missing_particle_states(Simulation *simulation, Span<std::string> state_names)
{
  for (StringRefNull name : state_names) {
    SimulationState *state = BKE_simulation_state_try_find_by_name(simulation, name.c_str());
    if (state != nullptr) {
      continue;
    }

    BKE_simulation_state_add(simulation, SIM_TYPE_NAME_PARTICLE_SIMULATION, name.c_str());
  }
}

static void reinitialize_empty_simulation_states(Simulation *simulation,
                                                 const SimulationStatesInfo &states_info)
{
  remove_unused_states(simulation, states_info.particle_simulation_names);
  BKE_simulation_state_reset_all(simulation);
  add_missing_particle_states(simulation, states_info.particle_simulation_names);
}

static void update_simulation_state_list(Simulation *simulation,
                                         const SimulationStatesInfo &states_info)
{
  remove_unused_states(simulation, states_info.particle_simulation_names);
  add_missing_particle_states(simulation, states_info.particle_simulation_names);
}

void update_simulation_in_depsgraph(Depsgraph *depsgraph,
                                    Scene *scene_cow,
                                    Simulation *simulation_cow)
{
  int current_frame = scene_cow->r.cfra;
  if (simulation_cow->current_frame == current_frame) {
    return;
  }

  /* Below we modify the original state/cache. Only the active depsgraph is allowed to do that. */
  if (!DEG_is_active(depsgraph)) {
    return;
  }

  Simulation *simulation_orig = (Simulation *)DEG_get_original_id(&simulation_cow->id);

  ResourceCollector resources;
  SimulationInfluences influences;
  SimulationStatesInfo states_info;

  /* TODO: Use simulation_cow, but need to add depsgraph relations before that. */
  collect_simulation_influences(*simulation_orig, resources, influences, states_info);

  if (current_frame == 1) {
    reinitialize_empty_simulation_states(simulation_orig, states_info);

    initialize_simulation_states(*simulation_orig, *depsgraph, influences);
    simulation_orig->current_frame = 1;

    copy_states_to_cow(simulation_orig, simulation_cow);
  }
  else if (current_frame == simulation_orig->current_frame + 1) {
    update_simulation_state_list(simulation_orig, states_info);

    float time_step = 1.0f / 24.0f;
    solve_simulation_time_step(*simulation_orig, *depsgraph, influences, time_step);
    simulation_orig->current_frame = current_frame;

    copy_states_to_cow(simulation_orig, simulation_cow);
  }
}

}  // namespace blender::sim
