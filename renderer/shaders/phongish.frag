/*
 * Provided under the BSD 3-Clause License, see LICENSE.
 *
 * Copyright (c) 2019, Gareth Francis
 * All rights reserved.
 */

#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable
#include "interface_uniforms.inc"

// In world space
layout(location = 0) in vec3 inPosWorld;
layout(location = 1) in vec3 inNormal;
// TODO: Need a mapping to say what these coords/samplers map to
layout(location = 2) in vec2 inUV0;
layout(location = 3) in vec2 inUV1;

layout(location = 0) out vec4 outColour;

void main() {
  // tbh this shader is a placeholder until a decent PBR one is implemented
  // It's a basic phong-like model but it's missing anything fancy
  // Will assume all lights are positional point lights, no attenuation
  vec3 normal = normalize(inNormal);
  vec3 eyeDir = normalize(uboPerFrame.eyePos.xyz - inPosWorld);

  outColour = vec4(0.0,0.0,0.0,0.0);
  for( uint i = 0; i < uboPerFrame.numLights; i++ ) {
    Light l = uboPerFrame.lights[i];
    vec3 lightDir = normalize(l.posOrDir.xyz - inPosWorld);

    // ambient hardcoded
    vec4 ambient = vec4(vec3(0.1,0.1,0.1) * uboMaterial.baseColourFactor.xyz, 1.0);

    // Diffuse from light + base colour
    //vec4 diffuse = l.colour + uboMaterial.baseColourFactor;
    vec4 diffuse = vec4(l.colour.xyz * max(dot(normal,lightDir) * uboMaterial.baseColourFactor.xyz, 0.0), 1.0);

    // Specular, 'shininess' pow factor hardcoded
    vec3 specReflectDir = reflect(-lightDir,normal);
    vec4 specular = vec4(l.colour.xyz * pow(max(dot(eyeDir, specReflectDir), 0.0), 4.0) * uboMaterial.specularFactor, 1.0);

    outColour += (ambient + diffuse /*+ specular*/) / float(uboPerFrame.numLights);
  }
  outColour.a = 1.0;
}
