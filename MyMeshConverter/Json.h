#pragma once

void CreateStringBuffers(unsigned int _number);

void WriteInfoToBuffer(unsigned int _index, class Mesh* _mesh,
    bool _withAnimation);

// TEMP
void PrintOut(unsigned int _index);

void SaveToFilePretty(const char* _fileName, unsigned int _index);

void SaveToFileNormal(const char* _fileName, unsigned int _index);

void ClearUp();
