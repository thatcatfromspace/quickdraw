#pragma once

#include <windows.h>

#include <vector>

#include "display/display_types.h"

namespace quickdraw {

class IDisplayBackend {
public:
    virtual ~IDisplayBackend() = default;
    virtual LONG EnumerateDisplays(std::vector<MonitorInfo>& displays) = 0;
    virtual LONG ReadCurrentMode(
        const std::wstring& gdiDeviceName,
        DEVMODEW& mode) = 0;
    virtual LONG TestMode(
        const std::wstring& gdiDeviceName,
        const DEVMODEW& mode) = 0;
    virtual LONG StageMode(
        const std::wstring& gdiDeviceName,
        const DEVMODEW& mode) = 0;
    virtual LONG ApplyStagedModes() = 0;
};

class Win32DisplayBackend final : public IDisplayBackend {
public:
    LONG EnumerateDisplays(std::vector<MonitorInfo>& displays) override;
    LONG ReadCurrentMode(
        const std::wstring& gdiDeviceName,
        DEVMODEW& mode) override;
    LONG TestMode(
        const std::wstring& gdiDeviceName,
        const DEVMODEW& mode) override;
    LONG StageMode(
        const std::wstring& gdiDeviceName,
        const DEVMODEW& mode) override;
    LONG ApplyStagedModes() override;
};

}  // namespace quickdraw
