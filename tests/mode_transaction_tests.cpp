#include <windows.h>

#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include "display/mode_transaction.h"

namespace {

DEVMODEW MakeNativeMode(DWORD width, DWORD height, DWORD frequency) {
    DEVMODEW mode{};
    mode.dmSize = sizeof(mode);
    mode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY |
                    DM_BITSPERPEL | DM_DISPLAYFLAGS;
    mode.dmPelsWidth = width;
    mode.dmPelsHeight = height;
    mode.dmDisplayFrequency = frequency;
    mode.dmBitsPerPel = 32;
    return mode;
}

quickdraw::DisplayMode MakeDisplayMode(
    DWORD width,
    DWORD height,
    DWORD frequency) {
    quickdraw::DisplayMode mode;
    mode.width = width;
    mode.height = height;
    mode.frequencyHz = frequency;
    mode.bitsPerPixel = 32;
    mode.nativeMode = MakeNativeMode(width, height, frequency);
    return mode;
}

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

class FakeDisplayBackend final : public quickdraw::IDisplayBackend {
public:
    LONG EnumerateDisplays(std::vector<quickdraw::MonitorInfo>&) override {
        return ERROR_SUCCESS;
    }

    LONG ReadCurrentMode(const std::wstring&, DEVMODEW& mode) override {
        mode = currentMode;
        return readResult;
    }

    LONG TestMode(const std::wstring&, const DEVMODEW&) override {
        ++testCalls;
        return testResult;
    }

    LONG StageMode(const std::wstring&, const DEVMODEW& mode) override {
        ++stageCalls;
        if (stageResult == DISP_CHANGE_SUCCESSFUL) {
            stagedMode = mode;
        }
        return stageResult;
    }

    LONG ApplyStagedModes() override {
        ++applyCalls;
        if (applyResult == DISP_CHANGE_SUCCESSFUL && applyChangesMode) {
            currentMode = stagedMode;
        }
        return applyResult;
    }

    DEVMODEW currentMode = MakeNativeMode(1920, 1080, 60);
    DEVMODEW stagedMode{};
    LONG readResult = DISP_CHANGE_SUCCESSFUL;
    LONG testResult = DISP_CHANGE_SUCCESSFUL;
    LONG stageResult = DISP_CHANGE_SUCCESSFUL;
    LONG applyResult = DISP_CHANGE_SUCCESSFUL;
    bool applyChangesMode = true;
    int testCalls = 0;
    int stageCalls = 0;
    int applyCalls = 0;
};

class BatchDisplayBackend final : public quickdraw::IDisplayBackend {
public:
    LONG EnumerateDisplays(std::vector<quickdraw::MonitorInfo>&) override {
        return ERROR_SUCCESS;
    }

    LONG ReadCurrentMode(const std::wstring& device, DEVMODEW& mode) override {
        const auto found = currentModes.find(device);
        if (found == currentModes.end()) {
            return DISP_CHANGE_FAILED;
        }
        mode = found->second;
        return DISP_CHANGE_SUCCESSFUL;
    }

    LONG TestMode(const std::wstring&, const DEVMODEW&) override {
        return DISP_CHANGE_SUCCESSFUL;
    }

    LONG StageMode(const std::wstring& device, const DEVMODEW& mode) override {
        stagedModes[device] = mode;
        return DISP_CHANGE_SUCCESSFUL;
    }

    LONG ApplyStagedModes() override {
        ++applyCalls;
        for (const auto& [device, mode] : stagedModes) {
            currentModes[device] = mode;
        }
        stagedModes.clear();
        return DISP_CHANGE_SUCCESSFUL;
    }

    std::map<std::wstring, DEVMODEW> currentModes;
    std::map<std::wstring, DEVMODEW> stagedModes;
    int applyCalls = 0;
};

class FakeRecoveryStore final : public quickdraw::IRecoveryStore {
public:
    bool Save(const std::vector<quickdraw::RecoveryTarget>& targets) override {
        ++saveCalls;
        savedTargets = targets;
        return saveSucceeds;
    }

