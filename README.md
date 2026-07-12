<h1>OpenMix</h1>

<a href="https://github.com/OpenMix-dev/OpenMix/actions/workflows/build.yml"><img src="https://github.com/OpenMix-dev/OpenMix/actions/workflows/build.yml/badge.svg" alt="Build"></a>
<a href="https://github.com/OpenMix-dev/OpenMix/releases/latest"><img src="https://img.shields.io/github/v/release/OpenMix-dev/OpenMix" alt="Release"></a>
<a href="LICENSE"><img src="https://img.shields.io/github/license/OpenMix-dev/OpenMix" alt="License"></a>

OpenMix is stage mixing software for live theatre. A show is programmed as a cue list; each cue assigns performers to DCAs, sets levels and mutes, and pushes scribble strip names to the console over the network. Playback, fades, and console state all run in OpenMix, so the console itself needs no show-specific programming. It runs on Windows, macOS, and Linux and drives desks from Behringer, Midas, Allen & Heath, Yamaha, and DiGiCo.

## Architecture

```
src/
  core/       show model and engines: Cue, CueList, Show, FadeEngine,
              PlaybackEngine, DCAMapping, actors/roles/ensembles, undo
  protocol/   console abstraction layer (see below)
  midi/       MIDI input: MSC and MTC parsing, control mapping (RtMidi)
  io/         project files, autosave, crash recovery, .tmix import
  app/        application shell, auto-update, external cue software
              clients (QLab, REAPER, SCS, CuePlayer), OSC remote server
  ui/         Qt Widgets UI
tests/        Qt Test suites, one binary per area, run via ctest
libs/         vendored RtMidi
```

The protocol layer is the main extension point:

- `MixerProtocol` is the abstract console interface. One subclass per console family under `protocol/behringer/`, `protocol/allenheath/`, `protocol/yamaha/`, `protocol/digico/`. Wire formats vary per family: OSC over UDP (X32/M32, WING), MIDI over TCP (SQ, GLD), binary TCP (Avantis, dLive, DiGiCo SD), text TCP (Yamaha).
- `MixerCapabilities` describes each model: channel and DCA counts, protocol type, port. Adding a model that speaks an existing protocol is mostly a capabilities entry.
- `ProtocolFactory` maps a `ConsoleType` to a protocol instance.
- `protocol/transport/` holds the shared OSC (liblo) and TCP transports; `protocol/discovery/` implements network auto-discovery with per-family probe strategies.
- `LoopbackProtocol` is an in-process fake console shared by the test suite and dry-run mode, so cue playback is testable without hardware.

## Technical notes

- Show files (`.omproj`) are indented JSON, written by `ProjectFile` (src/io/ProjectFile.cpp). They diff cleanly, so keeping a show in version control works. `.tmix` and `.x32tc` show files can be imported.
- `FadeEngine` runs on a 20 ms timer but computes levels from measured elapsed time, not tick counts, so fades stay on schedule even when the event loop stalls.
- Engines and UI live on the Qt main thread. Background threads exist only at the I/O edges (liblo's OSC server threads, RtMidi's input callback), and both hand their events to the main thread through queued Qt signals.
- `OscRemoteServer` exposes transport control over UDP: `/go`, `/stop`, `/next`, `/prev`, `/cue/goto <n>`, `/ctrl/fadeall`.
- Autosave snapshots the show every 5 minutes; `CrashRecovery` additionally persists playback position and mixer state, so a crash mid-show restores to the current cue, not just the last save.

## License

OpenMix is free software, released under the [GNU General Public License v3](LICENSE).
