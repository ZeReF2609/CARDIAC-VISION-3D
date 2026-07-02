#include "ModelInternal.h"
#include "stb_image.h"
#include <assimp/texture.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <iostream>

glm::mat4 ConvertMatrixToGLM(const aiMatrix4x4& from)
{
    glm::mat4 to;

    to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
    to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
    to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
    to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;

    return to;
}

void SetVertexBoneData(VertexInternal& vertex, int boneID, float weight)
{
    for (int i = 0; i < 4; i++)
    {
        if (vertex.BoneIDs[i] == -1)
        {
            vertex.BoneIDs[i] = boneID;
            vertex.Weights[i] = weight;
            return;
        }
    }
}

ModelInternal::ModelInternal(const std::string& path)
{
    size_t lastSlash = path.find_last_of("/\\");
    directory = path.substr(0, lastSlash);

    loadModel(path);
    setupMesh();
}

unsigned int ModelInternal::loadTexture(const char* path)
{
    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(false);

    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);

    if (data)
    {
        GLenum format = GL_RGB;
        if (nrChannels == 1) format = GL_RED;
        if (nrChannels == 3) format = GL_RGB;
        if (nrChannels == 4) format = GL_RGBA;

        glTexImage2D(GL_TEXTURE_2D, 0, format,
            width, height, 0, format, GL_UNSIGNED_BYTE, data);

        glGenerateMipmap(GL_TEXTURE_2D);
        std::cout << "Textura interna OK\n";
    }
    else
    {
        std::cout << "Error textura interna\n";
    }

    stbi_image_free(data);
    return texture;
}
unsigned int ModelInternal::loadEmbeddedTexture(const aiTexture* texture)
{
    if (!texture)
        return 0;

    int width, height, channels;

    unsigned char* data = stbi_load_from_memory(
        reinterpret_cast<unsigned char*>(texture->pcData),
        texture->mWidth,
        &width,
        &height,
        &channels,
        0
    );

    if (!data)
    {
        std::cout << "Error leyendo embedded texture\n";
        return 0;
    }

    GLenum format = GL_RGB;
    if (channels == 1) format = GL_RED;
    if (channels == 3) format = GL_RGB;
    if (channels == 4) format = GL_RGBA;

    unsigned int texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        format,
        width,
        height,
        0,
        format,
        GL_UNSIGNED_BYTE,
        data
    );

    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);

    std::cout << "Embedded texture cargada OK\n";

    return texID;
}
unsigned int ModelInternal::loadMaterialTexture(aiMaterial* mat, aiTextureType type)
{
    aiString texPath;

    if (mat->GetTexture(type, 0, &texPath) != AI_SUCCESS)
        return 0;

    std::string texName = texPath.C_Str();

    std::cout << "Cargando material texture: " << texName << std::endl;

    if (!texName.empty() && texName[0] == '*')
    {
        std::cout << "Texture embebida detectada: " << texName << std::endl;
        return 0;
    }

    std::string fullPath = directory + "/" + texName;
    return loadTexture(fullPath.c_str());
}

