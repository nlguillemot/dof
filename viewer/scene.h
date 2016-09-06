#pragma once

#include "opengl.h"
#include "packed_freelist.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>
#include <map>
#include <string>

struct DiffuseMap
{
    GLuint DiffuseMapTO;
};

struct Material
{
    std::string Name;

    float Ambient[3];
    float Diffuse[3];
    float Specular[3];
    float Shininess;
    uint32_t DiffuseMapID;
};

struct Mesh
{
    std::string Name;

    GLuint MeshVAO;
    GLuint PositionBO;
    GLuint TexCoordBO;
    GLuint NormalBO;
    GLuint IndexBO;

    GLuint IndexCount;
    GLuint VertexCount;

    std::vector<GLDrawElementsIndirectCommand> DrawCommands;
    std::vector<uint32_t> MaterialIDs;
};

struct Transform
{
    glm::vec3 Scale;
    glm::vec3 RotationOrigin;
    glm::quat Rotation;
    glm::vec3 Translation;
};

struct Instance
{
    uint32_t MeshID;
    uint32_t TransformID;
};

struct Camera
{
    // View
    glm::vec3 Eye;
    glm::vec3 Target;
    glm::vec3 Up;

    // Projection
    float FovY;
    float Aspect;
    float ZNear;
};

class Scene
{
public:
    packed_freelist<DiffuseMap> DiffuseMaps;
    packed_freelist<Material> Materials;
    packed_freelist<Mesh> Meshes;
    packed_freelist<Transform> Transforms;
    packed_freelist<Instance> Instances;
    packed_freelist<Camera> Cameras;

    uint32_t MainCameraID;

    void Init();
};

void LoadMeshes(
    Scene& scene,
    const std::string& filename,
    std::vector<uint32_t>* loadedMeshIDs);

void AddInstance(
    Scene& scene,
    uint32_t meshID,
    uint32_t* newInstanceID);