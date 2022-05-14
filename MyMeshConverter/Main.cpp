#include <iostream>
#include <string>

#include "Mesh.h"
#include "Json.h"
#include "Binary.h"

enum class CONVERT_TYPE
{
    JSON_PRETTY,
    JSON_NORMAL,
    BINARY,
    NONE
};

CONVERT_TYPE g_ConvertType = CONVERT_TYPE::JSON_NORMAL;
unsigned int g_ProcessFlag = 0;
bool g_WithAnimation = false;
std::vector<std::string> g_InputFiles = {};

int main(int argc, char** argv)
{
    std::cout << "Have " << argc << " arguments:" << std::endl;
    for (int i = 0; i < argc; ++i)
    {
        std::cout << argv[i] << std::endl;
    }

    if (argc == 1)
    {
        const char* helpInfo =
            "\n"\
            "  -type : file style to save info\n"\
            "\tjson-pretty : \tconvert into formated json\n"\
            "\tjson-normal : \tconvert into unformated json\n"\
            "\tbinary : \tconvert into binary file\n"\
            "\n  -flag : the flag pass to assimp to process mesh\n"\
            "\tleft-hand : \tprocess mesh to LH coord\n"\
            "\tright-hand : \tprocess mesh to RH coord\n"\
            "\tanimation : \tprocess mesh's animation\n"\
            "\toptimize : \tprocess with optimzie flag\n"\
            "\n  -input : the model file you want to convert\n"\
            "\t[file name] : \tthe file's name with path\n";
        std::cout << helpInfo << std::endl;
        return 0;
    }
    else
    {
        for (int i = 0; i < argc; i++)
        {
            std::string str(argv[i]);
            if (str == "-type")
            {
                if (i + 1 == argc)
                {
                    std::cout << "no convert type" << std::endl;
                    exit(-1);
                }
                std::string type(argv[i + 1]);
                if (type == "json-pretty")
                {
                    g_ConvertType = CONVERT_TYPE::JSON_PRETTY;
                }
                else if (type == "json-normal")
                {
                    g_ConvertType = CONVERT_TYPE::JSON_NORMAL;
                }
                else if (type == "binary")
                {
                    g_ConvertType = CONVERT_TYPE::BINARY;
                }
                else
                {
                    std::cout << "no convert type" << std::endl;
                    exit(-1);
                }
            }
            else if (str == "-flag")
            {
                for (int j = i + 1; j < argc; j++)
                {
                    std::string flag(argv[j]);
                    static std::string halven1 = "-t";
                    static std::string halven2 = "-i";
                    if (flag.find(halven1) != std::string::npos ||
                        flag.find(halven2) != std::string::npos)
                    {
                        break;
                    }
                    if (flag == "left-hand")
                    {
                        g_ProcessFlag |= aiProcess_ConvertToLeftHanded;
                    }
                    else if (flag == "right-hand")
                    {
                        continue;
                    }
                    else if (flag == "animation")
                    {
                        g_WithAnimation = true;
                        g_ProcessFlag |= aiProcess_LimitBoneWeights;
                    }
                    else if (flag == "optimize")
                    {
                        g_ProcessFlag |=
                            aiProcess_OptimizeMeshes;
                        g_ProcessFlag |=
                            aiProcess_OptimizeGraph;
                    }
                    else
                    {
                        std::cout << "no flag type" << std::endl;
                        exit(-1);
                    }
                }
            }
            else if (str == "-input")
            {
                for (int j = i + 1; j < argc; j++)
                {
                    std::string input(argv[j]);
                    static std::string halven = "-";
                    if (input.find(halven) == std::string::npos)
                    {
                        g_InputFiles.push_back(input);
                    }
                    else
                    {
                        break;
                    }
                }
            }
            else
            {
                continue;
            }
        }
    }

    CreateStringBuffers((unsigned int)g_InputFiles.size());
    unsigned int index = 0;
    std::string p = "";
    for (auto& file : g_InputFiles)
    {
        Mesh* m = new Mesh;
        m->Load(file, g_ProcessFlag);
        switch (g_ConvertType)
        {
        case CONVERT_TYPE::JSON_PRETTY:
            p = file + ".json";
            WriteInfoToBuffer(index, m, g_WithAnimation);
            SaveToFilePretty(p.c_str(), index);
            break;
        case CONVERT_TYPE::JSON_NORMAL:
            p = file + ".json";
            WriteInfoToBuffer(index, m, g_WithAnimation);
            SaveToFileNormal(p.c_str(), index);
            break;
        case CONVERT_TYPE::BINARY:
            p = file + ".meshdata";
            SaveToFileBinary(p.c_str(), m, g_WithAnimation);
            break;
        default:
            break;
        }
        delete m;

        ++index;
    }

    ClearUp();

    return 0;
}
