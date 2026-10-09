//------------------------------------------------------------------
//
//  MODULE  : CRASHREPORT.CPP
//
//  PURPOSE : When the game crashes, write down where and why.
//
//  Writes two files into the working folder (run\ when started by
//  run_nolf2*.bat):
//
//    crash_<date>_<time>.txt   what crashed, in which function and
//                              source line, and the chain of calls
//                              that led there (readable as-is)
//    crash_<date>_<time>.dmp   a minidump for the Visual Studio
//                              debugger (File > Open > the .dmp)
//
//  Function names and line numbers come from the .pdb files the build
//  writes next to Lithtech.exe and the game DLLs.
//
//------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#include <stdio.h>

#pragma comment(lib, "dbghelp.lib")

static char g_szCrashBase[MAX_PATH] = "";

static const char* ExceptionName(DWORD dwCode)
{
	switch (dwCode)
	{
	case EXCEPTION_ACCESS_VIOLATION:		return "access violation (bad pointer)";
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:	return "array bounds exceeded";
	case EXCEPTION_DATATYPE_MISALIGNMENT:	return "datatype misalignment";
	case EXCEPTION_FLT_DIVIDE_BY_ZERO:		return "float divide by zero";
	case EXCEPTION_INT_DIVIDE_BY_ZERO:		return "integer divide by zero";
	case EXCEPTION_ILLEGAL_INSTRUCTION:		return "illegal instruction";
	case EXCEPTION_STACK_OVERFLOW:			return "stack overflow";
	case EXCEPTION_PRIV_INSTRUCTION:		return "privileged instruction";
	case 0xE06D7363:						return "C++ exception";
	default:								return "unknown";
	}
}

static void WriteStack(FILE* pFile, HANDLE hProcess, HANDLE hThread, CONTEXT ctx)
{
	STACKFRAME64 frame;
	ZeroMemory(&frame, sizeof(frame));
	frame.AddrPC.Offset		= ctx.Eip;
	frame.AddrPC.Mode		= AddrModeFlat;
	frame.AddrFrame.Offset	= ctx.Ebp;
	frame.AddrFrame.Mode	= AddrModeFlat;
	frame.AddrStack.Offset	= ctx.Esp;
	frame.AddrStack.Mode	= AddrModeFlat;

	for (int nFrame = 0; nFrame < 64; ++nFrame)
	{
		if (!StackWalk64(IMAGE_FILE_MACHINE_I386, hProcess, hThread, &frame, &ctx, NULL,
			SymFunctionTableAccess64, SymGetModuleBase64, NULL))
			break;
		if (!frame.AddrPC.Offset)
			break;

		DWORD64 dwAddr = frame.AddrPC.Offset;

		char szModule[MAX_PATH] = "?";
		DWORD64 dwModBase = SymGetModuleBase64(hProcess, dwAddr);
		if (dwModBase)
		{
			char szPath[MAX_PATH];
			if (GetModuleFileNameA((HMODULE)(UINT_PTR)dwModBase, szPath, MAX_PATH))
			{
				const char* pszName = strrchr(szPath, '\\');
				strncpy(szModule, pszName ? pszName + 1 : szPath, MAX_PATH - 1);
			}
		}

		char aSymBuf[sizeof(SYMBOL_INFO) + 512];
		SYMBOL_INFO* pSym = (SYMBOL_INFO*)aSymBuf;
		ZeroMemory(aSymBuf, sizeof(aSymBuf));
		pSym->SizeOfStruct = sizeof(SYMBOL_INFO);
		pSym->MaxNameLen = 511;
		DWORD64 dwDisp = 0;
		const char* pszFunc = "?";
		if (SymFromAddr(hProcess, dwAddr, &dwDisp, pSym))
			pszFunc = pSym->Name;

		IMAGEHLP_LINE64 line;
		ZeroMemory(&line, sizeof(line));
		line.SizeOfStruct = sizeof(line);
		DWORD dwLineDisp = 0;
		if (SymGetLineFromAddr64(hProcess, dwAddr, &dwLineDisp, &line))
		{
			const char* pszFile = strrchr(line.FileName, '\\');
			fprintf(pFile, "  %2d  %-14s %s  (%s:%lu)\n", nFrame, szModule, pszFunc,
				pszFile ? pszFile + 1 : line.FileName, line.LineNumber);
		}
		else
		{
			fprintf(pFile, "  %2d  %-14s %s  +0x%llX\n", nFrame, szModule, pszFunc,
				(unsigned long long)(dwModBase ? dwAddr - dwModBase : dwAddr));
		}
	}
}

