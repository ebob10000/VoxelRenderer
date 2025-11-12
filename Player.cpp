#include "Player.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

Player::Player(const glm::vec3& spawnPosition)
    : m_Position(spawnPosition), // Feet position
    m_Camera(spawnPosition + glm::vec3(0.0f, Physics::EYE_HEIGHT, 0.0f)),
    m_PreviousPosition(spawnPosition),
    m_RenderPosition(spawnPosition),
    m_CurrentFOV(m_Camera.fov),
    m_CurrentEyeHeight(Physics::EYE_HEIGHT) {
    m_Inventory.resize(27);
    m_Hotbar.resize(9);
}

void Player::handleInput(GLFWwindow* window, bool isPaused) {
    if (isPaused) {
        m_MoveInput = glm::vec3(0.0f);
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

    m_MoveInput = glm::vec3(0.0f);
    if (pressingW) m_MoveInput.z += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) m_MoveInput.z -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) m_MoveInput.x -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) m_MoveInput.x += 1.0f;

    if (m_IsSneaking) {
        m_IsSprinting = false;
    }

    m_JumpInput = (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS);
}

void Player::update(float deltaTime, World& world, GLFWwindow* window) {
    if (!m_IsFlying && !m_IsSneaking && m_WasSneaking) {
        glm::vec3 standAABBMin = m_Position + glm::vec3(-Physics::PLAYER_WIDTH / 2.0f, 0.0f, -Physics::PLAYER_WIDTH / 2.0f);
        glm::vec3 standAABBMax = m_Position + glm::vec3(Physics::PLAYER_WIDTH / 2.0f, Physics::PLAYER_HEIGHT, Physics::PLAYER_WIDTH / 2.0f);
        glm::ivec3 minBlock = glm::floor(standAABBMin);
        glm::ivec3 maxBlock = glm::floor(standAABBMax);
        bool blocked = false;
        for (int y = minBlock.y; y <= maxBlock.y; ++y) {
            for (int x = minBlock.x; x <= maxBlock.x; ++x) {
                for (int z = minBlock.z; z <= maxBlock.z; ++z) {
                    if (world.getBlock(x, y, z) != 0) {
                        blocked = true;
                        break;
                    }
                }
                if (blocked) break;
            }
            if (blocked) break;
        }
        if (blocked) {
            m_IsSneaking = true;
        }
    }
    m_WasSneaking = m_IsSneaking;

    bool isEffectivelySprinting = m_IsSprinting && m_MoveInput.z > 0;
    float targetFOV = isEffectivelySprinting ? m_Camera.fov + 10.0f : m_Camera.fov;
    float targetEyeHeight = m_IsSneaking ? Physics::CROUCH_EYE_HEIGHT : Physics::EYE_HEIGHT;
    m_CurrentFOV = glm::mix(m_CurrentFOV, targetFOV, 15.0f * deltaTime);
    m_CurrentEyeHeight = glm::mix(m_CurrentEyeHeight, targetEyeHeight, 10.0f * deltaTime);

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

        m_RenderPosition = m_Position;
        m_PreviousPosition = m_Position;
    }
    else {
        m_WasOnGround = m_IsOnGround;
        m_TimeAccumulator += deltaTime;
        while (m_TimeAccumulator >= Physics::TICK_DURATION) {
            m_PreviousPosition = m_Position;
            runPhysicsTick(world);
            m_TimeAccumulator -= Physics::TICK_DURATION;
        }
        float alpha = m_TimeAccumulator / Physics::TICK_DURATION;
        m_RenderPosition = glm::mix(m_PreviousPosition, m_Position, alpha);
    }
    m_Camera.position = m_RenderPosition + glm::vec3(0.0f, m_CurrentEyeHeight, 0.0f);
}

