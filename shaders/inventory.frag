#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D u_Texture;
uniform vec4 u_Color;

void main()
{
    vec4 texColor = texture(u_Texture, TexCoords);
    if (texColor.a < 0.1)
        discard;

    FragColor = texColor * u_Color;
}