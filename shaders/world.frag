#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in float AO;
in float Light;

uniform sampler2D u_Texture;
uniform bool u_UseAO;
uniform bool u_UseSunlight;

void main()
{
    vec4 texColor = texture(u_Texture, TexCoords);

    // Don't render transparent pixels
    if(texColor.a < 0.1)
        discard;

    float aoFactor = 1.0;
    if (u_UseAO) {
        // Higher AO value means more darkness. Adjust the 0.25 to control strength.
        aoFactor = 1.0 - AO * 0.25;
    }

    float lightFactor = 1.0;
    if (u_UseSunlight) {
        // Map light level from [0, 15] to a brightness multiplier.
        lightFactor = Light / 15.0;
    }
    
    // THE FIX IS HERE: Only modify the .rgb channels, preserve the original .a
    FragColor = vec4(texColor.rgb * aoFactor * lightFactor, texColor.a);
}