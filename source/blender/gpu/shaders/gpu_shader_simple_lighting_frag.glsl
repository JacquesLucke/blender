
void main()
{
  fragColor = simple_lighting_data.l_color;
  fragColor.xyz *= clamp(dot(normalize(normal), simple_lighting_data.light), 0.0, 1.0);
}
