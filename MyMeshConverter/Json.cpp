#include "Json.h"
#include "Mesh.h"
#include <rapidjson\stringbuffer.h>
#include <rapidjson\prettywriter.h>
#include <vector>
#include <assert.h>
#include <iostream>

using namespace rapidjson;

static std::vector<StringBuffer*> g_Buffers = {};

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

	// TODO START
}

// TEMP
void PrintOut(unsigned int _index)
{
	std::cout << 
		std::string(_strdup(g_Buffers[_index]->GetString())) <<
		std::endl;
}

void ClearUp()
{
	for (auto& buffer : g_Buffers)
	{
		delete buffer;
	}

	g_Buffers.clear();
}
