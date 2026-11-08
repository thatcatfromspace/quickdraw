#pragma once

#include <windows.h>

#include <string>
#include <vector>

#include "display/display_types.h"

namespace quickdraw {

struct DisplayConfirmationItem {
    std::wstring monitorName;
    DisplayMode mode;
};

bool ConfirmDisplayMode(
    HWND owner,
    const std::wstring& monitorName,
    const DisplayMode& mode,
    unsigned timeoutSeconds);

bool ConfirmDisplayModes(
    HWND owner,
    const std::vector<DisplayConfirmationItem>& items,
    unsigned timeoutSeconds);

}  // namespace quickdraw
