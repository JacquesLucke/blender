void main()
{
  gl_Position = gl_in[0].gl_Position;
  EmitVertex();
  gl_Position.y += 0.1;
  EmitVertex();
  gl_Position.x += 0.1;
  EmitVertex();
  gl_Position.y -= 0.1;
  EmitVertex();
  gl_Position.x -= 0.1;
  EmitVertex();
}
