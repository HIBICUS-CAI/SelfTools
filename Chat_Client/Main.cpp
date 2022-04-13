#include "AppBasicDef.h"
#include "ChatApp.h"
#include "ScreenBuffer.h"

#define ca ChatApp::Instance()

int main()
{
    if (ca->Init())
    {
        uint exitCmd = 0;

        while (!exitCmd)
        {
            uint error = 1;
            while (error) { error = ca->RunLogIn(); }

            exitCmd = ca->RunChatRoom();
        }

        ca->Stop();
    }

    return 0;
}
