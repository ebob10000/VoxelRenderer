#pragma once
#include "Player.h"
#include "World.h"
#include "Shader.h"
#include "Frustum.h"
#include <memory>

struct GLFWwindow;

class Application {
public:
    Application();
    ~Application();
    void run();

private:
    void processInput();
    void update();
    void render();
    void initImGui();
    void renderImGui();
    void renderDebugOverlay();
    void applyTextureSettings();
    void findSpawnPosition();
    void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    void mouse_callback(GLFWwindow* window, double xpos, double ypos);
    void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
    void window_focus_callback(GLFWwindow* window, int focused);

    GLFWwindow* m_Window;
    std::unique_ptr<Shader> m_WorldShader;
    std::unique_ptr<Shader> m_UiShader;
    std::unique_ptr<World> m_World;
    std::unique_ptr<Player> m_Player;
    Frustum m_Frustum;

    // Core application state
    bool m_IsPaused = false;
    bool m_WireframeMode = false;
    bool m_ShowDebugOverlay = true;
    int m_MipmapLevel = 4;
    float m_DeltaTime = 0.0f;
    float m_LastFrame = 0.0f;
    int m_RenderedChunks = 0;

    // Mouse input
    double m_LastMouseX = 1280.0 / 2.0;
    double m_LastMouseY = 720.0 / 2.0;
    bool m_FirstMouse = true;

    unsigned int m_TextureID;
};