static LONG WINAPI CrashFilter(EXCEPTION_POINTERS* pInfo)
{
	static LONG s_nEntered = 0;
	if (InterlockedExchange(&s_nEntered, 1))
		return EXCEPTION_EXECUTE_HANDLER;	// crashed while reporting; give up

	SYSTEMTIME st;
	GetLocalTime(&st);
	_snprintf(g_szCrashBase, sizeof(g_szCrashBase), "crash_%04d%02d%02d_%02d%02d%02d",
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

	HANDLE hProcess = GetCurrentProcess();
	HANDLE hThread = GetCurrentThread();

	// The minidump first: it's the most valuable and the least likely to fail.
	char szPath[MAX_PATH];
	_snprintf(szPath, sizeof(szPath), "%s.dmp", g_szCrashBase);
	HANDLE hDump = CreateFileA(szPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hDump != INVALID_HANDLE_VALUE)
	{
		MINIDUMP_EXCEPTION_INFORMATION mei;
		mei.ThreadId = GetCurrentThreadId();
		mei.ExceptionPointers = pInfo;
		mei.ClientPointers = FALSE;
		MiniDumpWriteDump(hProcess, GetCurrentProcessId(), hDump,
			(MINIDUMP_TYPE)(MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithDataSegs),
			&mei, NULL, NULL);
		CloseHandle(hDump);
	}

	_snprintf(szPath, sizeof(szPath), "%s.txt", g_szCrashBase);
	FILE* pFile = fopen(szPath, "w");
	if (pFile)
	{
		const EXCEPTION_RECORD* pRec = pInfo->ExceptionRecord;
		fprintf(pFile, "NOLF2 crash report  %04d-%02d-%02d %02d:%02d:%02d\n\n",
			st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
		fprintf(pFile, "Exception: 0x%08lX %s\n", pRec->ExceptionCode, ExceptionName(pRec->ExceptionCode));
		if (pRec->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && pRec->NumberParameters >= 2)
		{
			fprintf(pFile, "           %s address 0x%08lX\n",
				pRec->ExceptionInformation[0] == 0 ? "reading" :
				pRec->ExceptionInformation[0] == 1 ? "writing" : "executing",
				(unsigned long)pRec->ExceptionInformation[1]);
		}
		fprintf(pFile, "At:        0x%08lX\n\n", (unsigned long)(UINT_PTR)pRec->ExceptionAddress);

		SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
		if (SymInitialize(hProcess, NULL, TRUE))
		{
			fprintf(pFile, "Call stack (most recent first):\n");
			WriteStack(pFile, hProcess, hThread, *pInfo->ContextRecord);
			SymCleanup(hProcess);
		}
		else
		{
			fprintf(pFile, "(could not load symbols for a call stack)\n");
		}

		const CONTEXT* c = pInfo->ContextRecord;
		fprintf(pFile, "\nRegisters: EAX=%08lX EBX=%08lX ECX=%08lX EDX=%08lX ESI=%08lX EDI=%08lX\n"
			"           EBP=%08lX ESP=%08lX EIP=%08lX\n",
			c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi, c->Ebp, c->Esp, c->Eip);
		fclose(pFile);
	}

	OutputDebugStringA("NOLF2: crash report written\n");
	return EXCEPTION_EXECUTE_HANDLER;	// end the process quietly
}

void CrashReport_Install()
{
	SetUnhandledExceptionFilter(CrashFilter);
}
