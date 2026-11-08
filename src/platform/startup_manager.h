#pragma once

namespace quickdraw {

class StartupManager final {
public:
    [[nodiscard]] bool IsEnabled() const;
    bool SetEnabled(bool enabled) const;
    void MigrateLegacyEntry() const noexcept;
};

}  // namespace quickdraw
