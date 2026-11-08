#include "platform/brand_migration.h"

#include <windows.h>
#include <shlobj.h>

#include <string>

namespace quickdraw {
namespace {

std::wstring LocalAppDataDirectory() {
    PWSTR knownFolderPath = nullptr;
    if (FAILED(SHGetKnownFolderPath(
            FOLDERID_LocalAppData,
            KF_FLAG_DEFAULT,
            nullptr,
            &knownFolderPath))) {
        return {};
    }
    std::wstring path = knownFolderPath;
    CoTaskMemFree(knownFolderPath);
    return path;
}

void MoveIfDestinationIsMissing(
    const std::wstring& source,
    const std::wstring& destination) {
    if (GetFileAttributesW(source.c_str()) == INVALID_FILE_ATTRIBUTES ||
        GetFileAttributesW(destination.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return;
    }
    MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH);
}

}  // namespace

void MigrateLegacyBrandData() noexcept {
    const std::wstring localAppData = LocalAppDataDirectory();
    if (localAppData.empty()) {
        return;
    }

    const std::wstring legacyDirectory = localAppData + L"\\QuickDisplay";
    if (GetFileAttributesW(legacyDirectory.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return;
    }

    const std::wstring currentDirectory = localAppData + L"\\Quickdraw";
    if (!CreateDirectoryW(currentDirectory.c_str(), nullptr) &&
        GetLastError() != ERROR_ALREADY_EXISTS) {
        return;
    }

    MoveIfDestinationIsMissing(
        legacyDirectory + L"\\config.bin",
        currentDirectory + L"\\config.bin");
    MoveIfDestinationIsMissing(
        legacyDirectory + L"\\pending-transaction.bin",
        currentDirectory + L"\\pending-transaction.bin");
    MoveIfDestinationIsMissing(
        legacyDirectory + L"\\QuickDisplay.log",
        currentDirectory + L"\\Quickdraw.log");
    MoveIfDestinationIsMissing(
        legacyDirectory + L"\\QuickDisplay.log.old",
        currentDirectory + L"\\Quickdraw.log.old");
    RemoveDirectoryW(legacyDirectory.c_str());
}

}  // namespace quickdraw