void Player::runPhysicsTick(World& world) {
    glm::vec3 forward = glm::normalize(glm::vec3(m_Camera.front.x, 0.0f, m_Camera.front.z));
    glm::vec3 right = glm::normalize(glm::cross(forward, m_Camera.up));

    bool isEffectivelySprinting = m_IsSprinting && m_MoveInput.z > 0;

    glm::vec3 wishDir = m_MoveInput.z * forward + m_MoveInput.x * right;
    if (glm::length(wishDir) > 0.0f) {
        wishDir = glm::normalize(wishDir);
    }

    if (m_IsOnGround) {
        float friction = Physics::GROUND_FRICTION * Physics::SLIPPERINESS;
        m_Velocity.x *= friction;
        m_Velocity.z *= friction;

        float finalTargetSpeed;
        float acceleration = Physics::BASE_ACCELERATION;
        if (m_IsSneaking) {
            finalTargetSpeed = Physics::SNEAK_SPEED;
            acceleration *= Physics::SNEAK_MULTIPLIER;
        }
        else if (isEffectivelySprinting) {
            finalTargetSpeed = Physics::SPRINT_SPEED;
            acceleration *= Physics::SPRINT_MULTIPLIER;
        }
        else {
            finalTargetSpeed = Physics::WALK_SPEED;
        }

        m_Velocity.x += wishDir.x * acceleration;
        m_Velocity.z += wishDir.z * acceleration;

        float horizontalSpeed = glm::length(glm::vec2(m_Velocity.x, m_Velocity.z));
        if (horizontalSpeed > finalTargetSpeed) {
            float scale = finalTargetSpeed / horizontalSpeed;
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

        if (glm::length(m_MoveInput) > 0.01f) {
            float airTargetSpeed = isEffectivelySprinting ? Physics::SPRINT_SPEED : Physics::WALK_SPEED;
            if (m_IsSneaking) airTargetSpeed = Physics::SNEAK_SPEED;

            float currentSpeedInWishDir = glm::dot(glm::vec3(m_Velocity.x, 0.0f, m_Velocity.z), wishDir);
            float speedToAdd = airTargetSpeed - currentSpeedInWishDir;

            if (speedToAdd > 0) {
                float accel = std::min(speedToAdd, Physics::AIR_ACCELERATION);
                m_Velocity.x += wishDir.x * accel;
                m_Velocity.z += wishDir.z * accel;
            }
        }
    }

    if (m_JumpInput && m_IsOnGround) {
        m_Velocity.y = Physics::JUMP_FORCE;
        m_IsOnGround = false;
        if (isEffectivelySprinting) {
            m_Velocity += forward * Physics::SPRINT_JUMP_BOOST;
        }
        m_WasSprintingOnJump = isEffectivelySprinting;
    }

    resolveCollisions(world);
}

void Player::resolveCollisions(World& world) {
    float currentHeight = m_IsSneaking ? Physics::CROUCH_PLAYER_HEIGHT : Physics::PLAYER_HEIGHT;
    auto checkCollision = [&](const glm::vec3& pos) {
        glm::vec3 aabbMin = pos + glm::vec3(-Physics::PLAYER_WIDTH / 2.0f, 0.0f, -Physics::PLAYER_WIDTH / 2.0f);
        glm::vec3 aabbMax = pos + glm::vec3(Physics::PLAYER_WIDTH / 2.0f, currentHeight, Physics::PLAYER_WIDTH / 2.0f);
        glm::ivec3 minBlock = glm::floor(aabbMin);
        glm::ivec3 maxBlock = glm::floor(aabbMax);
        for (int y = minBlock.y; y <= maxBlock.y; ++y) {
            for (int x = minBlock.x; x <= maxBlock.x; ++x) {
                for (int z = minBlock.z; z <= maxBlock.z; ++z) {
                    if (world.getBlock(x, y, z) != 0) return true;
                }
            }
        }
        return false;
        };
    m_Position.y += m_Velocity.y;
    if (checkCollision(m_Position)) {
        m_Position.y -= m_Velocity.y;
        if (m_Velocity.y < 0) {
            float step = 0.001f;
            float totalStep = 0.0f;
            while (!checkCollision(m_Position) && totalStep < 1.0f) {
                m_Position.y -= step;
                totalStep += step;
            }
            m_Position.y += step;
            m_IsOnGround = true;
            m_Velocity.y = 0.0f;
            m_WasSprintingOnJump = false;
        }
        else if (m_Velocity.y > 0) {
            m_Velocity.y = 0.0f;
        }
    }
    else {
        if (m_Velocity.y < -0.001f) {
            m_IsOnGround = false;
        }
        else if (m_IsOnGround) {
            glm::vec3 testPos = m_Position;
            testPos.y -= 0.1f;
            if (!checkCollision(testPos)) {
                m_IsOnGround = false;
            }
        }
    }

    glm::vec3 oldPos = m_Position;
    m_Position.x += m_Velocity.x;
    if (checkCollision(m_Position)) {
        m_Position.x -= m_Velocity.x;
        m_Velocity.x = 0.0f;
    }
    m_Position.z += m_Velocity.z;
    if (checkCollision(m_Position)) {
        m_Position.z -= m_Velocity.z;
        m_Velocity.z = 0.0f;
    }

    if (m_IsSneaking && m_IsOnGround) {
        glm::vec3 testPos = m_Position;
        testPos.y -= 0.1f;
        glm::vec3 aabbMin = testPos + glm::vec3(-Physics::PLAYER_WIDTH / 2.0f, 0.0f, -Physics::PLAYER_WIDTH / 2.0f);
        glm::vec3 aabbMax = testPos + glm::vec3(Physics::PLAYER_WIDTH / 2.0f, 0.0f, Physics::PLAYER_WIDTH / 2.0f);
        glm::ivec3 minBlock = glm::floor(aabbMin);
        glm::ivec3 maxBlock = glm::floor(aabbMax);
        bool groundFound = false;
        for (int x = minBlock.x; x <= maxBlock.x; ++x) {
            for (int z = minBlock.z; z <= maxBlock.z; ++z) {
                if (world.getBlock(x, minBlock.y, z) != 0) {
                    groundFound = true;
                    break;
                }
            }
            if (groundFound) break;
        }

        if (!groundFound) {
            m_Position.x = oldPos.x;
            m_Position.z = oldPos.z;
            m_Velocity.x = 0.0f;
            m_Velocity.z = 0.0f;
        }
    }
}

std::pair<glm::vec3, glm::vec3> Player::getAABB() const {
    float currentHeight = m_IsSneaking ? Physics::CROUCH_PLAYER_HEIGHT : Physics::PLAYER_HEIGHT;
    return std::make_pair(
        m_Position + glm::vec3(-Physics::PLAYER_WIDTH / 2.0f, 0.0f, -Physics::PLAYER_WIDTH / 2.0f),
        m_Position + glm::vec3(Physics::PLAYER_WIDTH / 2.0f, currentHeight, Physics::PLAYER_WIDTH / 2.0f)
    );
}

ItemStack& Player::getSelectedItemStack() {
    return m_Hotbar[m_SelectedHotbarSlot];
}

void Player::setSelectedSlot(int slot) {
    if (slot >= 0 && slot < 9) {
        m_SelectedHotbarSlot = slot;
    }
}