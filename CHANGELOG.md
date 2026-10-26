# Changelog

All notable changes to Fuzzion-5000 are documented here.

## [1.0.0] — 2026-10-26

### Added

- Transistor fuzz DSP core: pre-emphasis, biased asymmetric two-stage clipper,
  interstage coupling, full-wave octave rectifier, tilt/mid/low-pass tone stack,
  input-keyed gate, iron output transformer and DC blocking.
- Four characters: Silicon, Germanium, Velcro and Doom.
- Thirteen host parameters including bias starve, octave, mix, gate enable,
  tone-stack enable, iron enable and bypass.
- Ten factory presets and full A/B state serialisation.
- Lock-free waveform, saturation, gate and level telemetry.
- 4x oversampling with dry/wet summed at the oversampled rate so bypass latency
  never changes.
- Sleek futuristic studio-hardware UI with a central glowing waveform scope,
  transfer/bias scope, knurled knobs, glass sliders and glowing tactile buttons.
- Offline DSP test suite, VST3 host smoke test and headless 2x screenshot renderer.
- GitHub Actions CI for Windows x64, Linux x64 and macOS Universal with automated
  release packaging.
