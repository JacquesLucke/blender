/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(my_test)
    .do_static_compilation(true)
    .vertex_source("my_test_vert.glsl")
    .fragment_source("my_test_frag.glsl")
    .fragment_out(0, Type::VEC4, "out_color")
    .geometry_source("my_test_geom.glsl")
    .geometry_layout(PrimitiveIn::POINTS, PrimitiveOut::LINE_STRIP, 10)
    .vertex_in(0, Type::FLOAT, "my_pos")
    .additional_info("draw_modelmat");

GPU_SHADER_CREATE_INFO(my_test_comp)
    .do_static_compilation(true)
    .local_group_size(1, 1)
    .compute_source("my_test_comp.glsl")
    .storage_buf(0, Qualifier::WRITE, "float", "out_values[]");
