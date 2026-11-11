# Quickdraw installer specification

This document defines the installation contract for Quickdraw 1.1.0.

## Distribution

Each release publishes these Windows x64 artifacts:

- `Quickdraw-Setup-<version>.exe` — per-user installation wizard and the primary download.
- `Quickdraw.exe` — standalone portable executable.
- `Quickdraw-<version>-win-x64.zip` — portable executable with documentation.
- SHA-256 checksum files for every downloadable binary or archive.

Release signing is intentionally outside the 1.1.0 installer scope. Signing must
be added after a trusted publisher identity and certificate provider are chosen.

## Installation contract

- Install for the current user without requesting administrator privileges.
- Default to `%LOCALAPPDATA%\Programs\Quickdraw` and permit another per-user path.
- Support Windows 10 and Windows 11 on x64-compatible systems.
- Register Quickdraw in the current user's Apps & features list.
- Create a Start menu shortcut.
- Offer unchecked tasks for a desktop shortcut and launch at sign-in.
- Offer to launch Quickdraw after an interactive installation.
- Never launch Quickdraw automatically during a silent installation.
- Block installation and uninstallation while Quickdraw's single-instance mutex
  exists. The installer must not force-close the app during a pending display
  transaction.

## Startup contract

The startup task uses the same entry as Quickdraw's Settings dialog:

- Key: `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`
- Value: `Quickdraw`
- Data: the quoted absolute path to the installed `Quickdraw.exe`

On first install, launch at sign-in is disabled by default. On reinstall or
upgrade, the wizard reflects the actual registry state. `/TASKS` and
`/MERGETASKS` remain authoritative for unattended deployment.

Uninstall removes the startup value only when it points to the installation
being removed. An entry that points to a separate portable copy is preserved.

## Upgrade and uninstall contract

- A permanent Inno Setup App ID identifies all versions of Quickdraw.
- Upgrades reuse the previous installation folder and replace the application
  binary in place.
- User settings, presets, recovery data, and logs under
  `%LOCALAPPDATA%\Quickdraw` are never packaged or removed by Setup.
- Uninstall removes installer-owned files, shortcuts, and registration.
- Reinstalling must not reset the startup preference changed from inside the app.

## Silent deployment

The installer supports Inno Setup's standard `/SILENT`, `/VERYSILENT`,
`/NORESTART`, `/TASKS`, and `/MERGETASKS` switches. Examples:

```powershell
# Install without enabling startup.
Quickdraw-Setup-1.1.0.exe /VERYSILENT /NORESTART

# Install and enable startup.
Quickdraw-Setup-1.1.0.exe /VERYSILENT /NORESTART /MERGETASKS="startup"

# Upgrade and explicitly disable startup.
Quickdraw-Setup-1.1.0.exe /VERYSILENT /NORESTART /MERGETASKS="!startup"
```

## Release acceptance

The Windows pipeline must prove all of the following before publication:

1. Configure and build Quickdraw with MSVC.
2. Pass all native logic tests.
3. Compile the installer with Inno Setup.
4. Install silently into a temporary per-user directory.
5. Verify the installed executable, Start menu shortcut, optional desktop
   shortcut, uninstall registration, and startup value.
6. Reinstall while preserving the current startup state.
7. Explicitly disable startup through an installer task selection.
8. Uninstall and verify removal of installed files and installer-owned entries.
9. Preserve `%LOCALAPPDATA%\Quickdraw` and any startup value that targets a
   separate portable copy.
10. Publish checksums generated after the final binaries and archives exist.
