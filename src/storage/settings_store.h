#pragma once

#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

#include "display/display_types.h"

namespace quickdraw {

struct PresetMode {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t frequencyHz = 0;
    std::uint32_t bitsPerPixel = 0;
    std::uint32_t displayFlags = 0;
};

struct PresetTarget {
    PersistentMonitorId monitorId;
    PresetMode mode;
};

struct DisplayPreset {
    std::wstring id;
    std::wstring name;
    std::vector<PresetTarget> targets;
};

struct AppSettings {
    bool startWithWindows = false;
    unsigned confirmationSeconds = 15;
    std::vector<DisplayPreset> presets;
};

class SettingsStore final {
public:
    SettingsStore();

    bool Load(AppSettings& settings) const;
    bool Save(const AppSettings& settings) const;

    [[nodiscard]] const std::wstring& Path() const noexcept;

private:
    std::wstring directoryPath_;
    std::wstring filePath_;
};

std::wstring CreatePresetId();
DisplayPreset CapturePreset(
    const std::wstring& name,
    const std::vector<MonitorInfo>& displays);
const DisplayMode* FindPresetMode(
    const PresetMode& requested,
    const MonitorInfo& display);

}  // namespace quickdraw
