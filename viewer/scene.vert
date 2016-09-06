layout(location = SCENE_POSITION_ATTRIB_LOCATION)
in vec4 Position;

layout(location = SCENE_TEXCOORD_ATTRIB_LOCATION)
in vec2 TexCoord;

layout(location = SCENE_NORMAL_ATTRIB_LOCATION)
in vec3 Normal;

layout(location = SCENE_MW_UNIFORM_LOCATION)
uniform mat4 MW;

layout(location = SCENE_MVP_UNIFORM_LOCATION)
uniform mat4 MVP;

layout(location = SCENE_N_MW_UNIFORM_LOCATION)
uniform mat3 N_MW;

out vec3 fWorldPosition;
out vec2 fTexCoord;
out vec3 fWorldNormal;

void main()
{
    gl_Position = MVP * Position;
    fWorldPosition = (MW * Position).xyz;
    fTexCoord = TexCoord;
    fWorldNormal = N_MW * Normal;
}