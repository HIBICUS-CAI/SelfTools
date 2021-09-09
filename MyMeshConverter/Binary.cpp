#include "Binary.h"
#include "Mesh.h"
#include <fstream>
#include <assert.h>

void ProcessHeadInfo(std::ofstream* _file, Mesh* _mesh);

void ProcessSubInfo(std::ofstream* _file, SubMesh* _submesh);

void ProcessSubIndex(std::ofstream* _file,
    std::vector<unsigned int>* _index);

void ProcessSubVertex(std::ofstream* _file,
    std::vector<MESH_VERTEX>* _vertex);

void ProcessSubTexture(std::ofstream* _file,
    std::vector<MESH_TEXTURE>* _texture);

void SaveToFileBinary(const char* _fileName, Mesh* _mesh)
{
    assert(_fileName);
    assert(_mesh);

    std::ofstream outFile(_fileName,
        std::ios::out | std::ios::binary);

    ProcessHeadInfo(&outFile, _mesh);

    for (auto& sub : *(_mesh->GetSubVec()))
    {
        ProcessSubInfo(&outFile, &sub);
    }

    outFile.close();
}

void ProcessHeadInfo(std::ofstream* _file, Mesh* _mesh)
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

    // sub-model-size
    size = (int)_mesh->GetSubVec()->size();
    _file->write((char*)&size, sizeof(size));
}

void ProcessSubInfo(std::ofstream* _file, SubMesh* _submesh)
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
    ProcessSubVertex(_file, _submesh->GetVertexVec());

    // texture
    ProcessSubTexture(_file, _submesh->GetTextureVec());
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
    std::vector<MESH_VERTEX>* _vertex)
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
        _file->write(tex.Path.c_str(), size);
    }
}
