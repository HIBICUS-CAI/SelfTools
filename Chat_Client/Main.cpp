#include <cstdlib>
#include <cstdio>
#include <vector>
#include <string>

using uint = unsigned int;

bool Init();

void GetInRoom(uint _roomID);

int main()
{
    Init();

    return 0;
}

bool Init()
{
    std::system("mode con cols=120 lines=30 && cls");

    return true;
}

void GetInRoom(uint _roomID)
{

}
