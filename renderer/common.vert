/*
 * Provided under the BSD 3-Clause License, see LICENSE.
 *
 * Copyright (c) 2019, Gareth Francis
 * All rights reserved.
 */

#version 450

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

layout(binding = 0) uniform UBOSetPerFrame {
  mat4 viewMatrix;
  mat4 projectionMatrix;
} uboPerFrame;

// Frequently changing data as push constants
// Can be updated very quickly without synchronisation
// Upper limit of 128 bytes (Minimum available in spec)
layout(push_constant) uniform push_matrices_t {
    mat4 model;
} matrices;

void main() {
  // TODO: Skinning/Joint handling would go here, see https://github.com/SaschaWillems/Vulkan-glTF-PBR/blob/master/data/shaders/pbr.vert

  vec4 worldPos = matrices.model * vec4(inPosition, 1.0);
  outPosWorld = worldPos.xyz;

  mat4 normalMatrix = transpose(inverse(uboPerFrame.viewMatrix * matrices.model));
  outNormal = (normalMatrix * vec4(inNormal, 1.0)).xyz;

  outUV0 = inUV0;
  outUV1 = inUV1;

  gl_Position = uboPerFrame.projectionMatrix * uboPerFrame.viewMatrix * worldPos;
}
