#include "Application.h"
#include "Frustum.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <string>
#include <cstdio>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

Application::Application() {
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
    glfwSetWindowFocusCallback(m_Window, [](GLFWwindow* w, int f) {
        static_cast<Application*>(glfwGetWindowUserPointer(w))->window_focus_callback(w, f);
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load("textures/atlas.png", &width, &height, &nrChannels, 3);

    if (data) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else {
        std::cout << "Failed to load texture" << std::endl;
    }
    stbi_image_free(data);
    applyTextureSettings();

    // Create world first
    m_World = std::make_unique<World>();

    // Create player at a temporary safe position high up
    glm::vec3 tempPos(8.5f, 100.0f, 8.5f);
    m_Player = std::make_unique<Player>(tempPos);

    // Trigger initial chunk generation around spawn
    m_World->update(tempPos);

    // Wait for initial chunks to generate and mesh
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Process any finished meshes
    m_World->update(tempPos);

    // Now find the proper spawn position
    findSpawnPosition();
}

Application::~Application() {
    m_World->stopThreads();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(m_Window);
    glfwTerminate();
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
            sprintf_s(title, sizeof(title), "Voxel Engine | FPS: %.0f | Chunks: %llu", fps,
                (unsigned long long)m_World->getChunkCount());
            glfwSetWindowTitle(m_Window, title);
            frameCount = 0;
            lastTime = currentTime;
        }

        glfwSwapBuffers(m_Window);
        glfwPollEvents();
    }
}

void Application::processInput() {
    if (glfwGetKey(m_Window, GLFW_KEY_Q) == GLFW_PRESS) {
        glfwSetWindowShouldClose(m_Window, true);
    }

    ImGuiIO& io = ImGui::GetIO();
    bool isPaused = m_IsPaused || io.WantCaptureKeyboard;

    m_Player->handleInput(m_Window, isPaused);
}

void Application::update() {
    if (m_IsPaused) return;

    m_Player->update(m_DeltaTime, *m_World, m_Window);
    m_World->update(m_Player->getPosition());
}

void Application::render() {
    glClearColor(0.2f, 0.3f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (m_WireframeMode)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    m_WorldShader->use();
    m_WorldShader->setBool("u_UseAO", m_World->m_UseAO);
    m_WorldShader->setBool("u_UseSunlight", m_World->m_UseSunlight);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_TextureID);

    glm::mat4 projection = glm::perspective(
        glm::radians(m_Player->getFOV()),
        1280.0f / 720.0f,
        0.1f,
        1000.0f
    );

    glm::mat4 view = glm::lookAt(
        m_Player->getRenderPosition(),
        m_Player->getRenderPosition() + m_Player->getCamera().front,
        m_Player->getCamera().up
    );

    m_WorldShader->setMat4("projection", projection);
    m_WorldShader->setMat4("view", view);
    m_WorldShader->setMat4("model", glm::mat4(1.0f));

    m_Frustum.update(projection * view);
    m_RenderedChunks = m_World->render(*m_WorldShader, m_Frustum);

    renderDebugOverlay();
    renderImGui();
}

void Application::renderDebugOverlay() {
    if (!m_ShowDebugOverlay) return;

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
    const float PAD = 10.0f;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 work_pos = viewport->WorkPos;
    ImVec2 window_pos(work_pos.x + PAD, work_pos.y + PAD);
    ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.35f);

    if (ImGui::Begin("Debug Info", &m_ShowDebugOverlay, window_flags)) {
        float fps = (m_DeltaTime > 0.0f) ? (1.0f / m_DeltaTime) : 0.0f;
        ImGui::Text("FPS: %.1f", fps);
        ImGui::Separator();
        ImGui::Text("Position: (%.1f, %.1f, %.1f)",
            m_Player->getPosition().x, m_Player->getPosition().y, m_Player->getPosition().z);
        ImGui::Text("On Ground: %s", m_Player->isOnGround() ? "Yes" : "No");
        ImGui::Text("Sprinting: %s", m_Player->isSprinting() ? "Yes" : "No");
        ImGui::Text("Sneaking: %s", m_Player->isSneaking() ? "Yes" : "No");
        ImGui::Text("Render Distance: %d", m_World->m_RenderDistance);
        ImGui::Text("Chunks Rendered: %d / %llu", m_RenderedChunks, m_World->getChunkCount());
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

        if (ImGui::SliderInt("Mipmap Level", &m_MipmapLevel, 0, 4)) {
            applyTextureSettings();
        }

        ImGui::Separator();
        // FIX: Store isFlying in temp variable to get addressable l-value
        bool isFlying = m_Player->isFlying();
        if (ImGui::Checkbox("Flying Mode", &isFlying)) {
            m_Player->setFlying(isFlying);
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

void Application::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard) return;

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        m_IsPaused = !m_IsPaused;
        glfwSetInputMode(m_Window, GLFW_CURSOR, m_IsPaused ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
        if (!m_IsPaused) m_FirstMouse = true;
    }

    if (key == GLFW_KEY_F && action == GLFW_PRESS) {
        m_WireframeMode = !m_WireframeMode;
    }

    if (key == GLFW_KEY_F3 && action == GLFW_PRESS) {
        m_ShowDebugOverlay = !m_ShowDebugOverlay;
    }

    if (key == GLFW_KEY_G && action == GLFW_PRESS) {
        m_Player->setFlying(!m_Player->isFlying());
    }
}

