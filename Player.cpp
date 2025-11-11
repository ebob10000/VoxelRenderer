#include "Player.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

Player::Player(const glm::vec3& spawnPosition)
    : m_Position(spawnPosition),
    m_Camera(spawnPosition),
    m_PreviousPosition(spawnPosition) {
}

void Player::handleInput(GLFWwindow* window, bool isPaused) {
    if (isPaused) {
        m_MoveDirection = glm::vec3(0.0f);
        m_IsSprinting = false;
        m_IsSneaking = false;
        m_JumpInput = false;
        return;
    }

    if (m_IsFlying) {
        m_IsSprinting = false;
        m_IsSneaking = false;
        m_JumpInput = false;
        return; // Flying handled in update with window access
    }

    bool pressingW = (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS);
    bool pressingShift = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS);

    m_IsSprinting = pressingShift && pressingW && !m_IsSneaking;
    m_IsSneaking = (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS);

    glm::vec3 forward = glm::normalize(glm::vec3(m_Camera.front.x, 0.0f, m_Camera.front.z));
    glm::vec3 right = glm::normalize(glm::cross(forward, m_Camera.up));

    glm::vec3 moveDir(0.0f);
    if (pressingW) moveDir += forward;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) moveDir -= forward;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) moveDir -= right;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) moveDir += right;

    m_MoveDirection = (glm::length(moveDir) > 0.0f) ? glm::normalize(moveDir) : glm::vec3(0.0f);

    if (m_IsSneaking) {
        m_IsSprinting = false;
    }

    m_JumpInput = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);
}

void Player::update(float deltaTime, World& world, GLFWwindow* window) {
    if (m_IsFlying) {
        m_TimeAccumulator = 0.0f;
        m_Velocity = glm::vec3(0.0f);

        float speed = m_Camera.speed * (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 2.0f : 1.0f);
        glm::vec3 right = glm::normalize(glm::cross(m_Camera.front, m_Camera.up));

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) m_Position += speed * deltaTime * m_Camera.front;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) m_Position -= speed * deltaTime * m_Camera.front;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) m_Position -= speed * deltaTime * right;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) m_Position += speed * deltaTime * right;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) m_Position += speed * deltaTime * m_Camera.up;
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) m_Position -= speed * deltaTime * m_Camera.up;

        m_Camera.position = m_Position;
        m_PreviousPosition = m_Position;
        m_RenderPosition = m_Position;
        return;
    }

    m_WasOnGround = m_IsOnGround;
    m_TimeAccumulator += deltaTime;

    while (m_TimeAccumulator >= Physics::TICK_DURATION) {
        m_PreviousPosition = m_Position;
        runPhysicsTick(world);
        m_TimeAccumulator -= Physics::TICK_DURATION;
    }

    float alpha = m_TimeAccumulator / Physics::TICK_DURATION;
    m_RenderPosition = glm::mix(m_PreviousPosition, m_Position, alpha);
    m_Camera.position = m_RenderPosition;
}

void Player::runPhysicsTick(World& world) {
    // First, apply movement physics based on current state (on ground or in air)
    if (m_IsOnGround) {
        float friction = Physics::GROUND_FRICTION * Physics::SLIPPERINESS;
        m_Velocity.x *= friction;
        m_Velocity.z *= friction;

        float acceleration = Physics::BASE_ACCELERATION;
        if (m_IsSprinting) {
            acceleration *= Physics::SPRINT_MULTIPLIER;
        }
        else if (m_IsSneaking) {
            acceleration *= Physics::SNEAK_MULTIPLIER;
        }

        m_Velocity.x += m_MoveDirection.x * acceleration;
        m_Velocity.z += m_MoveDirection.z * acceleration;

        float targetSpeed;
        if (m_IsSprinting) targetSpeed = 0.7f;
        else if (m_IsSneaking) targetSpeed = 0.2f;
        else targetSpeed = 0.4f;

        float horizontalSpeed = glm::length(glm::vec2(m_Velocity.x, m_Velocity.z));
        if (horizontalSpeed > targetSpeed) {
            float scale = targetSpeed / horizontalSpeed;
            m_Velocity.x *= scale;
            m_Velocity.z *= scale;
        }
    }
    else { // In Air
        m_Velocity.y -= Physics::GRAVITY;
        m_Velocity.y *= Physics::VERTICAL_DRAG;
        m_Velocity.y = glm::max(m_Velocity.y, Physics::TERMINAL_VELOCITY);

        m_Velocity.x *= Physics::AIR_DRAG;
        m_Velocity.z *= Physics::AIR_DRAG;

        if (glm::length(m_MoveDirection) > 0.01f) {
            float targetSpeed;
            if (m_IsSprinting) targetSpeed = 0.7f;
            else if (m_IsSneaking) targetSpeed = 0.2f;
            else targetSpeed = 0.4f;

            glm::vec2 wishDir = glm::normalize(glm::vec2(m_MoveDirection.x, m_MoveDirection.z));
            float currentSpeedInWishDir = m_Velocity.x * wishDir.x + m_Velocity.z * wishDir.y;
            float speedToAdd = targetSpeed - currentSpeedInWishDir;

            if (speedToAdd > 0) {
                float accel = std::min(speedToAdd, Physics::AIR_ACCELERATION);
                m_Velocity.x += wishDir.x * accel;
                m_Velocity.z += wishDir.y * accel;
            }
        }
    }

    // After movement is calculated, process actions like jumping
    if (m_JumpInput && m_IsOnGround) {
        m_Velocity.y = Physics::JUMP_FORCE;
        m_IsOnGround = false; // Player is now airborne for the next tick

        if (m_IsSprinting && glm::length(m_MoveDirection) > 0.01f) {
            // Add boost to the velocity that was just calculated
            m_Velocity.x += m_MoveDirection.x * Physics::SPRINT_JUMP_BOOST;
            m_Velocity.z += m_MoveDirection.z * Physics::SPRINT_JUMP_BOOST;
            m_WasSprintingOnJump = true;
        }
        else {
            m_WasSprintingOnJump = false;
        }
    }

    // Finally, resolve collisions with the new velocity
    resolveCollisions(world);
}

