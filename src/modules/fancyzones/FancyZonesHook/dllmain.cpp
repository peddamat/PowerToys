#include "pch.h"
#include <stdio.h>
#include <windows.h>
#include <commctrl.h>
#include <array>
#include <vector>
#include <iostream>
#include <fstream>
#include <format>

#pragma comment(lib, "comctl32.lib")

HMODULE m_moduleHandle = NULL;
static DWORD dwTlsIndex;

const wchar_t PropertyZoneSizeID[] = L"FancyZones_ZoneSize";
const wchar_t PropertyZoneOriginID[] = L"FancyZones_ZoneOrigin";

LRESULT CALLBACK hookWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
bool AddHook(HWND hwnd);
bool RemoveHook(HWND hwnd);
void LOG(const char* message);
void LOG_MSG(const std::string message);
BOOL GetZoneSizeAndOrigin(HWND window, POINT& zoneSize, POINT& zoneOrigin) noexcept;

void DeepClean();
void DeepCleanByThread();

/******************************************************************************
* DLL Entrypoint
******************************************************************************/
INT APIENTRY DllMain(HMODULE hDLL, DWORD Reason, LPVOID Reserved) {

	switch (Reason) {
	case DLL_PROCESS_ATTACH:
		LOG("Attaching DLL\n");

		//if (m_moduleHandle != NULL)
  //      {
		//	LOG(" Error, DLL already loaded!\n");
  //          return TRUE;
  //      }

		// Allocate a Thread Local Storage (TLS) index, so each
		// hooked thread can independently store its HWND handle.
		if ((dwTlsIndex = TlsAlloc()) == TLS_OUT_OF_INDEXES)
		{ 
			LOG("Error allocating TLS index!\n");
			return FALSE;
		}
		LOG("Allocating TLS index!\n");

		// Increment the DLL reference count so the DLL is not
		// unloaded before we're able to clean-up
        if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCTSTR)DllMain, &m_moduleHandle))
        {
			LOG("Error incrementing reference count!\n");
            return FALSE;
        }
		LOG("Incrementing reference count!\n");
		break;

	case DLL_PROCESS_DETACH:
		LOG("Detaching DLL\n");

		// To ensure 
		DeepClean();
		TlsFree(dwTlsIndex);
        FreeLibraryAndExitThread(hDLL, 1); 
		return false;
		break;

	case DLL_THREAD_ATTACH:
		LOG_MSG(std::format("Attaching to thread: {}\n", GetCurrentThreadId()));
		break;
	case DLL_THREAD_DETACH:
		//LOG_MSG(std::format("Detaching to thread: {}\n", GetCurrentThreadId()));

		//RemoveHook(NULL);
		break;
	}

	return TRUE;
}

BOOL IsSameProcess(DWORD WindowPid)
{
	static const auto processId = GetCurrentProcessId();
	return processId == WindowPid;
}

void DeepCleanByThread()
{
	LOG("Starting deep clean by thread\n");
	auto tid = GetCurrentThreadId();
	PostThreadMessage(tid, WM_APP + 901, (WPARAM)tid, 0xFF);
}

void DeepClean()
{
	LOG("Starting deep clean by thread\n");
    using result_t = std::vector<DWORD>;
    result_t result;

    auto enumWindows = [](HWND hwnd, LPARAM param) -> BOOL {
		DWORD pid;
		DWORD tid = GetWindowThreadProcessId(hwnd, &pid);

		if (IsSameProcess(pid))
		{
            result_t& result = *reinterpret_cast<result_t*>(param);
            result.push_back(tid);
		}

        return TRUE;
    };

    EnumWindows(enumWindows, reinterpret_cast<LPARAM>(&result));

    for (DWORD tid: result)
    {
		LOG_MSG(std::format("Sending kill to: {}\n", tid));
		//LOG_MSG("Sending kill to: %#010X\n", (char *)tid);
		auto r = PostThreadMessage(tid, WM_APP + 901, (WPARAM)tid, 0xFF);
		auto f = GetLastError();
		//LOG_MSG("Got: %i\n", (char *)f);
    }
}

