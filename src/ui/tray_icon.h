#pragma once

#include <windows.h>
#include <shellapi.h>

namespace quickdraw {

class TrayIcon final {
public:
    TrayIcon() = default;
    ~TrayIcon();

    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

    bool Add(HWND owner, UINT callbackMessage);
    void Remove() noexcept;
    void ReturnFocus() noexcept;
    [[nodiscard]] POINT AnchorPoint() const noexcept;
    [[nodiscard]] bool IsAdded() const noexcept;

private:
    NOTIFYICONDATAW data_{};
    bool added_ = false;
};

}  // namespace quickdraw
