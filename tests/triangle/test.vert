#version 450

layout(location = 0) in vec3 vert_position;
layout(location = 1) in vec4 vert_colour;

layout(location = 0) out vec4 fragColour;

// Hardcoded coords in the vertex shader? Well it's simpler at least XD
// So vulkan default is for +y to be down, so anything written assuming OpenGL will be upside down
vec2 positions[3] = vec2[](
  vec2( 0.0, -0.5),
  vec2( -0.5, 0.5),
  vec2( 0.5, 0.5)
);

vec3 colours[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

void main() {
  gl_Position = vec4(vert_position, 1.0);
  fragColour = vert_colour;

  //gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
  //fragColour = colours[gl_VertexIndex];
}
