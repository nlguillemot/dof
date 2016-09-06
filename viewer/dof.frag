layout(binding = DOF_SAT_TEXTURE_BINDING) uniform usampler2D SAT;
layout(binding = DOF_DEPTH_TEXTURE_BINDING) uniform sampler2D Depth;

layout(location = DOF_ZNEAR_UNIFORM_LOCATION) uniform float ZNear;
layout(location = DOF_FOCUS_UNIFORM_LOCATION) uniform float Focus;

out vec4 FragColor;

#define UR 0
#define UL 1
#define LR 2
#define LL 3

void main()
{
    ivec2 sz = textureSize(SAT, 0);

    // radius of SAT blur
    int sw, sh;
    
    // sample ndc depth
    float depth = texelFetch(Depth, ivec2(gl_FragCoord.xy), 0).x;

    if (depth == 0.0)
    {
        // "infinitely far", so background.
        discard;
    }

    // convert to eye space depth
    depth = ZNear / depth;

    sw = sh = int(abs(depth - Focus));
    
    // each tap is offset from the box filter differently
    ivec2 tap_offsets[4];
    tap_offsets[UR] = ivec2(0, 0);
    tap_offsets[UL] = ivec2(1, 0);
    tap_offsets[LR] = ivec2(0, 1);
    tap_offsets[LL] = ivec2(1, 1);

    // the 4 locations that will be sampled ("tapped")
    ivec2 taps[4];
    taps[UR] = ivec2(gl_FragCoord.xy) + ivec2(+sw, +sh) - tap_offsets[UR];
    taps[UL] = ivec2(gl_FragCoord.xy) + ivec2(-sw, +sh) - tap_offsets[UL];
    taps[LR] = ivec2(gl_FragCoord.xy) + ivec2(+sw, -sh) - tap_offsets[LR];
    taps[LL] = ivec2(gl_FragCoord.xy) + ivec2(-sw, -sh) - tap_offsets[LL];

    // sample the 4 corners of the SAT region
    // also translate the taps to the corners of the sampling radius (to compute box area later)
    uvec4 sat[4];
    for (int i = 0; i < 4; i++)
    {
        // handle out-of-bounds by clamping
        if (any(lessThan(taps[i], ivec2(0)))) {
            sat[i] = uvec4(0);
        }
        else if (any(greaterThanEqual(taps[i], sz))) {
            sat[i] = texelFetch(SAT, min(taps[i], sz - ivec2(1)), 0);
        }
        else {
            sat[i] = texelFetch(SAT, taps[i], 0);
        }

        // translate taps to be within the corners of the box filter's rectangle
        ivec2 clamped_tap = clamp(taps[i], ivec2(0), sz - ivec2(1));
        if (clamped_tap != taps[i]) {
            taps[i] = clamped_tap;
        }
        else {
            taps[i] = taps[i] + tap_offsets[i];
        }
    }

    // the area of the blur might have changed from the clamping of the box
    int boxsz = (taps[UR].x + 1 - taps[LL].x) * (taps[UR].y + 1 - taps[LL].y);

    // perform a box filter
    vec4 sat_box = vec4(sat[UR] - sat[UL] - sat[LR] + sat[LL])  / float(boxsz);

    FragColor = vec4(sat_box) / 255.0;
}