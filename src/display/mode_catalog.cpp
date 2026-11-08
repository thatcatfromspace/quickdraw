#include "display/mode_catalog.h"

#include <algorithm>
#include <cstdint>

namespace quickdraw {
namespace {

bool HasField(const DEVMODEW& mode, DWORD field) {
    return (mode.dmFields & field) != 0;
}

bool SameModeKey(const DisplayMode& left, const DisplayMode& right) {
    return left.width == right.width &&
           left.height == right.height &&
           left.frequencyHz == right.frequencyHz &&
           left.bitsPerPixel == right.bitsPerPixel &&
           left.interlaced == right.interlaced;
}

bool SameTimingIgnoringScan(const DisplayMode& left, const DisplayMode& right) {
    return left.width == right.width &&
           left.height == right.height &&
           left.frequencyHz == right.frequencyHz &&
           left.bitsPerPixel == right.bitsPerPixel;
}

bool IsCurrentMode(const DEVMODEW& candidate, const DEVMODEW& current) {
    const bool candidateInterlaced = (candidate.dmDisplayFlags & DM_INTERLACED) != 0;
    const bool currentInterlaced = (current.dmDisplayFlags & DM_INTERLACED) != 0;
    return candidate.dmPelsWidth == current.dmPelsWidth &&
           candidate.dmPelsHeight == current.dmPelsHeight &&
           candidate.dmDisplayFrequency == current.dmDisplayFrequency &&
           candidateInterlaced == currentInterlaced;
}

DisplayMode ToDisplayMode(const DEVMODEW& nativeMode, const DEVMODEW& current) {
    DisplayMode mode;
    mode.width = nativeMode.dmPelsWidth;
    mode.height = nativeMode.dmPelsHeight;
    mode.frequencyHz = nativeMode.dmDisplayFrequency;
    mode.bitsPerPixel = nativeMode.dmBitsPerPel;
    mode.displayFlags = nativeMode.dmDisplayFlags;
    mode.interlaced = (nativeMode.dmDisplayFlags & DM_INTERLACED) != 0;
    mode.current = IsCurrentMode(nativeMode, current);
    mode.nativeMode = nativeMode;
    return mode;
}

bool IsUsableMode(const DEVMODEW& mode, const DEVMODEW& current) {
    if (!HasField(mode, DM_PELSWIDTH) || !HasField(mode, DM_PELSHEIGHT) ||
        mode.dmPelsWidth == 0 || mode.dmPelsHeight == 0) {
        return false;
    }

    if (!HasField(mode, DM_DISPLAYFREQUENCY) || mode.dmDisplayFrequency <= 1) {
        return false;
    }

    if (HasField(current, DM_BITSPERPEL) && HasField(mode, DM_BITSPERPEL) &&
        mode.dmBitsPerPel != current.dmBitsPerPel) {
        return false;
    }

    if (HasField(current, DM_DISPLAYORIENTATION) &&
        HasField(mode, DM_DISPLAYORIENTATION) &&
        mode.dmDisplayOrientation != current.dmDisplayOrientation) {
        return false;
    }

    return true;
}

}  // namespace

std::vector<DisplayMode> BuildModeCatalog(
    const std::vector<DEVMODEW>& enumeratedModes,
    const DEVMODEW& currentMode) {
    std::vector<DisplayMode> result;

    for (const DEVMODEW& nativeMode : enumeratedModes) {
        if (!IsUsableMode(nativeMode, currentMode)) {
            continue;
        }

        DisplayMode candidate = ToDisplayMode(nativeMode, currentMode);
        const auto duplicate = std::find_if(
            result.begin(),
            result.end(),
            [&candidate](const DisplayMode& existing) {
                return SameModeKey(existing, candidate);
            });
        if (duplicate != result.end()) {
            duplicate->current = duplicate->current || candidate.current;
            continue;
        }

        if (!candidate.interlaced) {
            result.erase(
                std::remove_if(
                    result.begin(),
                    result.end(),
                    [&candidate](const DisplayMode& existing) {
                        return existing.interlaced && !existing.current &&
                               SameTimingIgnoringScan(existing, candidate);
                    }),
                result.end());
        } else {
            const bool progressiveExists = std::any_of(
                result.begin(),
                result.end(),
                [&candidate](const DisplayMode& existing) {
                    return !existing.interlaced &&
                           SameTimingIgnoringScan(existing, candidate);
                });
            if (progressiveExists && !candidate.current) {
                continue;
            }
        }

        result.push_back(std::move(candidate));
    }

    const bool currentPresent = std::any_of(
        result.begin(),
        result.end(),
        [](const DisplayMode& mode) { return mode.current; });
    if (!currentPresent && IsUsableMode(currentMode, currentMode)) {
        result.push_back(ToDisplayMode(currentMode, currentMode));
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const DisplayMode& left, const DisplayMode& right) {
            const std::uint64_t leftArea =
                static_cast<std::uint64_t>(left.width) * left.height;
            const std::uint64_t rightArea =
                static_cast<std::uint64_t>(right.width) * right.height;
            if (leftArea != rightArea) {
                return leftArea > rightArea;
            }
            if (left.width != right.width) {
                return left.width > right.width;
            }
            if (left.height != right.height) {
                return left.height > right.height;
            }
            if (left.frequencyHz != right.frequencyHz) {
                return left.frequencyHz > right.frequencyHz;
            }
            return !left.interlaced && right.interlaced;
        });

    return result;
}

}  // namespace quickdraw
