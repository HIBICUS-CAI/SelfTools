#include "AppBasicDef.h"
#include "ChatApp.h"

int main()
{
	if (ChatApp::Instance()->Init())
	{
		ChatApp::Instance()->Stop();
	}

    return 0;
}