bool Player::checkGroundCollision(World& world, const glm::vec3& pos) {
    // Use normal AABB (not sneaking AABB) for ground prediction
    glm::vec3 aabbMin = pos + glm::vec3(-Physics::PLAYER_WIDTH / 2.0f, -Physics::EYE_HEIGHT, -Physics::PLAYER_WIDTH / 2.0f);
    glm::vec3 aabbMax = pos + glm::vec3(Physics::PLAYER_WIDTH / 2.0f, Physics::PLAYER_HEIGHT - Physics::EYE_HEIGHT, Physics::PLAYER_WIDTH / 2.0f);

    glm::ivec3 minBlock = glm::floor(aabbMin);
    glm::ivec3 maxBlock = glm::floor(aabbMax);

    // Only check blocks below the player's feet
    for (int y = minBlock.y; y <= minBlock.y; ++y) {
        for (int x = minBlock.x; x <= maxBlock.x; ++x) {
            for (int z = minBlock.z; z <= maxBlock.z; ++z) {
                if (world.getBlock(x, y, z) != 0) return true;
            }
        }
    }
    return false;
}

void Player::resolveCollisions(World& world) {
    auto getAABB = [&](const glm::vec3& pos, bool useSneakBox = true) {
        // When sneaking, use a slightly smaller height to allow edge hanging
        float heightOffset = (m_IsSneaking && useSneakBox) ? 0.15f : 0.0f;
        return std::make_pair(
            pos + glm::vec3(-Physics::PLAYER_WIDTH / 2.0f, -Physics::EYE_HEIGHT + heightOffset, -Physics::PLAYER_WIDTH / 2.0f),
            pos + glm::vec3(Physics::PLAYER_WIDTH / 2.0f, Physics::PLAYER_HEIGHT - Physics::EYE_HEIGHT, Physics::PLAYER_WIDTH / 2.0f)
        );
        };

    auto checkCollision = [&](const glm::vec3& pos, bool useSneakBox = true) {
        auto aabb = getAABB(pos, useSneakBox);
        glm::ivec3 minBlock = glm::floor(aabb.first);
        glm::ivec3 maxBlock = glm::floor(aabb.second);

        for (int y = minBlock.y; y <= maxBlock.y; ++y) {
            for (int x = minBlock.x; x <= maxBlock.x; ++x) {
                for (int z = minBlock.z; z <= maxBlock.z; ++z) {
                    if (world.getBlock(x, y, z) != 0) return true;
                }
            }
        }
        return false;
        };

    // Y-axis (vertical) collision - ALWAYS use normal collision box for vertical
    m_Position.y += m_Velocity.y;
    if (checkCollision(m_Position, false)) {  // Use normal box for vertical
        m_Position.y -= m_Velocity.y;

        if (m_Velocity.y < 0) {
            // Falling - snap to ground
            float step = 0.001f;
            float totalStep = 0.0f;
            while (!checkCollision(m_Position, false) && totalStep < 1.0f) {
                m_Position.y -= step;
                totalStep += step;
            }
            m_Position.y += step; // Step back from collision
            m_IsOnGround = true;
            m_Velocity.y = 0.0f;
            m_WasSprintingOnJump = false;
        }
        else if (m_Velocity.y > 0) {
            // Hit ceiling
            m_Velocity.y = 0.0f;
        }
    }
    else {
        // Not colliding vertically
        if (m_Velocity.y < -0.001f) {
            m_IsOnGround = false;
        }
        else if (m_IsOnGround) {
            // Check if there's still ground beneath us
            glm::vec3 testPos = m_Position;
            testPos.y -= 0.1f;
            if (!checkCollision(testPos, false)) {
                m_IsOnGround = false;
            }
        }
    }

    // X-axis collision - use sneak box for horizontal
    glm::vec3 oldPos = m_Position;
    m_Position.x += m_Velocity.x;
    if (checkCollision(m_Position, true)) {  // Use sneak box
        m_Position.x -= m_Velocity.x;
        m_Velocity.x = 0.0f;
    }

    // Z-axis collision - use sneak box for horizontal
    m_Position.z += m_Velocity.z;
    if (checkCollision(m_Position, true)) {  // Use sneak box
        m_Position.z -= m_Velocity.z;
        m_Velocity.z = 0.0f;
    }

    // Edge hanging when sneaking - prevent falling off edges
    if (m_IsSneaking && m_IsOnGround) {
        glm::vec3 testPos = m_Position;
        testPos.y -= 0.6f;

        if (!checkCollision(testPos, false)) {  // Use normal box for ground check
            m_Position.x = oldPos.x;
            m_Position.z = oldPos.z;
            m_Velocity.x = 0.0f;
            m_Velocity.z = 0.0f;
        }
    }
}

float Player::getFOV() const {
    float baseFov = m_Camera.fov;
    // Increase FOV when sprinting and moving (works in air too)
    if (m_IsSprinting && glm::length(m_MoveDirection) > 0.01f) {
        baseFov += 5.0f;
    }
    return baseFov;
}