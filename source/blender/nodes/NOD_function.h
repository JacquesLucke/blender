#ifndef __NOD_FUNCTION_H__
#define __NOD_FUNCTION_H__

#ifdef __cplusplus
extern "C" {
#endif

void register_node_type_fn_combine_xyz(void);
void register_node_type_fn_separate_xyz(void);
void register_node_type_fn_combine_rgb(void);
void register_node_type_fn_separate_rgb(void);
void register_node_type_fn_combine_hsv(void);
void register_node_type_fn_separate_hsv(void);
void register_node_type_fn_float_math(void);
void register_node_type_fn_vector_math(void);

#ifdef __cplusplus
}
#endif

#endif /* __NOD_FUNCTION_H__ */
