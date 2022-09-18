void main()
{
  vec4 p1 = gl_in[0].gl_Position;
  vec4 p2 = gl_in[1].gl_Position;

  gl_Position = p1;
  EmitVertex();
  gl_Position = p2;
  EmitVertex();
  gl_Position = p2 + vec4(0.1, 0.0, 0.0, 0.0);
  EmitVertex();
  gl_Position = p1 + vec4(0.1, 0.0, 0.0, 0.0);
  EmitVertex();
  gl_Position = p1;
  EmitVertex();
}
