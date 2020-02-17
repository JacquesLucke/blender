#ifndef __SIM_NODE_TREE_H__
#define __SIM_NODE_TREE_H__

#ifdef __cplusplus
extern "C" {
#endif

extern struct bNodeTreeType *ntreeType_Simulation;

void register_node_tree_type_sim(void);
void register_node_type_my_test_node(void);
void init_socket_data_types(void);
void free_socket_data_types(void);

#ifdef __cplusplus
}
#endif

#endif /* __SIM_NODE_TREE_H__ */
