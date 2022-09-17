#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  vec3 world_pos = point_object_to_world(pos);
  world_pos.z += 1.0;
  gl_Position = point_world_to_ndc(world_pos);
}
