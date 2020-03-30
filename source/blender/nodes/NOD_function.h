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
void register_node_type_fn_closest_surface(void);
void register_node_type_fn_surface_color(void);
void register_node_type_fn_surface_normal(void);
void register_node_type_fn_surface_position(void);
void register_node_type_fn_surface_weight(void);
void register_node_type_fn_boolean_math(void);
void register_node_type_fn_float_compare(void);
void register_node_type_fn_instance_identifier(void);
void register_node_type_fn_string_concatenation(void);
void register_node_type_fn_object_transforms(void);

#ifdef __cplusplus
}
#endif

#endif /* __NOD_FUNCTION_H__ */
