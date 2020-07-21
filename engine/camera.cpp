#include "camera.h"

#include <glm/gtc/matrix_transform.hpp>

Camera::Camera() {
    // TODO: Pass aspect in - handle viewport/window size changes
    projectionPerspective( glm::radians(30.f), 800.f / 600.f,  0.001f,1000.f);

    lookAt( {0.0,5.0,-5.0},  // eye
            {0.0,0.0,0.0}, // center
            {0.0,1.0,0.0}); // up
}

Camera::~Camera() {}

void Camera::lookAt(glm::vec3 eye, glm::vec3 center, glm::vec3 up) {
    mViewMatrix = glm::lookAt(eye, center, up);
    mPosition = eye;
}

void Camera::projectionOrtho(glm::vec4 ortho, float near, float far) {
    mProjectionMatrix = glm::ortho(ortho[0], ortho[1], ortho[2], ortho[3], near, far);
}

void Camera::projectionPerspective(float fov, float aspect, float near, float far) {
    mProjectionMatrix = glm::perspective(fov, aspect, near, far);
}
