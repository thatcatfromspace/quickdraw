#include "ui/tray_icon.h"

#include <strsafe.h>

#include "resource.h"

namespace quickdraw {
namespace {

// This GUID is the persistent identity of Quickdraw's notification icon.
constexpr GUID kTrayIconGuid = {
    0x6a994e25,
    0xa09a,
    0x495f,
    {0xaa, 0x87, 0xc9, 0x51, 0x87, 0x2c, 0x55, 0x78},
};

}  // namespace

TrayIcon::~TrayIcon() {
    Remove();
}

bool TrayIcon::Add(HWND owner, UINT callbackMessage) {
    Remove();

    data_ = {};
    data_.cbSize = sizeof(data_);
    data_.hWnd = owner;
    data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_GUID | NIF_SHOWTIP;
    data_.uCallbackMessage = callbackMessage;
    data_.hIcon = static_cast<HICON>(LoadImageW(
        GetModuleHandleW(nullptr),
        MAKEINTRESOURCEW(IDI_APP_ICON),
        IMAGE_ICON,
        0,
        0,
        LR_DEFAULTSIZE | LR_SHARED));
    if (data_.hIcon == nullptr) {
        data_.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    data_.guidItem = kTrayIconGuid;
    StringCchCopyW(
        data_.szTip,
        ARRAYSIZE(data_.szTip),
        L"Quickdraw — display modes in a click");

    if (!Shell_NotifyIconW(NIM_ADD, &data_)) {
        return false;
    }

    data_.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &data_);
    added_ = true;
    return true;
}

void TrayIcon::Remove() noexcept {
    if (added_) {
        Shell_NotifyIconW(NIM_DELETE, &data_);
        added_ = false;
    }
}

void TrayIcon::ReturnFocus() noexcept {
    if (added_) {
        Shell_NotifyIconW(NIM_SETFOCUS, &data_);
    }
}

POINT TrayIcon::AnchorPoint() const noexcept {
    POINT point{};
    NOTIFYICONIDENTIFIER identifier{};
    identifier.cbSize = sizeof(identifier);
    identifier.hWnd = data_.hWnd;
    identifier.guidItem = kTrayIconGuid;

    RECT bounds{};
    if (added_ && SUCCEEDED(Shell_NotifyIconGetRect(&identifier, &bounds))) {
        point.x = bounds.left + ((bounds.right - bounds.left) / 2);
        point.y = bounds.top + ((bounds.bottom - bounds.top) / 2);
        return point;
    }

    GetCursorPos(&point);
    return point;
}

bool TrayIcon::IsAdded() const noexcept {
    return added_;
}

}  // namespace quickdraw
