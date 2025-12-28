# ft2-plugin

This is a fork of [8bitbubsy's standalone ft2-clone](https://github.com/8bitbubsy/ft2-clone), including *a full rewrite* of the code to a multi-instance aware VST3/AU/LV2 plugin by [Blamstrain/TPOLM](https://blamstrain.com) with DAW sync capabilities.

**Consider the plugin ALPHA software.** I've tried to match standalone behavior as closely as possible. As it's a rewrite, there are still bugs and some divergence from the standalone version, however there should be no show-stopping crash bugs at this stage. If you find any issues **IN THE PLUGIN**, please report them with thorough reproduction steps.

**Standalone builds are not a concern in this repository.**

All credit goes to [8bitbubsy](https://16-bits.org) for his tireless work on the port from original, on which the plugin is based on. The plugin version is in its own folder so further updates from the standalone version can be pulled in, compared and integrated.

## Changes from standalone version

- General: Compiles as VST2/VST3/AU/LV2 plugin using JUCE framework.
- General: DAW sync to BPM and transport (toggleable in config).
- General: Only XM, MOD and S3M modules are supported.
- General: Some config settings do not make sense in a plugin, so they have been disabled.
- Graphics: Uses OpenGL instead of SDL.
- Disk op: WAV save option is removed (if you need to save a WAV, use the standalone version).
- Disk op: Rename and delete disk operation buttons are removed.
- Config: Settings for DAW transport, BPM and position sync have been added.

## License

Same as standalone version: BSD 3-Clause License.
