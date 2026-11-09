#include "app_controller.h"

#include <commctrl.h>
#include <powrprof.h>
#include <windowsx.h>

#include <algorithm>
#include <vector>

#include "platform/single_instance.h"
#include "platform/brand_migration.h"
#include "display/monitor_matcher.h"
#include "ui/confirmation_dialog.h"
#include "ui/settings_dialog.h"
#include "ui/tray_menu.h"
#include "resource.h"

namespace quickdraw {
namespace {

constexpr wchar_t kWindowClassName[] = L"Quickdraw.ControllerWindow";
constexpr wchar_t kWindowTitle[] = L"Quickdraw";
constexpr UINT kTrayCallbackMessage = WM_APP + 1;
constexpr UINT_PTR kDisplayRefreshTimer = 1;
constexpr UINT kDisplayRefreshDelayMilliseconds = 500;

}  // namespace

AppController::AppController(HINSTANCE instance) : instance_(instance) {}

AppController::~AppController() {
    logger_.Write(L"Quickdraw stopping.");
    trayIcon_.Remove();
    if (window_ != nullptr && IsWindow(window_)) {
        DestroyWindow(window_);
    }
}

bool AppController::Initialize() {
    MigrateLegacyBrandData();
    startupManager_.MigrateLegacyEntry();
    logger_.Initialize();
    logger_.Write(L"Quickdraw starting.");
    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&controls);

    taskbarCreatedMessage_ = RegisterWindowMessageW(L"TaskbarCreated");
    if (taskbarCreatedMessage_ == 0 || SingleInstance::ActivationMessage() == 0) {
        return false;
    }

    if (!RegisterWindowClass() || !CreateControllerWindow()) {
        return false;
    }

    if (!settingsStore_.Load(settings_)) {
        logger_.Write(L"Settings load failed; defaults are active.");
        MessageBoxW(
            window_,
            L"Quickdraw could not read its settings file. Default settings "
            L"will be used until the file is repaired or replaced.",
            L"Quickdraw",
            MB_OK | MB_ICONWARNING);
    }
    settings_.startWithWindows = startupManager_.IsEnabled();
    if (settings_.startWithWindows) {
        startupManager_.SetEnabled(true);
    }

    displayManager_.Refresh();
    logger_.Write(
        L"Display inventory contains " +
        std::to_wstring(displayManager_.Displays().size()) + L" active target(s).");
    RecoverPendingTransaction();
    RestoreTrayIcon();
    return trayIcon_.IsAdded();
}

