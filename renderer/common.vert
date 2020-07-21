/*
 * Provided under the BSD 3-Clause License, see LICENSE.
 *
 * Copyright (c) 2019, Gareth Francis
 * All rights reserved.
 */

#version 450

// Type declarations
// TODO: Should move to a common file/include
// As defined by glTF Punctual lights extension
// https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Khronos/KHR_lights_punctual
const int LightTypeDirectional = 0;
const int LightTypePoint = 1;
const int LightTypeSpot = 2;
struct Light {
  vec4 posOrDir; // position if w == 1 else direction
  vec4 colour;   // w == intensity
  vec4 typeAndParams; // x == range, y == innerConeCos, z == outerConeCos, w == type
  vec4 padding;
};

// Specialisation constants
layout(constant_id = 0) const uint maxLights = 100;

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

// Uniforms
layout(set = 0, binding = 0) uniform UBOSetPerFrame {
  mat4 viewMatrix;
  mat4 projectionMatrix;
  float numLights;
  vec3 padding;
  Light[maxLights] lights;
} uboPerFrame;

layout(set = 1, binding = 0) uniform UBOSetMaterial {
  vec4 baseColourFactor;
  vec4 emissiveFactor;
  vec4 diffuseFactor;
  vec3 specularFactor;
  float alphaCutOff;
} uboMaterial;

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
