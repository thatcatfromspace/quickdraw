#include "storage/settings_store.h"

#include <objbase.h>
#include <shlobj.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <type_traits>

namespace quickdraw {
namespace {

constexpr std::array<char, 8> kMagic{'Q', 'D', 'C', 'F', 'V', '1', '\0', '\0'};
constexpr std::uint32_t kSchemaVersion = 1;
constexpr std::uint32_t kMaximumPresets = 64;
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

bool WritePersistentId(HANDLE file, const PersistentMonitorId& id) {
    return WriteString(file, id.monitorDevicePath) &&
           WriteString(file, id.friendlyName) &&
           WriteValue(file, id.edidManufacturer) &&
           WriteValue(file, id.edidProduct) &&
           WriteValue(file, id.connectorInstance);
}

bool ReadPersistentId(HANDLE file, PersistentMonitorId& id) {
    return ReadString(file, id.monitorDevicePath) &&
           ReadString(file, id.friendlyName) &&
           ReadValue(file, id.edidManufacturer) &&
           ReadValue(file, id.edidProduct) &&
           ReadValue(file, id.connectorInstance);
}

bool WritePreset(HANDLE file, const DisplayPreset& preset) {
    if (preset.targets.empty() || preset.targets.size() > kMaximumTargets) {
        return false;
    }
    const auto targetCount = static_cast<std::uint32_t>(preset.targets.size());
    if (!WriteString(file, preset.id) || !WriteString(file, preset.name) ||
        !WriteValue(file, targetCount)) {
        return false;
    }
    for (const PresetTarget& target : preset.targets) {
        if (!WritePersistentId(file, target.monitorId) ||
            !WriteValue(file, target.mode.width) ||
            !WriteValue(file, target.mode.height) ||
            !WriteValue(file, target.mode.frequencyHz) ||
            !WriteValue(file, target.mode.bitsPerPixel) ||
            !WriteValue(file, target.mode.displayFlags)) {
            return false;
        }
    }
    return true;
}

bool ReadPreset(HANDLE file, DisplayPreset& preset) {
    std::uint32_t targetCount = 0;
    if (!ReadString(file, preset.id) || !ReadString(file, preset.name) ||
        !ReadValue(file, targetCount) || targetCount == 0 ||
        targetCount > kMaximumTargets) {
        return false;
    }
    preset.targets.resize(targetCount);
    for (PresetTarget& target : preset.targets) {
        if (!ReadPersistentId(file, target.monitorId) ||
            !ReadValue(file, target.mode.width) ||
            !ReadValue(file, target.mode.height) ||
            !ReadValue(file, target.mode.frequencyHz) ||
            !ReadValue(file, target.mode.bitsPerPixel) ||
            !ReadValue(file, target.mode.displayFlags)) {
            return false;
        }
    }
    return true;
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

bool IsAllowedConfirmationSeconds(unsigned seconds) {
    constexpr std::array<unsigned, 4> allowed{5, 10, 15, 30};
    return std::find(allowed.begin(), allowed.end(), seconds) != allowed.end();
}

}  // namespace

SettingsStore::SettingsStore() {
    const std::wstring localAppData = LocalAppDataDirectory();
    if (localAppData.empty()) {
        return;
    }
    directoryPath_ = localAppData + L"\\Quickdraw";
    filePath_ = directoryPath_ + L"\\config.bin";
}

bool SettingsStore::Load(AppSettings& settings) const {
    settings = {};
    settings.confirmationSeconds = 15;
    if (filePath_.empty()) {
        return false;
    }
    if (GetFileAttributesW(filePath_.c_str()) == INVALID_FILE_ATTRIBUTES) {
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
    std::uint32_t startWithWindows = 0;
    std::uint32_t confirmationSeconds = 0;
    std::uint32_t presetCount = 0;
    bool success = ReadBytes(file, magic.data(), static_cast<DWORD>(magic.size())) &&
                   magic == kMagic &&
                   ReadValue(file, version) && version == kSchemaVersion &&
                   ReadValue(file, startWithWindows) &&
                   ReadValue(file, confirmationSeconds) &&
                   IsAllowedConfirmationSeconds(confirmationSeconds) &&
                   ReadValue(file, presetCount) && presetCount <= kMaximumPresets;
    if (success) {
        settings.startWithWindows = startWithWindows != 0;
        settings.confirmationSeconds = confirmationSeconds;
        settings.presets.resize(presetCount);
        for (DisplayPreset& preset : settings.presets) {
            success = success && ReadPreset(file, preset);
        }
    }
    CloseHandle(file);

    if (!success) {
        settings = {};
        settings.confirmationSeconds = 15;
    }
    return success;
}

bool SettingsStore::Save(const AppSettings& settings) const {
    if (filePath_.empty() || settings.presets.size() > kMaximumPresets ||
        !IsAllowedConfirmationSeconds(settings.confirmationSeconds)) {
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

    const std::uint32_t startWithWindows = settings.startWithWindows ? 1 : 0;
    const auto confirmationSeconds =
        static_cast<std::uint32_t>(settings.confirmationSeconds);
    const auto presetCount = static_cast<std::uint32_t>(settings.presets.size());
    bool success = WriteBytes(file, kMagic.data(), static_cast<DWORD>(kMagic.size())) &&
                   WriteValue(file, kSchemaVersion) &&
                   WriteValue(file, startWithWindows) &&
                   WriteValue(file, confirmationSeconds) &&
                   WriteValue(file, presetCount);
    for (const DisplayPreset& preset : settings.presets) {
        success = success && WritePreset(file, preset);
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

const std::wstring& SettingsStore::Path() const noexcept {
    return filePath_;
}

std::wstring CreatePresetId() {
    GUID id{};
    if (FAILED(CoCreateGuid(&id))) {
        return std::to_wstring(GetTickCount64());
    }
    wchar_t buffer[40]{};
    StringFromGUID2(id, buffer, ARRAYSIZE(buffer));
    return buffer;
}

DisplayPreset CapturePreset(
    const std::wstring& name,
    const std::vector<MonitorInfo>& displays) {
    DisplayPreset preset;
    preset.id = CreatePresetId();
    preset.name = name;
    for (const MonitorInfo& display : displays) {
        if (!display.independentlyConfigurable) {
            preset.targets.clear();
            return preset;
        }
        const auto current = std::find_if(
            display.modes.begin(),
            display.modes.end(),
            [](const DisplayMode& mode) { return mode.current; });
        if (current == display.modes.end()) {
            preset.targets.clear();
            return preset;
        }
        preset.targets.push_back(PresetTarget{
            display.persistentId,
            PresetMode{
                current->width,
                current->height,
                current->frequencyHz,
                current->bitsPerPixel,
                current->displayFlags,
            },
        });
    }
    return preset;
}

const DisplayMode* FindPresetMode(
    const PresetMode& requested,
    const MonitorInfo& display) {
    const auto mode = std::find_if(
        display.modes.begin(),
        display.modes.end(),
        [&requested](const DisplayMode& candidate) {
            return candidate.width == requested.width &&
                   candidate.height == requested.height &&
                   candidate.frequencyHz == requested.frequencyHz &&
                   candidate.bitsPerPixel == requested.bitsPerPixel &&
                   (candidate.displayFlags & DM_INTERLACED) ==
                       (requested.displayFlags & DM_INTERLACED);
        });
    return mode == display.modes.end() ? nullptr : &*mode;
}

}  // namespace quickdraw
