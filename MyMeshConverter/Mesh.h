#pragma once

#include <string>
#include <vector>
#include <array>
#include <assimp\Importer.hpp>
#include <assimp\scene.h>
#include <assimp\postprocess.h>

using MAT4x4D = std::array<std::array<double, 4>, 4>;

struct MESH_VERTEX
{
    std::array<double, 3> Position = { 0.0,0.0,0.0 };
    std::array<double, 3> Normal = { 0.0,0.0,0.0 };
    std::array<double, 3> Tangent = { 0.0,0.0,0.0 };
    std::array<double, 2> TexCoord = { 0.0,0.0 };
    std::array<double, 4> Weight = { 0.0,0.0,0.0,0.0 };
    std::array<uint32_t, 4> BoneID = { (uint32_t)-1,(uint32_t)-1,(uint32_t)-1,(uint32_t)-1 };
};

struct MESH_TEXTURE
{
    std::string Type = "";
    std::string Path = "";
};

struct MESH_VERTEX_WEIGHT
{
    uint32_t VertexID = (uint32_t)-1;
    double Weight = 0.0;
};

struct MESH_BONE
{
    std::string Name = "";
    /* layout
    * 00 01 02 03
    * 10 11 12 13
    * 20 21 22 23
    * 30 31 32 33
    */
    MAT4x4D LocalToBoneMatrix = { 0.0 };
    std::vector<MESH_VERTEX_WEIGHT> VertexWeight = {};
};

struct SCENE_NODE
{
    std::string Name = "";
    MAT4x4D ThisToParentMatrix = { 0.0 };
    std::string ParentName = "";
    std::vector<std::string> ChildrenName = {};
};

struct DOUBLE3_KEY
{
    double Time = 0.0;
    std::array<double, 3> Value = { 0.0 };  // x, y, z
};

struct DOUBLE4_KEY
{
    double Time = 0.0;
    std::array<double, 4> Value = { 0.0 };  // w, x, y, z
};

struct CHANNEL
{
    std::string NodeName = "";
    std::vector<DOUBLE3_KEY> PositionKeys = {};
    std::vector<DOUBLE4_KEY> RotationKeys = {};
    std::vector<DOUBLE3_KEY> ScalingKeys = {};
};

struct ANIMATION
{
    std::string Name = "";
    double Duration = 0.0;
    double TicksPerSec = 0.0;
    std::vector<CHANNEL> NodeActions = {};
};

class SubMesh
{
public:
    SubMesh(
        std::vector<MESH_VERTEX>& _vertices,
        std::vector<unsigned int>& _indices,
        std::vector<MESH_TEXTURE>& _textures,
        std::vector<MESH_BONE>& _bones);

    ~SubMesh();

    std::vector<MESH_VERTEX>* GetVertexVec()
    {
        return &mVertices;
    }

    std::vector<unsigned int>* GetIndexVec()
    {
        return &mIndices;
    }

    std::vector<MESH_TEXTURE>* GetTextureVec()
    {
        return &mTextures;
    }

    std::vector<MESH_BONE>* GetBoneVec()
    {
        return &mBones;
    }

private:
    std::vector<MESH_VERTEX> mVertices;
    std::vector<unsigned int> mIndices;
    std::vector<MESH_TEXTURE> mTextures;
    std::vector<MESH_BONE> mBones;
};

class Mesh
{
public:
    Mesh();
    ~Mesh();

    bool Load(std::string fileName, unsigned int flag);

    void ProcessMetaInfo(std::string filePath, bool animated);

    std::vector<SubMesh>* GetSubVec()
    {
        return &mSubMeshes;
    }

    std::string GetTextureType()
    {
        return mTextureType;
    }

    std::string GetDirectory()
    {
        return mDirectory;
    }

    std::vector<SCENE_NODE>* GetNodeVec()
    {
        return &mNodes;
    }

    std::vector<ANIMATION>* GetAnimationVec()
    {
        return &mAnimations;
    }

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
    std::vector<SCENE_NODE> mNodes;
    std::vector<ANIMATION> mAnimations;
};
