# Fasttracker II (FT2) Plugin

<img width="4112" height="2658" alt="image_2025-12-29_01-04-36" src="https://github.com/user-attachments/assets/572e056a-48d6-42e2-b7dc-0b37e6ab08e2" />

This is a full rewrite of [8bitbubsy's standalone ft2-clone](https://github.com/8bitbubsy/ft2-clone) to a multi-instance aware VST3/AU/LV2 plugin by [Blamstrain/TPOLM](https://blamstrain.com). This version removes SDL and uses JUCE as the plugin framework.

Credit goes to [8bitbubsy](https://16-bits.org) for his work on the port from the original DOS version, on which the plugin is based on. **If you wish to donate, please consider supporting:**

- [8bitbubsy](https://16-bits.org/donate.php) for his work on the standalone version.
- [Transgender Europe (EU)](https://www.tgeu.org/), the largest pro-transgender rights organization in Europe.
- [Mermaids (UK)](https://mermaidsuk.org.uk/), helps trans, non-binary and gender-diverse kids and their families in the UK.
- [The Trevor Project (US)](https://www.thetrevorproject.org/), helps suicide prevention for LGBTQIA+ youth in the US.
- [Doctors Without Borders (MSF)](https://www.msf.org/), helps provide medical aid to those in need internationally.

## Plugin-specific features

- DAW sync capabilities; sync BPM, transport and position from DAW host. Can also disable Fxx speed changes for "perfect" sync. In this mode, you can choose between speeds 6 and 3.
- Module state saves with the plugin in your DAW session (no need to save/load modules manually, though still recommended for backup).
- Multi-instance support, including per-instance config.
- Multi-output support (can map any of the 32 channels to 15 busses plus a main mix).
- MIDI input and output support, including aftertouch, bender and mod wheel recording.
- Recycle-style sample slicer for slicing breaks.
- Drag and drop modules or samples to the plugin (including multiple samples at the same time).
- Right-click drag in instrument editor to "stretch" envelope points without having to move all of them.
- Right-click in envelope editor to move all points to the right of the selected point.
- Shift-click to add points on a line in envelope editor, cmd/alt-click to remove a point.

[Please check the changelog for changes and new features.](https://github.com/juho/ft2-plugin/blob/master/plugin/CHANGELOG.md)

### XM2/XI2 format (beta)

Includes an **OPTIONAL** "XM2 mode", enabled via the config, which extends the existing XM format for some modern nice-to-haves:

- Stereo and natively supported hi-res samples.
- Curved (log/exp) envelopes + envelope length multiplier.
- Max. 96 samples in an instrument.
- New resampler / bitcrusher in S.E.Ext.

Existing XM support is unchanged. You can convert an XM2 module into XM (losing new features) and vice versa.

### Keyboard shortcuts:

Most of the original keyboard shortcuts work as they are, however some have been changed for ease of use, mainly:

**Pattern editor clipboard (cut/copy/paste):**

| Scope | Mac | Windows/Linux |
|-------|-----|---------------|
| Block (selection) | Cmd+C/X/V | Ctrl+C/X/V |
| Track (column) | Cmd+Shift+C/X/V | Ctrl+Shift+C/X/V |
| Pattern (all) | Cmd+Option+C/X/V | Ctrl+Alt+C/X/V |

**Volume scale/fade:**

| Scope | Mac | Windows/Linux |
|-------|-----|---------------|
| Block | Option+V | Alt+V |
| Track | Shift+V | Shift+V |
| Pattern | Ctrl+V | (use block after marking all) |

For the rest, please see the in-plugin help.

## Download

Download the latest version from the [releases page](https://github.com/juho/ft2-plugin/releases).

### Mac users

The plugins aren't signed, so you need to:

1. Load the plugin, you'll get a warning message
2. Open System Settings > Privacy & Security
3. Scroll down to find Fasttracker II and click "Allow anyway"

OR run:

(change these to ~Library if you installed to your home directory)

VST3:
```
sudo xattr -r -d com.apple.quarantine "/Library/Audio/Plug-Ins/VST3/Fasttracker II.vst3"
```

AU:
```
sudo xattr -r -d com.apple.quarantine "/Library/Audio/Plug-Ins/Components/Fasttracker II.component"
```

## License

Same as standalone version: BSD 3-Clause License.
