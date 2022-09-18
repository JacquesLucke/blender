void main()
{
  out_values[gl_GlobalInvocationID.x] = vec2(float(gl_GlobalInvocationID.x), 2.0);
}
