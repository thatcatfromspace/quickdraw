#include "platform/logger.h"

#include <windows.h>
#include <shlobj.h>
#include <strsafe.h>

#include <cstdint>
#include <string>
#include <vector>

namespace quickdraw {
namespace {

constexpr std::uint64_t kMaximumLogBytes = 256 * 1024;

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

std::string Utf8FromWide(std::wstring_view value) {
    if (value.empty()) {
        return {};
    }
    const int byteCount = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (byteCount <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(byteCount), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        result.data(),
        byteCount,
        nullptr,
        nullptr);
    return result;
}

}  // namespace

bool Logger::Initialize() {
    const std::wstring localAppData = LocalAppDataDirectory();
    if (localAppData.empty()) {
        return false;
    }
    directoryPath_ = localAppData + L"\\Quickdraw";
    filePath_ = directoryPath_ + L"\\Quickdraw.log";
    if (!CreateDirectoryW(directoryPath_.c_str(), nullptr) &&
        GetLastError() != ERROR_ALREADY_EXISTS) {
        filePath_.clear();
        return false;
    }

    WIN32_FILE_ATTRIBUTE_DATA attributes{};
    if (GetFileAttributesExW(
            filePath_.c_str(),
            GetFileExInfoStandard,
            &attributes)) {
        const std::uint64_t size =
            (static_cast<std::uint64_t>(attributes.nFileSizeHigh) << 32) |
            attributes.nFileSizeLow;
        if (size > kMaximumLogBytes) {
            const std::wstring previousPath = filePath_ + L".old";
            MoveFileExW(
                filePath_.c_str(),
                previousPath.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
        }
    }
    return true;
}

void Logger::Write(std::wstring_view message) const noexcept {
    if (filePath_.empty()) {
        return;
    }

    SYSTEMTIME time{};
    GetLocalTime(&time);
    wchar_t prefix[64]{};
    StringCchPrintfW(
        prefix,
        ARRAYSIZE(prefix),
        L"%04u-%02u-%02u %02u:%02u:%02u.%03u ",
        time.wYear,
        time.wMonth,
        time.wDay,
        time.wHour,
        time.wMinute,
        time.wSecond,
        time.wMilliseconds);
    const std::string utf8 = Utf8FromWide(std::wstring(prefix) +
                                          std::wstring(message) + L"\r\n");
    if (utf8.empty()) {
        return;
    }

    HANDLE file = CreateFileW(
        filePath_.c_str(),
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    WriteFile(
        file,
        utf8.data(),
        static_cast<DWORD>(utf8.size()),
        &written,
        nullptr);
    CloseHandle(file);
}

const std::wstring& Logger::Path() const noexcept {
    return filePath_;
}

}  // namespace quickdraw
