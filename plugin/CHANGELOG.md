# Changelog for Fasttracker II Plugin

## 1.1.2 (Jan 24, 2026)

- Fix: Marking with alt+arrows works correctly.
- Fix: Clear sample immediately redraws sample list so the name gets emptied as well.
- Feat: Help text updates.

## 1.1.1 (Jan 24, 2026)

- Fix: Keyboard handling fixes (note-off/add/delete and other hotkeys)

## 1.1.0 (Jan 22, 2026)

- Feat: New XM2 format for new features (beta), which only work with this plugin. Plugin can convert between XM and XM2 formats, with destructive changes when going from XM2 to XM. **THIS IS OPT-IN; check the "Enable XM2 features" checkbox in config to enable it.** Songs that use XM2 features use .XM2 file extension when saving. **XM format support is unchanged.**
- Feat: New XI2 format for new instrument features (beta).

### XM2/XI2-specific updates:

#### Sample editor:

- Feat: Stereo sample loading, processing and editing.
- Feat: Change sample rate and bit depth (click the "Format" button).
- Feat: Convert from stereo to mono, and vice versa (click the "Chns" button).
- Feat: Redone resample panel in S.E.Ext: resamples sample rate and bit depth while keeping format as-is.

#### Instruments:

- Feat: Max. 96 samples (as many keys as there are available on the instrument editor piano) in an instrument.
- Feat: Internal sample format supports higher sample rates (48k, 96k) and bit depths (24, 32f) without downsampling.
- Feat: Sample rate and bit depth are now stored with the sample. This means that in XM2 mode, C-4 is always set to the sample rate and finetune is not used for setting it. Relative note is used to set the root note offset from C-4.
- Feat: Curved exp/log envelopes. Click and drag on an envelope line segment to adjust the curvature. Double click to reset to linear.
- Feat: Length (Len.) value in envelopes; 1 is normal length, 2 is double length, etc.
- UI: Add/del buttons have been moved to the same line as the predef buttons to give space for the new Length value.

### General updates:

#### Sample editor:

- Feat: Recycle-style sample slicer. Slices samples into multiple parts (as samples or instruments), with segment and playback preview. XM mode supports up to 16 slices. In XM2 mode, up to 96.
- Feat: Faster waveform drawing.
- Fix: Playhead correctly picks up the latest note playing.
- Fix: Minor scope nits.

#### Instrument editor:

- Feat: Shift-click on an envelope line segment to insert a point.
- Feat: Alt/cmd-click on an envelope line segment to delete a point.
- Fix: Loading of WAV samples now correctly infers the sample rate and finetuning.
- Fix: Finetune value uses decimal, not hex.

#### Other:

- Feat: New sample loaders (AIFF, IFF, RAW, SND, FLAC).
- Feat: Plugin is able to handle multiple samples being dropped.
- Feat: Hooked up hotkeys for Ctrl+I (instrument editor), Ctrl+E (extended sample editor), Ctrl+S (sample editor), Ctrl+P (pattern editor), Ctrl+M (inst.ed.ext), Ctrl+X (reset), Ctrl+Z (extended pattern editor).
- Fix: Regression in sinc8 interpolation.
- Fix: Keyjazz now correctly plays notes while holding down keys when not editing.
- Fix: Textbox caret position is movable with left/right/home/end.
- Fix: (Un)soloing by left+right click on scope block works correctly.

## 1.0.57 (Jan 14, 2026)

- Fix: scrolling in sample and instrument editor with mousewheel.

## 1.0.56 (Jan 13, 2026)

- Fix: Better key handling. Add key shortcuts for copy paste:

    | Scope | Mac | Windows/Linux |
    |-------|-----|---------------|
    | Block (selection) | Cmd+C/X/V | Ctrl+C/X/V |
    | Track (column) | Cmd+Shift+C/X/V | Ctrl+Shift+C/X/V |
    | Pattern (all) | Cmd+Option+C/X/V | Ctrl+Alt+C/X/V |

- Fix: Shift-scrollwheel scrolls horizontally. Scrollwheel also works in pattern editor now.

## 1.0.55 (Jan 13, 2026)

- Feat: "Stretch" envelopes with right-click drag in instrument editor.

## 1.0.54 (Jan 13, 2026)

- Feat: Add `Reset` button to I/O routing tab in config.

## 1.0.53 (Jan 11, 2026)

- Feat: Add a `legacy` build target for macOS 10.14+ (Mojave).

## 1.0.52 (Jan 10, 2026)

- Refactor: Better state tracking in sample editor, trim and about screen.
- Fix: Mouse wheel handling works properly with small deltas.

## 1.0.51 (Jan 10, 2026)

- Fix: Saving of user-defined colours in config works properly.

## 1.0.50 (Jan 10, 2026)

- Refactor: State tracking in widgets.

## 1.0.49 (Jan 10, 2026)

- Fix: Better textbox drawing.

## 1.0.48 (Jan 9, 2026)

- Fix: Restore view state after coming back from extended pattern editor.

## 1.0.47 (Jan 9, 2026)

- Feat: code cleanup pass.

## 1.0.46 (Jan 9, 2026)

- Fix: Drive buttons in Diskop on Windows.

## 1.0.45 (Jan 6, 2026)

- Fix: default disk op to home folder.

## 1.0.44 (Jan 6, 2026)

- Feat: Allocate empty instrument when clicking on I.E.Ext.
- Fix: Enable MIDI output in JUCE.
- Visual: Config cleanup.
- Feat: Aftertouch, bender and mod wheel midi input support + related config settings.
- Fix: Hide overlay buttons when loading a module.

## 1.0.43 (Jan 5, 2026)

- Fix: Pattern playback state redrawing on top of Nibbles.
- Feat: Add Github button and automatic update checking in the About screen.

## 1.0.42 (Jan 5, 2026)

- Feat: Auto-detect Speed 3 on module load, use it if Fxx speed changes are disabled. Otherwise use Speed 6.

## 1.0.41 (Jan 5, 2026)

- Feat: Add drive buttons on Windows to disk operation screen.
- Feat: Show "original" BPM in parentheses when DAW sync is active.
- Feat: Update "original" BPM whenever Fxx BPM changes are encountered in pattern data.
- Feat: Allow depressing pushbuttons for 'tabs', can switch speed between 6 and 3 when Fxx speed changes are disabled.

## 1.0.40 (Jan 4, 2026)

- Fix: Remove debug symbols, fix MSVC not generating exit condition in for loop for instrument editor.

## 1.0.30 (Jan 3, 2026)

- Fix: ABI structs between C/C++, add belt and suspenders.

## 1.0.29 (Jan 3, 2026)

- Fix: MSVC packing guards.

## 1.0.28 (Jan 3, 2026)

- Cleanup: Structs in instrument editor.

## 1.0.27 (Jan 3, 2026)

- Refactor: Derive editor from inst->ui.

## 1.0.26 (Jan 3, 2026)

- Refactor: Move function pointers to use *inst parameter.

## 1.0.25 (Jan 3, 2026)

- Fix: Set ft2_instr_ed_set_instance before drawing in instrument editor to avoid race condition.

## 1.0.23 (Jan 3, 2026)

- Fix: sanitize instrument before loading.

## 1.0.22 (Jan 3, 2026)

- Fix: Use macros for bounds checking, update inst. ed. buttons callbacks.

## 1.0.21 (Jan 3, 2026)

- Fix: Heap corruption in instrument editor.

## 1.0.20 (Jan 2, 2026)

- Fix: Plugin state serialization is now forward-compatible (won't destroy song between versions).
- Fix: Regressions in instrument editor.

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