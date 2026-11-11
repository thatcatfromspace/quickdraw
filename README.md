<p align="center">
  <img src="resources/branding/quickdraw-icon-256.png" width="128" height="128" alt="Quickdraw app icon">
</p>

<h1 align="center">Quickdraw</h1>

<p align="center">Switch monitor resolution and refresh rate from the Windows tray.</p>

<p align="center">
  <a href="https://github.com/thatcatfromspace/quickdraw/releases/latest"><strong>Download the installer</strong></a>
  ·
  <a href="https://github.com/thatcatfromspace/quickdraw/releases/latest/download/Quickdraw.exe">Portable version</a>
</p>

<p align="center">
  <img alt="Windows 10 and 11" src="https://img.shields.io/badge/Windows-10%20%7C%2011-0078D4?logo=windows">
  <a href="https://github.com/thatcatfromspace/quickdraw/actions/workflows/ci.yml"><img alt="Build status" src="https://github.com/thatcatfromspace/quickdraw/actions/workflows/ci.yml/badge.svg"></a>
  <a href="https://github.com/thatcatfromspace/quickdraw/releases/latest"><img alt="Latest release" src="https://img.shields.io/github/v/release/thatcatfromspace/quickdraw"></a>
</p>

Quickdraw is a small native Windows utility for people who change display modes often. It handles each active monitor independently and can save a complete multi-monitor setup as a named preset. The app stays out of the way in the notification-area overflow when you are finished.

## Download

Open the [latest GitHub Release](https://github.com/thatcatfromspace/quickdraw/releases/latest) and download `Quickdraw-Setup-<version>.exe`. The wizard adds Quickdraw to the Start menu and can launch it when you sign in. Installation stays within your Windows account and does not request administrator access.

Prefer a standalone file? Download [`Quickdraw.exe`](https://github.com/thatcatfromspace/quickdraw/releases/latest/download/Quickdraw.exe) and place it wherever you keep portable apps.

The current installer and executable are unsigned. Windows SmartScreen may show an **Unrecognized app** warning on first launch. Each release includes SHA-256 checksums for download verification.

## Pick a mode in seconds

1. Launch `Quickdraw.exe`.
2. Find the Quickdraw icon in the notification-area overflow.
3. Choose a monitor, resolution, and refresh rate.
4. Keep the change before the countdown ends.

If the new mode is unusable, wait. Quickdraw restores the previous display settings automatically.

## What Quickdraw handles

- Separate resolution and refresh-rate menus for every active monitor
- Named presets for complete multi-monitor setups
- A timed confirmation step with automatic rollback
- Recovery after an interrupted or unconfirmed display change
- Optional launch at sign-in for the current Windows user
- A single portable executable with no background service
- Native Win32 code with no managed runtime

## Presets

Open **Presets > Save current configuration** to name the current setup. A preset records the resolution and refresh rate of each active monitor. When you apply it later, Quickdraw matches the saved physical displays and leaves unrelated active monitors alone.

## Safety and recovery

Quickdraw writes a small recovery journal before changing a display mode. It removes the journal after you accept the change or after a successful rollback.

If Quickdraw or Windows exits while confirmation is pending, the journal remains. On the next launch, Quickdraw matches the recorded physical monitors and restores their previous modes. When a monitor is missing or ambiguous, it pauses further changes instead of guessing.

## Requirements

- Windows 10 or Windows 11
- 64-bit x86 processor

## Build from source

Install Visual Studio 2022 with the **Desktop development with C++** workload and CMake 3.24 or newer. From a Visual Studio Developer PowerShell, run:

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-release
ctest --test-dir build/windows-msvc -C Release --output-on-failure
```

The executable is written to `build/windows-msvc/Release/Quickdraw.exe`.

To create a versioned ZIP and SHA-256 checksum under `out`, run:

```powershell
./scripts/build-release.ps1
```

The release script also builds and tests the per-user installer. Install Inno Setup 6 or 7 before running it. Installer behavior and unattended deployment options are documented in [docs/installer.md](docs/installer.md).

## Local data and privacy

Quickdraw runs entirely on your PC. It reads the Windows display configuration and stores its own files under `%LOCALAPPDATA%\Quickdraw`:

- `config.bin` stores application settings and presets.
- `pending-transaction.bin` supports recovery while a display change awaits confirmation.
- `Quickdraw.log` and `Quickdraw.log.old` are bounded diagnostic logs.

The optional startup entry lives in the current user's Windows `Run` registry key. Quickdraw does not collect telemetry or send data over the network.

## Contributing

Bug reports and focused pull requests are welcome. Read [CONTRIBUTING.md](CONTRIBUTING.md) for the build, test, and review expectations. Windows hardware qualification steps live in [docs/windows-test-matrix.md](docs/windows-test-matrix.md).

## License

Quickdraw is released under the [MIT License](LICENSE).
