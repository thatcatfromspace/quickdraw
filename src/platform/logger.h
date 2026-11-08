#pragma once

#include <string>
#include <string_view>

namespace quickdraw {

class Logger final {
public:
    bool Initialize();
    void Write(std::wstring_view message) const noexcept;

    [[nodiscard]] const std::wstring& Path() const noexcept;

private:
    std::wstring directoryPath_;
    std::wstring filePath_;
};

}  // namespace quickdraw
