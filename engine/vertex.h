/*
 * Provided under the BSD 3-Clause License, see LICENSE.
 *
 * Copyright (c) 2020, Gareth Francis
 * All rights reserved.
 */

#ifndef VERTEX_H
#define VERTEX_H

#include <glm/glm.hpp>

/// The vertex definition in the engine
/// It's up to the Renderer whether it respects any of this,
/// but fixed at this level for model loaders/consistent format
struct Vertex
{
    glm::vec3 position = {0.f,0.f,0.f};
    glm::vec3 normal = {0.f,0.f,0.f};
    glm::vec2 uv0 = {0.f,0.f};
    glm::vec2 uv1 = {0.f,0.f};
    // TODO: For skinning/animation
    // glm::vec4 joint0 = {0.f,0.f,0.f,0.f};
    // glm::vec4 weight0 = {0.f,0.f,0.f,0.f};
};

#endif
