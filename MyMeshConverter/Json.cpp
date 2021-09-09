#define _CRT_SECURE_NO_WARNINGS

#include "Json.h"
#include "Mesh.h"
#include <rapidjson\stringbuffer.h>
#include <rapidjson\prettywriter.h>
#include <rapidjson\filewritestream.h>
#include <rapidjson\document.h>
#include <cstdio>
#include <vector>
#include <assert.h>
#include <Windows.h>

using namespace rapidjson;

static std::vector<StringBuffer*> g_Buffers = {};

void ProcessMeshPretty(PrettyWriter<StringBuffer>* _writer,
    Mesh* _mesh);

void CreateStringBuffers(unsigned int _number)
{
    g_Buffers.reserve(_number);

    for (unsigned int i = 0; i < _number; i++)
    {
        StringBuffer* b = new StringBuffer;
        g_Buffers.push_back(b);
    }
}

void WriteInfoToBuffer(unsigned int _index, Mesh* _mesh)
{
    assert(_mesh);

    StringBuffer* buffer = g_Buffers[_index];
    buffer->Clear();
    PrettyWriter<StringBuffer> writer(*buffer);
    writer.SetMaxDecimalPlaces(18);

    ProcessMeshPretty(&writer, _mesh);

    assert(writer.IsComplete());
}

// TEMP
void PrintOut(unsigned int _index)
{
    OutputDebugStringA((g_Buffers[_index])->GetString());
}

void SaveToFilePretty(const char* _fileName, unsigned int _index)
{
    FILE* fp = std::fopen(_fileName, "wb");
    assert(fp);

    Document d = {};
    d.Parse((g_Buffers[_index])->GetString());

    char* writeBuffer = new char[65536];
    FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));

    PrettyWriter<FileWriteStream> writer(os);
    d.Accept(writer);

    std::fclose(fp);
    delete[] writeBuffer;
}

void SaveToFileNormal(const char* _fileName, unsigned int _index)
{
    FILE* fp = std::fopen(_fileName, "wb");
    assert(fp);

    Document d = {};
    d.Parse((g_Buffers[_index])->GetString());

    char* writeBuffer = new char[65536];
    FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));

    Writer<FileWriteStream> writer(os);
    d.Accept(writer);

    std::fclose(fp);
    delete[] writeBuffer;
}

void ClearUp()
{
    for (auto& buffer : g_Buffers)
    {
        delete buffer;
    }

    g_Buffers.clear();
}

void ProcessMeshPretty(PrettyWriter<StringBuffer>* _writer,
    Mesh* _mesh)
{
    _writer->StartObject();

    _writer->String("directory");
    _writer->String(_mesh->GetDirectory().c_str());
    _writer->String("texture-type");
    _writer->String(_mesh->GetTextureType().c_str());

    _writer->String("sub-model-size");
    _writer->Uint((unsigned int)(_mesh->GetSubVec()->size()));
    _writer->String("sub-model");

    _writer->StartArray();
    for (auto& sub : *(_mesh->GetSubVec()))
    {
        _writer->StartObject();
        _writer->String("index-size");
        _writer->Uint((unsigned int)(sub.GetIndexVec()->size()));
        _writer->String("vertex-size");
        _writer->Uint((unsigned int)(sub.GetVertexVec()->size()));
        _writer->String("texture-size");
        _writer->Uint((unsigned int)(sub.GetTextureVec()->size()));

        _writer->String("index");
        _writer->StartArray();
        for (auto& index : *(sub.GetIndexVec()))
        {
            _writer->Uint(index);
        }
        _writer->EndArray();

        _writer->String("vertex");
        _writer->StartArray();
        for (auto& vertex : *(sub.GetVertexVec()))
        {
            _writer->StartObject();

            _writer->String("position");
            _writer->StartArray();
            for (size_t i = 0; i < 3; i++)
            {
                _writer->Double(vertex.Position[i]);
            }
            _writer->EndArray();

            _writer->String("normal");
            _writer->StartArray();
            for (size_t i = 0; i < 3; i++)
            {
                _writer->Double(vertex.Normal[i]);
            }
            _writer->EndArray();

            _writer->String("tangent");
            _writer->StartArray();
            for (size_t i = 0; i < 3; i++)
            {
                _writer->Double(vertex.Tangent[i]);
            }
            _writer->EndArray();

            _writer->String("texcoord");
            _writer->StartArray();
            for (size_t i = 0; i < 2; i++)
            {
                _writer->Double(vertex.TexCoord[i]);
            }
            _writer->EndArray();

            _writer->EndObject();
        }
        _writer->EndArray();

        _writer->String("texture");
        _writer->StartArray();
        for (auto& texture : *(sub.GetTextureVec()))
        {
            _writer->StartObject();

            _writer->String("type");
            _writer->String(texture.Type.c_str());

            _writer->String("path");
            _writer->String(texture.Path.c_str());

            _writer->EndObject();
        }
        _writer->EndArray();

        _writer->EndObject();
    }

    _writer->EndArray();

    _writer->EndObject();
}
