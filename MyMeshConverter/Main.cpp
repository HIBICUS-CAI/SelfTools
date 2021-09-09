#include <iostream>
#include <string>

#include "Mesh.h"
#include "Json.h"

int main(int argc, char** argv)
{
    std::cout << "Have " << argc << " arguments:" << std::endl;
    for (int i = 0; i < argc; ++i)
    {
        std::cout << argv[i] << std::endl;
    }

    if (argc == 1)
    {
        std::cout << "no file input" << std::endl;
        return 0;
    }

    Mesh* m = new Mesh;
    m->Load(argv[1]);

    CreateStringBuffers(1);
    WriteInfoToBuffer(0, m);
    std::string p = std::string(argv[1]);
    p = p + "P.json";
    std::string n = std::string(argv[1]);
    n = n + "N.json";
    SaveToFilePretty(p.c_str(), 0);
    SaveToFileNormal(n.c_str(), 0);
    ClearUp();

    return 0;
}
