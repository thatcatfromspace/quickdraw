# Windows hardware qualification matrix

Run this matrix against the MSVC Release build before publishing Quickdraw.
Record the Windows build, GPU, driver version, monitor connection, and result for
every test session.

## Baseline lifecycle

- Launch with one monitor and confirm that no normal application window appears.
- Confirm that a second launch activates the first instance and exits.
- Confirm that the tray icon begins in notification-area overflow.
- Restart Explorer and confirm that the tray icon returns once, without a ghost.
- Enable and disable **Start with Windows** and verify the current-user Run entry.
- Move the executable, launch it, and verify that an enabled startup entry repairs
  its path.
- Sleep and resume, then reconnect a display and verify that the menu refreshes.
- Open the tray menu while disconnecting a monitor and verify that the stale menu
  closes without applying an action.

## Display inventory

- One internal laptop display.
- Two extended displays with different resolutions and refresh rates.
- Three extended displays.
- Two identical monitor models on different connectors.
- Mixed HDMI, DisplayPort, USB-C, and DisplayLink where available.
- A portrait display; verify that Quickdraw does not alter rotation.
- Mixed 100%, 125%, 150%, and 200% DPI scaling.
- 59/60, 120, 144, 165, 240, and higher refresh modes where supported.
- Mirrored displays; verify that mode actions are read-only.
- Remote Desktop; verify that display-changing actions are read-only.

## Single-monitor transactions

- Apply a supported resolution change and keep it.
- Apply a supported refresh-only change and keep it.
- Revert manually.
- Allow the countdown to expire and verify restoration.
- Close the confirmation dialog and verify restoration.
- Change one monitor and verify that the other monitor remains unchanged.
- Disconnect the target during confirmation and inspect recovery behavior.
- Kill Quickdraw during confirmation, relaunch it, and verify journal recovery.

## Presets

- Save, rename, update, and delete a preset.
- Restart Quickdraw and verify preset persistence and alphabetical ordering.
- Apply a two-monitor preset and keep it.
- Apply a two-monitor preset and allow automatic rollback.
- Disconnect a required monitor and verify that the preset is refused.
- Connect an additional monitor and verify that it remains unchanged.
- Test identical monitor models and confirm that ambiguous matches are refused.
- Make a stored mode unavailable and confirm that the preset is refused before
  any display changes.

## Failure and recovery

- Corrupt `config.bin`; verify safe defaults and a clear warning.
- Corrupt `pending-transaction.bin`; verify that changes are blocked and the file
  remains available for inspection.
- Deny write access to the local data directory and verify that no display change
  begins without a recovery journal.
- Confirm that a failed or partial stage restores the prior registry modes.
- Confirm that `Quickdraw.log` rotates after approximately 256 KB.

## Release measurements

- Idle CPU remains effectively zero for five minutes.
- Record private bytes and working set after one minute idle.
- Record cold launch time until the tray icon appears.
- Confirm that the executable is x64, GUI subsystem, Per-Monitor V2 aware, and
  requests `asInvoker` privileges.
- Run the release archive through Windows Defender and the chosen signing flow.
