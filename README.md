# ft2-plugin

<img width="4112" height="2658" alt="image_2025-12-29_01-04-36" src="https://github.com/user-attachments/assets/572e056a-48d6-42e2-b7dc-0b37e6ab08e2" />

This is a fork of [8bitbubsy's standalone ft2-clone](https://github.com/8bitbubsy/ft2-clone), including *a full rewrite* of the code to a multi-instance aware VST3/AU/LV2 plugin by [Blamstrain/TPOLM](https://blamstrain.com) with DAW sync capabilities.

**Consider the plugin ALPHA software.** If you find any issues, please report them with thorough reproduction steps. I've tried to match standalone behavior as closely as possible. As it's a rewrite, there are still bugs and some divergence, however there should be no show-stopping crash bugs at this stage.

**Standalone builds are not a concern in this repository.** If you're looking for the standalone version, you can find it [here](https://github.com/8bitbubsy/ft2-clone). Standalone code is kept around so that we can compare and integrate changes to the plugin from the standalone version.

All credit goes to [8bitbubsy](https://16-bits.org) for his tireless work on the port from original, on which the plugin is based on.

## Current limitations

- No multi-output support (yet)
- No MIDI support (yet)

## Changes from standalone version

- General: Compiles as VST2/VST3/AU/LV2 plugin using JUCE framework.
- General: Only XM, MOD and S3M modules are supported.
- General: Some config settings do not make sense in a plugin, so they have been disabled.
- Graphics: Uses OpenGL instead of SDL.
- Disk op: WAV save option is removed (if you need to save a WAV, use the standalone version).
- Disk op: Rename and delete disk operation buttons are removed.
- Config: Settings for DAW transport, BPM and position sync have been added.
- Config: Reset config resets to factory defaults. Load and save config saves to user's home directory. Instances have their own config which can override any of the settings.

## License

Same as standalone version: BSD 3-Clause License.
