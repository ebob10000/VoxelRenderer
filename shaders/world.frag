#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in float vOcclusion;
in float vLight;

uniform sampler2D ourTexture;
uniform bool u_UseAO;
uniform bool u_UseSunlight;

void main()
{
    vec4 texColor = texture(ourTexture, TexCoord);

    float brightness = 1.0;

    if (u_UseSunlight) {
        brightness *= (vLight / 15.0);
    }

    if (u_UseAO) {
        float ao_factor = 1.0 - vOcclusion * 0.2;
        brightness *= ao_factor;
    }
    
    brightness = max(brightness, 0.15); 

    FragColor = vec4(texColor.rgb * brightness, texColor.a);
}