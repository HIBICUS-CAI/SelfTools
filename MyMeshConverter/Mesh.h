#pragma once

#include <string>
#include <vector>
#include <array>
#include <assimp\Importer.hpp>
#include <assimp\scene.h>
#include <assimp\postprocess.h>

struct MESH_VERTEX
{
    std::array<double, 3> Position = { 0.0,0.0,0.0 };
    std::array<double, 3> Normal = { 0.0,0.0,0.0 };
    std::array<double, 3> Tangent = { 0.0,0.0,0.0 };
    std::array<double, 2> TexCoord = { 0.0,0.0 };
};

struct MESH_TEXTURE
{
    std::string Type = "";
    std::string Path = "";
};

class SubMesh
{
public:
    SubMesh(
        std::vector<MESH_VERTEX> vertices,
        std::vector<unsigned int> indices,
        std::vector<MESH_TEXTURE> textures);

    ~SubMesh();

private:
    std::vector<MESH_VERTEX> mVertices;
    std::vector<unsigned int> mIndices;
    std::vector<MESH_TEXTURE> mTextures;
};

class Mesh
{
public:
    Mesh();
    ~Mesh();

    bool Load(std::string fileName);

private:
    void ProcessNode(
        aiNode* node, const aiScene* scene);
    SubMesh ProcessSubMesh(
        aiMesh* mesh, const aiScene* scene);
    std::vector<MESH_TEXTURE> LoadMaterialTextures(
        aiMaterial* mat, aiTextureType type,
        std::string typeName, const aiScene* scene);
    std::string DetermineDiffuseTextureType(
        const aiScene* scene, aiMaterial* mat);
    int GetTextureIndex(aiString* str);

    std::vector<SubMesh> mSubMeshes;
    std::string mDirectory;
    std::vector<MESH_TEXTURE> mLoadedTexs;
    std::string mTextureType;
};