int AppController::Run() {
    MSG message{};
    while (true) {
        const BOOL result = GetMessageW(&message, nullptr, 0, 0);
        if (result == 0) {
            return static_cast<int>(message.wParam);
        }
        if (result == -1) {
            return 1;
        }

        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

LRESULT CALLBACK AppController::WindowProcedure(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam) {
    AppController* controller = nullptr;

    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        controller = static_cast<AppController*>(create->lpCreateParams);
        controller->window_ = window;
        SetWindowLongPtrW(
            window,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(controller));
    } else {
        controller = reinterpret_cast<AppController*>(
            GetWindowLongPtrW(window, GWLP_USERDATA));
    }

    if (controller != nullptr) {
        return controller->HandleMessage(message, wParam, lParam);
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT AppController::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == taskbarCreatedMessage_) {
        RestoreTrayIcon();
        return 0;
    }

    if (message == SingleInstance::ActivationMessage()) {
        ShowTrayMenu(trayIcon_.AnchorPoint());
        return 0;
    }

    switch (message) {
        case kTrayCallbackMessage:
            HandleTrayCallback(wParam, lParam);
            return 0;

        case WM_DISPLAYCHANGE:
        case WM_DEVICECHANGE:
            if (menuOpen_) {
                EndMenu();
            }
            logger_.Write(L"Display or device topology change received.");
            ScheduleDisplayRefresh();
            return 0;

        case WM_POWERBROADCAST:
            if (wParam == PBT_APMRESUMEAUTOMATIC ||
                wParam == PBT_APMRESUMESUSPEND ||
                wParam == PBT_APMRESUMECRITICAL) {
                logger_.Write(L"Power resume received; refreshing displays.");
                ScheduleDisplayRefresh();
            }
            return TRUE;

        case WM_TIMER:
            if (wParam == kDisplayRefreshTimer) {
                KillTimer(window_, kDisplayRefreshTimer);
                displayManager_.Refresh();
                logger_.Write(
                    L"Display inventory refreshed: " +
                    std::to_wstring(displayManager_.Displays().size()) +
                    L" active target(s).");
            }
            return 0;

        case WM_QUERYENDSESSION:
            return TRUE;

        case WM_ENDSESSION:
            if (wParam != FALSE) {
                trayIcon_.Remove();
            }
            return 0;

        case WM_DESTROY:
            trayIcon_.Remove();
            window_ = nullptr;
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(window_, message, wParam, lParam);
    }
}

bool AppController::RegisterWindowClass() {
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.hInstance = instance_;
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.lpszClassName = kWindowClassName;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = static_cast<HICON>(LoadImageW(
        instance_,
        MAKEINTRESOURCEW(IDI_APP_ICON),
        IMAGE_ICON,
        32,
        32,
        LR_SHARED));
    windowClass.hIconSm = static_cast<HICON>(LoadImageW(
        instance_,
        MAKEINTRESOURCEW(IDI_APP_ICON),
        IMAGE_ICON,
        16,
        16,
        LR_SHARED));
    return RegisterClassExW(&windowClass) != 0;
}

bool AppController::CreateControllerWindow() {
    window_ = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kWindowClassName,
        kWindowTitle,
        WS_OVERLAPPED,
        0,
        0,
        0,
        0,
        nullptr,
        nullptr,
        instance_,
        this);
    return window_ != nullptr;
}

void AppController::ShowTrayMenu(POINT anchor) {
    TrayMenu menu;
    if (!menu.Build(
            displayManager_.Displays(),
            displayManager_.LastError(),
            settings_.presets,
            GetSystemMetrics(SM_REMOTESESSION) == 0)) {
        return;
    }

    SetForegroundWindow(window_);
    menuOpen_ = true;
    const UINT command = TrackPopupMenuEx(
        menu.Handle(),
        TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN |
            TPM_RETURNCMD | TPM_NONOTIFY,
        anchor.x,
        anchor.y,
        window_,
        nullptr);
    menuOpen_ = false;
    PostMessageW(window_, WM_NULL, 0, 0);
    trayIcon_.ReturnFocus();

    if (command == TrayMenu::kExitCommand) {
        DestroyWindow(window_);
    } else if (command == TrayMenu::kSettingsCommand) {
        ShowSettings(false);
    } else if (command == TrayMenu::kSavePresetCommand) {
        ShowSettings(true);
    } else if (command == TrayMenu::kAboutCommand) {
        MessageBoxW(
            window_,
            L"Quickdraw 1.0.0\n\nDisplay modes in a click.\n\n"
            L"A lightweight resolution and refresh-rate switcher.",
            L"About Quickdraw",
            MB_OK | MB_ICONINFORMATION);
    } else if (const TrayMenuModeAction* modeAction = menu.FindModeAction(command)) {
        ApplyDisplayMode(*modeAction);
    } else if (const TrayMenuPresetAction* presetAction = menu.FindPresetAction(command)) {
        ApplyPreset(presetAction->presetId);
    }
}

void AppController::HandleTrayCallback(WPARAM wParam, LPARAM lParam) {
    const UINT event = LOWORD(lParam);
    switch (event) {
        case WM_CONTEXTMENU:
        case NIN_SELECT:
        case NIN_KEYSELECT: {
            POINT anchor{
                GET_X_LPARAM(wParam),
                GET_Y_LPARAM(wParam),
            };
            if (anchor.x == -1 && anchor.y == -1) {
                anchor = trayIcon_.AnchorPoint();
            }
            ShowTrayMenu(anchor);
            break;
        }
        default:
            break;
    }
}

