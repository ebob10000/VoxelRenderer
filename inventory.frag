#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D u_Texture;
uniform vec4 u_Color;
uniform bool u_UseTexture;

void main()
{
    vec4 finalColor = u_Color;
    if (u_UseTexture) {
        vec4 texColor = texture(u_Texture, TexCoords);
        if (texColor.a < 0.1)
            discard;
        finalColor = texColor; // Use texture color directly for items
    }
    
    FragColor = finalColor;
}