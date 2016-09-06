layout(binding = DOF_INPUT_SAT_TEXTURE_BINDING) uniform usampler2D SAT;

out vec4 FragColor;

void main()
{
    // radius of SAT blur
    int sw = 1;
    int sh = 1;
    
    uvec4 s_ur = texelFetch(SAT, ivec2(gl_FragCoord.xy + ivec2(+sw, +sh)), 0);
    uvec4 s_ul = texelFetch(SAT, ivec2(gl_FragCoord.xy + ivec2(-sw, +sh)), 0);
    uvec4 s_lr = texelFetch(SAT, ivec2(gl_FragCoord.xy + ivec2(+sw, -sh)), 0);
    uvec4 s_ll = texelFetch(SAT, ivec2(gl_FragCoord.xy + ivec2(-sw, -sh)), 0);

    // width/height of the blur from the radius
    int w = (2 * sw) - 1;
    int h = (2 * sw) - 1;
    uvec4 s_filter = (s_ur - s_ul - s_lr + s_ll) / (w*h);

    FragColor = vec4(s_filter) / 255.0;
}