void AppController::RestoreTrayIcon() {
    trayIcon_.Add(window_, kTrayCallbackMessage);
}

void AppController::ScheduleDisplayRefresh() {
    SetTimer(
        window_,
        kDisplayRefreshTimer,
        kDisplayRefreshDelayMilliseconds,
        nullptr);
}

void AppController::ApplyDisplayMode(const TrayMenuModeAction& action) {
    if (!EnsureNoPendingRecovery()) {
        return;
    }

    ModeTransaction transaction(displayManager_.Backend(), &recoveryStore_);
    logger_.Write(
        L"Requested " + std::to_wstring(action.mode.width) + L"x" +
        std::to_wstring(action.mode.height) + L" @ " +
        std::to_wstring(action.mode.frequencyHz) + L" Hz for " +
        action.monitorName + L".");
    ModeChangeRequest request;
    request.monitorId = action.monitorId;
    request.gdiDeviceName = action.gdiDeviceName;
    request.requestedMode = action.mode;
    const ModeChangeResult beginResult = transaction.Begin({request});
    if (!beginResult.success) {
        logger_.Write(
            L"Display change failed with code " +
            std::to_wstring(beginResult.code) + L".");
        ShowModeChangeError(beginResult);
        displayManager_.Refresh();
        return;
    }

    if (ConfirmDisplayMode(
            window_,
            action.monitorName,
            action.mode,
            settings_.confirmationSeconds)) {
        if (!transaction.Keep()) {
            MessageBoxW(
                window_,
                L"The display settings were kept, but Quickdraw could not "
                L"remove its recovery journal. Do not restart Quickdraw "
                L"until the pending-transaction file has been removed.",
                L"Quickdraw recovery",
                MB_OK | MB_ICONWARNING);
        }
        logger_.Write(L"Display change confirmed.");
    } else {
        const ModeChangeResult revertResult = transaction.Revert();
        if (!revertResult.success) {
            ShowModeChangeError(revertResult);
        }
        logger_.Write(L"Display change reverted.");
    }
    displayManager_.Refresh();
}

void AppController::ShowModeChangeError(const ModeChangeResult& result) {
    const std::wstring message =
        L"Quickdraw encountered an error while " +
        std::wstring(ModeChangeStageName(result.stage)) +
        L".\n\n" + DisplayChangeCodeDescription(result.code) +
        L"\n\nError code: " + std::to_wstring(result.code);
    MessageBoxW(
        window_,
        message.c_str(),
        L"Quickdraw",
        MB_OK | MB_ICONERROR);
}

void AppController::RecoverPendingTransaction() {
    if (!recoveryStore_.HasPending()) {
        return;
    }
    logger_.Write(L"Pending display recovery journal found.");

    std::vector<RecoveryTarget> targets;
    if (!recoveryStore_.Load(targets)) {
        logger_.Write(L"Recovery journal could not be read.");
        MessageBoxW(
            window_,
            L"Quickdraw found an unreadable display recovery journal. "
            L"It has been left in place for inspection.",
            L"Quickdraw recovery",
            MB_OK | MB_ICONWARNING);
        return;
    }

    const auto& displays = displayManager_.Displays();
    for (RecoveryTarget& target : targets) {
        const auto match = MatchMonitor(target.monitorId, displays);
        if (!match.has_value()) {
            logger_.Write(L"Recovery monitor matching failed.");
            MessageBoxW(
                window_,
                L"Quickdraw could not safely match every monitor in the "
                L"pending recovery journal. Reconnect the missing monitor and "
                L"restart Quickdraw to retry recovery.",
                L"Quickdraw recovery",
                MB_OK | MB_ICONWARNING);
            return;
        }
        target.gdiDeviceName = displays[*match].runtimeId.gdiDeviceName;
    }

    ModeTransaction transaction(displayManager_.Backend());
    const ModeChangeResult result = transaction.RestorePending(targets);
    if (!result.success) {
        logger_.Write(
            L"Pending display recovery failed with code " +
            std::to_wstring(result.code) + L".");
        ShowModeChangeError(result);
        return;
    }

    recoveryStore_.Clear();
    logger_.Write(L"Pending display recovery completed.");
    displayManager_.Refresh();
}

