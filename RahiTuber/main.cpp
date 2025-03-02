﻿
#include "MainEngine.h"

MainEngine* engine;

void createEngine()
{
	engine = new MainEngine();
}


int main()
{
	createEngine();

#if defined(_WIN32)// && defined(DEBUG)
	__try
	{
#endif
		engine->Init();

		////////////////////////////////////// MAIN LOOP /////////////////////////////////////
		engine->MainLoop();

		engine->Cleanup();
		delete engine;

#if defined(_WIN32)// && defined(DEBUG)
	}
	__except (CrashHandler::CreateMiniDump(GetExceptionInformation(), engine->appConfig), EXCEPTION_EXECUTE_HANDLER)
	{
		engine->Cleanup();
		delete engine;
	}
#endif

	return 0;
}

