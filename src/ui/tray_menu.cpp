#include "ui/tray_menu.h"

#include <map>
#include <string>

namespace quickdraw {
namespace {

std::wstring ResolutionLabel(std::uint32_t width, std::uint32_t height) {
    return std::to_wstring(width) + L" \u00d7 " + std::to_wstring(height);
}

std::wstring RefreshLabel(const DisplayMode& mode) {
    std::wstring label = std::to_wstring(mode.frequencyHz) + L" Hz";
    if (mode.interlaced) {
        label += L" (interlaced)";
    }
    return label;
}

void SetRadioStyle(HMENU menu, UINT itemId) {
    MENUITEMINFOW item{};
    item.cbSize = sizeof(item);
    item.fMask = MIIM_FTYPE;
    item.fType = MFT_STRING | MFT_RADIOCHECK;
    SetMenuItemInfoW(menu, itemId, FALSE, &item);
}

}  // namespace

TrayMenu::~TrayMenu() {
    Reset();
}

bool TrayMenu::Build(
    const std::vector<MonitorInfo>& displays,
    LONG inventoryError,
    const std::vector<DisplayPreset>& presets,
    bool changesAllowed) {
    Reset();
    root_ = CreatePopupMenu();
    if (root_ == nullptr) {
        return false;
    }

    AppendMenuW(root_, MF_STRING | MF_DISABLED, 0, L"Quickdraw");
    AppendMenuW(root_, MF_SEPARATOR, 0, nullptr);
    if (!changesAllowed) {
        AppendMenuW(
            root_,
            MF_STRING | MF_DISABLED,
            0,
            L"Display changes are unavailable in Remote Desktop");
        AppendMenuW(root_, MF_SEPARATOR, 0, nullptr);
    }

    std::map<std::wstring, std::size_t> nameTotals;
    for (const MonitorInfo& display : displays) {
        ++nameTotals[display.persistentId.friendlyName];
    }
    std::map<std::wstring, std::size_t> nameIndexes;

    UINT nextModeCommand = 2000;
    for (const MonitorInfo& display : displays) {
        HMENU monitorMenu = CreatePopupMenu();
        if (monitorMenu == nullptr) {
            continue;
        }

        std::size_t modeIndex = 0;
        while (modeIndex < display.modes.size()) {
            const std::uint32_t width = display.modes[modeIndex].width;
            const std::uint32_t height = display.modes[modeIndex].height;
            HMENU resolutionMenu = CreatePopupMenu();
            if (resolutionMenu == nullptr) {
                break;
            }

            while (modeIndex < display.modes.size() &&
                   display.modes[modeIndex].width == width &&
                   display.modes[modeIndex].height == height) {
                const DisplayMode& mode = display.modes[modeIndex];
                const UINT command = nextModeCommand++;
                UINT flags = MF_STRING;
                if (mode.current) {
                    flags |= MF_CHECKED | MF_GRAYED;
                } else if (!display.independentlyConfigurable || !changesAllowed) {
                    flags |= MF_GRAYED;
                }
                const std::wstring label = RefreshLabel(mode);
                AppendMenuW(resolutionMenu, flags, command, label.c_str());
                SetRadioStyle(resolutionMenu, command);
                if (!mode.current && display.independentlyConfigurable && changesAllowed) {
                    modeActions_.push_back(TrayMenuModeAction{
                        command,
                        display.runtimeId.gdiDeviceName,
                        display.persistentId.friendlyName,
                        display.persistentId,
                        mode,
                    });
                }
                ++modeIndex;
            }

            const std::wstring label = ResolutionLabel(width, height);
            AppendMenuW(
                monitorMenu,
                MF_POPUP,
                reinterpret_cast<UINT_PTR>(resolutionMenu),
                label.c_str());
        }

        if (display.modes.empty()) {
            AppendMenuW(monitorMenu, MF_STRING | MF_DISABLED, 0, L"No modes available");
        }

        std::wstring monitorLabel = display.persistentId.friendlyName;
        if (nameTotals[display.persistentId.friendlyName] > 1) {
            const std::size_t index = ++nameIndexes[display.persistentId.friendlyName];
            monitorLabel += L" (" + std::to_wstring(index) + L")";
        }
        if (display.primary) {
            monitorLabel += L" \u2014 Primary";
        }
        if (!display.independentlyConfigurable) {
            monitorLabel += L" \u2014 Mirrored";
        }

        AppendMenuW(
            root_,
            MF_POPUP,
            reinterpret_cast<UINT_PTR>(monitorMenu),
            monitorLabel.c_str());
    }

    if (displays.empty()) {
        const wchar_t* message = inventoryError == ERROR_SUCCESS
            ? L"No active displays"
            : L"Displays unavailable";
        AppendMenuW(root_, MF_STRING | MF_DISABLED, 0, message);
    }

    AppendMenuW(root_, MF_SEPARATOR, 0, nullptr);

    HMENU presetsMenu = CreatePopupMenu();
    if (presetsMenu != nullptr) {
        UINT nextPresetCommand = 40000;
        for (const DisplayPreset& preset : presets) {
            const UINT command = nextPresetCommand++;
            AppendMenuW(
                presetsMenu,
                changesAllowed ? MF_STRING : MF_STRING | MF_GRAYED,
                command,
                preset.name.c_str());
            if (changesAllowed) {
                presetActions_.push_back(TrayMenuPresetAction{command, preset.id});
            }
        }
        if (presets.empty()) {
            AppendMenuW(presetsMenu, MF_STRING | MF_DISABLED, 0, L"No saved presets");
        }
        AppendMenuW(presetsMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(
            presetsMenu,
            changesAllowed ? MF_STRING : MF_STRING | MF_GRAYED,
            kSavePresetCommand,
            L"Save current configuration\u2026");
        AppendMenuW(presetsMenu, MF_STRING, kSettingsCommand, L"Manage presets\u2026");
        AppendMenuW(
            root_,
            MF_POPUP,
            reinterpret_cast<UINT_PTR>(presetsMenu),
            L"Presets");
    }

    AppendMenuW(root_, MF_STRING, kSettingsCommand, L"Settings\u2026");
    AppendMenuW(root_, MF_STRING, kAboutCommand, L"About\u2026");
    AppendMenuW(root_, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(root_, MF_STRING, kExitCommand, L"Exit");
    return true;
}

HMENU TrayMenu::Handle() const noexcept {
    return root_;
}

const TrayMenuModeAction* TrayMenu::FindModeAction(UINT command) const noexcept {
    for (const TrayMenuModeAction& action : modeActions_) {
        if (action.command == command) {
            return &action;
        }
    }
    return nullptr;
}

const TrayMenuPresetAction* TrayMenu::FindPresetAction(UINT command) const noexcept {
    for (const TrayMenuPresetAction& action : presetActions_) {
        if (action.command == command) {
            return &action;
        }
    }
    return nullptr;
}

void TrayMenu::Reset() noexcept {
    if (root_ != nullptr) {
        DestroyMenu(root_);
        root_ = nullptr;
    }
    modeActions_.clear();
    presetActions_.clear();
}

}  // namespace quickdraw
