#include "Binary.h"
#include "Mesh.h"
#include <fstream>
#include <assert.h>

void ProcessHeadInfo(std::ofstream* _file, Mesh* _mesh,
    bool _withAnimation);

void ProcessSubInfo(std::ofstream* _file, SubMesh* _submesh,
    bool _withAnimation);

void ProcessSubIndex(std::ofstream* _file,
    std::vector<unsigned int>* _index);

void ProcessSubVertex(std::ofstream* _file,
    std::vector<MESH_VERTEX>* _vertex,
    bool _withAnimation);

void ProcessSubTexture(std::ofstream* _file,
    std::vector<MESH_TEXTURE>* _texture);

void ProcessSubBone(std::ofstream* _file,
    std::vector<MESH_BONE>* _bones);

void ProcessSceneNodes(std::ofstream* _file,
    std::vector<SCENE_NODE>* _nodes);

void ProcessAnimations(std::ofstream* _file,
    std::vector<ANIMATION>* _animations);

void SaveToFileBinary(const char* _fileName, Mesh* _mesh,
    bool _withAnimation)
{
    assert(_fileName);
    assert(_mesh);

    std::ofstream outFile(_fileName,
        std::ios::out | std::ios::binary);

    ProcessHeadInfo(&outFile, _mesh, _withAnimation);

    for (auto& sub : *(_mesh->GetSubVec()))
    {
        ProcessSubInfo(&outFile, &sub, _withAnimation);
    }

    if (_withAnimation)
    {
        ProcessSceneNodes(&outFile, _mesh->GetNodeVec());
        ProcessAnimations(&outFile, _mesh->GetAnimationVec());
    }

    outFile.close();
}

void ProcessHeadInfo(std::ofstream* _file, Mesh* _mesh,
    bool _withAnimation)
{
    assert(_file);
    assert(_mesh);

    // directory
    int size = (int)_mesh->GetDirectory().length() + 1;
    _file->write((char*)&size, sizeof(size));
    _file->write(_mesh->GetDirectory().c_str(), size);

    // texture-type
    size = (int)_mesh->GetTextureType().length() + 1;
    _file->write((char*)&size, sizeof(size));
    _file->write(_mesh->GetTextureType().c_str(), size);

    // with-animation
    size = _withAnimation ? 1 : 0;
    _file->write((char*)&size, sizeof(size));

    // sub-model-size
    size = (int)_mesh->GetSubVec()->size();
    _file->write((char*)&size, sizeof(size));
}

void ProcessSubInfo(std::ofstream* _file, SubMesh* _submesh,
    bool _withAnimation)
{
    assert(_file);
    assert(_submesh);

    // index-size
    int indexSize = (int)_submesh->GetIndexVec()->size();
    _file->write((char*)&indexSize, sizeof(indexSize));

    // vertex-size
    int vertexSize = (int)_submesh->GetVertexVec()->size();
    _file->write((char*)&vertexSize, sizeof(vertexSize));

    // texture-size
    int textureSize = (int)_submesh->GetTextureVec()->size();
    _file->write((char*)&textureSize, sizeof(textureSize));

    // index
    ProcessSubIndex(_file, _submesh->GetIndexVec());

    // vertex
    ProcessSubVertex(_file, _submesh->GetVertexVec(), _withAnimation);

    // texture
    ProcessSubTexture(_file, _submesh->GetTextureVec());

    // bone
    if (_withAnimation)
    {
        ProcessSubBone(_file, _submesh->GetBoneVec());
    }
}

void ProcessSubIndex(std::ofstream* _file,
    std::vector<unsigned int>* _index)
{
    assert(_file);
    assert(_index);

    for (auto& index : *_index)
    {
        _file->write((char*)&index, sizeof(index));
    }
}

void ProcessSubVertex(std::ofstream* _file,
    std::vector<MESH_VERTEX>* _vertex,
    bool _withAnimation)
{
    assert(_file);
    assert(_vertex);

    for (auto& vert : *_vertex)
    {
        // position
        for (int i = 0; i < 3; i++)
        {
            _file->write((char*)&vert.Position[i],
                sizeof(double));
        }

        // normal
        for (int i = 0; i < 3; i++)
        {
            _file->write((char*)&vert.Normal[i],
                sizeof(double));
        }

        // tangent
        for (int i = 0; i < 3; i++)
        {
            _file->write((char*)&vert.Tangent[i],
                sizeof(double));
        }

        // texcoord
        for (int i = 0; i < 2; i++)
        {
            _file->write((char*)&vert.TexCoord[i],
                sizeof(double));
        }

        if (_withAnimation)
        {
            // weight
            for (int i = 0; i < 4; i++)
            {
                _file->write((char*)&vert.Weight[i],
                    sizeof(double));
            }

            // boneid
            for (int i = 0; i < 4; i++)
            {
                _file->write((char*)&vert.BoneID[i],
                    sizeof(uint32_t));
            }
        }
    }
}

