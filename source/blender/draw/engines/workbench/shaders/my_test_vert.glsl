#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  vec3 world_pos = point_object_to_world(vec3(my_pos.xy, 1.0));
  gl_Position = point_world_to_ndc(world_pos);
}
