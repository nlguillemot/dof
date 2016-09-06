#include "scene.h"

#include "preamble.glsl"

#include "tiny_obj_loader.h"
#include "stb_image.h"

void Scene::Init()
{
    DiffuseMaps = packed_freelist<DiffuseMap>(512);
    Materials = packed_freelist<Material>(512);
    Meshes = packed_freelist<Mesh>(512);
    Transforms = packed_freelist<Transform>(4096);
    Instances = packed_freelist<Instance>(4096);
    Cameras = packed_freelist<Camera>(32);
}

void LoadMeshes(
    Scene& scene,
    const std::string& filename,
    std::vector<uint32_t>* loadedMeshIDs)
{
    // assume mtl is in the same folder as the obj
    std::string mtl_basepath = filename;
    size_t last_slash = mtl_basepath.find_last_of("/");
    if (last_slash == std::string::npos)
        mtl_basepath = "./";
    else
        mtl_basepath = mtl_basepath.substr(0, last_slash + 1);

    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;
    if (!tinyobj::LoadObj(
        shapes, materials, err, 
        filename.c_str(), mtl_basepath.c_str(),
        tinyobj::triangulation | tinyobj::calculate_normals))
    {
        fprintf(stderr, "tinyobj::LoadObj(%s) error: %s\n", filename.c_str(), err.c_str());
        return;
    }
    
    if (!err.empty())
    {
        fprintf(stderr, "tinyobj::LoadObj(%s) warning: %s\n", filename.c_str(), err.c_str());
    }

    // Add materials to the scene
    std::map<std::string, uint32_t> diffuseMapCache;
    std::vector<uint32_t> newMaterialIDs;
    for (const tinyobj::material_t& materialToAdd : materials)
    {
        Material newMaterial;

        newMaterial.Name = materialToAdd.name;

        newMaterial.Ambient[0] = materialToAdd.ambient[0];
        newMaterial.Ambient[1] = materialToAdd.ambient[1];
        newMaterial.Ambient[2] = materialToAdd.ambient[2];
        newMaterial.Diffuse[0] = materialToAdd.diffuse[0];
        newMaterial.Diffuse[1] = materialToAdd.diffuse[1];
        newMaterial.Diffuse[2] = materialToAdd.diffuse[2];
        newMaterial.Specular[0] = materialToAdd.specular[0];
        newMaterial.Specular[1] = materialToAdd.specular[1];
        newMaterial.Specular[2] = materialToAdd.specular[2];
        newMaterial.Shininess = materialToAdd.shininess;

        newMaterial.DiffuseMapID = -1;

        if (!materialToAdd.diffuse_texname.empty())
        {
            auto cachedTexture = diffuseMapCache.find(materialToAdd.diffuse_texname);

            if (cachedTexture != end(diffuseMapCache))
            {
                newMaterial.DiffuseMapID = cachedTexture->second;
            }
            else
            {
                std::string diffuse_texname_full = mtl_basepath + materialToAdd.diffuse_texname;
                int x, y, comp;
                stbi_set_flip_vertically_on_load(1);
                stbi_uc* pixels = stbi_load(diffuse_texname_full.c_str(), &x, &y, &comp, 4);
                stbi_set_flip_vertically_on_load(0);

                if (!pixels)
                {
                    fprintf(stderr, "stbi_load(%s): %s\n", diffuse_texname_full.c_str(), stbi_failure_reason());
                }
                else
                {
                    float maxAnisotropy;
                    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);

                    GLuint newDiffuseMapTO;
                    glGenTextures(1, &newDiffuseMapTO);
                    glBindTexture(GL_TEXTURE_2D, newDiffuseMapTO);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAnisotropy);
                    glGenerateMipmap(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, 0);

                    DiffuseMap newDiffuseMap;
                    newDiffuseMap.DiffuseMapTO = newDiffuseMapTO;
                    
                    uint32_t newDiffuseMapID = scene.DiffuseMaps.insert(newDiffuseMap);

                    diffuseMapCache.emplace(materialToAdd.diffuse_texname, newDiffuseMapID);

                    newMaterial.DiffuseMapID = newDiffuseMapID;

                    stbi_image_free(pixels);
                }
            }
        }

        uint32_t newMaterialID = scene.Materials.insert(newMaterial);

        newMaterialIDs.push_back(newMaterialID);
    }

    // Add meshes (and prototypes) to the scene
    for (const tinyobj::shape_t& shapeToAdd : shapes)
    {
        const tinyobj::mesh_t& meshToAdd = shapeToAdd.mesh;

        Mesh newMesh;

        newMesh.Name = shapeToAdd.name;

        newMesh.IndexCount = (GLuint)meshToAdd.indices.size();
        newMesh.VertexCount = (GLuint)meshToAdd.positions.size() / 3;

        if (meshToAdd.positions.empty())
        {
            // should never happen
            newMesh.PositionBO = 0;
        }
        else
        {
            GLuint newPositionBO;
            glGenBuffers(1, &newPositionBO);
            glBindBuffer(GL_ARRAY_BUFFER, newPositionBO);
            glBufferData(GL_ARRAY_BUFFER, meshToAdd.positions.size() * sizeof(meshToAdd.positions[0]), meshToAdd.positions.data(), GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            newMesh.PositionBO = newPositionBO;
        }

        if (meshToAdd.texcoords.empty())
        {
            newMesh.TexCoordBO = 0;
        }
        else
        {
            GLuint newTexCoordBO;
            glGenBuffers(1, &newTexCoordBO);
            glBindBuffer(GL_ARRAY_BUFFER, newTexCoordBO);
            glBufferData(GL_ARRAY_BUFFER, meshToAdd.texcoords.size() * sizeof(meshToAdd.texcoords[0]), meshToAdd.texcoords.data(), GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            newMesh.TexCoordBO = newTexCoordBO;
        }

        if (meshToAdd.normals.empty())
        {
            newMesh.NormalBO = 0;
        }
        else
        {
            GLuint newNormalBO;
            glGenBuffers(1, &newNormalBO);
            glBindBuffer(GL_ARRAY_BUFFER, newNormalBO);
            glBufferData(GL_ARRAY_BUFFER, meshToAdd.normals.size() * sizeof(meshToAdd.normals[0]), meshToAdd.normals.data(), GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            newMesh.NormalBO = newNormalBO;
        }

        if (meshToAdd.indices.empty())
        {
            // should never happen
            newMesh.IndexBO = 0;
        }
        else
        {
            GLuint newIndexBO;
            glGenBuffers(1, &newIndexBO);
            // Why not bind to GL_ELEMENT_ARRAY_BUFFER?
            // Because binding to GL_ELEMENT_ARRAY_BUFFER attaches the EBO to the currently bound VAO, which might stomp somebody else's state.
            glBindBuffer(GL_ARRAY_BUFFER, newIndexBO);
            glBufferData(GL_ARRAY_BUFFER, meshToAdd.indices.size() * sizeof(meshToAdd.indices[0]), meshToAdd.indices.data(), GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            newMesh.IndexBO = newIndexBO;
        }

        // Hook up VAO
        {
            GLuint newMeshVAO;
            glGenVertexArrays(1, &newMeshVAO);

            glBindVertexArray(newMeshVAO);

            if (newMesh.PositionBO)
            {
                glBindBuffer(GL_ARRAY_BUFFER, newMesh.PositionBO);
                glVertexAttribPointer(SCENE_POSITION_ATTRIB_LOCATION, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);
                glBindBuffer(GL_ARRAY_BUFFER, 0);

                glEnableVertexAttribArray(SCENE_POSITION_ATTRIB_LOCATION);
            }

            if (newMesh.TexCoordBO)
            {
                glBindBuffer(GL_ARRAY_BUFFER, newMesh.TexCoordBO);
                glVertexAttribPointer(SCENE_TEXCOORD_ATTRIB_LOCATION, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, 0);
                glBindBuffer(GL_ARRAY_BUFFER, 0);

                glEnableVertexAttribArray(SCENE_TEXCOORD_ATTRIB_LOCATION);
            }

            if (newMesh.NormalBO)
            {
                glBindBuffer(GL_ARRAY_BUFFER, newMesh.NormalBO);
                glVertexAttribPointer(SCENE_NORMAL_ATTRIB_LOCATION, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);
                glBindBuffer(GL_ARRAY_BUFFER, 0);

                glEnableVertexAttribArray(SCENE_NORMAL_ATTRIB_LOCATION);
            }

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, newMesh.IndexBO);

            glBindVertexArray(0);

            newMesh.MeshVAO = newMeshVAO;
        }

        // split mesh into draw calls with different materials
        int numFaces = (int)meshToAdd.indices.size() / 3;
        int currMaterialFirstFaceIndex = 0;
        for (int faceIdx = 0; faceIdx < numFaces; faceIdx++)
        {
            bool isLastFace = faceIdx + 1 == numFaces;
            bool isNextFaceDifferent = isLastFace || meshToAdd.material_ids[faceIdx + 1] != meshToAdd.material_ids[faceIdx];
            if (isNextFaceDifferent)
            {
                GLDrawElementsIndirectCommand currDrawCommand;
                currDrawCommand.count = ((faceIdx + 1) - currMaterialFirstFaceIndex) * 3;
                currDrawCommand.primCount = 1;
                currDrawCommand.firstIndex = currMaterialFirstFaceIndex * 3;
                currDrawCommand.baseVertex = 0;
                currDrawCommand.baseInstance = 0;

                uint32_t currMaterialID = newMaterialIDs[meshToAdd.material_ids[faceIdx]];

                newMesh.DrawCommands.push_back(currDrawCommand);
                newMesh.MaterialIDs.push_back(currMaterialID);

                currMaterialFirstFaceIndex = faceIdx + 1;
            }
        }

        uint32_t newMeshID = scene.Meshes.insert(newMesh);

        if (loadedMeshIDs)
        {
            loadedMeshIDs->push_back(newMeshID);
        }
    }
}

void AddInstance(
    Scene& scene,
    uint32_t meshID,
    uint32_t* newInstanceID)
{
    Transform newTransform;
    newTransform.Scale = glm::vec3(1.0f);

    uint32_t newTransformID = scene.Transforms.insert(newTransform);

    Instance newInstance;
    newInstance.MeshID = meshID;
    newInstance.TransformID = newTransformID;

    uint32_t tmpNewInstanceID = scene.Instances.insert(newInstance);
    if (newInstanceID)
    {
        *newInstanceID = tmpNewInstanceID;
    }
}