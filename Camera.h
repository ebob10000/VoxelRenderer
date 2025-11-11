#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

const float PLAYER_WIDTH = 0.6f;
const float PLAYER_HEIGHT = 1.8f;
const float EYE_HEIGHT = 1.6f;

class Camera {
public:
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;

    float yaw = -90.0f;
    float pitch = 0.0f;
    float speed = 5.0f;
    float sensitivity = 0.1f;
    float fov = 85.0f;

    Camera(glm::vec3 startPosition)
        : position(startPosition), front(glm::vec3(0.0f, 0.0f, -1.0f)), up(glm::vec3(0.0f, 1.0f, 0.0f)) {
    }

    glm::mat4 getViewMatrix() {
        return glm::lookAt(position, position + front, up);
    }

    void updateCameraVectors() {
        glm::vec3 newFront;
        newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        newFront.y = sin(glm::radians(pitch));
        newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(newFront);
    }

    // FIX: Add method to reset mouse state
    void resetMouse() {
        // This will be called by Application when unpausing
    }
};