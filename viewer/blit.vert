// This shader renders a fullscreen triangle and outputs texcoords
// Call it by rendering 1 triangle, hook up your own fragment shader to it.
// Texcoords follow GL conventions.

layout(location = BLIT_TEXCOORD_VARYING_LOCATION) out vec2 oTexCoord;

void main()
{
    vec4 pos;
    pos.x = float(gl_VertexID / 2) * 4.0 - 1.0;
    pos.y = float(gl_VertexID % 2) * 4.0 - 1.0;
    pos.z = 0.0;
    pos.w = 1.0;

    vec2 tc;
    tc.x = float(gl_VertexID / 2) * 2.0;
    tc.y = float(gl_VertexID % 2) * 2.0;

    gl_Position = pos;
    oTexCoord = tc;
}