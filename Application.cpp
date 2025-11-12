#include "Application.h"
#include "Frustum.h"
#include "Ray.h"
#include "Block.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <string>
#include <cstdio>
#include <optional>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

Application::Application() {
    m_WindowWidth = 1280;
    m_WindowHeight = 720;
    m_LastMouseX = m_WindowWidth / 2.0;
    m_LastMouseY = m_WindowHeight / 2.0;

    glfwInit();
    const char* glsl_version = "#version 330";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    m_Window = glfwCreateWindow(m_WindowWidth, m_WindowHeight, "Voxel Engine", NULL, NULL);
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
    glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* w, int b, int a, int m) {
        static_cast<Application*>(glfwGetWindowUserPointer(w))->mouse_button_callback(w, b, a, m);
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
    m_OutlineShader = std::make_unique<Shader>("shaders/outline.vert", "shaders/outline.frag");

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

    initCrosshair();
    initOutline();

    m_World = std::make_unique<World>();

    glm::vec3 tempPos(8.5f, 100.0f, 8.5f);
    m_Player = std::make_unique<Player>(tempPos);

    m_CreativeItems.push_back(BlockID::Stone);
    m_CreativeItems.push_back(BlockID::Dirt);
    m_CreativeItems.push_back(BlockID::Grass);
    m_CreativeItems.push_back(BlockID::Glowstone);
    m_CreativeItems.push_back(BlockID::Bedrock);

    m_World->update(tempPos);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    m_World->update(tempPos);
    findSpawnPosition();
}

Application::~Application() {
    m_World->stopThreads();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &m_CrosshairVAO);
    glDeleteBuffers(1, &m_CrosshairVBO);
    glDeleteVertexArrays(1, &m_OutlineVAO);
    glDeleteBuffers(1, &m_OutlineVBO);
    glDeleteBuffers(1, &m_OutlineEBO);

    glfwDestroyWindow(m_Window);
    glfwTerminate();
}

