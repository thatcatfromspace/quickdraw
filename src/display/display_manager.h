#pragma once

#include <windows.h>

#include <vector>

#include "display/display_backend.h"
#include "display/display_types.h"

namespace quickdraw {

class DisplayManager final {
public:
    bool Refresh();

    [[nodiscard]] const std::vector<MonitorInfo>& Displays() const noexcept;
    [[nodiscard]] LONG LastError() const noexcept;
    [[nodiscard]] IDisplayBackend& Backend() noexcept;

private:
    Win32DisplayBackend backend_;
    std::vector<MonitorInfo> displays_;
    LONG lastError_ = ERROR_SUCCESS;
};

}  // namespace quickdraw
