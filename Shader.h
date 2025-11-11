#pragma once
#include <string>
#include <glm/glm.hpp>
#include <glad/glad.h>

class Shader {
public:
    unsigned int ID;

    Shader(const char* vertexPath, const char* fragmentPath);

    void use();
    void setMat4(const std::string& name, const glm::mat4& mat) const;
    void setBool(const std::string& name, bool value) const;
    void setVec3(const std::string& name, const glm::vec3& value) const;
};