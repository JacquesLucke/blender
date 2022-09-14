/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(overlay_attribute_iface, "").smooth(Type::VEC3, "attribute_color");

GPU_SHADER_CREATE_INFO(overlay_attribute_mesh)
    .do_static_compilation(true)
    .vertex_source("overlay_attribute_mesh_vert.glsl")
    .fragment_source("overlay_attribute_frag.glsl")
    .fragment_out(0, Type::VEC4, "out_color")
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::VEC3, "vertex_color")
    .vertex_out(overlay_attribute_iface)
    .additional_info("draw_mesh");

GPU_SHADER_CREATE_INFO(overlay_attribute_mesh_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_attribute_mesh", "drw_clipped");

GPU_SHADER_CREATE_INFO(overlay_attribute_pointcloud)
    .do_static_compilation(true)
    .vertex_source("overlay_attribute_pointcloud_vert.glsl")
    .fragment_source("overlay_attribute_frag.glsl")
    .fragment_out(0, Type::VEC4, "out_color")
    .vertex_in(3, Type::VEC3, "vertex_color")
    .vertex_out(overlay_attribute_iface)
    .additional_info("draw_pointcloud");

GPU_SHADER_CREATE_INFO(overlay_attribute_pointcloud_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_attribute_pointcloud", "drw_clipped");

GPU_SHADER_CREATE_INFO(overlay_attribute_curve)
    .do_static_compilation(true)
    .vertex_source("overlay_attribute_curve_vert.glsl")
    .fragment_source("overlay_attribute_frag.glsl")
    .fragment_out(0, Type::VEC4, "out_color")
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::VEC3, "vertex_color")
    .vertex_out(overlay_attribute_iface)
    .additional_info("draw_modelmat", "draw_resource_id");

GPU_SHADER_CREATE_INFO(overlay_attribute_curve_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_attribute_curve", "drw_clipped");
