# Contributing to Quickdraw

Thanks for helping improve Quickdraw. Focused issues and pull requests are easiest to review and test.

## Report a bug

Include these details when they are available:

- Windows version and build
- GPU and driver version
- Monitor models and connection types
- Steps that reproduce the problem
- Expected and actual behavior
- Relevant lines from `%LOCALAPPDATA%\Quickdraw\Quickdraw.log`

Remove personal paths or device details you do not want to share.

## Build and test

Install Visual Studio 2022 with the **Desktop development with C++** workload and CMake 3.24 or newer. Then run these commands from a Visual Studio Developer PowerShell:

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-release
ctest --test-dir build/windows-msvc -C Release --output-on-failure
```

For changes that affect display switching, complete the relevant checks in [docs/windows-test-matrix.md](docs/windows-test-matrix.md).

Installer changes also require Inno Setup 6 or 7. Build and exercise the full
install, upgrade, startup, and uninstall lifecycle with:

```powershell
./scripts/build-release.ps1
```

The installer contract and unattended switches are documented in
[docs/installer.md](docs/installer.md).

## Pull requests

- Keep each pull request focused on one change.
- Add or update tests for behavior changes.
- Build with warnings treated as errors.
- Describe hardware testing for display-related changes.
- Avoid committing generated builds, logs, IDE state, or local app data.
