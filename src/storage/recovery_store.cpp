#include "storage/recovery_store.h"

#include <shlobj.h>

#include <array>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace quickdraw {
namespace {

constexpr std::array<char, 8> kMagic{'Q', 'D', 'R', 'C', 'V', '1', '\0', '\0'};
constexpr std::uint32_t kSchemaVersion = 1;
constexpr std::uint32_t kMaximumTargets = 32;
constexpr std::uint32_t kMaximumStringCharacters = 2048;

bool WriteBytes(HANDLE file, const void* data, DWORD size) {
    DWORD written = 0;
    return WriteFile(file, data, size, &written, nullptr) && written == size;
}

template <typename T>
bool WriteValue(HANDLE file, const T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    return WriteBytes(file, &value, static_cast<DWORD>(sizeof(value)));
}

bool WriteString(HANDLE file, const std::wstring& value) {
    if (value.size() > kMaximumStringCharacters) {
        return false;
    }
    const auto length = static_cast<std::uint32_t>(value.size());
    return WriteValue(file, length) &&
           (length == 0 ||
            WriteBytes(
                file,
                value.data(),
                static_cast<DWORD>(length * sizeof(wchar_t))));
}

bool ReadBytes(HANDLE file, void* data, DWORD size) {
    DWORD read = 0;
    return ReadFile(file, data, size, &read, nullptr) && read == size;
}

template <typename T>
bool ReadValue(HANDLE file, T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    return ReadBytes(file, &value, static_cast<DWORD>(sizeof(value)));
}

bool ReadString(HANDLE file, std::wstring& value) {
    std::uint32_t length = 0;
    if (!ReadValue(file, length) || length > kMaximumStringCharacters) {
        return false;
    }
    value.resize(length);
    return length == 0 ||
           ReadBytes(
               file,
               value.data(),
               static_cast<DWORD>(length * sizeof(wchar_t)));
}

bool WriteTarget(HANDLE file, const RecoveryTarget& target) {
    return WriteString(file, target.monitorId.monitorDevicePath) &&
           WriteString(file, target.monitorId.friendlyName) &&
           WriteString(file, target.gdiDeviceName) &&
           WriteValue(file, target.monitorId.edidManufacturer) &&
           WriteValue(file, target.monitorId.edidProduct) &&
           WriteValue(file, target.monitorId.connectorInstance) &&
           WriteValue(file, target.mode.dmPelsWidth) &&
           WriteValue(file, target.mode.dmPelsHeight) &&
           WriteValue(file, target.mode.dmDisplayFrequency) &&
           WriteValue(file, target.mode.dmBitsPerPel) &&
           WriteValue(file, target.mode.dmDisplayFlags);
}

bool ReadTarget(HANDLE file, RecoveryTarget& target) {
    target.mode = {};
    target.mode.dmSize = sizeof(target.mode);
    target.mode.dmDriverExtra = 0;
    target.mode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY |
                           DM_BITSPERPEL | DM_DISPLAYFLAGS;
    return ReadString(file, target.monitorId.monitorDevicePath) &&
           ReadString(file, target.monitorId.friendlyName) &&
           ReadString(file, target.gdiDeviceName) &&
           ReadValue(file, target.monitorId.edidManufacturer) &&
           ReadValue(file, target.monitorId.edidProduct) &&
           ReadValue(file, target.monitorId.connectorInstance) &&
           ReadValue(file, target.mode.dmPelsWidth) &&
           ReadValue(file, target.mode.dmPelsHeight) &&
           ReadValue(file, target.mode.dmDisplayFrequency) &&
           ReadValue(file, target.mode.dmBitsPerPel) &&
           ReadValue(file, target.mode.dmDisplayFlags);
}

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

}  // namespace

FileRecoveryStore::FileRecoveryStore() {
    const std::wstring localAppData = LocalAppDataDirectory();
    if (localAppData.empty()) {
        return;
    }
    directoryPath_ = localAppData + L"\\Quickdraw";
    filePath_ = directoryPath_ + L"\\pending-transaction.bin";
}

bool FileRecoveryStore::Save(const std::vector<RecoveryTarget>& targets) {
    if (filePath_.empty() || targets.empty() ||
        targets.size() > kMaximumTargets) {
        return false;
    }

    if (!CreateDirectoryW(directoryPath_.c_str(), nullptr) &&
        GetLastError() != ERROR_ALREADY_EXISTS) {
        return false;
    }

    const std::wstring temporaryPath = filePath_ + L".tmp";
    HANDLE file = CreateFileW(
        temporaryPath.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    const auto targetCount = static_cast<std::uint32_t>(targets.size());
    bool success = WriteBytes(file, kMagic.data(), static_cast<DWORD>(kMagic.size())) &&
                   WriteValue(file, kSchemaVersion) &&
                   WriteValue(file, targetCount);
    for (const RecoveryTarget& target : targets) {
        success = success && WriteTarget(file, target);
    }
    success = success && FlushFileBuffers(file);
    CloseHandle(file);

    if (!success || !MoveFileExW(
            temporaryPath.c_str(),
            filePath_.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporaryPath.c_str());
        return false;
    }
    return true;
}

bool FileRecoveryStore::Load(std::vector<RecoveryTarget>& targets) {
    targets.clear();
    if (!HasPending()) {
        return true;
    }

    HANDLE file = CreateFileW(
        filePath_.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    std::array<char, 8> magic{};
    std::uint32_t version = 0;
    std::uint32_t targetCount = 0;
    bool success = ReadBytes(file, magic.data(), static_cast<DWORD>(magic.size())) &&
                   magic == kMagic &&
                   ReadValue(file, version) && version == kSchemaVersion &&
                   ReadValue(file, targetCount) &&
                   targetCount > 0 && targetCount <= kMaximumTargets;
    if (success) {
        targets.resize(targetCount);
        for (RecoveryTarget& target : targets) {
            success = success && ReadTarget(file, target);
        }
    }

    CloseHandle(file);
    if (!success) {
        targets.clear();
    }
    return success;
}

bool FileRecoveryStore::Clear() {
    if (filePath_.empty() || !HasPending()) {
        return true;
    }
    return DeleteFileW(filePath_.c_str()) != FALSE;
}

bool FileRecoveryStore::HasPending() const {
    return !filePath_.empty() &&
           GetFileAttributesW(filePath_.c_str()) != INVALID_FILE_ATTRIBUTES;
}

const std::wstring& FileRecoveryStore::Path() const noexcept {
    return filePath_;
}

}  // namespace quickdraw
