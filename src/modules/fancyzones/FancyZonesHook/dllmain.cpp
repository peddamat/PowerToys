#include "pch.h"
#include <windows.h>

#include <array>
#include <vector>

#include <map>
#include <mutex>

#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

#include "FancyZonesHookEventIDs.h"

const wchar_t PropertyZoneSizeID[] = L"FancyZones_ZoneSize";
const wchar_t PropertyZoneOriginID[] = L"FancyZones_ZoneOrigin";

std::map<HWND, DWORD> hookedWindows;
std::mutex mutex;

bool AddHook(HWND hwnd);
bool RemoveHook(HWND hwnd);
void CleanupHookedWindows();
BOOL GetStampedZoneProperties(HWND window, POINT& zoneSize, POINT& zoneOrigin) noexcept;
LRESULT CALLBACK hookWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);


/******************************************************************************
* DLL Entrypoint
******************************************************************************/
INT APIENTRY DllMain(HMODULE hDLL, DWORD Reason, LPVOID Reserved) {

	switch (Reason) 
	{

	case DLL_PROCESS_ATTACH:
		// Unused
		break;

	// Generated after Fancy Zones has called UnsetWindowsHookEx() on the last
	// message listener hook on the process this DLL was injected into
	case DLL_PROCESS_DETACH:
		CleanupHookedWindows();
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


/******************************************************************************
* Window Process Hook Functions
******************************************************************************/
bool AddHook(HWND hwnd)
{
	std::lock_guard<std::mutex> guard(mutex);

	// If the window isn't already subclassed...
	if (!GetWindowSubclass(hwnd, &hookWndProc, 1, 0))
	{
		if (!SetWindowSubclass(hwnd, &hookWndProc, 1, 0)) 
		{
			return FALSE;
		}
	}

	hookedWindows[hwnd] = GetCurrentThreadId();

	return TRUE;
}
	
bool RemoveHook(HWND hwnd)
{
	if (hwnd == NULL)
		return false;

	std::lock_guard<std::mutex> guard(mutex);

	// Make sure the window is actually subclassed
	if (GetWindowSubclass(hwnd, &hookWndProc, 1, 0))
	{
		if (!RemoveWindowSubclass(hwnd, &hookWndProc, 1)) {
			hookedWindows.erase(hwnd);
			return FALSE;
		}

		hookedWindows.erase(hwnd);

		return TRUE;
	}
	else
	{
		return false;
	}
}

void CleanupHookedWindows()
{
	std::map<HWND, DWORD>::iterator it;
	for (it=hookedWindows.begin(); it!=hookedWindows.end();)
	{
        if (IsWindow(it->first))
        {
            SendMessage(it->first, WM_PRIV_UNHOOK_WINDOW, (WPARAM)it->first, 0);
        }
        else
        {
			hookedWindows.erase(it->first);
        }

		it = hookedWindows.begin();
	}
}


/******************************************************************************
* SetWindowsHookEx Callback
******************************************************************************/
extern "C" __declspec(dllexport) 
LRESULT CALLBACK getMsgProc(int code, WPARAM wParam, LPARAM lParam) {
	if (code < 0) 
		goto end;

	// We only process messages that have been removed from the 
	// message queue to guarantee that we only process them once
	if (wParam == PM_REMOVE)
	{
		auto msg = reinterpret_cast<MSG*>(lParam);
		if (msg->message == WM_PRIV_HOOK_WINDOW)
		{
			auto hwnd = reinterpret_cast<HWND>(msg->wParam);
			AddHook(hwnd);
		}
		else if (msg->message == WM_PRIV_UNHOOK_ALL_WINDOWS)
		{
			auto hwnd = reinterpret_cast<HWND>(msg->wParam);
			RemoveHook(hwnd);
		}
	}

	end:
	return(CallNextHookEx(NULL, code, wParam, lParam));
}


/******************************************************************************
* FancyZones Window Process Subclass
******************************************************************************/
LRESULT CALLBACK hookWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	// If the window isn't stamped with a zone, or the Shift 
	// key is being held, pretend that we're not even here...
    if (!GetPropW(window, PropertyZoneSizeID) || (GetAsyncKeyState(VK_SHIFT) & 0x8000))
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

		// The system sets the WS_MAXIMIZE style prior to posting a 
		// WM_WINDOWPOSCHANGING message, which is convenient for us...
		if (WS_MAXIMIZE & GetWindowLong(window, GWL_STYLE))
		{
			RECT c;
			GetClientRect(window, &c);

			MONITORINFO mi;
			mi.cbSize = sizeof(mi);

			// If the window is actually maximized, as in, filling the entire monitor, let the default
			// window handler handle the event.  This prevents weird quirks, like WM_DLGFRAME being left
			// on a window after it has been restored.
            if (GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &mi))
            {
				if (((mi.rcWork.right - mi.rcWork.left) == (c.right - c.left) &&
					((mi.rcWork.bottom - mi.rcWork.top) == (c.bottom - c.top))))
				{
					break;
				}
            }

			POINT zoneSize, zoneOrigin;
			if (GetStampedZoneProperties(window, zoneSize, zoneOrigin))
			{
                RECT w;
                GetWindowRect(window, &w);

				auto windowpos = reinterpret_cast<WINDOWPOS*>(lParam);

				windowpos->x = zoneOrigin.x;
				windowpos->y = zoneOrigin.y;

				windowpos->cx = zoneSize.x;
				windowpos->cy = zoneSize.y;


				// If the window is not already filling the zone...
                if (!((zoneOrigin.x == w.left) &&
                      (zoneOrigin.y == w.top) &&
                      (zoneSize.x == (w.right - w.left)) &&
                      (zoneSize.y == (w.bottom - w.top))))
                {
					// ... remove the WS_MAXIMIZE style
                    SetWindowLong(window, GWL_STYLE, GetWindowLong(window, GWL_STYLE) & ~WS_MAXIMIZE);

					windowpos->flags = SWP_FRAMECHANGED | SWP_SHOWWINDOW;
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
        RemoveHook(window);
		return DefSubclassProc(window, message, wParam, lParam);
    }
	break;

	default:
    {
        if (message == WM_PRIV_UNHOOK_WINDOW)
        {
            RemoveHook(window);
            return 1;
        }
		else if (message == WM_PRIV_UNHOOK_ALL_WINDOWS)
        {
            CleanupHookedWindows();
            return 1;
        }
    }
    break;
	}

	end:
	return DefSubclassProc(window, message, wParam, lParam);
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


