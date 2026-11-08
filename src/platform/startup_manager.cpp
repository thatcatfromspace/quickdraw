#include "platform/startup_manager.h"

#include <windows.h>

#include <string>
#include <vector>

namespace quickdraw {
namespace {

constexpr wchar_t kRunKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kValueName[] = L"Quickdraw";
constexpr wchar_t kLegacyValueName[] = L"QuickDisplay";

std::wstring ExecutableCommand() {
    std::vector<wchar_t> buffer(512);
    while (true) {
        const DWORD length = GetModuleFileNameW(
            nullptr,
            buffer.data(),
            static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return {};
        }
        if (static_cast<std::size_t>(length) < buffer.size() - 1) {
            return L"\"" + std::wstring(buffer.data(), length) + L"\"";
        }
        buffer.resize(buffer.size() * 2);
    }
}

}  // namespace

bool StartupManager::IsEnabled() const {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &key) !=
        ERROR_SUCCESS) {
        return false;
    }
    const LONG result = RegQueryValueExW(
        key,
        kValueName,
        nullptr,
        nullptr,
        nullptr,
        nullptr);
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

bool StartupManager::SetEnabled(bool enabled) const {
    HKEY key = nullptr;
    if (RegCreateKeyExW(
            HKEY_CURRENT_USER,
            kRunKey,
            0,
            nullptr,
            0,
            KEY_SET_VALUE,
            nullptr,
            &key,
            nullptr) != ERROR_SUCCESS) {
        return false;
    }

    LONG result = ERROR_SUCCESS;
    if (enabled) {
        const std::wstring command = ExecutableCommand();
        if (command.empty()) {
            RegCloseKey(key);
            return false;
        }
        result = RegSetValueExW(
            key,
            kValueName,
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(command.c_str()),
            static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    } else {
        result = RegDeleteValueW(key, kValueName);
        if (result == ERROR_FILE_NOT_FOUND) {
            result = ERROR_SUCCESS;
        }
    }
    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

void StartupManager::MigrateLegacyEntry() const noexcept {
    HKEY key = nullptr;
    if (RegOpenKeyExW(
            HKEY_CURRENT_USER,
            kRunKey,
            0,
            KEY_QUERY_VALUE | KEY_SET_VALUE,
            &key) != ERROR_SUCCESS) {
        return;
    }

    const bool legacyExists = RegQueryValueExW(
        key,
        kLegacyValueName,
        nullptr,
        nullptr,
        nullptr,
        nullptr) == ERROR_SUCCESS;
    const bool currentExists = RegQueryValueExW(
        key,
        kValueName,
        nullptr,
        nullptr,
        nullptr,
        nullptr) == ERROR_SUCCESS;
    RegCloseKey(key);

    if (!legacyExists) {
        return;
    }
    if (currentExists || SetEnabled(true)) {
        if (RegOpenKeyExW(
                HKEY_CURRENT_USER,
                kRunKey,
                0,
                KEY_SET_VALUE,
                &key) == ERROR_SUCCESS) {
            RegDeleteValueW(key, kLegacyValueName);
            RegCloseKey(key);
        }
    }
}

}  // namespace quickdraw
