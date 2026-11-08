#pragma once

#include <windows.h>

#include <string>
#include <vector>

#include "display/display_types.h"

namespace quickdraw {

struct RecoveryTarget {
    PersistentMonitorId monitorId;
    std::wstring gdiDeviceName;
    DEVMODEW mode{};
};

class IRecoveryStore {
public:
    virtual ~IRecoveryStore() = default;
    virtual bool Save(const std::vector<RecoveryTarget>& targets) = 0;
    virtual bool Load(std::vector<RecoveryTarget>& targets) = 0;
    virtual bool Clear() = 0;
    [[nodiscard]] virtual bool HasPending() const = 0;
};

class FileRecoveryStore final : public IRecoveryStore {
public:
    FileRecoveryStore();

    bool Save(const std::vector<RecoveryTarget>& targets) override;
    bool Load(std::vector<RecoveryTarget>& targets) override;
    bool Clear() override;
    [[nodiscard]] bool HasPending() const override;

    [[nodiscard]] const std::wstring& Path() const noexcept;

private:
    std::wstring directoryPath_;
    std::wstring filePath_;
};

}  // namespace quickdraw
