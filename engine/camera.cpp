#include "camera.h"

#include <glm/gtc/matrix_transform.hpp>

Camera::Camera() {
    projectionPerspective( 90.0, 1.0, 0.01, 100.0);
    // projectionOrtho(glm::vec4(-10.0, 10.0, -10.0, 10.0), 0.01, 100.0);
    lookAt( {0.0,0.0,0.0},
            {0.0,5.0,-5.0},
            {0.0,1.0,0.0});
}

Camera::~Camera() {}

void Camera::lookAt(glm::vec3 eye, glm::vec3 center, glm::vec3 up) {
    mViewMatrix = glm::lookAt(eye, center, up);
}

void Camera::projectionOrtho(glm::vec4 ortho, float near, float far) {
    mProjectionMatrix = glm::ortho(ortho[0], ortho[1], ortho[2], ortho[3], near, far);
}

void Camera::projectionPerspective(float fov, float aspect, float near, float far) {
    mProjectionMatrix = glm::perspective(fov, aspect, near, far);
}
