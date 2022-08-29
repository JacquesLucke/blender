/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(overlay_attribute)
    .do_static_compilation(true)
    .vertex_source("overlay_attribute_vert.glsl")
    .fragment_source("overlay_attribute_frag.glsl")
    .fragment_out(0, Type::VEC4, "out_color")
    .vertex_in(0, Type::VEC3, "pos")
    .additional_info("draw_mesh");

GPU_SHADER_CREATE_INFO(overlay_attribute_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_sculpt_curves_selection", "drw_clipped");
