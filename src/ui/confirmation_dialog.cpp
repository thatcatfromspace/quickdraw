#include "ui/confirmation_dialog.h"

#include <commctrl.h>

#include <string>

#include "resource.h"

namespace quickdraw {
namespace {

constexpr int kKeepButton = 100;
constexpr int kRevertButton = 101;

struct ConfirmationState {
    unsigned timeoutSeconds = 0;
    int lastDisplayedSecond = -1;
    std::wstring modeDescription;
    std::wstring content;
};

void UpdateContent(HWND dialog, ConfirmationState& state, unsigned remaining) {
    state.content = state.modeDescription + L"\n\nReverting automatically in " +
                    std::to_wstring(remaining) + L" second";
    if (remaining != 1) {
        state.content += L"s";
    }
    state.content += L".";
    SendMessageW(
        dialog,
        TDM_SET_ELEMENT_TEXT,
        TDE_CONTENT,
        reinterpret_cast<LPARAM>(state.content.c_str()));
}

HRESULT CALLBACK ConfirmationCallback(
    HWND dialog,
    UINT notification,
    WPARAM wParam,
    LPARAM,
    LONG_PTR referenceData) {
    auto& state = *reinterpret_cast<ConfirmationState*>(referenceData);
    if (notification == TDN_CREATED) {
        SetForegroundWindow(dialog);
        UpdateContent(dialog, state, state.timeoutSeconds);
    } else if (notification == TDN_TIMER) {
        const unsigned elapsedSeconds = static_cast<unsigned>(wParam / 1000);
        const unsigned remaining = elapsedSeconds >= state.timeoutSeconds
            ? 0
            : state.timeoutSeconds - elapsedSeconds;
        if (static_cast<int>(remaining) != state.lastDisplayedSecond) {
            state.lastDisplayedSecond = static_cast<int>(remaining);
            UpdateContent(dialog, state, remaining);
        }
        if (remaining == 0) {
            SendMessageW(dialog, TDM_CLICK_BUTTON, kRevertButton, 0);
        }
    }
    return S_OK;
}

}  // namespace

bool ConfirmDisplayMode(
    HWND owner,
    const std::wstring& monitorName,
    const DisplayMode& mode,
    unsigned timeoutSeconds) {
    return ConfirmDisplayModes(
        owner,
        std::vector<DisplayConfirmationItem>{{monitorName, mode}},
        timeoutSeconds);
}

bool ConfirmDisplayModes(
    HWND owner,
    const std::vector<DisplayConfirmationItem>& items,
    unsigned timeoutSeconds) {
    if (items.empty()) {
        return false;
    }

    ConfirmationState state;
    state.timeoutSeconds = timeoutSeconds;
    for (std::size_t index = 0; index < items.size(); ++index) {
        const DisplayConfirmationItem& item = items[index];
        if (index != 0) {
            state.modeDescription += L"\n";
        }
        state.modeDescription += item.monitorName + L": " +
                                 std::to_wstring(item.mode.width) + L" \u00d7 " +
                                 std::to_wstring(item.mode.height) + L" @ " +
                                 std::to_wstring(item.mode.frequencyHz) + L" Hz";
    }

    const TASKDIALOG_BUTTON buttons[] = {
        {kKeepButton, L"Keep changes"},
        {kRevertButton, L"Revert"},
    };

    TASKDIALOGCONFIG configuration{};
    configuration.cbSize = sizeof(configuration);
    configuration.hwndParent = owner;
    configuration.dwFlags =
        TDF_ALLOW_DIALOG_CANCELLATION | TDF_CALLBACK_TIMER | TDF_SIZE_TO_CONTENT;
    configuration.pszWindowTitle = L"Quickdraw";
    const HICON mainIcon = static_cast<HICON>(LoadImageW(
        GetModuleHandleW(nullptr),
        MAKEINTRESOURCEW(IDI_APP_ICON),
        IMAGE_ICON,
        32,
        32,
        LR_SHARED));
    if (mainIcon != nullptr) {
        configuration.dwFlags |= TDF_USE_HICON_MAIN;
        configuration.hMainIcon = mainIcon;
    }
    configuration.pszMainInstruction = L"Keep these display settings?";
    configuration.pszContent = L"The previous settings will be restored automatically.";
    configuration.cButtons = ARRAYSIZE(buttons);
    configuration.pButtons = buttons;
    configuration.nDefaultButton = kRevertButton;
    configuration.pfCallback = ConfirmationCallback;
    configuration.lpCallbackData = reinterpret_cast<LONG_PTR>(&state);

    int selectedButton = kRevertButton;
    const HRESULT result = TaskDialogIndirect(
        &configuration,
        &selectedButton,
        nullptr,
        nullptr);
    return SUCCEEDED(result) && selectedButton == kKeepButton;
}

}  // namespace quickdraw
