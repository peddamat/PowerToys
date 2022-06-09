#include "pch.h"
#include "FancyZonesWindowProperties.h"

#include <common/display/dpi_aware.h>
#include <FancyZonesLib/ZoneIndexSetBitmask.h>

#include <common/logger/logger.h>
#include <common/utils/winapi_error.h>

// Zoned window properties are not localized.
namespace ZonedWindowProperties
{
    const wchar_t PropertyMultipleZone64ID[] = L"FancyZones_zones"; // maximum possible zone count = 64
    const wchar_t PropertyMultipleZone128ID[] = L"FancyZones_zones_max128"; // additional property to allow maximum possible zone count = 128

    const wchar_t PropertySortKeyWithinZone[] = L"FancyZones_TabSortKeyWithinZone";

    const wchar_t PropertyZoneSizeID[] = L"FancyZones_ZoneSize";
	const wchar_t PropertyZoneOriginID[] = L"FancyZones_ZoneOrigin";
}

// Based on FancyZonesWindowUtils::SaveWindowSizeAndOrigin(HWND window)
BOOL FancyZonesWindowProperties::StampZoneDimensions(HWND window, const RECT& rect)
{
	float width = static_cast<float>(rect.right - rect.left);
	float height = static_cast<float>(rect.bottom - rect.top);
	float originX = static_cast<float>(rect.left);
	float originY = static_cast<float>(rect.top);

	DPIAware::InverseConvert(MonitorFromWindow(window, MONITOR_DEFAULTTONULL), width, height);
	DPIAware::InverseConvert(MonitorFromWindow(window, MONITOR_DEFAULTTONULL), originX, originY);

	std::array<int, 2> windowSizeData = { static_cast<int>(width), static_cast<int>(height) };
	std::array<int, 2> windowOriginData = { static_cast<int>(originX), static_cast<int>(originY) };
	HANDLE rawData;
	memcpy(&rawData, windowSizeData.data(), sizeof rawData);
	SetPropW(window, ZonedWindowProperties::PropertyZoneSizeID, rawData);
	memcpy(&rawData, windowOriginData.data(), sizeof rawData);
	SetPropW(window, ZonedWindowProperties::PropertyZoneOriginID, rawData);

	return true;
}

void FancyZonesWindowProperties::StampZoneIndexProperty(HWND window, const ZoneIndexSet& zoneSet)
{
    RemoveZoneIndexProperty(window);
    ZoneIndexSetBitmask bitmask = ZoneIndexSetBitmask::FromIndexSet(zoneSet);

    if (bitmask.part1 != 0)
    {
        std::array<int32_t, 2> data{
            static_cast<int>(bitmask.part1),
            static_cast<int>(bitmask.part1 >> 32)
        };

        HANDLE rawData;
        memcpy(&rawData, data.data(), sizeof data);

        if (!SetProp(window, ZonedWindowProperties::PropertyMultipleZone64ID, rawData))
        {
            Logger::error(L"Failed to stamp window {}", get_last_error_or_default(GetLastError()));
        }
    }

    if (bitmask.part2 != 0)
    {
        std::array<int32_t, 2> data{
            static_cast<int>(bitmask.part2),
            static_cast<int>(bitmask.part2 >> 32)
        };

        HANDLE rawData;
        memcpy(&rawData, data.data(), sizeof data);

        if (!SetProp(window, ZonedWindowProperties::PropertyMultipleZone128ID, rawData))
        {
            Logger::error(L"Failed to stamp window {}", get_last_error_or_default(GetLastError()));
        }
    }
}

void FancyZonesWindowProperties::RemoveZoneIndexProperty(HWND window)
{
    ::RemoveProp(window, ZonedWindowProperties::PropertyMultipleZone64ID);
    ::RemoveProp(window, ZonedWindowProperties::PropertyMultipleZone128ID);

    //::RemoveProp(window, ZonedWindowProperties::PropertyZoneOriginID);
    //::RemoveProp(window, ZonedWindowProperties::PropertyZoneSizeID);
}

ZoneIndexSet FancyZonesWindowProperties::RetrieveZoneIndexProperty(HWND window)
{
    HANDLE handle64 = ::GetProp(window, ZonedWindowProperties::PropertyMultipleZone64ID);
    HANDLE handle128 = ::GetProp(window, ZonedWindowProperties::PropertyMultipleZone128ID);

    ZoneIndexSetBitmask bitmask{};

    if (handle64)
    {
        std::array<int32_t, 2> data;
        memcpy(data.data(), &handle64, sizeof data);
        bitmask.part1 = (static_cast<decltype(bitmask.part1)>(data[1]) << 32) + data[0];
    }

    if (handle128)
    {
        std::array<int32_t, 2> data;
        memcpy(data.data(), &handle128, sizeof data);
        bitmask.part2 = (static_cast<decltype(bitmask.part2)>(data[1]) << 32) + data[0];
    }

    return bitmask.ToIndexSet();
}

std::optional<size_t> FancyZonesWindowProperties::GetTabSortKeyWithinZone(HWND window)
{
    auto rawTabSortKeyWithinZone = ::GetPropW(window, ZonedWindowProperties::PropertySortKeyWithinZone);
    if (rawTabSortKeyWithinZone == NULL)
    {
        return std::nullopt;
    }

    auto tabSortKeyWithinZone = reinterpret_cast<uint64_t>(rawTabSortKeyWithinZone) - 1;
    return tabSortKeyWithinZone;
}

void FancyZonesWindowProperties::SetTabSortKeyWithinZone(HWND window, std::optional<size_t> tabSortKeyWithinZone)
{
    if (!tabSortKeyWithinZone.has_value())
    {
        ::RemovePropW(window, ZonedWindowProperties::PropertySortKeyWithinZone);
    }
    else
    {
        auto rawTabSortKeyWithinZone = reinterpret_cast<HANDLE>(tabSortKeyWithinZone.value() + 1);
        ::SetPropW(window, ZonedWindowProperties::PropertySortKeyWithinZone, rawTabSortKeyWithinZone);
    }
}
