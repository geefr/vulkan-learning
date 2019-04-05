#version 450

layout(location = 0) in vec3 vert_partPos;
layout(location = 1) in vec3 vert_colour;

layout(location = 0) out vec4 fragColour;

layout(push_constant) uniform PushConstants {
  mat4 modelM;
  mat4 viewM;
  mat4 projM;
} push;

void main() {
  gl_Position = push.projM * push.viewM * push.modelM * vec4(vert_partPos,1.0);

  // Set the particle size based on dimensions
  gl_PointSize = 4;

  fragColour = vec4(vert_colour, 1.0);

  //gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
  //fragColour = colours[gl_VertexIndex];
}
