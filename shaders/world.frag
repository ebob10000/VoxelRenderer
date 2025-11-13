#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in float AO;
in float Light;
in float FaceIndex;
in vec3 FragPos;

uniform sampler2D u_Texture;
uniform bool u_UseAO;
uniform bool u_UseSunlight;

uniform bool u_UseFog;
uniform vec3 u_FogColor;
uniform float u_FogDensity;
uniform float u_FogGradient;
uniform vec3 u_CameraPos;

void main()
{
    vec4 texColor = texture(u_Texture, TexCoords);
    
    if(texColor.a < 0.1)
        discard;
    
    float aoFactor = 1.0;
    if (u_UseAO) {
        aoFactor = 1.0 - AO * 0.1;
    }

    float directionalShade = 1.0;
    if (u_UseSunlight) {
        int face = int(FaceIndex + 0.5);
        if (face == 0) {        // -X (Left)
            directionalShade = 0.75;
        } else if (face == 1) { // +X (Right)
            directionalShade = 0.75;
        } else if (face == 2) { // -Y (Bottom)
            directionalShade = 0.5;
        } else if (face == 3) { // +Y (Top)
            directionalShade = 1.0;
        } else if (face == 4) { // -Z (Back)
            directionalShade = 0.85;
        } else if (face == 5) { // +Z (Front)
            directionalShade = 0.85;
        }
    }

    float lightFactor = 1.0;
    if (u_UseSunlight) {
        lightFactor = Light / 15.0;
        lightFactor = max(lightFactor, 0.1);
    }

    float finalBrightness = aoFactor * directionalShade * lightFactor;
    vec3 litColor = texColor.rgb * finalBrightness;

    vec3 finalColor = litColor;
    if (u_UseFog) {
        float dist = length(FragPos - u_CameraPos);
        float fogFactor = exp(-pow(dist * u_FogDensity, u_FogGradient));
        fogFactor = clamp(fogFactor, 0.0, 1.0);
        finalColor = mix(u_FogColor, litColor, fogFactor);
    }
    
    FragColor = vec4(finalColor, texColor.a);
}