/******************************************************************************
* FancyZones Window Process Subclass
******************************************************************************/
LRESULT CALLBACK hookWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	// Only process windows that are in a zone
	if (!GetPropW(hwnd, PropertyZoneSizeID))
		goto end;

	switch (msg)
	{

	/**
	  *	WM_WINDOWPOSCHANGING events are generated when a window is being sized
	  * or moved.  If the window is maximized, we override the default behavior 
	  * and manually define the window's size and position.
	  */
	case WM_WINDOWPOSCHANGING:
		LOG("Entered WM_WINDOWPOSCHANGING: \n");

		//if (WS_MAXIMIZE & GetWindowLong(hwnd, GWL_STYLE))
		{
			POINT zoneSize = { 0,0 };
			POINT zoneOrigin = { 0,0 };

			if (GetZoneSizeAndOrigin(hwnd, zoneSize, zoneOrigin))
			{
				auto windowpos = reinterpret_cast<WINDOWPOS*>(lParam);

				windowpos->x = zoneOrigin.x;
				windowpos->y = zoneOrigin.y;

				windowpos->cx = zoneSize.x;
				windowpos->cy = zoneSize.y;

				LOG(" window moved\n");
				return 0;
			}
			else
			{
				LOG(" error getting window property!\n");
			}
		}
		break;

	case WM_DESTROY:
		LOG("Entered WM_DESTROY\n");
		RemoveHook(NULL);
		break;

	case WM_APP+901:
        LOG_MSG(std::format("Received wndProc Unhook Message: {}\n", (char*)hwnd));
		RemoveHook(NULL);
		return 1;
		break;

	case WM_APP+902:
        LOG_MSG(std::format("Received wndProc Deep Clean Message: {}\n", (char*)hwnd));
		DeepClean();
		return 1;
		break;

	}

	end:
	return DefSubclassProc(hwnd, msg, wParam, lParam);
}


/******************************************************************************
* Window Process Hook Functions
******************************************************************************/
bool AddHook(HWND hwnd)
{
	//LOG_MSG(std::format("Hooking wndProc of: {}\n", (char *)hwnd));
    LOG("Adding hook\n");

	if (!SetWindowSubclass(hwnd, &hookWndProc, 1, 0)) {
		LOG(" Error!\n");
		return FALSE;
	}

	// Save handle to subclassed window
	if (!TlsSetValue(dwTlsIndex, hwnd))
	{
		LOG(" Error saving hwnd for thread!\n");
		return FALSE;
	}

	LOG(" Success!\n");
	return TRUE;
}
	
bool RemoveHook(HWND hwnd)
{
	if (hwnd == NULL)
	{
		hwnd = (HWND)TlsGetValue(dwTlsIndex);
	}
	LOG_MSG(std::format("Unhooking wndProc of: {}\n", (char *)hwnd));

	if (hwnd == NULL)
	{
		LOG(" Skipping, thread wasn't hooked!\n");
		return TRUE;
	}

	if (!RemoveWindowSubclass(hwnd, &hookWndProc, 1)) {
		LOG(" Error, couldn't remove subclass!\n");
		return FALSE;
	}

	TlsSetValue(dwTlsIndex, 0);

	LOG(" Success!\n");
	return TRUE;
}

extern "C" __declspec(dllexport) 
LRESULT CALLBACK getMsgProc(int code, WPARAM wParam, LPARAM lParam) {
	auto msg = reinterpret_cast<MSG*>(lParam);

	if (code < 0) // Do not process
		goto end;

	// Only process the message after it's been removed from the message queue, 
	// otherwise we may accidentally process it more than once.
	if (wParam != PM_REMOVE)
		goto end; 

	if (msg->message == WM_APP + 900)
	{
		LOG("Received AddHook message\n");
		auto hwnd = reinterpret_cast<HWND>(msg->wParam);
		AddHook(hwnd);
	}
	else if (msg->message == WM_APP+901)
	{
		LOG("Received RemoveHook message\n");
		auto hwnd = reinterpret_cast<HWND>(msg->wParam);
		RemoveHook(NULL);
	}

	end:
	return(CallNextHookEx(NULL, code, wParam, lParam));
}


/******************************************************************************
* Utility functions
******************************************************************************/
BOOL GetZoneSizeAndOrigin(HWND window, POINT &zoneSize, POINT &zoneOrigin) noexcept
{
	// Retrieve the zone size
    auto zsProp = GetPropW(window, PropertyZoneSizeID);
	if (!zsProp)
		return false;

	std::array<int, 2> zsArray;
	memcpy(zsArray.data(), &zsProp, sizeof zsArray);

	zoneSize.x = static_cast<int>(zsArray[0]);
	zoneSize.y = static_cast<int>(zsArray[1]);

	// Retrieve the zone position
    auto zoProp = GetPropW(window, PropertyZoneOriginID);
	if (!zoProp)
		return false;

	std::array<int, 2> zoArray;
	memcpy(zoArray.data(), &zoProp, sizeof zoArray);

	zoneOrigin.x = static_cast<int>(zoArray[0]);
	zoneOrigin.y = static_cast<int>(zoArray[1]);

	// {width, height}
	//DPIAware::Convert(MonitorFromWindow(window, MONITOR_DEFAULTTONULL), windowWidth, windowHeight);

	return true;
}


void LOG(const char* message)
{	
	return;
	FILE* file;
	fopen_s(&file, "c:\\users\\me\\debug\\dllmain2.txt", "a+");

	fprintf(file, message);

	fclose(file);
}

void LOG_MSG(const std::string message)
{	
	return;
	std::ofstream myfile("c:\\users\\me\\debug\\dllmain3.txt", std::ios::app);

	myfile << message;

	myfile.close();
}
