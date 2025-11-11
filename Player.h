#pragma once
#include "Camera.h"
#include "World.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

namespace Physics {
    const float TICK_RATE = 20.0f;
    const float TICK_DURATION = 1.0f / TICK_RATE;

    const float GRAVITY = 0.08f;
    const float VERTICAL_DRAG = 0.98f;
    const float JUMP_FORCE = 0.47f;
    const float TERMINAL_VELOCITY = -3.92f;

    const float SLIPPERINESS = 0.6f;
    const float GROUND_FRICTION = 0.546f;
    const float AIR_DRAG = 0.91f;
    const float BASE_ACCELERATION = 0.18f;
    const float AIR_ACCELERATION = 0.03f;

    const float SPRINT_MULTIPLIER = 1.5f;
    const float SNEAK_MULTIPLIER = 0.3f;
    const float SPRINT_JUMP_BOOST = 0.05f;

    const float PLAYER_WIDTH = 0.6f;
    const float PLAYER_HEIGHT = 1.8f;
    const float EYE_HEIGHT = 1.6f;
}

class Player {
public:
    Player(const glm::vec3& spawnPosition);

    void handleInput(GLFWwindow* window, bool isPaused);
    void update(float deltaTime, World& world, GLFWwindow* window);

    glm::vec3 getRenderPosition() const { return m_RenderPosition; }
    const glm::vec3& getPosition() const { return m_Position; }
    Camera& getCamera() { return m_Camera; }

    bool isOnGround() const { return m_IsOnGround; }
    bool isSprinting() const { return m_IsSprinting; }
    bool isSneaking() const { return m_IsSneaking; }
    bool isFlying() const { return m_IsFlying; }
    void setFlying(bool flying) { m_IsFlying = flying; m_Velocity = glm::vec3(0.0f); }

    float getFOV() const;

private:
    void runPhysicsTick(World& world);
    void resolveCollisions(World& world);
    bool checkGroundCollision(World& world, const glm::vec3& pos);

    Camera m_Camera;
    glm::vec3 m_Position;
    glm::vec3 m_Velocity;
    glm::vec3 m_MoveDirection;

    bool m_IsOnGround = false;
    bool m_IsSprinting = false;
    bool m_IsSneaking = false;
    bool m_IsFlying = false;
    bool m_JumpInput = false;
    bool m_WasOnGround = false;
    bool m_WasSprintingOnJump = false;

    glm::vec3 m_PreviousPosition;
    glm::vec3 m_RenderPosition;
    float m_TimeAccumulator = 0.0f;
};