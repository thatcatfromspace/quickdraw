#include "display/monitor_matcher.h"

#include <functional>

namespace quickdraw {
namespace {

template <typename Predicate>
std::optional<std::size_t> UniqueMatch(
    const std::vector<MonitorInfo>& displays,
    Predicate predicate) {
    std::optional<std::size_t> match;
    for (std::size_t index = 0; index < displays.size(); ++index) {
        if (!predicate(displays[index])) {
            continue;
        }
        if (match.has_value()) {
            return std::nullopt;
        }
        match = index;
    }
    return match;
}

bool HasEdidIdentity(const PersistentMonitorId& identity) {
    return identity.edidManufacturer != 0 || identity.edidProduct != 0;
}

}  // namespace

std::optional<std::size_t> MatchMonitor(
    const PersistentMonitorId& requested,
    const std::vector<MonitorInfo>& displays) {
    if (!requested.monitorDevicePath.empty()) {
        const auto exactPath = UniqueMatch(
            displays,
            [&requested](const MonitorInfo& display) {
                return display.persistentId.monitorDevicePath ==
                       requested.monitorDevicePath;
            });
        if (exactPath.has_value()) {
            return exactPath;
        }
    }

    if (HasEdidIdentity(requested)) {
        const auto edidAndConnector = UniqueMatch(
            displays,
            [&requested](const MonitorInfo& display) {
                return display.persistentId.edidManufacturer ==
                           requested.edidManufacturer &&
                       display.persistentId.edidProduct == requested.edidProduct &&
                       display.persistentId.connectorInstance ==
                           requested.connectorInstance;
            });
        if (edidAndConnector.has_value()) {
            return edidAndConnector;
        }

        const auto uniqueEdid = UniqueMatch(
            displays,
            [&requested](const MonitorInfo& display) {
                return display.persistentId.edidManufacturer ==
                           requested.edidManufacturer &&
                       display.persistentId.edidProduct == requested.edidProduct;
            });
        if (uniqueEdid.has_value()) {
            return uniqueEdid;
        }
    }

    if (!requested.friendlyName.empty()) {
        return UniqueMatch(
            displays,
            [&requested](const MonitorInfo& display) {
                return display.persistentId.friendlyName == requested.friendlyName;
            });
    }

    return std::nullopt;
}

}  // namespace quickdraw
