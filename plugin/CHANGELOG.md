# Changelog for Fasttracker II Plugin

## 1.0.19 (Jan 1, 2026)

- Fix: Hook up directory sorting priority buttons in config.
- Fix: Hook up overwrite warning button in config.
- Visual: Remove some superfluous text from config.
- Fix: Hook up Amiga frequency slides button in config.

## 1.0.18 (Dec 31, 2025)

- Fix: Clean up code in help.
- Visual: Remove some superfluous text from config.
- Fix: Missing quantize up/down buttons in config.
- Fix: Hook up 'Sample "Cut to buffer"', 'Pattern "Cut to buffer"', and 'Kill voices at music stop' buttons in config.

## 1.0.17 (Dec 31, 2025)

- Feat: Add automatic update checking with a notification dialog.
- Config: Bump config version to 2, defaulting the update check to enabled.
- Help: Replace "Known Bugs" section with "Plugin" section.
- Help: Remove "Problems/FAQ" section.

## 1.0.16 (Dec 30, 2025)

- Fix: Use double buffering and synchronized rendering to avoid visual glitches.
- Feat: Separate plugin help files from standalone.

## 1.0.15 (Dec 30, 2025)

- Fix: Reset pattern editor scrollbar position when loading modules.

## 1.0.14 (Dec 30, 2025)

- Fix: Only apply BPM effect if not syncing BPM from DAW.

## 1.0.13 (Dec 30, 2025)

- Fix: Completely separate BPM sync from transport sync (was coupled before).
- Visual: Move MIDI input settings around a little bit.
- Visual: Add disabled state to UI widgets.

## 1.0.12 (Dec 30, 2025)

- Visual: Remove some superfluous text from config.
- Feat: Trigger patterns with MIDI notes (change in MIDI settings).
- Fix: Quickly ramp volume down when stopping instead of setting to 0 immediately to avoid clicks.

## 1.0.11 (Dec 29, 2025)

- Fix: Better config buttons placement.
- Fix: MIDI in note off works correctly.
- Fix: Textbox background stays same size no matter the state.

## 1.0.10 (Dec 29, 2025)

- Feat: Enable Extend GUI mode.
- Fix: Drag-drop module in extended mode goes back to normal pattern editor.

## 1.0.9 (Dec 29, 2025)

- Fix: Better multi-instance access.
- Fix: Silence multi-out channels when not playing.
- Fix: Better UI toggling logic.

## 1.0.8 (Dec 29, 2025)

- Fix: unify mod/s3m/xm loading code.

## 1.0.7 (Dec 29, 2025)

- Feat: Support multiple outputs coming into the DAW.

## 1.0.6 (Dec 29, 2025)

- Feat: Persists module state.
- Feat: Configuration story is now more coherent- there's a global versioned config which can be overridden by per-instance configs.

## 1.0.5 (Dec 29, 2025)

- Feat: Support modules with odd-numbered channels properly.

## 1.0.4 (Dec 29, 2025)

- Fix: Implement instrument/sample clearing (with numpad period key).
- Feat: Handle mouse wheel in help/disk operations screens.
- Version in "About" now comes from CMake.
- Fix: Show "Not enough memory!" dialog when trim calculation fails.

## 1.0.3 (Dec 29, 2025)

- Fix: Saving modules now works correctly.

## 1.0.2 (Dec 29, 2025)

- Fix: Radio button flicker when clicking on them.
- Sync: Timemap calculates song infinite loops correctly.
- Sync: Timemap invalidates when the "Allow Fxx speed changes" option is changed in config.
- Sync: Song speed is locked to 6 when "Allow Fxx speed changes" is disabled even when loading modules.
- Sync: Restore speed/BPM if "Allow Fxx speed changes" is enabled after disabling it.

## 1.0.1 (Dec 29, 2025)

- Sync: Timemap now gets invalidated when playlist is edited, and when pattern length changes.

## 1.0.0 (Dec 28, 2025)

- Initial release