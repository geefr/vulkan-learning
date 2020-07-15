/*
 * Provided under the BSD 3-Clause License, see LICENSE.
 *
 * Copyright (c) 2019, Gareth Francis
 * All rights reserved.
 */

#version 450

layout(location = 0) in vec4 vert_coord;
layout(location = 1) in vec4 vert_colour;

layout(location = 0) out vec4 fragColour;

layout(push_constant) uniform push_matrices_t {
    mat4 model;
    mat4 view;
    mat4 projection;
} matrices;

void main() {
  gl_Position = matrices.projection * matrices.view * matrices.model * vert_coord;

  fragColour = vert_colour;
}