void Application::initCrosshair() {
    float crosshairVertices[] = {
        -10.f,  0.0f,  10.f,  0.0f,
         0.0f, -10.f,  0.0f,  10.f
    };

    glGenVertexArrays(1, &m_CrosshairVAO);
    glGenBuffers(1, &m_CrosshairVBO);
    glBindVertexArray(m_CrosshairVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_CrosshairVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(crosshairVertices), crosshairVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void Application::initOutline() {
    const float s = 0.502f;
    const float outlineVertices[] = {
        -s, -s, -s, s, -s, -s, s,  s, -s, -s,  s, -s,
        -s, -s,  s, s, -s,  s, s,  s,  s, -s,  s,  s
    };

    const unsigned int outlineIndices[] = {
        0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4,
        0, 4, 1, 5, 2, 6, 3, 7
    };

    glGenVertexArrays(1, &m_OutlineVAO);
    glGenBuffers(1, &m_OutlineVBO);
    glGenBuffers(1, &m_OutlineEBO);

    glBindVertexArray(m_OutlineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_OutlineVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(outlineVertices), outlineVertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_OutlineEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(outlineIndices), outlineIndices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
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
    bool isPaused = m_IsPaused || m_ShowInventory || io.WantCaptureKeyboard;

    m_Player->handleInput(m_Window, isPaused);
}

void Application::update() {
    if (m_IsPaused || m_ShowInventory) {
        m_HighlightedBlock.reset();
        return;
    }

    m_Player->update(m_DeltaTime, *m_World, m_Window);
    m_World->update(m_Player->getPosition());

    glm::vec3 rayOrigin = m_Player->getCamera().position;
    glm::vec3 rayDir = m_Player->getCamera().front;

    m_HighlightedBlock = raycast(rayOrigin, rayDir, *m_World, 6.0f);
}

void Application::render() {
    glClearColor(0.2f, 0.3f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // --- 3D WORLD RENDERING ---
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    if (m_WireframeMode)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    m_WorldShader->use();
    m_WorldShader->setBool("u_UseSunlight", m_World->m_UseSunlight);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_TextureID);

    glm::mat4 projection = glm::perspective(
        glm::radians(m_Player->getCurrentFOV()),
        (float)m_WindowWidth / (float)m_WindowHeight,
        0.1f,
        1000.0f
    );

    glm::mat4 view = m_Player->getCamera().getViewMatrix();

    m_WorldShader->setMat4("projection", projection);
    m_WorldShader->setMat4("view", view);
    m_WorldShader->setMat4("model", glm::mat4(1.0f));

    m_Frustum.update(projection * view);
    m_RenderedChunks = m_World->render(*m_WorldShader, m_Frustum);

    renderOutline(projection, view);

    // --- 2D UI RENDERING ---
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    renderImGui(); // All UI is now handled by ImGui

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Application::renderOutline(const glm::mat4& projection, const glm::mat4& view) {
    if (!m_HighlightedBlock.has_value() || m_ShowInventory) return;

    glDisable(GL_BLEND);

    m_OutlineShader->use();

    glm::vec3 pos = m_HighlightedBlock->blockPosition;
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(pos.x + 0.5f, pos.y + 0.5f, pos.z + 0.5f));

    m_OutlineShader->setMat4("projection", projection);
    m_OutlineShader->setMat4("view", view);
    m_OutlineShader->setMat4("model", model);

    glBindVertexArray(m_OutlineVAO);
    glLineWidth(3.5f);
    glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
    glLineWidth(1.0f);
    glBindVertexArray(0);
}

void Application::renderCrosshair() {
    if (m_IsPaused || m_ShowInventory) return;

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    float x = io.DisplaySize.x / 2.0f;
    float y = io.DisplaySize.y / 2.0f;

    drawList->AddLine(ImVec2(x - 10, y), ImVec2(x + 10, y), IM_COL32(255, 255, 255, 255), 2.0f);
    drawList->AddLine(ImVec2(x, y - 10), ImVec2(x, y + 10), IM_COL32(255, 255, 255, 255), 2.0f);
}

void Application::renderImGuiHotbar() {
    if (m_ShowInventory || m_IsPaused) return;

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoMove;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    // Estimate size based on content. ImGui will auto-resize.
    ImVec2 hotbarSize(9 * 44, 50);
    ImVec2 hotbarPos(viewport->WorkPos.x + (viewport->WorkSize.x - hotbarSize.x) / 2, viewport->WorkPos.y + viewport->WorkSize.y - hotbarSize.y - 10);
    ImGui::SetNextWindowPos(hotbarPos);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (ImGui::Begin("Hotbar", nullptr, flags)) {
        float slotSize = 40.0f;
        ImVec2 uv0, uv1;
        ImVec4 bg_col = ImVec4(0.0f, 0.0f, 0.0f, 0.0f); // transparent bg
        ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

        for (int i = 0; i < 9; ++i) {
            ImGui::PushID(i);
            ImGui::SameLine(0, (i == 0) ? 0 : 4);

            ItemStack& stack = m_Player->m_Hotbar[i];

            // Selection indicator
            ImU32 border_col = (i == m_Player->getSelectedSlot()) ? IM_COL32(255, 255, 255, 255) : IM_COL32(100, 100, 100, 255);
            ImVec2 p0 = ImGui::GetCursorScreenPos();
            ImVec2 p1 = ImVec2(p0.x + slotSize, p0.y + slotSize);
            ImGui::GetWindowDrawList()->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, 128));

            if (!stack.isEmpty()) {
                const auto& data = BlockDataManager::getData(stack.id);
                glm::ivec2 texCoords = data.faces[3].tex_coords; // Top face
                uv0 = ImVec2(texCoords.x / 16.0f, texCoords.y / 16.0f);
                uv1 = ImVec2((texCoords.x + 1) / 16.0f, (texCoords.y + 1) / 16.0f);
                ImGui::Image((ImTextureID)(intptr_t)m_TextureID, ImVec2(slotSize, slotSize), uv0, uv1, tint_col, ImVec4(0, 0, 0, 0));
            }
            else {
                // To keep alignment correct
                ImGui::Dummy(ImVec2(slotSize, slotSize));
            }
            ImGui::GetWindowDrawList()->AddRect(p0, p1, border_col);

            ImGui::PopID();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

void Application::renderImGuiInventory() {
    if (!m_ShowInventory) return;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 window_size(410, 450);
    ImGui::SetNextWindowSize(window_size);
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + (viewport->WorkSize.x - window_size.x) / 2, viewport->WorkPos.y + (viewport->WorkSize.y - window_size.y) / 2));

    if (!ImGui::Begin("Inventory", &m_ShowInventory, ImGuiWindowFlags_NoResize)) {
        ImGui::End();
        return;
    }

    auto draw_item_slot = [&](ItemStack& stack, int id) {
        ImGui::PushID(id);
        float slotSize = 38.0f;
        ImVec2 uv0, uv1;
        ImVec4 bg_col = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
        ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

        if (stack.isEmpty()) {
            if (ImGui::Button("##empty", ImVec2(slotSize, slotSize))) {
                std::swap(m_HeldItemStack, stack);
            }
        }
        else {
            const auto& data = BlockDataManager::getData(stack.id);
            glm::ivec2 texCoords = data.faces[3].tex_coords;
            uv0 = ImVec2(texCoords.x / 16.0f, texCoords.y / 16.0f);
            uv1 = ImVec2((texCoords.x + 1) / 16.0f, (texCoords.y + 1) / 16.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, bg_col);
            if (ImGui::ImageButton("##item", (ImTextureID)(intptr_t)m_TextureID, ImVec2(slotSize, slotSize), uv0, uv1, bg_col, tint_col)) {
                std::swap(m_HeldItemStack, stack);
            }
            ImGui::PopStyleColor();
        }
        ImGui::PopID();
        };


    if (ImGui::BeginTabBar("InventoryTabs")) {
        if (ImGui::BeginTabItem("Creative")) {
            for (size_t i = 0; i < m_CreativeItems.size(); ++i) {
                if (i % 9 != 0) ImGui::SameLine();

                ImGui::PushID((int)i);
                const auto& data = BlockDataManager::getData(m_CreativeItems[i]);
                glm::ivec2 texCoords = data.faces[3].tex_coords;
                ImVec2 uv0 = ImVec2(texCoords.x / 16.0f, texCoords.y / 16.0f);
                ImVec2 uv1 = ImVec2((texCoords.x + 1) / 16.0f, (texCoords.y + 1) / 16.0f);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
                if (ImGui::ImageButton("##creativeitem", (ImTextureID)(intptr_t)m_TextureID, ImVec2(38, 38), uv0, uv1)) {
                    m_HeldItemStack = ItemStack(m_CreativeItems[i], 64);
                }
                ImGui::PopStyleColor();
                ImGui::PopID();
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::Separator();

    ImGui::Text("Inventory");
    for (int i = 0; i < 27; ++i) {
        if (i % 9 != 0) ImGui::SameLine();
        draw_item_slot(m_Player->m_Inventory[i], i);
    }

    ImGui::Separator();

    ImGui::Text("Hotbar");
    for (int i = 0; i < 9; ++i) {
        if (i % 9 != 0) ImGui::SameLine();
        draw_item_slot(m_Player->m_Hotbar[i], i + 27);
    }

    ImGui::End();

    // If window is closed by 'X' button
    if (!m_ShowInventory) {
        m_HeldItemStack.clear();
        glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        m_FirstMouse = true;
    }
}


void Application::renderImGuiHeldItem() {
    if (m_HeldItemStack.isEmpty()) return;

    const auto& data = BlockDataManager::getData(m_HeldItemStack.id);
    glm::ivec2 texCoords = data.faces[3].tex_coords;
    ImVec2 uv0 = ImVec2(texCoords.x / 16.0f, texCoords.y / 16.0f);
    ImVec2 uv1 = ImVec2((texCoords.x + 1) / 16.0f, (texCoords.y + 1) / 16.0f);

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    ImVec2 mousePos = ImGui::GetMousePos();
    float size = 40.0f;
    drawList->AddImage((ImTextureID)(intptr_t)m_TextureID,
        ImVec2(mousePos.x - size / 2, mousePos.y - size / 2),
        ImVec2(mousePos.x + size / 2, mousePos.y + size / 2),
        uv0, uv1);
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
            m_Player->getPosition().x, m_Player->getCamera().position.y, m_Player->getPosition().z);
        if (m_HighlightedBlock.has_value()) {
            ImGui::Text("Target: (%d, %d, %d)", m_HighlightedBlock->blockPosition.x, m_HighlightedBlock->blockPosition.y, m_HighlightedBlock->blockPosition.z);
        }
        else {
            ImGui::Text("Target: None");
        }
        ImGui::Text("On Ground: %s", m_Player->isOnGround() ? "Yes" : "No");
        ImGui::Text("Sprinting: %s", m_Player->isSprinting() ? "Yes" : "No");
        ImGui::Text("Sneaking: %s", m_Player->isSneaking() ? "Yes" : "No");
        ImGui::Text("Render Distance: %d", m_World->m_RenderDistance);
        ImGui::Text("Chunks Rendered: %d / %llu", m_RenderedChunks, m_World->getChunkCount());
        ImGui::Text("Mesher: %s", (m_World->m_UseGreedyMesher && !m_World->m_SmoothLighting) ? "Greedy" : "Simple");
    }
    ImGui::End();
}

void Application::renderImGui() {
    renderDebugOverlay();
    renderCrosshair();
    renderImGuiHotbar();
    renderImGuiInventory();
    renderImGuiHeldItem();

    if (m_IsPaused) {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::Begin("Pause Menu", &m_IsPaused, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("Game is Paused");
        ImGui::Separator();

        if (ImGui::SliderInt("Render Distance", &m_World->m_RenderDistance, 2, 32)) {
            m_World->forceReload();
        }

        if (ImGui::Checkbox("Sunlight", &m_World->m_UseSunlight)) {
            m_World->forceReload();
        }

        if (ImGui::Checkbox("Smooth Lighting", &m_World->m_SmoothLighting)) {
            m_World->forceReload();
        }

        ImGui::BeginDisabled(m_World->m_SmoothLighting);
        if (ImGui::Checkbox("Use Greedy Meshing", &m_World->m_UseGreedyMesher)) {
            m_World->forceReload();
        }
        ImGui::EndDisabled();

        if (ImGui::SliderInt("Mipmap Level", &m_MipmapLevel, 0, 4)) {
            applyTextureSettings();
        }

        ImGui::Separator();
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
}

void Application::key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard) return;

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        if (m_ShowInventory) {
            m_ShowInventory = false;
            m_HeldItemStack.clear();
            glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            m_FirstMouse = true;
        }
        else {
            m_IsPaused = !m_IsPaused;
            glfwSetInputMode(m_Window, GLFW_CURSOR, m_IsPaused ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
            if (!m_IsPaused) m_FirstMouse = true;
        }
    }

    if (key == GLFW_KEY_E && action == GLFW_PRESS && !m_IsPaused) {
        m_ShowInventory = !m_ShowInventory;
        glfwSetInputMode(m_Window, GLFW_CURSOR, m_ShowInventory ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
        if (!m_ShowInventory) {
            m_FirstMouse = true;
            m_HeldItemStack.clear();
        }
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

    if (action == GLFW_PRESS) {
        if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
            m_Player->setSelectedSlot(key - GLFW_KEY_1);
        }
    }
}

void Application::mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse || m_IsPaused || m_ShowInventory) return;

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

void Application::mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

    if (m_IsPaused || m_ShowInventory) return;

    if (m_HighlightedBlock.has_value() && action == GLFW_PRESS) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) { // Break block
            const auto& result = m_HighlightedBlock.value();

            BlockID blockID = (BlockID)m_World->getBlock(result.blockPosition.x, result.blockPosition.y, result.blockPosition.z);
            if (blockID == BlockID::Bedrock) {
                return;
            }

            m_World->setBlock(result.blockPosition.x, result.blockPosition.y, result.blockPosition.z, BlockID::Air);
        }
        else if (button == GLFW_MOUSE_BUTTON_RIGHT) { // Place block
            ItemStack& heldItem = m_Player->getSelectedItemStack();
            if (heldItem.isEmpty()) return;

            const auto& result = m_HighlightedBlock.value();
            glm::ivec3 placePos = result.blockPosition + result.faceNormal;

            glm::vec3 blockMin(placePos);
            glm::vec3 blockMax = blockMin + glm::vec3(1.0f);
            auto playerAABB = m_Player->getAABB();

            bool intersectX = playerAABB.first.x < blockMax.x && playerAABB.second.x > blockMin.x;
            bool intersectY = playerAABB.first.y < blockMax.y && playerAABB.second.y > blockMin.y;
            bool intersectZ = playerAABB.first.z < blockMax.z && playerAABB.second.z > blockMin.z;

            if (!(intersectX && intersectY && intersectZ)) {
                m_World->setBlock(placePos.x, placePos.y, placePos.z, heldItem.id);
            }
        }
    }
}

