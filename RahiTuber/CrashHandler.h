#pragma once

#include <windows.h>
#include <tchar.h>
#include <dbghelp.h>
#include <stdio.h>
#include <crtdbg.h>

#include "Config.h"

#pragma comment ( lib, "dbghelp.lib" )

class CrashHandler
{
public:
	///////////////////////////////////////////////////////////////////////////////
// Minidump creation function 
//

	static void CreateMiniDump(EXCEPTION_POINTERS* pep, AppConfig* appConfig)
	{
		// Open the file 

		logToFile(appConfig, "Crash! Creating dump file. Please contact the developer and send this :)");

		HANDLE hFile = CreateFile(_T("RahiTuber_Crash.dmp"), GENERIC_READ | GENERIC_WRITE,
			0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

		if ((hFile != NULL) && (hFile != INVALID_HANDLE_VALUE))
		{
			// Create the minidump 

			MINIDUMP_EXCEPTION_INFORMATION mdei;

			mdei.ThreadId = GetCurrentThreadId();
			mdei.ExceptionPointers = pep;
			mdei.ClientPointers = FALSE;

			MINIDUMP_CALLBACK_INFORMATION mci;

			mci.CallbackRoutine = (MINIDUMP_CALLBACK_ROUTINE)MyMiniDumpCallback;
			mci.CallbackParam = 0;

			MINIDUMP_TYPE mdt = (MINIDUMP_TYPE)(MiniDumpWithPrivateReadWriteMemory |
				MiniDumpWithDataSegs |
				MiniDumpWithHandleData |
				MiniDumpWithFullMemoryInfo |
				MiniDumpWithThreadInfo |
				MiniDumpWithUnloadedModules);

			BOOL rv = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
				hFile, mdt, (pep != 0) ? &mdei : 0, 0, &mci);

			if (!rv)
				//_tprintf(_T("MiniDumpWriteDump failed. Error: %u \n"), GetLastError());
				logToFile(appConfig, "MiniDumpWriteDump failed. Error: " + GetLastError());
			else
				_tprintf(_T("Minidump created.\n"));

			// Close the file 

			CloseHandle(hFile);

		}
		else
		{
			_tprintf(_T("CreateFile failed. Error: %u \n"), GetLastError());
		}

	}

	///////////////////////////////////////////////////////////////////////////////
	// This function determines whether we need data sections of the given module 
	//

	static bool IsDataSectionNeeded(const WCHAR* pModuleName)
	{
		// Check parameters 

		if (pModuleName == 0)
		{
			_ASSERTE(_T("Parameter is null."));
			return false;
		}


		// Extract the module name 

		WCHAR szFileName[_MAX_FNAME] = L"";

		_wsplitpath_s(pModuleName, NULL, 0, NULL, 0, szFileName, _MAX_FNAME, NULL, 0);

		// Compare the name with the list of known names and decide 

		// Note: For this to work, the executable name must be "mididump.exe"
#ifdef PROGRAM_NAME
		if (_wcsicmp(szFileName, PROGRAM_NAME) == 0)
		{
			return true;
		}
		else 
#endif
			if (_wcsicmp(szFileName, L"ntdll") == 0)
		{
			return true;
		}


		// Complete 

		return false;

	}

	static BOOL CALLBACK MyMiniDumpCallback(
    PVOID                            pParam,
    const PMINIDUMP_CALLBACK_INPUT   pInput,
    PMINIDUMP_CALLBACK_OUTPUT        pOutput
  )
  {
		BOOL bRet = FALSE;


		// Check parameters 

		if (pInput == 0)
			return FALSE;

		if (pOutput == 0)
			return FALSE;


		// Process the callbacks 

		switch (pInput->CallbackType)
		{
		case IncludeModuleCallback:
		{
			// Include the module into the dump 
			bRet = TRUE;
		}
		break;

		case IncludeThreadCallback:
		{
			// Include the thread into the dump 
			bRet = TRUE;
		}
		break;

		case ModuleCallback:
		{
			// Are data sections available for this module ? 

			if (pOutput->ModuleWriteFlags & ModuleWriteDataSeg)
			{
				// Yes, they are, but do we need them? 

				if (!IsDataSectionNeeded(pInput->Module.FullPath))
				{
					wprintf(L"Excluding module data sections: %s \n", pInput->Module.FullPath);

					pOutput->ModuleWriteFlags &= (~ModuleWriteDataSeg);
				}
			}

			bRet = TRUE;
		}
		break;

		case ThreadCallback:
		{
			// Include all thread information into the minidump 
			bRet = TRUE;
		}
		break;

		case ThreadExCallback:
		{
			// Include this information 
			bRet = TRUE;
		}
		break;

		case MemoryCallback:
		{
			// We do not include any information here -> return FALSE 
			bRet = FALSE;
		}
		break;

		case CancelCallback:
			break;
		}

		return bRet;
  }
};

