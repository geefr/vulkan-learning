/*
 * Provided under the BSD 3-Clause License, see LICENSE.
 *
 * Copyright (c) 2019, Gareth Francis
 * All rights reserved.
 */

// Type declarations
// As defined by glTF Punctual lights extension
// https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Khronos/KHR_lights_punctual
const int LightTypeDirectional = 0;
const int LightTypePoint = 1;
const int LightTypeSpot = 2;
struct Light {
  vec4 posOrDir; // position if w == 1 else direction
  vec4 colour;   // w == intensity
  vec4 typeAndParams; // x == range, y == innerConeCos, z == outerConeCos, w == type
};

// Specialisation constants
layout(constant_id = 0) const uint maxLights = 100;

// Uniforms
layout(set = 0, binding = 0) uniform UBOSetPerFrame {
  mat4 viewMatrix;
  mat4 projectionMatrix;
  vec4 eyePos;
  float numLights;
  float pad1;
  float pad2;
  float pad3;
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

