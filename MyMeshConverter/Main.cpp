#include <iostream>
#include <string>
#include <rapidjson\stringbuffer.h>
#include <rapidjson\prettywriter.h>
#include <rapidjson\document.h>

#include "Mesh.h"

using namespace rapidjson;

void ShowJsonText(Document* doc)
{
    rapidjson::StringBuffer buffer;

    buffer.Clear();

    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    writer.SetMaxDecimalPlaces(18);
    doc->Accept(writer);

    std::cout << std::string(_strdup(buffer.GetString())) << std::endl;
}

int main(int argc, char** argv)
{
    const char* json = "{\"project\":\"rapidjson\",\"stars\":10}";

    std::cout << "Have " << argc << " arguments:" << std::endl;
    for (int i = 0; i < argc; ++i)
    {
        std::cout << argv[i] << std::endl;
    }

    Document d = {};
    d.Parse(json);
    Value& s = d["stars"];
    s.SetDouble(1000.0 / 60.0);

    ShowJsonText(&d);

    Mesh* m = new Mesh;
    m->Load("");

    return 0;
}