void Application::mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse || m_IsPaused) return;

    if (m_FirstMouse) {
        m_LastMouseX = xpos;
        m_LastMouseY = ypos;
        m_FirstMouse = false;
    }

    float xoffset = static_cast<float>(xpos - m_LastMouseX);
    float yoffset = static_cast<float>(m_LastMouseY - ypos);
    m_LastMouseX = xpos;
    m_LastMouseY = ypos;

    xoffset *= m_Player->getCamera().sensitivity;
    yoffset *= m_Player->getCamera().sensitivity;

    m_Player->getCamera().yaw += xoffset;
    m_Player->getCamera().pitch += yoffset;

    if (m_Player->getCamera().pitch > 89.0f) m_Player->getCamera().pitch = 89.0f;
    if (m_Player->getCamera().pitch < -89.0f) m_Player->getCamera().pitch = -89.0f;

    m_Player->getCamera().updateCameraVectors();
}

void Application::scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

    if (m_Player->isFlying()) {
        float speedMultiplier = 1.1f;
        if (yoffset > 0) m_Player->getCamera().speed *= speedMultiplier;
        else m_Player->getCamera().speed /= speedMultiplier;

        if (m_Player->getCamera().speed < 0.1f) m_Player->getCamera().speed = 0.1f;
    }
}

void Application::applyTextureSettings() {
    glBindTexture(GL_TEXTURE_2D, m_TextureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, m_MipmapLevel);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Application::window_focus_callback(GLFWwindow* window, int focused) {
    if (focused) {
        m_FirstMouse = true;
    }
}

void Application::framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void Application::findSpawnPosition() {
    float spawnX = 8.5f;
    float spawnZ = 8.5f;

    // Find the highest solid block at spawn coordinates
    int spawnY = CHUNK_HEIGHT - 1;
    for (int y = CHUNK_HEIGHT - 1; y >= 0; --y) {
        if (m_World->getBlock((int)spawnX, y, (int)spawnZ) != 0) {
            spawnY = y + 1; // Spawn one block above the solid block
            break;
        }
    }

    // If no solid block found, use a default height
    if (spawnY == CHUNK_HEIGHT - 1) {
        // Use noise to calculate terrain height as fallback
        FastNoiseLite noise;
        noise.SetSeed(1337);
        noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        noise.SetFrequency(0.005f);
        noise.SetFractalType(FastNoiseLite::FractalType_FBm);
        noise.SetFractalOctaves(5);
        noise.SetFractalLacunarity(2.0f);
        noise.SetFractalGain(0.5f);

        float noiseValue = noise.GetNoise(spawnX, spawnZ);
        spawnY = static_cast<int>(((noiseValue + 1.0f) / 2.0f) * (CHUNK_HEIGHT - 10) + 5);
    }

    // Create final spawn position with eye height
    glm::vec3 spawnPos(spawnX, spawnY + Physics::EYE_HEIGHT, spawnZ);

    // Recreate player at correct position
    m_Player = std::make_unique<Player>(spawnPos);

    std::cout << "Player spawned at: (" << spawnPos.x << ", " << spawnPos.y << ", " << spawnPos.z << ")" << std::endl;
}