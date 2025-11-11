#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;
layout (location = 2) in float aAO;
layout (location = 3) in float aLight;
layout (location = 4) in float aFaceIndex;

out vec2 TexCoords;
out float AO;
out float Light;
out float FaceIndex;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * vec4(aPos, 1.0);
    
    TexCoords = aTexCoords;
    AO = aAO;
    Light = aLight;
    FaceIndex = aFaceIndex;
}
