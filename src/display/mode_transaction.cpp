#include "display/mode_transaction.h"

#include <windows.h>

#include <algorithm>
#include <utility>

namespace quickdraw {
namespace {

constexpr int kVerificationAttempts = 6;
constexpr DWORD kVerificationDelayMilliseconds = 100;

ModeChangeResult Failure(ModeChangeStage stage, LONG code) {
    return ModeChangeResult{false, stage, code};
}

ModeChangeResult Success() {
    return ModeChangeResult{true, ModeChangeStage::None, DISP_CHANGE_SUCCESSFUL};
}

}  // namespace

ModeTransaction::ModeTransaction(
    IDisplayBackend& backend,
    IRecoveryStore* recoveryStore)
    : backend_(backend), recoveryStore_(recoveryStore) {}

ModeTransaction::~ModeTransaction() {
    if (active_) {
        RestoreSnapshot();
    }
}

ModeChangeResult ModeTransaction::Begin(
    const std::wstring& gdiDeviceName,
    const DisplayMode& requestedMode) {
    ModeChangeRequest request;
    request.gdiDeviceName = gdiDeviceName;
    request.requestedMode = requestedMode;
    return Begin(std::vector<ModeChangeRequest>{std::move(request)});
}

ModeChangeResult ModeTransaction::Begin(
    const std::vector<ModeChangeRequest>& requests) {
    if (active_) {
        return Failure(ModeChangeStage::Snapshot, ERROR_BUSY);
    }
    if (requests.empty()) {
        return Failure(ModeChangeStage::Snapshot, ERROR_INVALID_PARAMETER);
    }

    entries_.clear();
    entries_.reserve(requests.size());
    for (const ModeChangeRequest& request : requests) {
        const bool duplicate = std::any_of(
            entries_.begin(),
            entries_.end(),
            [&request](const TransactionEntry& entry) {
                return entry.request.gdiDeviceName == request.gdiDeviceName;
            });
        if (duplicate || request.gdiDeviceName.empty()) {
            entries_.clear();
            return Failure(ModeChangeStage::Snapshot, ERROR_INVALID_PARAMETER);
        }

        TransactionEntry entry;
        entry.request = request;
        LONG result = backend_.ReadCurrentMode(
            request.gdiDeviceName,
            entry.snapshot);
        if (result != DISP_CHANGE_SUCCESSFUL) {
            entries_.clear();
            return Failure(ModeChangeStage::Snapshot, result);
        }
        entry.desired = BuildRequestedMode(request.requestedMode.nativeMode);
        entries_.push_back(std::move(entry));
    }

    for (const TransactionEntry& entry : entries_) {
        const LONG result = backend_.TestMode(
            entry.request.gdiDeviceName,
            entry.desired);
        if (result != DISP_CHANGE_SUCCESSFUL) {
            entries_.clear();
            return Failure(ModeChangeStage::Test, result);
        }
    }

    if (!SaveRecoveryJournal()) {
        entries_.clear();
        return Failure(ModeChangeStage::Journal, ERROR_WRITE_FAULT);
    }

    active_ = true;
    for (const TransactionEntry& entry : entries_) {
        const LONG result = backend_.StageMode(
            entry.request.gdiDeviceName,
            entry.desired);
        if (result != DISP_CHANGE_SUCCESSFUL) {
            RestoreSnapshot();
            return Failure(ModeChangeStage::Stage, result);
        }
    }

    const LONG result = backend_.ApplyStagedModes();
    if (result != DISP_CHANGE_SUCCESSFUL) {
        RestoreSnapshot();
        return Failure(ModeChangeStage::Apply, result);
    }

    if (!VerifyEntries(entries_)) {
        RestoreSnapshot();
        return Failure(ModeChangeStage::Verify, ERROR_INVALID_DATA);
    }

    return Success();
}

ModeChangeResult ModeTransaction::Revert() {
    if (!active_) {
        return Success();
    }
    return RestoreSnapshot();
}

bool ModeTransaction::Keep() noexcept {
    active_ = false;
    entries_.clear();
    return ClearRecoveryJournal();
}

bool ModeTransaction::IsActive() const noexcept {
    return active_;
}

DEVMODEW ModeTransaction::BuildRequestedMode(const DEVMODEW& source) {
    DEVMODEW requested = source;
    requested.dmSize = sizeof(requested);
    requested.dmDriverExtra = 0;
    requested.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL |
                         DM_DISPLAYFREQUENCY | DM_DISPLAYFLAGS;
    return requested;
}

bool ModeTransaction::SameMode(
    const DEVMODEW& current,
    const DEVMODEW& requested) {
    return current.dmPelsWidth == requested.dmPelsWidth &&
           current.dmPelsHeight == requested.dmPelsHeight &&
           current.dmBitsPerPel == requested.dmBitsPerPel &&
           current.dmDisplayFrequency == requested.dmDisplayFrequency &&
           (current.dmDisplayFlags & DM_INTERLACED) ==
               (requested.dmDisplayFlags & DM_INTERLACED);
}

ModeChangeResult ModeTransaction::RestoreSnapshot() {
    LONG result = DISP_CHANGE_SUCCESSFUL;
    for (const TransactionEntry& entry : entries_) {
        const DEVMODEW restoreMode = BuildRequestedMode(entry.snapshot);
        result = backend_.StageMode(entry.request.gdiDeviceName, restoreMode);
        if (result != DISP_CHANGE_SUCCESSFUL) {
            break;
        }
    }
    if (result == DISP_CHANGE_SUCCESSFUL) {
        result = backend_.ApplyStagedModes();
    }

    std::vector<TransactionEntry> restoreEntries = entries_;
    for (TransactionEntry& entry : restoreEntries) {
        entry.desired = BuildRequestedMode(entry.snapshot);
    }

    if (result == DISP_CHANGE_SUCCESSFUL && VerifyEntries(restoreEntries)) {
        active_ = false;
        entries_.clear();
        ClearRecoveryJournal();
        return Success();
    }

    return Failure(
        ModeChangeStage::Revert,
        result == DISP_CHANGE_SUCCESSFUL ? ERROR_INVALID_DATA : result);
}

bool ModeTransaction::VerifyEntries(
    const std::vector<TransactionEntry>& entries) {
    for (int attempt = 0; attempt < kVerificationAttempts; ++attempt) {
        bool allMatch = true;
        for (const TransactionEntry& entry : entries) {
            DEVMODEW current{};
            if (backend_.ReadCurrentMode(entry.request.gdiDeviceName, current) !=
                    DISP_CHANGE_SUCCESSFUL ||
                !SameMode(current, entry.desired)) {
                allMatch = false;
                break;
            }
        }
        if (allMatch) {
            return true;
        }
        if (attempt + 1 < kVerificationAttempts) {
            Sleep(kVerificationDelayMilliseconds);
        }
    }
    return false;
}

bool ModeTransaction::VerifyRecoveryTargets(
    const std::vector<RecoveryTarget>& targets) {
    for (int attempt = 0; attempt < kVerificationAttempts; ++attempt) {
        bool allMatch = true;
        for (const RecoveryTarget& target : targets) {
            DEVMODEW current{};
            if (backend_.ReadCurrentMode(target.gdiDeviceName, current) !=
                    DISP_CHANGE_SUCCESSFUL ||
                !SameMode(current, target.mode)) {
                allMatch = false;
                break;
            }
        }
        if (allMatch) {
            return true;
        }
        if (attempt + 1 < kVerificationAttempts) {
            Sleep(kVerificationDelayMilliseconds);
        }
    }
    return false;
}

bool ModeTransaction::SaveRecoveryJournal() {
    if (recoveryStore_ == nullptr) {
        return true;
    }
    std::vector<RecoveryTarget> targets;
    targets.reserve(entries_.size());
    for (const TransactionEntry& entry : entries_) {
        targets.push_back(RecoveryTarget{
            entry.request.monitorId,
            entry.request.gdiDeviceName,
            BuildRequestedMode(entry.snapshot),
        });
    }
    return recoveryStore_->Save(targets);
}

bool ModeTransaction::ClearRecoveryJournal() noexcept {
    if (recoveryStore_ != nullptr) {
        return recoveryStore_->Clear();
    }
    return true;
}

ModeChangeResult ModeTransaction::RestorePending(
    const std::vector<RecoveryTarget>& recoveryTargets) {
    if (active_ || recoveryTargets.empty()) {
        return Failure(ModeChangeStage::Revert, ERROR_INVALID_PARAMETER);
    }

    for (const RecoveryTarget& target : recoveryTargets) {
        const LONG testResult = backend_.TestMode(
            target.gdiDeviceName,
            BuildRequestedMode(target.mode));
        if (testResult != DISP_CHANGE_SUCCESSFUL) {
            return Failure(ModeChangeStage::Revert, testResult);
        }
    }
    for (const RecoveryTarget& target : recoveryTargets) {
        const LONG stageResult = backend_.StageMode(
            target.gdiDeviceName,
            BuildRequestedMode(target.mode));
        if (stageResult != DISP_CHANGE_SUCCESSFUL) {
            return Failure(ModeChangeStage::Revert, stageResult);
        }
    }
    const LONG applyResult = backend_.ApplyStagedModes();
    if (applyResult != DISP_CHANGE_SUCCESSFUL) {
        return Failure(ModeChangeStage::Revert, applyResult);
    }
    if (!VerifyRecoveryTargets(recoveryTargets)) {
        return Failure(ModeChangeStage::Revert, ERROR_INVALID_DATA);
    }
    return Success();
}

const wchar_t* ModeChangeStageName(ModeChangeStage stage) noexcept {
    switch (stage) {
        case ModeChangeStage::Snapshot:
            return L"reading the current display mode";
        case ModeChangeStage::Test:
            return L"validating the requested display mode";
        case ModeChangeStage::Stage:
            return L"staging the requested display mode";
        case ModeChangeStage::Apply:
            return L"applying the requested display mode";
        case ModeChangeStage::Verify:
            return L"verifying the requested display mode";
        case ModeChangeStage::Journal:
            return L"saving the display recovery journal";
        case ModeChangeStage::Revert:
            return L"restoring the previous display mode";
        case ModeChangeStage::None:
        default:
            return L"changing the display mode";
    }
}

std::wstring DisplayChangeCodeDescription(LONG code) {
    switch (code) {
        case DISP_CHANGE_SUCCESSFUL:
            return L"The display change completed successfully.";
        case DISP_CHANGE_BADDUALVIEW:
            return L"The graphics driver rejected the change in the current mirrored configuration.";
        case DISP_CHANGE_BADFLAGS:
            return L"Windows rejected the display-change flags.";
        case DISP_CHANGE_BADMODE:
            return L"The requested display mode is not supported by the driver.";
        case DISP_CHANGE_BADPARAM:
            return L"Windows rejected one of the display parameters.";
        case DISP_CHANGE_FAILED:
            return L"The graphics driver failed to apply the display mode.";
        case DISP_CHANGE_NOTUPDATED:
            return L"Windows could not save the display settings.";
        case DISP_CHANGE_RESTART:
            return L"The driver requires a restart, so this mode is not suitable for quick switching.";
        default:
            break;
    }

    wchar_t* systemMessage = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        static_cast<DWORD>(code),
        0,
        reinterpret_cast<wchar_t*>(&systemMessage),
        0,
        nullptr);
    if (length == 0 || systemMessage == nullptr) {
        return L"Windows returned an unrecognized display error.";
    }
    std::wstring description(systemMessage, length);
    LocalFree(systemMessage);
    return description;
}

}  // namespace quickdraw
