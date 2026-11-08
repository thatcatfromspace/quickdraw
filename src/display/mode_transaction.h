#pragma once

#include <windows.h>

#include <string>
#include <vector>

#include "display/display_backend.h"
#include "display/display_types.h"
#include "storage/recovery_store.h"

namespace quickdraw {

enum class ModeChangeStage {
    None,
    Snapshot,
    Test,
    Stage,
    Apply,
    Verify,
    Journal,
    Revert,
};

struct ModeChangeResult {
    bool success = false;
    ModeChangeStage stage = ModeChangeStage::None;
    LONG code = DISP_CHANGE_FAILED;
};

struct ModeChangeRequest {
    PersistentMonitorId monitorId;
    std::wstring gdiDeviceName;
    DisplayMode requestedMode;
};

class ModeTransaction final {
public:
    explicit ModeTransaction(
        IDisplayBackend& backend,
        IRecoveryStore* recoveryStore = nullptr);
    ~ModeTransaction();

    ModeTransaction(const ModeTransaction&) = delete;
    ModeTransaction& operator=(const ModeTransaction&) = delete;

    ModeChangeResult Begin(
        const std::wstring& gdiDeviceName,
        const DisplayMode& requestedMode);
    ModeChangeResult Begin(const std::vector<ModeChangeRequest>& requests);
    ModeChangeResult RestorePending(
        const std::vector<RecoveryTarget>& recoveryTargets);
    ModeChangeResult Revert();
    bool Keep() noexcept;

    [[nodiscard]] bool IsActive() const noexcept;

private:
    struct TransactionEntry {
        ModeChangeRequest request;
        DEVMODEW snapshot{};
        DEVMODEW desired{};
    };

    static DEVMODEW BuildRequestedMode(const DEVMODEW& source);
    static bool SameMode(const DEVMODEW& current, const DEVMODEW& requested);
    ModeChangeResult RestoreSnapshot();
    bool VerifyEntries(const std::vector<TransactionEntry>& entries);
    bool VerifyRecoveryTargets(const std::vector<RecoveryTarget>& targets);
    bool SaveRecoveryJournal();
    bool ClearRecoveryJournal() noexcept;

    IDisplayBackend& backend_;
    IRecoveryStore* recoveryStore_ = nullptr;
    std::vector<TransactionEntry> entries_;
    bool active_ = false;
};

const wchar_t* ModeChangeStageName(ModeChangeStage stage) noexcept;
std::wstring DisplayChangeCodeDescription(LONG code);

}  // namespace quickdraw
