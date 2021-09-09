#include "Mesh.h"
#include <assert.h>

SubMesh::SubMesh(
    std::vector<MESH_VERTEX> vertices,
    std::vector<unsigned int> indices,
    std::vector<MESH_TEXTURE> textures)
    :mVertices(vertices), mIndices(indices), mTextures(textures)
{

}

SubMesh::~SubMesh()
{

}

Mesh::Mesh()
    :mSubMeshes({}), mLoadedTexs({}),
    mDirectory(""), mTextureType("")
{

}

Mesh::~Mesh()
{

}

bool Mesh::Load(std::string fileName, unsigned int flag)
{
    Assimp::Importer importer;

    const aiScene* pScene = importer.ReadFile(fileName,
        aiProcess_Triangulate | aiProcess_CalcTangentSpace |
        flag);

    if (pScene == nullptr)
    {
        return false;
    }

    this->mDirectory = fileName.substr(
        0, fileName.find_last_of('/'));

    ProcessNode(pScene->mRootNode, pScene);

    return true;
}

void Mesh::ProcessNode(
    aiNode* node, const aiScene* scene)
{
    for (unsigned int i = 0; i < node->mNumMeshes; i++)
    {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        mSubMeshes.push_back(this->ProcessSubMesh(mesh, scene));
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++)
    {
        this->ProcessNode(node->mChildren[i], scene);
    }
}

SubMesh Mesh::ProcessSubMesh(
    aiMesh* mesh, const aiScene* scene)
{
    std::vector<MESH_VERTEX> vertices;
    std::vector<unsigned int> indices;
    std::vector<MESH_TEXTURE> textures;

    if (mesh->mMaterialIndex >= 0)
    {
        aiMaterial* mat =
            scene->mMaterials[mesh->mMaterialIndex];
        if (mTextureType.empty())
        {
            mTextureType =
                DetermineDiffuseTextureType(scene, mat);
        }
    }

    for (unsigned int i = 0; i < mesh->mNumVertices; i++)
    {
        MESH_VERTEX vertex;

        vertex.Position[0] = (double)mesh->mVertices[i].x;
        vertex.Position[1] = (double)mesh->mVertices[i].y;
        vertex.Position[2] = (double)mesh->mVertices[i].z;

        if (mesh->mNormals)
        {
            vertex.Normal[0] = (double)mesh->mNormals[i].x;
            vertex.Normal[1] = (double)mesh->mNormals[i].y;
            vertex.Normal[2] = (double)mesh->mNormals[i].z;
        }

        if (mesh->mTangents)
        {
            vertex.Tangent[0] = (double)mesh->mTangents[i].x;
            vertex.Tangent[1] = (double)mesh->mTangents[i].y;
            vertex.Tangent[2] = (double)mesh->mTangents[i].z;
        }

        if (mesh->mTextureCoords[0])
        {
            vertex.TexCoord[0] =
                (double)mesh->mTextureCoords[0][i].x;
            vertex.TexCoord[1] =
                (double)mesh->mTextureCoords[0][i].y;
        }

        vertices.push_back(vertex);
    }

    for (unsigned int i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace face = mesh->mFaces[i];

        for (unsigned int j = 0; j < face.mNumIndices; j++)
        {
            indices.push_back(face.mIndices[j]);
        }
    }

    if (mesh->mMaterialIndex >= 0)
    {
        aiMaterial* material = 
            scene->mMaterials[mesh->mMaterialIndex];

        std::vector<MESH_TEXTURE> diffuseMaps =
            LoadMaterialTextures(
                material, aiTextureType_DIFFUSE,
                "texture_diffuse", scene);
        textures.insert(textures.end(),
            diffuseMaps.begin(), diffuseMaps.end());
    }

    return SubMesh(vertices, indices, textures);
}

std::vector<MESH_TEXTURE> Mesh::LoadMaterialTextures(
    aiMaterial* mat, aiTextureType type,
    std::string typeName, const aiScene* scene)
{
    std::vector<MESH_TEXTURE> textures;
    for (unsigned int i = 0; i < mat->GetTextureCount(type); i++)
    {
        aiString str;
        mat->GetTexture(type, i, &str);
        bool skip = false;
        for (unsigned int j = 0; j < mLoadedTexs.size(); j++)
        {
            if (std::strcmp(mLoadedTexs[j].Path.c_str(),
                str.C_Str()) == 0)
            {
                textures.push_back(mLoadedTexs[j]);
                skip = true;
                break;
            }
        }
        if (!skip)
        {
            MESH_TEXTURE meshTex;
            if (mTextureType == "embedded compressed texture")
            {
                assert(false);
            }
            else
            {
                std::string fileName = std::string(str.C_Str());
                std::wstring wFileName = std::wstring(
                    fileName.begin(), fileName.end());
            }
            meshTex.Type = typeName;
            meshTex.Path = str.C_Str();
            textures.push_back(meshTex);
            mLoadedTexs.push_back(meshTex);
        }
    }
    return textures;
}

std::string Mesh::DetermineDiffuseTextureType(
    const aiScene* scene, aiMaterial* mat)
{
    aiString aiTexTypeStr;
    mat->GetTexture(aiTextureType_DIFFUSE, 0, &aiTexTypeStr);
    std::string texTypeStr = aiTexTypeStr.C_Str();
    if (texTypeStr == "*0" || texTypeStr == "*1" ||
        texTypeStr == "*2" || texTypeStr == "*3" ||
        texTypeStr == "*4" || texTypeStr == "*5")
    {
        if (scene->mTextures[0]->mHeight == 0)
        {
            return "embedded compressed texture";
        }
        else
        {
            return "embedded non-compressed texture";
        }
    }
    if (texTypeStr.find('.') != std::string::npos)
    {
        return "textures are on disk";
    }

    return "no path to go";
}

int Mesh::GetTextureIndex(aiString* str)
{
    std::string tistr;
    tistr = str->C_Str();
    tistr = tistr.substr(1);
    return stoi(tistr);
}
