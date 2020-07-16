/*
 * Provided under the BSD 3-Clause License, see LICENSE.
 *
 * Copyright (c) 2019, Gareth Francis
 * All rights reserved.
 */

#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosWorld;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV0;
layout(location = 3) in vec2 inUV1;

layout(location = 0) out vec4 outColour;

// TODO: Push constants vs UBOs? What's best practice?
layout(push_constant) uniform push_matrices_t {
    mat4 model;
    mat4 view;
    mat4 projection;
} matrices;

void main() {
  outColour = vec4(1.0, 0.5, 0.5, 1.0);
}