void AppController::ApplyPreset(const std::wstring& presetId) {
    if (!EnsureNoPendingRecovery()) {
        return;
    }

    const auto preset = std::find_if(
        settings_.presets.begin(),
        settings_.presets.end(),
        [&presetId](const DisplayPreset& candidate) {
            return candidate.id == presetId;
        });
    if (preset == settings_.presets.end()) {
        return;
    }
    logger_.Write(L"Applying preset: " + preset->name + L".");

    if (!displayManager_.Refresh()) {
        MessageBoxW(
            window_,
            L"The active displays could not be read.",
            L"Quickdraw",
            MB_OK | MB_ICONERROR);
        return;
    }

    const auto& displays = displayManager_.Displays();
    std::vector<std::size_t> matchedDisplayIndexes;
    std::vector<ModeChangeRequest> requests;
    std::vector<DisplayConfirmationItem> confirmationItems;
    for (const PresetTarget& target : preset->targets) {
        const auto match = MatchMonitor(target.monitorId, displays);
        if (!match.has_value() ||
            std::find(
                matchedDisplayIndexes.begin(),
                matchedDisplayIndexes.end(),
                *match) != matchedDisplayIndexes.end() ||
            !displays[*match].independentlyConfigurable) {
            MessageBoxW(
                window_,
                L"This preset cannot be matched safely to the currently "
                L"connected monitors.",
                L"Quickdraw preset",
                MB_OK | MB_ICONWARNING);
            return;
        }

        const DisplayMode* mode = FindPresetMode(target.mode, displays[*match]);
        if (mode == nullptr) {
            MessageBoxW(
                window_,
                L"One or more modes in this preset are not currently available.",
                L"Quickdraw preset",
                MB_OK | MB_ICONWARNING);
            return;
        }

        matchedDisplayIndexes.push_back(*match);
        requests.push_back(ModeChangeRequest{
            displays[*match].persistentId,
            displays[*match].runtimeId.gdiDeviceName,
            *mode,
        });
        confirmationItems.push_back(DisplayConfirmationItem{
            displays[*match].persistentId.friendlyName,
            *mode,
        });
    }

    ModeTransaction transaction(displayManager_.Backend(), &recoveryStore_);
    const ModeChangeResult beginResult = transaction.Begin(requests);
    if (!beginResult.success) {
        ShowModeChangeError(beginResult);
        displayManager_.Refresh();
        return;
    }

    if (ConfirmDisplayModes(
            window_,
            confirmationItems,
            settings_.confirmationSeconds)) {
        if (!transaction.Keep()) {
            MessageBoxW(
                window_,
                L"The preset was kept, but the recovery journal could not be removed.",
                L"Quickdraw recovery",
                MB_OK | MB_ICONWARNING);
        }
    } else {
        const ModeChangeResult revertResult = transaction.Revert();
        if (!revertResult.success) {
            ShowModeChangeError(revertResult);
        }
    }
    displayManager_.Refresh();
}

void AppController::ShowSettings(bool focusPresetName) {
    ShowSettingsDialog(
        instance_,
        window_,
        settings_,
        settingsStore_,
        displayManager_,
        startupManager_,
        focusPresetName);
}

bool AppController::EnsureNoPendingRecovery() {
    if (!recoveryStore_.HasPending()) {
        return true;
    }
    MessageBoxW(
        window_,
        L"A previous display recovery is still pending. Restart Quickdraw "
        L"after reconnecting the affected monitor before making another "
        L"display change.",
        L"Quickdraw recovery",
        MB_OK | MB_ICONWARNING);
    return false;
}

}  // namespace quickdraw
