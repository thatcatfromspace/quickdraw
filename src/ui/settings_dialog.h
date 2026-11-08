#pragma once

#include <windows.h>

#include "display/display_manager.h"
#include "platform/startup_manager.h"
#include "storage/settings_store.h"

namespace quickdraw {

bool ShowSettingsDialog(
    HINSTANCE instance,
    HWND owner,
    AppSettings& settings,
    const SettingsStore& store,
    DisplayManager& displayManager,
    const StartupManager& startupManager,
    bool focusPresetName = false);

}  // namespace quickdraw
