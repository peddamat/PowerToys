#include "pch.h"
#include <windows.h>

#include <array>
#include <vector>

#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

#include <FancyZonesHookEventIDs.h>

HMODULE m_moduleHandle = NULL;
static DWORD dwTlsIndex;

const wchar_t PropertyZoneSizeID[] = L"FancyZones_ZoneSize";
const wchar_t PropertyZoneOriginID[] = L"FancyZones_ZoneOrigin";

bool AddHook(HWND hwnd);
bool RemoveHook(HWND hwnd);
LRESULT CALLBACK hookWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

BOOL GetStampedZoneProperties(HWND window, POINT& zoneSize, POINT& zoneOrigin) noexcept;

void DeepClean();
void DeepCleanByThread();

/******************************************************************************
* DLL Entrypoint
******************************************************************************/
INT APIENTRY DllMain(HMODULE hDLL, DWORD Reason, LPVOID Reserved) {

	switch (Reason) {
	case DLL_PROCESS_ATTACH:

		// Allocate a Thread Local Storage (TLS) index, so each
		// hooked thread can independently store its HWND handle.
		if ((dwTlsIndex = TlsAlloc()) == TLS_OUT_OF_INDEXES)
		{ 
			return FALSE;
		}

		// Increment the DLL reference count so the DLL is not
		// unloaded before we're able to clean-up
        if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCTSTR)DllMain, &m_moduleHandle))
        {
            return FALSE;
        }
		break;

	case DLL_PROCESS_DETACH:

		// To ensure 
		DeepClean();
		TlsFree(dwTlsIndex);
        FreeLibraryAndExitThread(hDLL, 1); 
		return false;
		break;

	case DLL_THREAD_ATTACH:
		// Unused
		break;

	case DLL_THREAD_DETACH:
		// Unused
		break;
	}

	return TRUE;
}

BOOL IsOurWindow(DWORD windowPid)
{
	return windowPid == GetCurrentProcessId();
}

void DeepCleanByThread()
{
	// TODO: Fix this
	//PostThreadMessage(GetCurrentThreadId(), WM_PRIV_HOOK_WINDOW, (WPARAM)GetCurrentThreadId(), 0);
}

void DeepClean()
{
    using result_t = std::vector<HWND>;
    result_t result;

    auto enumWindows = [](HWND hwnd, LPARAM param) -> BOOL {
		DWORD pid;
		DWORD tid = GetWindowThreadProcessId(hwnd, &pid);

		if (IsOurWindow(pid))
		{
            result_t& result = *reinterpret_cast<result_t*>(param);
            result.push_back(hwnd);
		}

        return TRUE;
    };

    EnumWindows(enumWindows, reinterpret_cast<LPARAM>(&result));

    for (HWND window: result)
    {
        PostMessage(window, WM_PRIV_HOOK_WINDOW, (WPARAM)window, 0);
    }
}

/******************************************************************************
* FancyZones Window Process Subclass
******************************************************************************/
LRESULT CALLBACK hookWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	// Only process windows that are in a zone
	if (!GetPropW(window, PropertyZoneSizeID))
		goto end;


	switch (message)
	{

	/**
	  *	WM_WINDOWPOSCHANGING events are generated when a window is being sized
	  * or moved.  If the window is maximized, we override the default behavior 
	  * and manually define the window's size and position.
	  */
	case WM_WINDOWPOSCHANGING:
    {
		// Skip if Shift is being pressed...
        if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
            break;

		// The system sets the WS_MAXIMIZE style prior to posting a 
		// WM_WINDOWPOSCHANGING message, which is convenient for us...
		if (WS_MAXIMIZE & GetWindowLong(window, GWL_STYLE))
		{
			POINT zoneSize;
			POINT zoneOrigin;

			if (GetStampedZoneProperties(window, zoneSize, zoneOrigin))
			{
                RECT c;
                GetWindowRect(window, &c);

				auto windowpos = reinterpret_cast<WINDOWPOS*>(lParam);

				windowpos->x = zoneOrigin.x;
				windowpos->y = zoneOrigin.y;

				windowpos->cx = zoneSize.x;
				windowpos->cy = zoneSize.y;

				// Unset the WS_MAXIMIZE style, unless the window was already filling the zone
                if (!((zoneOrigin.x == c.left) &&
                      (zoneOrigin.y == c.top) &&
                      (zoneSize.x == (c.right - c.left)) &&
                      (zoneSize.y == (c.bottom - c.top))))
                {
                    SetWindowLong(window, GWL_STYLE, GetWindowLong(window, GWL_STYLE) & ~WS_MAXIMIZE);
                }
				return 0;
			}
		}
    }
	break;

	/**
	  *	WM_DESTROY events are generated when the user closes an application
	  * window.  If this happens, we need to...
	  */
	case WM_DESTROY:
    {
        RemoveHook(NULL);
    }
	break;

	default:
    {
        if (message == WM_PRIV_UNHOOK_WINDOW)
        {
            RemoveHook(NULL);
            return 1;
        }
		else if (message == WM_PRIV_DEEP_CLEAN)
        {
            DeepClean();
            return 1;
        }
    }
    break;
	}

	end:
	return DefSubclassProc(window, message, wParam, lParam);
}


/******************************************************************************
* Window Process Hook Functions
******************************************************************************/
bool AddHook(HWND hwnd)
{
	if (!SetWindowSubclass(hwnd, &hookWndProc, 1, 0)) {
		return FALSE;
	}

	// Save handle to subclassed window
	if (!TlsSetValue(dwTlsIndex, hwnd))
	{
		return FALSE;
	}

	return TRUE;
}
	

// By default we retrieve the stored window handle
bool RemoveHook(HWND hwnd = (HWND) TlsGetValue(dwTlsIndex))
{
	if (hwnd == NULL)
	{
		return TRUE;
	}

	if (!RemoveWindowSubclass(hwnd, &hookWndProc, 1)) {
		return FALSE;
	}

	TlsSetValue(dwTlsIndex, 0);

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

	if (msg->message == WM_PRIV_HOOK_WINDOW)
	{
		auto window = reinterpret_cast<HWND>(msg->wParam);
		AddHook(window);
	}
	else if (msg->message == WM_PRIV_UNHOOK_WINDOW)
	{
		auto window = reinterpret_cast<HWND>(msg->wParam);
		RemoveHook(NULL);
	}

	end:
	return(CallNextHookEx(NULL, code, wParam, lParam));
}


/******************************************************************************
* Utility functions
******************************************************************************/
BOOL GetStampedZoneProperties(HWND window, POINT &zoneSize, POINT &zoneOrigin) noexcept
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