    bool Load(std::vector<quickdraw::RecoveryTarget>& targets) override {
        targets = savedTargets;
        return true;
    }

    bool Clear() override {
        ++clearCalls;
        savedTargets.clear();
        return true;
    }

    bool HasPending() const override {
        return !savedTargets.empty();
    }

    bool saveSucceeds = true;
    int saveCalls = 0;
    int clearCalls = 0;
    std::vector<quickdraw::RecoveryTarget> savedTargets;
};

}  // namespace

int main() {
    {
        FakeDisplayBackend backend;
        quickdraw::ModeTransaction transaction(backend);
        const quickdraw::ModeChangeResult result = transaction.Begin(
            L"\\\\.\\DISPLAY1",
            MakeDisplayMode(2560, 1440, 165));
        Require(result.success);
        Require(transaction.IsActive());
        Require(backend.currentMode.dmPelsWidth == 2560);

        const quickdraw::ModeChangeResult revertResult = transaction.Revert();
        Require(revertResult.success);
        Require(!transaction.IsActive());
        Require(backend.currentMode.dmPelsWidth == 1920);
    }

    {
        FakeDisplayBackend backend;
        backend.testResult = DISP_CHANGE_BADMODE;
        quickdraw::ModeTransaction transaction(backend);
        const quickdraw::ModeChangeResult result = transaction.Begin(
            L"\\\\.\\DISPLAY1",
            MakeDisplayMode(2560, 1440, 165));
        Require(!result.success);
        Require(result.stage == quickdraw::ModeChangeStage::Test);
        Require(backend.stageCalls == 0);
    }

    {
        FakeDisplayBackend backend;
        backend.applyChangesMode = false;
        quickdraw::ModeTransaction transaction(backend);
        const quickdraw::ModeChangeResult result = transaction.Begin(
            L"\\\\.\\DISPLAY1",
            MakeDisplayMode(2560, 1440, 165));
        Require(!result.success);
        Require(result.stage == quickdraw::ModeChangeStage::Verify);
        Require(!transaction.IsActive());
        Require(backend.currentMode.dmPelsWidth == 1920);
    }

    {
        BatchDisplayBackend backend;
        backend.currentModes[L"display-a"] = MakeNativeMode(1920, 1080, 60);
        backend.currentModes[L"display-b"] = MakeNativeMode(3840, 2160, 60);
        FakeRecoveryStore recovery;
        quickdraw::ModeTransaction transaction(backend, &recovery);

        quickdraw::ModeChangeRequest first;
        first.gdiDeviceName = L"display-a";
        first.requestedMode = MakeDisplayMode(2560, 1440, 165);
        quickdraw::ModeChangeRequest second;
        second.gdiDeviceName = L"display-b";
        second.requestedMode = MakeDisplayMode(2560, 1440, 120);

        const quickdraw::ModeChangeResult result =
            transaction.Begin({first, second});
        Require(result.success);
        Require(backend.applyCalls == 1);
        Require(recovery.saveCalls == 1);
        Require(recovery.savedTargets.size() == 2);
        Require(backend.currentModes[L"display-a"].dmDisplayFrequency == 165);
        Require(backend.currentModes[L"display-b"].dmDisplayFrequency == 120);

        const quickdraw::ModeChangeResult revertResult = transaction.Revert();
        Require(revertResult.success);
        Require(backend.applyCalls == 2);
        Require(recovery.clearCalls == 1);
        Require(backend.currentModes[L"display-a"].dmPelsWidth == 1920);
        Require(backend.currentModes[L"display-b"].dmPelsWidth == 3840);
    }

    {
        FakeDisplayBackend backend;
        FakeRecoveryStore recovery;
        recovery.saveSucceeds = false;
        quickdraw::ModeTransaction transaction(backend, &recovery);
        const quickdraw::ModeChangeResult result = transaction.Begin(
            L"\\\\.\\DISPLAY1",
            MakeDisplayMode(2560, 1440, 165));
        Require(!result.success);
        Require(result.stage == quickdraw::ModeChangeStage::Journal);
        Require(backend.stageCalls == 0);
    }

    return EXIT_SUCCESS;
}
