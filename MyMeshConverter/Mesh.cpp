#include "Mesh.h"
#include <assert.h>

SubMesh::SubMesh(
    std::vector<MESH_VERTEX>& _vertices,
    std::vector<unsigned int>& _indices,
    std::vector<MESH_TEXTURE>& _textures,
    std::vector<MESH_BONE>& _bones) :
    mVertices(_vertices), mIndices(_indices), mTextures(_textures),
    mBones(_bones)
{

}

SubMesh::~SubMesh()
{

}

Mesh::Mesh()
    :mSubMeshes({}), mLoadedTexs({}),
    mDirectory(""), mTextureType(""),
    mNodes({})
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
    SCENE_NODE thisNode = {};
    thisNode.Name = node->mName.C_Str();
    if (node->mParent)
    {
        thisNode.ParentName = node->mParent->mName.C_Str();
    }
    thisNode.ChildrenName.resize(node->mNumChildren);
    for (uint32_t end = node->mNumChildren, i = 0; i < end; i++)
    {
        thisNode.ChildrenName[i] = node->mChildren[i]->mName.C_Str();
    }
    thisNode.ThisToParentMatrix[0][0] =
        (double)node->mTransformation.a1;
    thisNode.ThisToParentMatrix[0][1] =
        (double)node->mTransformation.a2;
    thisNode.ThisToParentMatrix[0][2] =
        (double)node->mTransformation.a3;
    thisNode.ThisToParentMatrix[0][3] =
        (double)node->mTransformation.a4;
    thisNode.ThisToParentMatrix[1][0] =
        (double)node->mTransformation.b1;
    thisNode.ThisToParentMatrix[1][1] =
        (double)node->mTransformation.b2;
    thisNode.ThisToParentMatrix[1][2] =
        (double)node->mTransformation.b3;
    thisNode.ThisToParentMatrix[1][3] =
        (double)node->mTransformation.b4;
    thisNode.ThisToParentMatrix[2][0] =
        (double)node->mTransformation.c1;
    thisNode.ThisToParentMatrix[2][1] =
        (double)node->mTransformation.c2;
    thisNode.ThisToParentMatrix[2][2] =
        (double)node->mTransformation.c3;
    thisNode.ThisToParentMatrix[2][3] =
        (double)node->mTransformation.c4;
    thisNode.ThisToParentMatrix[3][0] =
        (double)node->mTransformation.d1;
    thisNode.ThisToParentMatrix[3][1] =
        (double)node->mTransformation.d2;
    thisNode.ThisToParentMatrix[3][2] =
        (double)node->mTransformation.d3;
    thisNode.ThisToParentMatrix[3][3] =
        (double)node->mTransformation.d4;

    mNodes.emplace_back(thisNode);

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
    std::vector<MESH_BONE> bones;

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

    if (mesh->HasBones())
    {
        bones.resize(mesh->mNumBones);
        for (uint32_t i = 0, end = mesh->mNumBones; i < end; i++)
        {
            aiBone* pBone = mesh->mBones[i];

            bones[i].Name = pBone->mName.C_Str();

            bones[i].LocalToBoneMatrix[0][0] =
                (double)pBone->mOffsetMatrix.a1;
            bones[i].LocalToBoneMatrix[0][1] =
                (double)pBone->mOffsetMatrix.a2;
            bones[i].LocalToBoneMatrix[0][2] =
                (double)pBone->mOffsetMatrix.a3;
            bones[i].LocalToBoneMatrix[0][3] =
                (double)pBone->mOffsetMatrix.a4;
            bones[i].LocalToBoneMatrix[1][0] =
                (double)pBone->mOffsetMatrix.b1;
            bones[i].LocalToBoneMatrix[1][1] =
                (double)pBone->mOffsetMatrix.b2;
            bones[i].LocalToBoneMatrix[1][2] =
                (double)pBone->mOffsetMatrix.b3;
            bones[i].LocalToBoneMatrix[1][3] =
                (double)pBone->mOffsetMatrix.b4;
            bones[i].LocalToBoneMatrix[2][0] =
                (double)pBone->mOffsetMatrix.c1;
            bones[i].LocalToBoneMatrix[2][1] =
                (double)pBone->mOffsetMatrix.c2;
            bones[i].LocalToBoneMatrix[2][2] =
                (double)pBone->mOffsetMatrix.c3;
            bones[i].LocalToBoneMatrix[2][3] =
                (double)pBone->mOffsetMatrix.c4;
            bones[i].LocalToBoneMatrix[3][0] =
                (double)pBone->mOffsetMatrix.d1;
            bones[i].LocalToBoneMatrix[3][1] =
                (double)pBone->mOffsetMatrix.d2;
            bones[i].LocalToBoneMatrix[3][2] =
                (double)pBone->mOffsetMatrix.d3;
            bones[i].LocalToBoneMatrix[3][3] =
                (double)pBone->mOffsetMatrix.d4;

            bones[i].VertexWeight.resize(pBone->mNumWeights);
            for (uint32_t end = pBone->mNumWeights, j = 0;
                j < end; j++)
            {
                bones[i].VertexWeight[j].VertexID =
                    pBone->mWeights[j].mVertexId;
                bones[i].VertexWeight[j].Weight =
                    (double)(pBone->mWeights[j].mWeight);
            }
        }

        uint32_t boneID = 0;
        for (auto& b : bones)
        {
            for (auto& id_weight : b.VertexWeight)
            {
                MESH_VERTEX& thisVert = vertices[id_weight.VertexID];
                for (uint16_t i = 0; i < 4; i++)
                {
                    if (thisVert.Weight[i] < id_weight.Weight)
                    {
                        thisVert.Weight[i] = id_weight.Weight;
                        thisVert.BoneID[i] = boneID;
                        break;
                    }
                }
            }
            ++boneID;
        }
    }

    return SubMesh(vertices, indices, textures, bones);
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
