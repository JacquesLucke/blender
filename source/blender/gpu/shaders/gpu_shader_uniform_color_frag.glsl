#pragma BLENDER_REQUIRE(gpu_shader_colorspace_lib.glsl)

void main()
{
  fragColor = blender_srgb_to_framebuffer_space(color);
}
