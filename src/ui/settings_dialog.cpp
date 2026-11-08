#include "ui/settings_dialog.h"

#include <commctrl.h>

#include <algorithm>
#include <array>
#include <cwchar>
#include <string>

#include "resource.h"

namespace quickdraw {
namespace {

struct SettingsDialogContext {
    AppSettings* settings = nullptr;
    const SettingsStore* store = nullptr;
    DisplayManager* displayManager = nullptr;
    const StartupManager* startupManager = nullptr;
    bool focusPresetName = false;
    bool changed = false;
};

SettingsDialogContext* Context(HWND dialog) {
    return reinterpret_cast<SettingsDialogContext*>(
        GetWindowLongPtrW(dialog, DWLP_USER));
}

std::wstring ControlText(HWND dialog, int controlId) {
    const HWND control = GetDlgItem(dialog, controlId);
    const int length = GetWindowTextLengthW(control);
    std::wstring value(static_cast<std::size_t>(length) + 1, L'\0');
    if (length > 0) {
        GetWindowTextW(control, value.data(), length + 1);
    }
    value.resize(static_cast<std::size_t>(length));
    return value;
}

void ShowValidationError(HWND dialog, const wchar_t* message) {
    MessageBoxW(dialog, message, L"Quickdraw Settings", MB_OK | MB_ICONWARNING);
}

bool NameExists(
    const AppSettings& settings,
    const std::wstring& name,
    const DisplayPreset* ignored = nullptr) {
    return std::any_of(
        settings.presets.begin(),
        settings.presets.end(),
        [&name, ignored](const DisplayPreset& preset) {
            return &preset != ignored && _wcsicmp(preset.name.c_str(), name.c_str()) == 0;
        });
}

void SortPresets(AppSettings& settings) {
    std::sort(
        settings.presets.begin(),
        settings.presets.end(),
        [](const DisplayPreset& left, const DisplayPreset& right) {
            return _wcsicmp(left.name.c_str(), right.name.c_str()) < 0;
        });
}

void RefreshPresetList(HWND dialog) {
    SettingsDialogContext& context = *Context(dialog);
    const HWND list = GetDlgItem(dialog, IDC_PRESET_LIST);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    for (const DisplayPreset& preset : context.settings->presets) {
        SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(preset.name.c_str()));
    }
}

int SelectedPresetIndex(HWND dialog) {
    return static_cast<int>(SendDlgItemMessageW(
        dialog,
        IDC_PRESET_LIST,
        LB_GETCURSEL,
        0,
        0));
}

DisplayPreset* SelectedPreset(HWND dialog) {
    SettingsDialogContext& context = *Context(dialog);
    const int index = SelectedPresetIndex(dialog);
    if (index == LB_ERR || index < 0 ||
        static_cast<std::size_t>(index) >= context.settings->presets.size()) {
        return nullptr;
    }
    return &context.settings->presets[static_cast<std::size_t>(index)];
}

bool CaptureCurrentPreset(
    HWND dialog,
    const std::wstring& name,
    DisplayPreset& preset) {
    SettingsDialogContext& context = *Context(dialog);
    if (!context.displayManager->Refresh()) {
        ShowValidationError(dialog, L"The active displays could not be read.");
        return false;
    }
    preset = CapturePreset(name, context.displayManager->Displays());
    if (preset.targets.empty()) {
        ShowValidationError(
            dialog,
            L"The current display configuration cannot be saved. Mirrored "
            L"displays are not supported in version 1.");
        return false;
    }
    return true;
}

void SavePreset(HWND dialog) {
    SettingsDialogContext& context = *Context(dialog);
    const std::wstring name = ControlText(dialog, IDC_PRESET_NAME);
    if (name.empty()) {
        ShowValidationError(dialog, L"Enter a preset name first.");
        return;
    }
    if (NameExists(*context.settings, name)) {
        ShowValidationError(dialog, L"A preset with that name already exists.");
        return;
    }

    DisplayPreset preset;
    if (!CaptureCurrentPreset(dialog, name, preset)) {
        return;
    }
    context.settings->presets.push_back(std::move(preset));
    SortPresets(*context.settings);
    context.changed = true;
    RefreshPresetList(dialog);
    SetDlgItemTextW(dialog, IDC_PRESET_NAME, L"");
}

void RenamePreset(HWND dialog) {
    SettingsDialogContext& context = *Context(dialog);
    DisplayPreset* preset = SelectedPreset(dialog);
    if (preset == nullptr) {
        ShowValidationError(dialog, L"Select a preset to rename.");
        return;
    }
    const std::wstring name = ControlText(dialog, IDC_PRESET_NAME);
    if (name.empty() || NameExists(*context.settings, name, preset)) {
        ShowValidationError(dialog, L"Enter a unique preset name.");
        return;
    }
    preset->name = name;
    SortPresets(*context.settings);
    context.changed = true;
    RefreshPresetList(dialog);
}

void UpdatePreset(HWND dialog) {
    SettingsDialogContext& context = *Context(dialog);
    DisplayPreset* preset = SelectedPreset(dialog);
    if (preset == nullptr) {
        ShowValidationError(dialog, L"Select a preset to update.");
        return;
    }
    const std::wstring id = preset->id;
    const std::wstring name = preset->name;
    DisplayPreset replacement;
    if (!CaptureCurrentPreset(dialog, name, replacement)) {
        return;
    }
    replacement.id = id;
    *preset = std::move(replacement);
    context.changed = true;
}

void DeletePreset(HWND dialog) {
    SettingsDialogContext& context = *Context(dialog);
    const int index = SelectedPresetIndex(dialog);
    if (index == LB_ERR || index < 0 ||
        static_cast<std::size_t>(index) >= context.settings->presets.size()) {
        ShowValidationError(dialog, L"Select a preset to delete.");
        return;
    }
    if (MessageBoxW(
            dialog,
            L"Delete the selected preset?",
            L"Quickdraw Settings",
            MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) {
        return;
    }
    context.settings->presets.erase(
        context.settings->presets.begin() +
        static_cast<std::vector<DisplayPreset>::difference_type>(index));
    context.changed = true;
    RefreshPresetList(dialog);
    SetDlgItemTextW(dialog, IDC_PRESET_NAME, L"");
}

void LoadControls(HWND dialog) {
    SettingsDialogContext& context = *Context(dialog);
    CheckDlgButton(
        dialog,
        IDC_START_WITH_WINDOWS,
        context.startupManager->IsEnabled() ? BST_CHECKED : BST_UNCHECKED);

    constexpr std::array<unsigned, 4> timeouts{5, 10, 15, 30};
    for (unsigned seconds : timeouts) {
        const std::wstring label = std::to_wstring(seconds) + L" seconds";
        const LRESULT index = SendDlgItemMessageW(
            dialog,
            IDC_CONFIRM_TIMEOUT,
            CB_ADDSTRING,
            0,
            reinterpret_cast<LPARAM>(label.c_str()));
        SendDlgItemMessageW(
            dialog,
            IDC_CONFIRM_TIMEOUT,
            CB_SETITEMDATA,
            static_cast<WPARAM>(index),
            static_cast<LPARAM>(seconds));
        if (seconds == context.settings->confirmationSeconds) {
            SendDlgItemMessageW(
                dialog,
                IDC_CONFIRM_TIMEOUT,
                CB_SETCURSEL,
                static_cast<WPARAM>(index),
                0);
        }
    }
    RefreshPresetList(dialog);
    if (context.focusPresetName) {
        SetFocus(GetDlgItem(dialog, IDC_PRESET_NAME));
    }
}

bool SaveControls(HWND dialog) {
    SettingsDialogContext& context = *Context(dialog);
    const bool startWithWindows =
        IsDlgButtonChecked(dialog, IDC_START_WITH_WINDOWS) == BST_CHECKED;
    const LRESULT selectedTimeout = SendDlgItemMessageW(
        dialog,
        IDC_CONFIRM_TIMEOUT,
        CB_GETCURSEL,
        0,
        0);
    if (selectedTimeout == CB_ERR) {
        return false;
    }
    const auto timeout = static_cast<unsigned>(SendDlgItemMessageW(
        dialog,
        IDC_CONFIRM_TIMEOUT,
        CB_GETITEMDATA,
        selectedTimeout,
        0));

    if (!context.startupManager->SetEnabled(startWithWindows)) {
        ShowValidationError(dialog, L"The Windows startup setting could not be updated.");
        return false;
    }
    context.settings->startWithWindows = startWithWindows;
    context.settings->confirmationSeconds = timeout;
    if (!context.store->Save(*context.settings)) {
        ShowValidationError(dialog, L"Quickdraw settings could not be saved.");
        return false;
    }
    return true;
}

INT_PTR CALLBACK DialogProcedure(
    HWND dialog,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {
    if (message == WM_INITDIALOG) {
        SetWindowLongPtrW(dialog, DWLP_USER, lParam);
        LoadControls(dialog);
        return Context(dialog)->focusPresetName ? FALSE : TRUE;
    }

    SettingsDialogContext* context = Context(dialog);
    if (context == nullptr) {
        return FALSE;
    }

    if (message == WM_COMMAND) {
        switch (LOWORD(wParam)) {
            case IDC_PRESET_LIST:
                if (HIWORD(wParam) == LBN_SELCHANGE) {
                    if (DisplayPreset* preset = SelectedPreset(dialog)) {
                        SetDlgItemTextW(dialog, IDC_PRESET_NAME, preset->name.c_str());
                    }
                }
                return TRUE;
            case IDC_PRESET_SAVE:
                SavePreset(dialog);
                return TRUE;
            case IDC_PRESET_RENAME:
                RenamePreset(dialog);
                return TRUE;
            case IDC_PRESET_UPDATE:
                UpdatePreset(dialog);
                return TRUE;
            case IDC_PRESET_DELETE:
                DeletePreset(dialog);
                return TRUE;
            case IDOK:
                if (SaveControls(dialog)) {
                    EndDialog(dialog, IDOK);
                }
                return TRUE;
            case IDCANCEL:
                if (context->changed) {
                    AppSettings reloaded;
                    context->store->Load(reloaded);
                    *context->settings = std::move(reloaded);
                }
                EndDialog(dialog, IDCANCEL);
                return TRUE;
            default:
                break;
        }
    }
    return FALSE;
}

}  // namespace

bool ShowSettingsDialog(
    HINSTANCE instance,
    HWND owner,
    AppSettings& settings,
    const SettingsStore& store,
    DisplayManager& displayManager,
    const StartupManager& startupManager,
    bool focusPresetName) {
    SettingsDialogContext context{
        &settings,
        &store,
        &displayManager,
        &startupManager,
        focusPresetName,
        false,
    };
    return DialogBoxParamW(
               instance,
               MAKEINTRESOURCEW(IDD_SETTINGS),
               owner,
               DialogProcedure,
               reinterpret_cast<LPARAM>(&context)) == IDOK;
}

}  // namespace quickdraw
