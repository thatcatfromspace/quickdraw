#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "display/display_types.h"

namespace quickdraw {

std::optional<std::size_t> MatchMonitor(
    const PersistentMonitorId& requested,
    const std::vector<MonitorInfo>& displays);

}  // namespace quickdraw
