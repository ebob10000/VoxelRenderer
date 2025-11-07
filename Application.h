#pragma once
#include "Camera.h"
#include "Chunk.h"
#include "Shader.h"
#include <memory>

// Forward declare GLFWwindow
struct GLFWwindow;

class Application {
public:
    Application();
    void run();

private:
    void processInput();
    void update();
    void render();

    // Member variables - NO globals
    GLFWwindow* m_Window;
    std::unique_ptr<Shader> m_WorldShader;
    std::unique_ptr<Shader> m_UiShader;
    std::unique_ptr<Chunk> m_Chunk;
    Camera m_Camera;

    bool m_IsPaused = false;
    bool m_WireframeMode = false;
    float m_DeltaTime = 0.0f;
    float m_LastFrame = 0.0f;

    double m_LastMouseX = 1280.0 / 2.0;
    double m_LastMouseY = 720.0 / 2.0;
    bool m_FirstMouse = true;

    unsigned int m_UiVAO;
    unsigned int m_TextureID;

    void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    void mouse_callback(GLFWwindow* window, double xpos, double ypos);
    void scroll_callback(GLFWwindow* window, double xoffset, double yoffset); // Typo fixed, correct arguments
};