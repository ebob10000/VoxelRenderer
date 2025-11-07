#include "Application.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <string>
#include <cstdio>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

Application::Application() : m_Camera(glm::vec3(8.0f, 12.0f, 25.0f)) {
    // --- GLFW/GLAD Initialization ---
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    m_Window = glfwCreateWindow(1280, 720, "Voxel Engine", NULL, NULL);
    glfwMakeContextCurrent(m_Window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glfwSwapInterval(0); // Disable VSync

    glfwSetWindowUserPointer(m_Window, this);

    // --- Set Callbacks using Lambdas ---
    glfwSetFramebufferSizeCallback(m_Window, [](GLFWwindow* w, int width, int height) {
        static_cast<Application*>(glfwGetWindowUserPointer(w))->framebuffer_size_callback(w, width, height);
        });
    glfwSetKeyCallback(m_Window, [](GLFWwindow* w, int k, int s, int a, int m) {
        static_cast<Application*>(glfwGetWindowUserPointer(w))->key_callback(w, k, s, a, m);
        });
    glfwSetCursorPosCallback(m_Window, [](GLFWwindow* w, double x, double y) {
        static_cast<Application*>(glfwGetWindowUserPointer(w))->mouse_callback(w, x, y);
        });
    glfwSetScrollCallback(m_Window, [](GLFWwindow* w, double x, double y) {
        static_cast<Application*>(glfwGetWindowUserPointer(w))->scroll_callback(w, x, y);
        });

    glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // --- OpenGL State ---
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // --- Load Shaders ---
    m_WorldShader = std::make_unique<Shader>("shaders/world.vert", "shaders/world.frag");
    m_UiShader = std::make_unique<Shader>("shaders/ui.vert", "shaders/ui.frag");

    // --- Texture Loading ---
    glGenTextures(1, &m_TextureID);
    glBindTexture(GL_TEXTURE_2D, m_TextureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    // Force loading as 3 channels (RGB) to discard alpha
    unsigned char* data = stbi_load("textures/stone.png", &width, &height, &nrChannels, 3);
    if (data) {
        // Tell OpenGL the source data is RGB
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else {
        std::cout << "Failed to load texture" << std::endl;
    }
    stbi_image_free(data);

    // --- Create Game Objects ---
    m_Chunk = std::make_unique<Chunk>();
    m_Chunk->generateMesh();
    m_Chunk->uploadMesh();

    // --- UI Quad ---
    float uiVertices[] = { -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f, -1.0f };
    unsigned int uiVBO;
    glGenVertexArrays(1, &m_UiVAO);
    glGenBuffers(1, &uiVBO);
    glBindVertexArray(m_UiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(uiVertices), &uiVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Application::run() {
    double lastTime = glfwGetTime();
    int frameCount = 0;
    while (!glfwWindowShouldClose(m_Window)) {
        float currentFrame = (float)glfwGetTime();
        m_DeltaTime = currentFrame - m_LastFrame;
        m_LastFrame = currentFrame;

        processInput();
        update();
        render();

        double currentTime = glfwGetTime();
        frameCount++;
        if (currentTime - lastTime >= 1.0) {
            char title[256];
            double fps = frameCount > 0 ? (double)frameCount / (currentTime - lastTime) : 0.0;
            sprintf_s(title, sizeof(title), "Voxel Engine | FPS: %.0f | Frame Time: %.2f ms", fps, 1000.0 / fps);
            glfwSetWindowTitle(m_Window, title);
            frameCount = 0;
            lastTime = currentTime;
        }
        glfwSwapBuffers(m_Window);
        glfwPollEvents();
    }
    glfwTerminate();
}

void Application::processInput() {
    if (glfwGetKey(m_Window, GLFW_KEY_Q) == GLFW_PRESS) {
        glfwSetWindowShouldClose(m_Window, true);
    }
    if (m_IsPaused) return;
    const float cameraSpeed = m_Camera.speed * m_DeltaTime;
    if (glfwGetKey(m_Window, GLFW_KEY_W) == GLFW_PRESS) m_Camera.position += cameraSpeed * m_Camera.front;
    if (glfwGetKey(m_Window, GLFW_KEY_S) == GLFW_PRESS) m_Camera.position -= cameraSpeed * m_Camera.front;
    if (glfwGetKey(m_Window, GLFW_KEY_A) == GLFW_PRESS) m_Camera.position -= glm::normalize(glm::cross(m_Camera.front, m_Camera.up)) * cameraSpeed;
    if (glfwGetKey(m_Window, GLFW_KEY_D) == GLFW_PRESS) m_Camera.position += glm::normalize(glm::cross(m_Camera.front, m_Camera.up)) * cameraSpeed;
    if (glfwGetKey(m_Window, GLFW_KEY_SPACE) == GLFW_PRESS) m_Camera.position += cameraSpeed * m_Camera.up;
    if (glfwGetKey(m_Window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) m_Camera.position -= cameraSpeed * m_Camera.up;
}

void Application::update() {
    // Game logic would go here in the future
}

void Application::render() {
    glClearColor(0.2f, 0.3f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (m_WireframeMode) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // Render 3D World
    m_WorldShader->use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_TextureID);

    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
    glm::mat4 view = m_Camera.getViewMatrix();
    m_WorldShader->setMat4("projection", projection);
    m_WorldShader->setMat4("view", view);
    m_WorldShader->setMat4("model", glm::mat4(1.0f));
    m_Chunk->draw();

    // Render 2D UI if paused
    if (m_IsPaused) {
        glDisable(GL_DEPTH_TEST);
        m_UiShader->use();
        glBindVertexArray(m_UiVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glEnable(GL_DEPTH_TEST);
    }
}

void Application::framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void Application::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        m_IsPaused = !m_IsPaused;
        if (m_IsPaused) {
            glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        else {
            glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            m_FirstMouse = true;
        }
    }
    if (key == GLFW_KEY_F && action == GLFW_PRESS) {
        m_WireframeMode = !m_WireframeMode;
    }
}

void Application::mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (m_IsPaused) return;
    if (m_FirstMouse) {
        m_LastMouseX = xpos;
        m_LastMouseY = ypos;
        m_FirstMouse = false;
    }
    float xoffset = (float)xpos - m_LastMouseX;
    float yoffset = m_LastMouseY - (float)ypos;
    m_LastMouseX = xpos;
    m_LastMouseY = ypos;
    xoffset *= m_Camera.sensitivity;
    yoffset *= m_Camera.sensitivity;
    m_Camera.yaw += xoffset;
    m_Camera.pitch += yoffset;
    if (m_Camera.pitch > 89.0f) m_Camera.pitch = 89.0f;
    if (m_Camera.pitch < -89.0f) m_Camera.pitch = -89.0f;
    m_Camera.updateCameraVectors();
}

void Application::scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    m_Camera.speed += (float)yoffset;
    if (m_Camera.speed < 1.0f) m_Camera.speed = 1.0f;
    if (m_Camera.speed > 45.0f) m_Camera.speed = 45.0f;
}