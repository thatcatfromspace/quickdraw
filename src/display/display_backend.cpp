#include "display/display_backend.h"

#include <algorithm>
#include <cwchar>
#include <vector>

#include "display/mode_catalog.h"

namespace quickdraw {
namespace {

bool SameAdapter(const LUID& left, const LUID& right) {
    return left.HighPart == right.HighPart && left.LowPart == right.LowPart;
}

bool SameSource(const MonitorInfo& left, const MonitorInfo& right) {
    return SameAdapter(left.runtimeId.adapterId, right.runtimeId.adapterId) &&
           left.runtimeId.sourceId == right.runtimeId.sourceId;
}

bool IsPrimaryGdiDevice(const std::wstring& deviceName) {
    for (DWORD index = 0;; ++index) {
        DISPLAY_DEVICEW device{};
        device.cb = sizeof(device);
        if (!EnumDisplayDevicesW(nullptr, index, &device, 0)) {
            return false;
        }

        if (deviceName == device.DeviceName) {
            return (device.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) != 0;
        }
    }
}

std::wstring FallbackMonitorName(const std::wstring& gdiDeviceName) {
    if (!gdiDeviceName.empty()) {
        return L"Display " + gdiDeviceName;
    }
    return L"Unknown display";
}

LONG QueryActiveConfiguration(
    std::vector<DISPLAYCONFIG_PATH_INFO>& paths,
    std::vector<DISPLAYCONFIG_MODE_INFO>& modes) {
    for (int attempt = 0; attempt < 3; ++attempt) {
        UINT32 pathCount = 0;
        UINT32 modeCount = 0;
        LONG result = GetDisplayConfigBufferSizes(
            QDC_ONLY_ACTIVE_PATHS,
            &pathCount,
            &modeCount);
        if (result != ERROR_SUCCESS) {
            return result;
        }

        paths.resize(pathCount);
        modes.resize(modeCount);
        result = QueryDisplayConfig(
            QDC_ONLY_ACTIVE_PATHS,
            &pathCount,
            paths.data(),
            &modeCount,
            modes.data(),
            nullptr);
        if (result == ERROR_SUCCESS) {
            paths.resize(pathCount);
            modes.resize(modeCount);
            return ERROR_SUCCESS;
        }
        if (result != ERROR_INSUFFICIENT_BUFFER) {
            return result;
        }
    }

    return ERROR_INSUFFICIENT_BUFFER;
}

LONG BuildMonitorInfo(
    const DISPLAYCONFIG_PATH_INFO& path,
    MonitorInfo& monitor) {
    DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName{};
    sourceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
    sourceName.header.size = sizeof(sourceName);
    sourceName.header.adapterId = path.sourceInfo.adapterId;
    sourceName.header.id = path.sourceInfo.id;
    LONG result = DisplayConfigGetDeviceInfo(&sourceName.header);
    if (result != ERROR_SUCCESS) {
        return result;
    }

    DISPLAYCONFIG_TARGET_DEVICE_NAME targetName{};
    targetName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
    targetName.header.size = sizeof(targetName);
    targetName.header.adapterId = path.targetInfo.adapterId;
    targetName.header.id = path.targetInfo.id;
    result = DisplayConfigGetDeviceInfo(&targetName.header);
    if (result != ERROR_SUCCESS) {
        return result;
    }

    monitor.runtimeId.adapterId = path.sourceInfo.adapterId;
    monitor.runtimeId.sourceId = path.sourceInfo.id;
    monitor.runtimeId.targetId = path.targetInfo.id;
    monitor.runtimeId.gdiDeviceName = sourceName.viewGdiDeviceName;
    monitor.persistentId.monitorDevicePath = targetName.monitorDevicePath;
    monitor.persistentId.edidManufacturer = targetName.edidManufactureId;
    monitor.persistentId.edidProduct = targetName.edidProductCodeId;
    monitor.persistentId.connectorInstance = targetName.connectorInstance;
    monitor.persistentId.friendlyName = targetName.monitorFriendlyDeviceName;
    if (monitor.persistentId.friendlyName.empty()) {
        monitor.persistentId.friendlyName =
            FallbackMonitorName(monitor.runtimeId.gdiDeviceName);
    }
    monitor.outputTechnology = path.targetInfo.outputTechnology;
    monitor.currentRefreshNumerator = path.targetInfo.refreshRate.Numerator;
    monitor.currentRefreshDenominator =
        path.targetInfo.refreshRate.Denominator == 0
            ? 1
            : path.targetInfo.refreshRate.Denominator;
    monitor.primary = IsPrimaryGdiDevice(monitor.runtimeId.gdiDeviceName);

    monitor.currentNativeMode = {};
    monitor.currentNativeMode.dmSize = sizeof(monitor.currentNativeMode);
    if (!EnumDisplaySettingsExW(
            monitor.runtimeId.gdiDeviceName.c_str(),
            ENUM_CURRENT_SETTINGS,
            &monitor.currentNativeMode,
            0)) {
        return ERROR_GEN_FAILURE;
    }

    monitor.position = monitor.currentNativeMode.dmPosition;

    std::vector<DEVMODEW> enumeratedModes;
    for (DWORD index = 0;; ++index) {
        DEVMODEW mode{};
        mode.dmSize = sizeof(mode);
        if (!EnumDisplaySettingsExW(
                monitor.runtimeId.gdiDeviceName.c_str(),
                index,
                &mode,
                0)) {
            break;
        }
        enumeratedModes.push_back(mode);
    }
    monitor.modes = BuildModeCatalog(enumeratedModes, monitor.currentNativeMode);
    return ERROR_SUCCESS;
}

}  // namespace

LONG Win32DisplayBackend::EnumerateDisplays(std::vector<MonitorInfo>& displays) {
    displays.clear();

    std::vector<DISPLAYCONFIG_PATH_INFO> paths;
    std::vector<DISPLAYCONFIG_MODE_INFO> modes;
    const LONG queryResult = QueryActiveConfiguration(paths, modes);
    if (queryResult != ERROR_SUCCESS) {
        return queryResult;
    }

    for (const DISPLAYCONFIG_PATH_INFO& path : paths) {
        if ((path.flags & DISPLAYCONFIG_PATH_ACTIVE) == 0) {
            continue;
        }

        MonitorInfo monitor;
        const LONG result = BuildMonitorInfo(path, monitor);
        if (result != ERROR_SUCCESS) {
            return result;
        }
        displays.push_back(std::move(monitor));
    }

    for (MonitorInfo& monitor : displays) {
        const auto sharedSourceCount = std::count_if(
            displays.begin(),
            displays.end(),
            [&monitor](const MonitorInfo& other) {
                return SameSource(monitor, other);
            });
        monitor.independentlyConfigurable = sharedSourceCount == 1;
    }

    std::sort(
        displays.begin(),
        displays.end(),
        [](const MonitorInfo& left, const MonitorInfo& right) {
            if (left.primary != right.primary) {
                return left.primary;
            }
            if (left.position.x != right.position.x) {
                return left.position.x < right.position.x;
            }
            if (left.position.y != right.position.y) {
                return left.position.y < right.position.y;
            }
            return left.persistentId.friendlyName < right.persistentId.friendlyName;
        });

    return ERROR_SUCCESS;
}

LONG Win32DisplayBackend::ReadCurrentMode(
    const std::wstring& gdiDeviceName,
    DEVMODEW& mode) {
    mode = {};
    mode.dmSize = sizeof(mode);
    return EnumDisplaySettingsExW(
               gdiDeviceName.c_str(),
               ENUM_CURRENT_SETTINGS,
               &mode,
               0)
        ? DISP_CHANGE_SUCCESSFUL
        : DISP_CHANGE_FAILED;
}

LONG Win32DisplayBackend::TestMode(
    const std::wstring& gdiDeviceName,
    const DEVMODEW& mode) {
    DEVMODEW requestedMode = mode;
    return ChangeDisplaySettingsExW(
        gdiDeviceName.c_str(),
        &requestedMode,
        nullptr,
        CDS_TEST,
        nullptr);
}

LONG Win32DisplayBackend::StageMode(
    const std::wstring& gdiDeviceName,
    const DEVMODEW& mode) {
    DEVMODEW requestedMode = mode;
    return ChangeDisplaySettingsExW(
        gdiDeviceName.c_str(),
        &requestedMode,
        nullptr,
        CDS_UPDATEREGISTRY | CDS_NORESET,
        nullptr);
}

LONG Win32DisplayBackend::ApplyStagedModes() {
    return ChangeDisplaySettingsExW(nullptr, nullptr, nullptr, 0, nullptr);
}

}  // namespace quickdraw
