#pragma once

#include <windows.h>

namespace quickdraw {

class SingleInstance final {
public:
    SingleInstance() = default;
    ~SingleInstance();

    SingleInstance(const SingleInstance&) = delete;
    SingleInstance& operator=(const SingleInstance&) = delete;

    bool Acquire();
    [[nodiscard]] bool IsPrimary() const noexcept;

    static UINT ActivationMessage();
    static void SignalPrimaryInstance();

private:
    HANDLE mutex_ = nullptr;
    bool primary_ = false;
};

}  // namespace quickdraw
