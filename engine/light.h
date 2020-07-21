/*
 * Provided under the BSD 3-Clause License, see LICENSE.
 *
 * Copyright (c) 2020, Gareth Francis
 * All rights reserved.
 */

#ifndef LIGHT_H
#define LIGHT_H

#include <glm/glm.hpp>

#include <numeric>
#include <string>

/**
 * A light in the scene
 * Specification based on glTF punctual lights (But sensible types for most setups)
 * https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Khronos/KHR_lights_punctual
 */
class Light {
public:
  enum class Type {
    Directional = 0,
    Point,
    Spot,
  };

  Light(std::string typeStr);
  Light(Type t);
  ~Light();

  std::string name = "";
  glm::vec3 position = {0.f,0.f,0.f}; // if type != Directional
  glm::vec3 direction = {0.f,0.f,-1.f}; // if type == Directional
  glm::vec3 colour = {1.f,1.f,1.f};
  float intensity = 1.f;
  Light::Type type() const;
  float range = 0.f;
  float innerConeAngle = 0.f;
  float outerConeAngle = M_PI / 4.f;

private:
  Light::Type mType = Type::Directional;
};

#endif