void Application::scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureMouse) return;

    if (m_IsPaused || m_ShowInventory) return;

    if (m_Player->isFlying()) {
        float speedMultiplier = 1.1f;
        if (yoffset > 0) m_Player->getCamera().speed *= speedMultiplier;
        else m_Player->getCamera().speed /= speedMultiplier;
        if (m_Player->getCamera().speed < 0.1f) m_Player->getCamera().speed = 0.1f;
    }
    else {
        int currentSlot = m_Player->getSelectedSlot();
        if (yoffset > 0) { // Scroll Up
            currentSlot = (currentSlot - 1 + 9) % 9;
        }
        else if (yoffset < 0) { // Scroll Down
            currentSlot = (currentSlot + 1) % 9;
        }
        m_Player->setSelectedSlot(currentSlot);
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
    m_WindowWidth = width;
    m_WindowHeight = height;
}

void Application::findSpawnPosition() {
    float spawnX = 8.5f;
    float spawnZ = 8.5f;

    int spawnY = CHUNK_HEIGHT - 1;
    for (int y = CHUNK_HEIGHT - 1; y >= 0; --y) {
        if (m_World->getBlock((int)spawnX, y, (int)spawnZ) != 0) {
            spawnY = y + 1;
            break;
        }
    }

    if (spawnY == CHUNK_HEIGHT - 1) {
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

    glm::vec3 spawnPos(spawnX, spawnY, spawnZ);
    m_Player = std::make_unique<Player>(spawnPos);
    std::cout << "Player spawned at: (" << spawnPos.x << ", " << spawnPos.y << ", " << spawnPos.z << ")" << std::endl;
}