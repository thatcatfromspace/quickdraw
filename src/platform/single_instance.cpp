#include "platform/single_instance.h"

namespace quickdraw {
namespace {

constexpr wchar_t kMutexName[] =
    L"Local\\Quickdraw.SingleInstance.A69E7DDA-E892-4798-93E1-921CC8F2DCFA";
constexpr wchar_t kActivationMessageName[] =
    L"Quickdraw.Activate.A69E7DDA-E892-4798-93E1-921CC8F2DCFA";

}  // namespace

SingleInstance::~SingleInstance() {
    if (mutex_ != nullptr) {
        CloseHandle(mutex_);
    }
}

bool SingleInstance::Acquire() {
    mutex_ = CreateMutexW(nullptr, FALSE, kMutexName);
    if (mutex_ == nullptr) {
        primary_ = false;
        return false;
    }

    primary_ = GetLastError() != ERROR_ALREADY_EXISTS;
    return true;
}

bool SingleInstance::IsPrimary() const noexcept {
    return primary_;
}

UINT SingleInstance::ActivationMessage() {
    static const UINT message = RegisterWindowMessageW(kActivationMessageName);
    return message;
}

void SingleInstance::SignalPrimaryInstance() {
    const UINT message = ActivationMessage();
    if (message != 0) {
        PostMessageW(HWND_BROADCAST, message, 0, 0);
    }
}

}  // namespace quickdraw
