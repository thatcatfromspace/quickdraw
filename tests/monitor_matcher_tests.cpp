#include <windows.h>

#include <cstdlib>
#include <vector>

#include "display/monitor_matcher.h"

namespace {

quickdraw::MonitorInfo MakeMonitor(
    const wchar_t* path,
    const wchar_t* name,
    std::uint16_t manufacturer,
    std::uint16_t product,
    std::uint32_t connector) {
    quickdraw::MonitorInfo monitor;
    monitor.persistentId.monitorDevicePath = path;
    monitor.persistentId.friendlyName = name;
    monitor.persistentId.edidManufacturer = manufacturer;
    monitor.persistentId.edidProduct = product;
    monitor.persistentId.connectorInstance = connector;
    return monitor;
}

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

}  // namespace

int main() {
    const std::vector<quickdraw::MonitorInfo> displays{
        MakeMonitor(L"path-a", L"Same Model", 1, 10, 1),
        MakeMonitor(L"path-b", L"Same Model", 1, 10, 2),
        MakeMonitor(L"path-c", L"Unique Model", 2, 20, 1),
    };

    quickdraw::PersistentMonitorId exact;
    exact.monitorDevicePath = L"path-b";
    Require(quickdraw::MatchMonitor(exact, displays) == 1);

    quickdraw::PersistentMonitorId connector;
    connector.edidManufacturer = 1;
    connector.edidProduct = 10;
    connector.connectorInstance = 1;
    Require(quickdraw::MatchMonitor(connector, displays) == 0);

    quickdraw::PersistentMonitorId ambiguous;
    ambiguous.friendlyName = L"Same Model";
    Require(!quickdraw::MatchMonitor(ambiguous, displays).has_value());

    quickdraw::PersistentMonitorId unique;
    unique.friendlyName = L"Unique Model";
    Require(quickdraw::MatchMonitor(unique, displays) == 2);

    return EXIT_SUCCESS;
}
