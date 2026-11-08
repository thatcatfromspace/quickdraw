#pragma once

#include <windows.h>

#include <string>
#include <vector>

#include "display/display_types.h"
#include "storage/settings_store.h"

namespace quickdraw {

struct TrayMenuModeAction {
    UINT command = 0;
    std::wstring gdiDeviceName;
    std::wstring monitorName;
    PersistentMonitorId monitorId;
    DisplayMode mode;
};

struct TrayMenuPresetAction {
    UINT command = 0;
    std::wstring presetId;
};

class TrayMenu final {
public:
    static constexpr UINT kExitCommand = 1002;
    static constexpr UINT kSettingsCommand = 1003;
    static constexpr UINT kSavePresetCommand = 1004;
    static constexpr UINT kAboutCommand = 1005;

    TrayMenu() = default;
    ~TrayMenu();

    TrayMenu(const TrayMenu&) = delete;
    TrayMenu& operator=(const TrayMenu&) = delete;

    bool Build(
        const std::vector<MonitorInfo>& displays,
        LONG inventoryError,
        const std::vector<DisplayPreset>& presets,
        bool changesAllowed);
    [[nodiscard]] HMENU Handle() const noexcept;
    [[nodiscard]] const TrayMenuModeAction* FindModeAction(UINT command) const noexcept;
    [[nodiscard]] const TrayMenuPresetAction* FindPresetAction(UINT command) const noexcept;

private:
    void Reset() noexcept;

    HMENU root_ = nullptr;
    std::vector<TrayMenuModeAction> modeActions_;
    std::vector<TrayMenuPresetAction> presetActions_;
};

}  // namespace quickdraw
