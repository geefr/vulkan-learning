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

layout(set = 0, binding = 0) uniform UBOSetPerFrame {
  mat4 viewMatrix;
  mat4 projectionMatrix;
} uboPerFrame;

layout(set = 1, binding = 0) uniform UBOSetMaterial {
  vec4 baseColourFactor;
  vec4 emissiveFactor;
  vec4 diffuseFactor;
  vec3 specularFactor;
  float alphaCutOff;
} uboMaterial;

void main() {
  outColour = uboMaterial.baseColourFactor;
}
