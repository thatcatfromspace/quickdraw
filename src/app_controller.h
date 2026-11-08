#pragma once

#include <windows.h>

#include "display/display_manager.h"
#include "display/mode_transaction.h"
#include "platform/startup_manager.h"
#include "platform/logger.h"
#include "storage/recovery_store.h"
#include "storage/settings_store.h"
#include "ui/tray_icon.h"
#include "ui/tray_menu.h"

namespace quickdraw {

class AppController final {
public:
    explicit AppController(HINSTANCE instance);
    ~AppController();

    AppController(const AppController&) = delete;
    AppController& operator=(const AppController&) = delete;

    bool Initialize();
    int Run();

private:
    static LRESULT CALLBACK WindowProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam);

    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    bool RegisterWindowClass();
    bool CreateControllerWindow();
    void ShowTrayMenu(POINT anchor);
    void HandleTrayCallback(WPARAM wParam, LPARAM lParam);
    void RestoreTrayIcon();
    void ScheduleDisplayRefresh();
    void ApplyDisplayMode(const TrayMenuModeAction& action);
    void ShowModeChangeError(const ModeChangeResult& result);
    void RecoverPendingTransaction();
    void ApplyPreset(const std::wstring& presetId);
    void ShowSettings(bool focusPresetName);
    bool EnsureNoPendingRecovery();

    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    UINT taskbarCreatedMessage_ = 0;
    TrayIcon trayIcon_;
    DisplayManager displayManager_;
    FileRecoveryStore recoveryStore_;
    SettingsStore settingsStore_;
    AppSettings settings_;
    StartupManager startupManager_;
    Logger logger_;
    bool menuOpen_ = false;
};

}  // namespace quickdraw