void ModelInternal::loadModel(std::string path)
{
    
    scene = importer.ReadFile(
        path,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_FlipUVs |
        aiProcess_JoinIdenticalVertices |
        aiProcess_CalcTangentSpace
    );

    if (!scene || !scene->mRootNode)
    {
        std::cout << "Error Assimp: "
            << importer.GetErrorString()
            << std::endl;
        return;
    }

    std::cout << "Embedded textures: " << scene->mNumTextures << std::endl;
    std::cout << "Animations: " << scene->mNumAnimations << std::endl;

        
    if (scene->mNumAnimations > 0)
    {
        animation = scene->mAnimations[0];
    }

    if (scene->mAnimations)
    {
        aiAnimation* anim = scene->mAnimations[0];

        std::cout << "Animation duration: "
            << anim->mDuration << std::endl;

        std::cout << "Ticks per second: "
            << anim->mTicksPerSecond << std::endl;

        std::cout << "Channels: "
            << anim->mNumChannels << std::endl;

        for (unsigned int i = 0; i < anim->mNumChannels; i++)
        {
            aiNodeAnim* channel = anim->mChannels[i];

            if (i == 3)
            {
                std::cout << "=== DEBUG HEART_JNT (CHANNEL 3) ===" << std::endl;

                int samples[3] = { 0, 15, 30 };

                for (int s = 0; s < 3; s++)
                {
                    int k = samples[s];

                    auto pos = channel->mPositionKeys[k];
                    auto rot = channel->mRotationKeys[k];
                    auto scl = channel->mScalingKeys[k];

                    std::cout << "KEY " << k << std::endl;

                    std::cout << " POS: ("
                        << pos.mValue.x << ", "
                        << pos.mValue.y << ", "
                        << pos.mValue.z << ")" << std::endl;

                    std::cout << " ROT: ("
                        << rot.mValue.x << ", "
                        << rot.mValue.y << ", "
                        << rot.mValue.z << ", "
                        << rot.mValue.w << ")" << std::endl;

                    std::cout << " SCALE: ("
                        << scl.mValue.x << ", "
                        << scl.mValue.y << ", "
                        << scl.mValue.z << ")" << std::endl;
                }
            }

            std::cout << "Channel " << i << ": "
                << channel->mNodeName.C_Str()
                << std::endl;

            std::cout << "  Position keys: "
                << channel->mNumPositionKeys
                << std::endl;

            std::cout << "  Rotation keys: "
                << channel->mNumRotationKeys
                << std::endl;

            std::cout << "  Scaling keys: "
                << channel->mNumScalingKeys
                << std::endl;
        }
    }

    std::cout << "Embedded textures: " << scene->mNumTextures << std::endl;

    for (unsigned int i = 0; i < scene->mNumTextures; i++)
    {
        std::cout << "Texture " << i << std::endl;
        std::cout << "Width: " << scene->mTextures[i]->mWidth << std::endl;
        std::cout << "Height: " << scene->mTextures[i]->mHeight << std::endl;
    }


    for (unsigned int m = 0; m < scene->mNumMeshes; m++)
    {
        std::cout << "Entrando mesh " << m << std::endl;

        aiMesh* mesh = scene->mMeshes[m];
        std::cout << "Bones in mesh: " << mesh->mNumBones << std::endl;

        for (unsigned int b = 0; b < mesh->mNumBones; b++)
        {
            std::cout << "Bone " << b << ": "
                << mesh->mBones[b]->mName.C_Str()
                << std::endl;
        }

        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

        std::cout << "--- MATERIAL TEST ---" << std::endl;
        std::cout << "Diffuse count: "
            << material->GetTextureCount(aiTextureType_DIFFUSE) << std::endl;

        std::cout << "BaseColor count: "
            << material->GetTextureCount(aiTextureType_BASE_COLOR) << std::endl;

        std::cout << "Normals count: "
            << material->GetTextureCount(aiTextureType_NORMALS) << std::endl;

        aiString texPath;

        if (textureID == 0)
        {
            if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS ||
                material->GetTexture(aiTextureType_BASE_COLOR, 0, &texPath) == AI_SUCCESS)
            {
                std::string name = texPath.C_Str();
                std::cout << "Diffuse/BaseColor: " << name << std::endl;

                if (!name.empty() && name[0] == '*')
                {
                    int index = std::stoi(name.substr(1));
                    textureID = loadEmbeddedTexture(scene->mTextures[index]);
                }
            }
        }

        if (normalTextureID == 0)
        {
            if (material->GetTexture(aiTextureType_NORMALS, 0, &texPath) == AI_SUCCESS)
            {
                std::string name = texPath.C_Str();
                std::cout << "Normal: " << name << std::endl;

                if (!name.empty() && name[0] == '*')
                {
                    int index = std::stoi(name.substr(1));
                    normalTextureID = loadEmbeddedTexture(scene->mTextures[index]);
                }
            }
        }

        unsigned int offset = (unsigned int)vertices.size();
        float minY = 999999.0f;
        float maxY = -999999.0f;
        float minValveY = 9999.0f;
        float maxValveY = -9999.0f;

        for (unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            VertexInternal vertex;

            vertex.Position = glm::vec3(
                mesh->mVertices[i].x,
                mesh->mVertices[i].y,
                mesh->mVertices[i].z

            );
            if (vertex.Position.y < minY) minY = vertex.Position.y;
            if (vertex.Position.y > maxY) maxY = vertex.Position.y;
            float y = mesh->mVertices[i].y;

            if (y < minValveY) minValveY = y;
            if (y > maxValveY) maxValveY = y;
            vertex.Normal = glm::vec3(
                mesh->mNormals[i].x,
                mesh->mNormals[i].y,
                mesh->mNormals[i].z
            );

            if (mesh->mTextureCoords[0])
            {
                vertex.TexCoords = glm::vec2(
                    mesh->mTextureCoords[0][i].x,
                    mesh->mTextureCoords[0][i].y
                );
            }
            else
            {
                vertex.TexCoords = glm::vec2(0.0f);
            }

            if (mesh->HasTangentsAndBitangents())
            {
                vertex.Tangent = glm::vec3(
                    mesh->mTangents[i].x,
                    mesh->mTangents[i].y,
                    mesh->mTangents[i].z
                );

                vertex.Bitangent = glm::vec3(
                    mesh->mBitangents[i].x,
                    mesh->mBitangents[i].y,
                    mesh->mBitangents[i].z
                );
            }
            else
            {
                vertex.Tangent = glm::vec3(1, 0, 0);
                vertex.Bitangent = glm::vec3(0, 1, 0);
            }

            vertices.push_back(vertex);
        }
        extractBoneWeightForVertices(mesh);

        std::cout << "Min Y: " << minY << std::endl;
        std::cout << "Max Y: " << maxY << std::endl;
        std::cout << "Mesh MinY: " << minValveY << std::endl;
        std::cout << "Mesh MaxY: " << maxValveY << std::endl;

        for (unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];

            for (unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(offset + face.mIndices[j]);
        }
    }
}