void ProcessSubTexture(std::ofstream* _file,
    std::vector<MESH_TEXTURE>* _texture)
{
    assert(_file);
    assert(_texture);

    for (auto& tex : *_texture)
    {
        int size = (int)tex.Type.length() + 1;
        _file->write((char*)&size, sizeof(size));
        _file->write(tex.Type.c_str(), size);

        size = (int)tex.Path.length() + 1;
        _file->write((char*)&size, sizeof(size));
        tex.Path.replace(tex.Path.find_last_of("."), 4, ".png");
        tex.Path.replace(tex.Path.find_last_of("."), 4, ".png");
        tex.Path.replace(tex.Path.find_last_of("."), 4, ".png");
        _file->write(tex.Path.c_str(), size);
    }
}

void ProcessSubBone(std::ofstream* _file,
    std::vector<MESH_BONE>* _bones)
{
    // size of all bones in this submesh
    int boneSize = (int)_bones->size();
    _file->write((char*)&boneSize, sizeof(boneSize));

    for (auto& b : *_bones)
    {
        // bone's name
        int nameLen = (int)b.Name.length() + 1;
        _file->write((char*)&nameLen, sizeof(nameLen));
        _file->write(b.Name.c_str(), nameLen);

        // to bone matrix
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                double v = b.LocalToBoneMatrix[i][j];
                _file->write((char*)&v, sizeof(double));
            }
        }
    }
}

void ProcessSceneNodes(std::ofstream* _file,
    std::vector<SCENE_NODE>* _nodes)
{
    // node size
    int nodeSize = (int)_nodes->size();
    _file->write((char*)&nodeSize, sizeof(nodeSize));

    for (auto& n : *_nodes)
    {
        // node's name
        int nameLen = (int)n.Name.length() + 1;
        _file->write((char*)&nameLen, sizeof(nameLen));
        _file->write(n.Name.c_str(), nameLen);

        // to parent matrix
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 4; j++)
            {
                double v = n.ThisToParentMatrix[i][j];
                _file->write((char*)&v, sizeof(double));
            }
        }

        // parent's name
        nameLen = (int)n.ParentName.length() + 1;
        _file->write((char*)&nameLen, sizeof(nameLen));
        _file->write(n.ParentName.c_str(), nameLen);

        // children size
        int childrenSize = (int)n.ChildrenName.size();
        _file->write((char*)&childrenSize, sizeof(childrenSize));

        for (auto& c : n.ChildrenName)
        {
            // child's name
            nameLen = (int)c.length() + 1;
            _file->write((char*)&nameLen, sizeof(nameLen));
            _file->write(c.c_str(), nameLen);
        }
    }
}

void ProcessAnimations(std::ofstream* _file,
    std::vector<ANIMATION>* _animations)
{
    // animations size
    int aniSize = (int)_animations->size();
    _file->write((char*)&aniSize, sizeof(aniSize));

    for (auto& ani : *_animations)
    {
        // animation's name
        int aniNameLen = (int)ani.Name.length() + 1;
        _file->write((char*)&aniNameLen, sizeof(int));
        _file->write(ani.Name.c_str(), aniNameLen);

        // animation's duration
        _file->write((char*)&ani.Duration, sizeof(double));

        // animation's ticks per second
        _file->write((char*)&ani.TicksPerSec, sizeof(double));

        // node actions size
        int nodeActSize = (int)ani.NodeActions.size();
        _file->write((char*)&nodeActSize, sizeof(int));

        for (auto& nAct : ani.NodeActions)
        {
            // node name
            int nodeNameLen = (int)nAct.NodeName.length() + 1;
            _file->write((char*)&nodeNameLen, sizeof(int));
            _file->write(nAct.NodeName.c_str(), nodeNameLen);

            // position keys size
            int pkSize = (int)nAct.PositionKeys.size();
            _file->write((char*)&pkSize, sizeof(int));

            for (auto& pk : nAct.PositionKeys)
            {
                // position key's time
                _file->write((char*)&pk.Time, sizeof(double));

                // position key's value
                for (int i = 0; i < 3; i++)
                {
                    _file->write((char*)&pk.Value[i], sizeof(double));
                }
            }

            // rotation keys size
            int rkSize = (int)nAct.RotationKeys.size();
            _file->write((char*)&rkSize, sizeof(int));

            for (auto& rk : nAct.RotationKeys)
            {
                // rotation key's time
                _file->write((char*)&rk.Time, sizeof(double));

                // rotation key's value
                for (int i = 0; i < 4; i++)
                {
                    _file->write((char*)&rk.Value[i], sizeof(double));
                }
            }

            // scaling keys size
            int skSize = (int)nAct.ScalingKeys.size();
            _file->write((char*)&skSize, sizeof(int));

            for (auto& sk : nAct.ScalingKeys)
            {
                // scaling key's time
                _file->write((char*)&sk.Time, sizeof(double));

                // scaling key's value
                for (int i = 0; i < 3; i++)
                {
                    _file->write((char*)&sk.Value[i], sizeof(double));
                }
            }
        }
    }
}
