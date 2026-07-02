#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>

#include <vector>
#include <string>
#include <map>

#include <assimp/material.h>
#include <assimp/texture.h>
#include <assimp/mesh.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>

struct VertexInternal
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
    glm::vec3 Tangent;
    glm::vec3 Bitangent;

    int BoneIDs[4];
    float Weights[4];

    VertexInternal()
    {
        for (int i = 0; i < 4; i++)
        {
            BoneIDs[i] = -1;
            Weights[i] = 0.0f;
        }
    }
};

struct BoneInfo
{
    int id;
    glm::mat4 offset;

    BoneInfo()
    {
        id = -1;
        offset = glm::mat4(1.0f);
    }
};


class ModelInternal
{
private:
    std::vector<VertexInternal> vertices;
    std::vector<unsigned int> indices;

    std::map<std::string, BoneInfo> boneInfoMap;
    int boneCounter = 0;

    unsigned int VAO = 0;
    unsigned int VBO = 0;
    unsigned int EBO = 0;

    unsigned int textureID = 0;
    unsigned int normalTextureID = 0;

    std::string directory;

    Assimp::Importer importer;
    const aiScene* scene = nullptr;
    aiAnimation* animation = nullptr;

    void loadModel(std::string path);
    void setupMesh();
    void extractBoneWeightForVertices(aiMesh* mesh);


    // Viejo (lo dejo por si acaso)
    unsigned int loadTexture(const char* path);

    // Para texturas embebidas del GLB
    unsigned int loadEmbeddedTexture(const aiTexture* texture);

    // Cargar material desde Assimp
    unsigned int loadMaterialTexture(aiMaterial* mat, aiTextureType type);

public:
    ModelInternal(const std::string& path);
    void Draw();

    const std::map<std::string, BoneInfo>& GetBoneInfoMap() const
    {
        return boneInfoMap;
    }

    const BoneInfo* GetBoneInfo(const std::string& name) const
    {
        auto it = boneInfoMap.find(name);

        if (it == boneInfoMap.end())
            return nullptr;

        return &it->second;
    }
};