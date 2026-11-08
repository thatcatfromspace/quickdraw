#pragma once

#include <windows.h>

#include <vector>

#include "display/display_types.h"

namespace quickdraw {

std::vector<DisplayMode> BuildModeCatalog(
    const std::vector<DEVMODEW>& enumeratedModes,
    const DEVMODEW& currentMode);

}  // namespace quickdraw
