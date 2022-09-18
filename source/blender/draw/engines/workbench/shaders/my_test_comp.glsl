void main()
{
  float x = float(gl_GlobalInvocationID.x) * 0.1;
  out_values[gl_GlobalInvocationID.x] = vec2(x, x * x);
}
