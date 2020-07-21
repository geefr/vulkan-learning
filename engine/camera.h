/*
 * Provided under the BSD 3-Clause License, see LICENSE.
 *
 * Copyright (c) 2020, Gareth Francis
 * All rights reserved.
 */

#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>

/**
 * A camera, used to render the scene
 */
class Camera {
public:
    Camera();
    ~Camera();

    void lookAt(glm::vec3 eye, glm::vec3 center, glm::vec3 up);

    void projectionOrtho(glm::vec4 ortho, float near, float far);
    void projectionPerspective(float fov, float aspect, float near, float far);

    glm::mat4x4 mViewMatrix;
    glm::mat4x4 mProjectionMatrix;
    glm::vec3 mPosition;
};

#endif
