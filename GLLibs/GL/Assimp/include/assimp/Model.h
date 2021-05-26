#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "3dMath.h"

#include "Mesh.h"
#include "Shader.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>
#include <Windows.h>
#include <stb_image_aug.h>

using namespace std;
unsigned int TextureFromFile(const char* path, const string& directory, bool gamma = false);

class Model
{
public:
    vec3 position;
    vector<texture> textures_loaded;
    vector<Mesh>    meshes;
    string directory;
    bool gammaCorrection;

    Model() {};

    Model(string const& path, vec3 pos, Shader s, bool gamma = false) : gammaCorrection(gamma)
    {
        model = mat4(1.0f);
        shader = s;
        position = pos;

        loadModel(path);

        model = glm::translate(model, position);
    }


    virtual void Draw()
    {
        shader.setMat4("model", model);

        for (unsigned int i = 0; i < meshes.size(); i++)
            meshes[i].Draw(shader);
    }
    void Rotate(GLfloat angle, vec3 axis) {
        model = glm::rotate(model, radians(angle), axis);
    }
    void Scale(vec3 sc) {
        model = glm::scale(model, sc);
    }
    void trans(vec3 tr) {
        model = glm::translate(model, tr);
    }
    void setproj(mat4 prj) {
        shader.setMat4("projection", prj);
    }
    void setview(mat4 view) {
        shader.setMat4("view", view);
    }
    void setModel(mat4 mdl) {
        model = mdl;
    }

protected:
    mat4 model;
    Shader shader;
    /*  Методы   */
    void loadModel(string const& path) {
        Assimp::Importer import;
        const aiScene* scene = import.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);

        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        {
            cout << "ERROR::ASSIMP::" << import.GetErrorString() << endl;
            return;
        }
        directory = path.substr(0, path.find_last_of('\\'));

        processNode(scene->mRootNode, scene);
    }
    void processNode(aiNode* node, const aiScene* scene) {
        // обработать все полигональные сетки в узле(если есть)
        for (unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            meshes.push_back(processMesh(mesh, scene));
        }
        // выполнить ту же обработку и для каждого потомка узла
        for (unsigned int i = 0; i < node->mNumChildren; i++)
        {
            processNode(node->mChildren[i], scene);
        }
    }
    Mesh processMesh(aiMesh* mesh, const aiScene* scene) {
        vector<Vertex> vertices;
        vector<unsigned int> indices;
        vector<texture> textures;

        for (unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            Vertex vertex;
            glm::vec3 vector;
            vector.x = mesh->mVertices[i].x;
            vector.y = mesh->mVertices[i].y;
            vector.z = mesh->mVertices[i].z;
            vertex.Position = vector;

            vector.x = mesh->mNormals[i].x;
            vector.y = mesh->mNormals[i].y;
            vector.z = mesh->mNormals[i].z;
            vertex.Normal = vector;

            if (mesh->mTextureCoords[0]) // сетка обладает набором текстурных координат?
            {
                glm::vec2 vec;
                vec.x = mesh->mTextureCoords[0][i].x;
                vec.y = mesh->mTextureCoords[0][i].y;
                vertex.TexCoords = vec;
            }
            else
                vertex.TexCoords = glm::vec2(0.0f, 0.0f);

            // tangent
            vector.x = mesh->mTangents[i].x;
            vector.y = mesh->mTangents[i].y;
            vector.z = mesh->mTangents[i].z;
            vertex.Tangent = vector;
            // bitangent
            vector.x = mesh->mBitangents[i].x;
            vector.y = mesh->mBitangents[i].y;
            vector.z = mesh->mBitangents[i].z;
            vertex.Bitangent = vector;
            vertices.push_back(vertex);
        }
        // орбаботка индексов
        for (unsigned int i = 0; i < mesh->mNumFaces; i++)
        {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(face.mIndices[j]);
        }
        // обработка материала
        if (mesh->mMaterialIndex >= 0)
        {
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
            vector<texture> diffuseMaps = loadMaterialTextures(material,
                aiTextureType_DIFFUSE, "diffuseMap");
            textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
            vector<texture> specularMaps = loadMaterialTextures(material,
                aiTextureType_SPECULAR, "specularMap");
            textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());

            // 3. normal maps
            std::vector<texture> normalMaps = loadMaterialTextures(material,
                aiTextureType_HEIGHT, "normalMap");
            textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());

        }

        return Mesh(vertices, indices, textures);
    }
    vector<texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, string typeName) {
        vector<texture> textures;
        for (unsigned int i = 0; i < mat->GetTextureCount(type); i++)
        {
            aiString str;
            mat->GetTexture(type, i, &str);
            string buf = str.C_Str();
            string buf2;
            for (int i = 0; i < buf.length(); i++) {
                if (buf[i] != '\t') {
                    buf2 += buf[i];
                }
                else
                    break;
            }
            str = buf2;

            bool skip = false;
            for (unsigned int j = 0; j < textures_loaded.size(); j++)
            {
                if (std::strcmp(textures_loaded[j].path.c_str(), str.C_Str()) == 0)
                {
                    textures.push_back(textures_loaded[j]);
                    skip = true;
                    break;
                }
            }
            if (!skip)
            {   // если текстура была загружена – сделаем это
                texture texture;
                texture.id = TextureFromFile(buf2.c_str(), directory);
                texture.type = typeName;
                texture.path = str.C_Str();
                textures.push_back(texture);
                // занесем текстуру в список уже загруженных
                textures_loaded.push_back(texture);
            }
        }
        return textures;
    }

};

unsigned int TextureFromFile(const char* path, const string& directory, bool gamma)
{
    string filename = string(path);
    filename = directory + '\\' + filename;

    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}
