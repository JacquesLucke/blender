#ifndef __NOD_SIMULATION_H__
#define __NOD_SIMULATION_H__

#ifdef __cplusplus
extern "C" {
#endif

extern struct bNodeTreeType *ntreeType_Simulation;

void register_node_tree_type_sim(void);

void register_node_type_sim_group(void);

void register_node_type_sim_particle_simulation(void);
void register_node_type_sim_custom_force(void);
void register_node_type_set_particle_attribute(void);
void register_node_type_sim_particle_birth_event(void);

#ifdef __cplusplus
}
#endif

#endif /* __NOD_SIMULATION_H__ */
