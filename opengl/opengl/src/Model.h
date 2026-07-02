#pragma once

#include <vector>
#include <string>

#include <GL/glew.h>
#include <glm/glm.hpp>

struct VertexModel
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;

    glm::vec3 Tangent;
    glm::vec3 Bitangent;
};

class Model
{
public:
    std::vector<VertexModel> vertices;
    std::vector<unsigned int> indices;

    unsigned int VAO, VBO, EBO;
    unsigned int textureID;
    unsigned int normalTextureID;

    Model(const std::string& path);

    void Draw();

private:
    void loadModel(std::string path);
    void setupMesh();
    unsigned int loadTexture(const char* path); // NUEVO
};