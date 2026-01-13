# Fasttracker II (FT2) Plugin

<img width="4112" height="2658" alt="image_2025-12-29_01-04-36" src="https://github.com/user-attachments/assets/572e056a-48d6-42e2-b7dc-0b37e6ab08e2" />

This is a fork of [8bitbubsy's standalone ft2-clone](https://github.com/8bitbubsy/ft2-clone), including *a full rewrite* of the code to a multi-instance aware VST3/AU/LV2 plugin by [Blamstrain/TPOLM](https://blamstrain.com).

**Standalone builds are not a concern in this repository.** If you're looking for the standalone version, you can find it [here](https://github.com/8bitbubsy/ft2-clone). Standalone code is kept around so that we can compare and integrate changes to the plugin from the standalone version.

**All credit goes to [8bitbubsy](https://16-bits.org) for his tireless work on the port from original, on which the plugin is based on. If you wish to donate, please consider supporting:**

- [8bitbubsy](https://16-bits.org/donate.php) for his work on the standalone version.
- [Transgender Europe (EU)](https://www.tgeu.org/), the largest pro-transgender rights organization in Europe.
- [Mermaids (UK)](https://mermaidsuk.org.uk/), helps trans, non-binary and gender-diverse kids and their families in the UK.
- [The Trevor Project (US)](https://www.thetrevorproject.org/), helps suicide prevention for LGBTQIA+ youth in the US.
- [Doctors Without Borders (MSF)](https://www.msf.org/), helps provide medical aid to those in need internationally.

## Plugin-specific features

- DAW sync capabilities; sync BPM, transport and position from DAW host. Can also disable Fxx speed changes for "perfect" sync.
- Module state saves with the plugin in your DAW session (no need to save/load modules manually, though still recommended for backup)
- Multi-instance support, including per-instance config
- Multi-output support (can map any of the 32 channels to 15 busses plus a main mix)
- MIDI input and output support, including aftertouch, bender and mod wheel recording.
- Drag and drop modules or samples to the plugin
- Right-click drag in instrument editor to "stretch" envelope points without having to move all of them

### Under active development!

Note that there are lots of updates coming at a rapid pace right now, and I am taking input for reasonable feature requests and of course want to fix any bugs. If there is a newer version than the one you have already downloaded (look at the "About" screen for your plugin's version), issues might have been fixed already. [Please check the changelog for changes.](https://github.com/juho/ft2-plugin/blob/master/plugin/CHANGELOG.md)

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
