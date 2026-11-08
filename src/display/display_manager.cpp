#include "display/display_manager.h"

namespace quickdraw {

bool DisplayManager::Refresh() {
    std::vector<MonitorInfo> refreshedDisplays;
    lastError_ = backend_.EnumerateDisplays(refreshedDisplays);
    if (lastError_ != ERROR_SUCCESS) {
        displays_.clear();
        return false;
    }

    displays_ = std::move(refreshedDisplays);
    return true;
}

const std::vector<MonitorInfo>& DisplayManager::Displays() const noexcept {
    return displays_;
}

LONG DisplayManager::LastError() const noexcept {
    return lastError_;
}

IDisplayBackend& DisplayManager::Backend() noexcept {
    return backend_;
}

}  // namespace quickdraw
