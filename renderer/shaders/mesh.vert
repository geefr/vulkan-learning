/*
 * Provided under the BSD 3-Clause License, see LICENSE.
 *
 * Copyright (c) 2019, Gareth Francis
 * All rights reserved.
 */

#version 450

#extension GL_GOOGLE_include_directive : enable
#include "interface_uniforms.inc"

// Vertex Specification
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV0;
layout(location = 3) in vec2 inUV1;
// layout(location = 4) in vec4 inJoint0;
// layout(location = 5) in vec4 inWeight0;

layout(location = 0) out vec3 outPosWorld;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outUV0;
layout(location = 3) out vec2 outUV1;

void main() {
  // TODO: Skinning/Joint handling would go here, see https://github.com/SaschaWillems/Vulkan-glTF-PBR/blob/master/data/shaders/pbr.vert

  vec4 worldPos = matrices.model * vec4(inPosition, 1.0);
  outPosWorld = worldPos.xyz;

  // Lighting calculations are performed in world space
  // This uses the 'normal matrix' which scales/rotates correctly for the normals
  // TODO: Should move normal matrix to cpu side, quit being wasteful here
  mat3 normalMatrix = transpose(inverse(mat3(uboPerFrame.viewMatrix * matrices.model)));
  outNormal = normalMatrix * inNormal;

  outUV0 = inUV0;
  outUV1 = inUV1;

  gl_Position = uboPerFrame.projectionMatrix * uboPerFrame.viewMatrix * worldPos;
}
