#include "Application.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <string>
#include <cstdio>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

Application::Application() : m_Camera(glm::vec3(8.0f, 25.0f, 8.0f)) {
    glfwInit();
    const char* glsl_version = "#version 330";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    m_Window = glfwCreateWindow(1280, 720, "Voxel Engine", NULL, NULL);
    glfwMakeContextCurrent(m_Window);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glfwSwapInterval(0);

    glfwSetWindowUserPointer(m_Window, this);

    glfwSetKeyCallback(m_Window, [](GLFWwindow* w, int k, int s, int a, int m) {
        static_cast<Application*>(glfwGetWindowUserPointer(w))->key_callback(w, k, s, a, m);
        });
    glfwSetCursorPosCallback(m_Window, [](GLFWwindow* w, double x, double y) {
        static_cast<Application*>(glfwGetWindowUserPointer(w))->mouse_callback(w, x, y);
        });
    glfwSetScrollCallback(m_Window, [](GLFWwindow* w, double x, double y) {
        static_cast<Application*>(glfwGetWindowUserPointer(w))->scroll_callback(w, x, y);
        });
    glfwSetFramebufferSizeCallback(m_Window, [](GLFWwindow* w, int width, int height) {
        static_cast<Application*>(glfwGetWindowUserPointer(w))->framebuffer_size_callback(w, width, height);
        });

    initImGui();

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_WorldShader = std::make_unique<Shader>("shaders/world.vert", "shaders/world.frag");
    m_UiShader = std::make_unique<Shader>("shaders/ui.vert", "shaders/ui.frag");

    glGenTextures(1, &m_TextureID);
    glBindTexture(GL_TEXTURE_2D, m_TextureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load("textures/stone.png", &width, &height, &nrChannels, 3);
    if (data) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else {
        std::cout << "Failed to load texture" << std::endl;
    }
    stbi_image_free(data);

    m_World = std::make_unique<World>();
}

Application::~Application() {
    m_World->stopThreads();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void Application::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(m_Window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

void Application::run() {
    double lastTime = glfwGetTime();
    int frameCount = 0;
    while (!glfwWindowShouldClose(m_Window)) {
        float currentFrame = (float)glfwGetTime();
        m_DeltaTime = currentFrame - m_LastFrame;
        m_LastFrame = currentFrame;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        processInput();
        update();
        render();

        double currentTime = glfwGetTime();
        frameCount++;
        if (currentTime - lastTime >= 1.0) {
            char title[256];
            double fps = frameCount > 0 ? (double)frameCount / (currentTime - lastTime) : 0.0;
            sprintf_s(title, sizeof(title), "Voxel Engine | FPS: %.0f | Chunks: %llu", fps, (unsigned long long)m_World->getChunkCount());
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

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard) return;

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
    if (!m_IsPaused) {
        m_World->update(m_Camera.position);
    }
}

void Application::render() {
    glClearColor(0.2f, 0.3f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (m_WireframeMode) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    m_WorldShader->use();
    m_WorldShader->setBool("u_UseAO", m_World->m_UseAO);
    m_WorldShader->setBool("u_UseSunlight", m_World->m_UseSunlight);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_TextureID);

    glm::mat4 projection = glm::perspective(glm::radians(m_Camera.fov), 1280.0f / 720.0f, 0.1f, 1000.0f);
    glm::mat4 view = m_Camera.getViewMatrix();
    m_WorldShader->setMat4("projection", projection);
    m_WorldShader->setMat4("view", view);
    m_WorldShader->setMat4("model", glm::mat4(1.0f));

    m_World->render(*m_WorldShader);

    renderDebugOverlay();
    renderImGui();
}

void Application::renderDebugOverlay() {
    if (!m_ShowDebugOverlay) return;

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
    const float PAD = 10.0f;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 work_pos = viewport->WorkPos;
    ImVec2 window_pos;
    window_pos.x = work_pos.x + PAD;
    window_pos.y = work_pos.y + PAD;
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.35f);

    if (ImGui::Begin("Debug Info", &m_ShowDebugOverlay, window_flags)) {
        float fps = (m_DeltaTime > 0.0f) ? (1.0f / m_DeltaTime) : 0.0f;
        ImGui::Text("FPS: %.1f", fps);
        ImGui::Separator();
        ImGui::Text("Camera Position: (%.1f, %.1f, %.1f)", m_Camera.position.x, m_Camera.position.y, m_Camera.position.z);
        ImGui::Text("Camera Front: (%.1f, %.1f, %.1f)", m_Camera.front.x, m_Camera.front.y, m_Camera.front.z);
        ImGui::Text("Render Distance: %d", m_World->m_RenderDistance);
        ImGui::Text("Mesher: %s", m_World->m_UseGreedyMesher ? "Greedy" : "Simple");
    }
    ImGui::End();
}

void Application::renderImGui() {
    if (m_IsPaused) {
        ImGui::Begin("Pause Menu");
        ImGui::Text("Game is Paused");
        ImGui::Separator();

        if (ImGui::SliderInt("Render Distance", &m_World->m_RenderDistance, 2, 32)) {
            m_World->forceReload();
        }

        if (ImGui::Checkbox("Use Greedy Meshing", &m_World->m_UseGreedyMesher)) {
            m_World->forceReload();
        }

        if (ImGui::Checkbox("Ambient Occlusion", &m_World->m_UseAO)) {
            m_World->forceReload();
        }

        if (ImGui::Checkbox("Sunlight", &m_World->m_UseSunlight)) {
            m_World->forceReload();
        }

        ImGui::Separator();
        if (ImGui::Button("Quit")) {
            glfwSetWindowShouldClose(m_Window, true);
        }
        ImGui::End();
    }
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Application::framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void Application::key_callback(GLFWwindow* window, int key, int scode, int action, int mods) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard) return;

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
    if (key == GLFW_KEY_F3 && action == GLFW_PRESS) {
        m_ShowDebugOverlay = !m_ShowDebugOverlay;
    }
}

void Application::mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

    if (m_IsPaused) return;
    if (m_FirstMouse) {
        m_LastMouseX = xpos;
        m_LastMouseY = ypos;
        m_FirstMouse = false;
    }
    float xoffset = (float)xpos - (float)m_LastMouseX;
    float yoffset = (float)m_LastMouseY - (float)ypos;
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
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

    float speedMultiplier = 1.1f;
    if (yoffset > 0) m_Camera.speed *= speedMultiplier;
    else m_Camera.speed /= speedMultiplier;
    if (m_Camera.speed < 0.1f) m_Camera.speed = 0.1f;
}