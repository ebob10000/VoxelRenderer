#pragma once
#include "Player.h"
#include "Shader.h"
#include "Frustum.h"
#include "Ray.h"
#include "ItemStack.h"
#include "GraphicsSettings.h"
#include <memory>
#include <optional>
#include <vector>

struct GLFWwindow;
class World;

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
    void renderImGuiHotbar();
    void renderImGuiInventory();
    void renderImGuiHeldItem();
    void renderImGuiFogSettings();
    void renderDebugOverlay();

    void initCrosshair();
    void renderCrosshair();
    void initOutline();
    void renderOutline(const glm::mat4& projection, const glm::mat4& view);
    void applyTextureSettings();
    void findSpawnPosition();
    void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    void mouse_callback(GLFWwindow* window, double xpos, double ypos);
    void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
    void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
    void window_focus_callback(GLFWwindow* window, int focused);

    GLFWwindow* m_Window;
    int m_WindowWidth, m_WindowHeight;
    std::unique_ptr<Shader> m_WorldShader;
    std::unique_ptr<Shader> m_UiShader;
    std::unique_ptr<Shader> m_OutlineShader;
    std::unique_ptr<World> m_World;
    std::unique_ptr<Player> m_Player;
    Frustum m_Frustum;

    bool m_IsPaused = false;
    bool m_ShowInventory = false;
    bool m_WireframeMode = false;
    bool m_ShowDebugOverlay = true;
    int m_MipmapLevel = 4;
    float m_DeltaTime = 0.0f;
    float m_LastFrame = 0.0f;
    int m_RenderedChunks = 0;

    double m_LastMouseX;
    double m_LastMouseY;
    bool m_FirstMouse = true;

    std::optional<RaycastResult> m_HighlightedBlock;

    ItemStack m_HeldItemStack;
    std::vector<BlockID> m_CreativeItems;

    unsigned int m_TextureID;
    unsigned int m_CrosshairVAO, m_CrosshairVBO;
    unsigned int m_OutlineVAO, m_OutlineVBO, m_OutlineEBO;

    // Fog settings
    bool m_UseFog = true;
    glm::vec3 m_FogColor = glm::vec3(0.5f, 0.6f, 0.7f);
    float m_FogDensity = 0.01f;
    float m_FogGradient = 1.5f;
    bool m_AutoFogDensity = true;

    LeafQuality m_LeafQuality = LeafQuality::Fancy;
};