void ModelInternal::extractBoneWeightForVertices(aiMesh* mesh)
{
    for (unsigned int boneIndex = 0; boneIndex < mesh->mNumBones; boneIndex++)
    {
        int boneID = -1;
        std::string boneName = mesh->mBones[boneIndex]->mName.C_Str();

        if (boneName == "aortic_valve_01_jnt.21")
        {
            std::cout << "\n=== BONE 17 FOUND ===\n";
            std::cout << "Weights count: "
                << mesh->mBones[boneIndex]->mNumWeights
                << std::endl;
        }

        if (boneInfoMap.find(boneName) == boneInfoMap.end())
        {
            BoneInfo newBoneInfo;
            newBoneInfo.id = boneCounter;
            newBoneInfo.offset =
                ConvertMatrixToGLM(mesh->mBones[boneIndex]->mOffsetMatrix);

            boneInfoMap[boneName] = newBoneInfo;
            boneID = boneCounter;
            boneCounter++;
        }
        else
        {
            boneID = boneInfoMap[boneName].id;
        }

        aiBone* aiBonePtr = mesh->mBones[boneIndex];

        for (unsigned int weightIndex = 0; weightIndex < aiBonePtr->mNumWeights; weightIndex++)
        {
            int vertexId = aiBonePtr->mWeights[weightIndex].mVertexId;
            float weight = aiBonePtr->mWeights[weightIndex].mWeight;

            if (boneName == "aortic_valve_01_jnt.21" && weight > 0.05f)
            {
                std::cout << "Vertex " << vertexId
                    << " weight " << weight
                    << std::endl;
            }

            SetVertexBoneData(vertices[vertexId], boneID, weight);
        }
    }

    std::cout << "Bones loaded: " << boneCounter << std::endl;

    if (!vertices.empty())
    {
        std::cout << "Vertex 0 bones: ";
        for (int i = 0; i < 4; i++)
        {
            std::cout
                << "[" << vertices[0].BoneIDs[i]
                << ", " << vertices[0].Weights[i] << "] ";
        }
        std::cout << std::endl;
    }
}

void ModelInternal::setupMesh()
{
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER,
        vertices.size() * sizeof(VertexInternal),
        &vertices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        indices.size() * sizeof(unsigned int),
        &indices[0], GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
        sizeof(VertexInternal),
        (void*)offsetof(VertexInternal, Position));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
        sizeof(VertexInternal),
        (void*)offsetof(VertexInternal, Normal));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
        sizeof(VertexInternal),
        (void*)offsetof(VertexInternal, TexCoords));
    glEnableVertexAttribArray(2);

    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE,
        sizeof(VertexInternal),
        (void*)offsetof(VertexInternal, Tangent));
    glEnableVertexAttribArray(3);

    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE,
        sizeof(VertexInternal),
        (void*)offsetof(VertexInternal, Bitangent));
    glEnableVertexAttribArray(4);

    // Bone IDs (location 5)
    glVertexAttribIPointer(
        5,
        4,
        GL_INT,
        sizeof(VertexInternal),
        (void*)offsetof(VertexInternal, BoneIDs)
    );
    glEnableVertexAttribArray(5);

    // Bone Weights (location 6)
    glVertexAttribPointer(
        6,
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(VertexInternal),
        (void*)offsetof(VertexInternal, Weights)
    );
    glEnableVertexAttribArray(6);
}

void ModelInternal::Draw()
{
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normalTextureID);

    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_INT, 0);
}