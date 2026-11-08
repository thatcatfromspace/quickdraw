#include <windows.h>

#include "app_controller.h"
#include "platform/single_instance.h"

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE,
    PWSTR,
    int) {
    quickdraw::SingleInstance singleInstance;
    if (!singleInstance.Acquire()) {
        MessageBoxW(
            nullptr,
            L"Quickdraw could not create its single-instance lock.",
            L"Quickdraw",
            MB_OK | MB_ICONERROR);
        return 1;
    }

    if (!singleInstance.IsPrimary()) {
        quickdraw::SingleInstance::SignalPrimaryInstance();
        return 0;
    }

    quickdraw::AppController application(instance);
    if (!application.Initialize()) {
        MessageBoxW(
            nullptr,
            L"Quickdraw could not initialize its notification icon.",
            L"Quickdraw",
            MB_OK | MB_ICONERROR);
        return 1;
    }

    return application.Run();
}
