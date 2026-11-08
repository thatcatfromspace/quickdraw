#include <windows.h>

#include <cstdlib>
#include <vector>

#include "display/mode_catalog.h"

namespace {

DEVMODEW MakeMode(
    DWORD width,
    DWORD height,
    DWORD frequency,
    DWORD bitsPerPixel = 32,
    DWORD orientation = DMDO_DEFAULT,
    bool interlaced = false) {
    DEVMODEW mode{};
    mode.dmSize = sizeof(mode);
    mode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY |
                    DM_BITSPERPEL | DM_DISPLAYORIENTATION | DM_DISPLAYFLAGS;
    mode.dmPelsWidth = width;
    mode.dmPelsHeight = height;
    mode.dmDisplayFrequency = frequency;
    mode.dmBitsPerPel = bitsPerPixel;
    mode.dmDisplayOrientation = orientation;
    mode.dmDisplayFlags = interlaced ? DM_INTERLACED : 0;
    return mode;
}

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

}  // namespace

int main() {
    const DEVMODEW current = MakeMode(2560, 1440, 165);
    const std::vector<DEVMODEW> rawModes{
        MakeMode(1920, 1080, 60),
        MakeMode(2560, 1440, 165),
        MakeMode(2560, 1440, 165),
        MakeMode(2560, 1440, 144),
        MakeMode(3840, 2160, 60, 32, DMDO_90),
        MakeMode(1920, 1080, 60, 16),
        MakeMode(1920, 1080, 60, 32, DMDO_DEFAULT, true),
        MakeMode(3840, 2160, 60),
    };

    const std::vector<quickdraw::DisplayMode> modes =
        quickdraw::BuildModeCatalog(rawModes, current);

    Require(modes.size() == 4);
    Require(modes[0].width == 3840 && modes[0].height == 2160);
    Require(modes[1].width == 2560 && modes[1].frequencyHz == 165);
    Require(modes[1].current);
    Require(modes[2].width == 2560 && modes[2].frequencyHz == 144);
    Require(modes[3].width == 1920 && !modes[3].interlaced);

    const DEVMODEW interlacedCurrent =
        MakeMode(1920, 1080, 60, 32, DMDO_DEFAULT, true);
    const std::vector<quickdraw::DisplayMode> interlacedModes =
        quickdraw::BuildModeCatalog(
            {MakeMode(1920, 1080, 60), interlacedCurrent},
            interlacedCurrent);
    Require(interlacedModes.size() == 2);
    Require(interlacedModes[1].interlaced);
    Require(interlacedModes[1].current);

    return EXIT_SUCCESS;
}
