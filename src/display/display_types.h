#pragma once

#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

namespace quickdraw {

struct RuntimeMonitorId {
    LUID adapterId{};
    UINT32 sourceId = 0;
    UINT32 targetId = 0;
    std::wstring gdiDeviceName;
};

struct PersistentMonitorId {
    std::wstring monitorDevicePath;
    std::uint16_t edidManufacturer = 0;
    std::uint16_t edidProduct = 0;
    std::uint32_t connectorInstance = 0;
    std::wstring friendlyName;
};

struct DisplayMode {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t frequencyHz = 0;
    std::uint32_t bitsPerPixel = 0;
    std::uint32_t displayFlags = 0;
    bool interlaced = false;
    bool current = false;
    DEVMODEW nativeMode{};
};

struct MonitorInfo {
    RuntimeMonitorId runtimeId;
    PersistentMonitorId persistentId;
    DISPLAYCONFIG_VIDEO_OUTPUT_TECHNOLOGY outputTechnology =
        DISPLAYCONFIG_OUTPUT_TECHNOLOGY_OTHER;
    POINTL position{};
    bool primary = false;
    bool independentlyConfigurable = true;
    std::uint32_t currentRefreshNumerator = 0;
    std::uint32_t currentRefreshDenominator = 1;
    DEVMODEW currentNativeMode{};
    std::vector<DisplayMode> modes;
};

}  // namespace quickdraw
