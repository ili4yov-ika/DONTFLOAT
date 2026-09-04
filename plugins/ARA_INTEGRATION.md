# ARA (Audio Random Access) — how Melodyne works, and the DONTFLOAT roadmap

## What ARA is

ARA is an extension **on top of** the normal plug‑in APIs (VST3 / AU / AAX / CLAP)
created by Celemony for Melodyne. A plain audio plug‑in only ever sees the small
real‑time buffers the host streams to `process()`. ARA adds two things:

1. **Random access to the whole track audio** — the plug‑in can read *any* range
   of the host's audio files at will (not just the current block), so it can do
   offline analysis and non‑real‑time editing (pitch/time) like a sample editor.
2. **A bi‑directional "musical information channel"** — host and plug‑in exchange
   tempo map, bar/beat timeline, key/scale, chords and notes, and the plug‑in's
   edits (time‑stretch, pitch moves) are mapped back onto the host arrangement so
   the **DAW's own track waveform and playhead reflect the plug‑in's edits**.

### ARA model graph (Celemony ARA_SDK / JUCE `ARADocumentController`)

```
ARADocument
 ├─ ARAMusicalContext        (tempo map, bar/beat, key/chords)
 ├─ ARAAudioSource           (a host audio file — random-access readable)
 │   └─ ARAAudioModification  (the plug-in's non-destructive edit of a source)
 │        └─ ARAPlaybackRegion (a region on the timeline; what actually plays)
 └─ ARARegionSequence        (a track = ordered playback regions)
```

- The host creates `ARAAudioSource`s for the track's audio and lets the plug‑in
  read samples through an **audio reader** (random access).
- The plug‑in owns `ARAAudioModification` / `ARAPlaybackRegion`; when the user
  drags a note or a stretch marker, the plug‑in updates the playback region's
  time map. The host re‑renders through the plug‑in's `PlaybackRenderer` and
  **redraws the track waveform** using the region's transformation — this is why,
  in Melodyne + an ARA host, moving a marker changes the DAW waveform.
- Playhead / transport is shared through the host's timeline + playback region
  mapping, so the plug‑in caret and the DAW caret are always in sync.

Requires: the **Celemony ARA_SDK** (Apache‑2.0 API headers + library) *and* an
**ARA‑capable host** (Studio One, REAPER, Cubase/Nuendo, Logic, Cakewalk, …).

## Where DONTFLOAT is today (no ARA SDK / no ARA host available)

Neither the ARA SDK nor an ARA host is available in this environment, so full ARA
cannot be built or tested here. Instead the plug‑ins approximate the ARA workflow
with the facilities that *are* available in plain CLAP/LV2/VST3:

| ARA capability | DONTFLOAT approximation today | File |
|----------------|-------------------------------|------|
| Random access to track audio | Capture the host audio streamed to `process()` into the plug‑in session (`appendHostFrames`), so the editor works on the **DAW track**, not an imported file | `plugins/core/dontfloat_plugin_core.cpp`, format `*_impl.cpp` |
| Musical info channel (tempo/playhead) | Read the host **transport** (`clap_event_transport`) each block and drive the editor | `plugins/clap/dontfloat_clap_plugin_impl.cpp` |
| Synchronized playhead / caret | Transport → editor playback cursor (`setHostPlayheadSeconds`), updated on the GUI thread via the host timer | `plugins/ui/*editor*.cpp` |
| Host reflects plug‑in edits (waveform) | **Not possible without ARA** — a plain plug‑in cannot repaint the host's track. The plug‑in's own editor waveform reflects marker moves; the processed result is available on the plug‑in's audio output / export | see roadmap |

### Threading note (important, ARA‑style)

`process()` runs on the audio thread and must not touch Qt. It only appends audio
and stores the transport playhead in atomics. A host timer callback
(`clap.timer-support`, or the LV2 UI idle) runs on the GUI thread, pumps Qt, and
pushes the captured audio + playhead into the editor. This mirrors how ARA keeps
the realtime renderer separate from the editor/controller.

## Roadmap to real ARA

1. **Add the ARA_SDK** as an optional dependency (`DONTFLOAT_ARA_SDK_ROOT`), gated
   like the VST3 SDK, and vendor the CLAP/VST3 ARA companion headers.
2. **Document controller**: implement an `ARADocumentControllerSpecialisation`
   mapping our `TrackToolSession` onto `ARAAudioSource`/`AudioModification`/
   `PlaybackRegion`. Read the track audio via an ARA audio reader instead of
   `appendHostFrames` (true random access, full track instantly).
3. **Content readers**: expose our BPM/beat grid, key and detected notes as ARA
   content (tempo, bar/beat, key/scale, notes) so the host can use them.
4. **Playback renderer**: render playback regions with the marker time‑stretch /
   pitch edits so the host plays — and, via the region time map, **draws** — the
   edited audio. This is what makes "move a marker → DAW waveform changes".
5. **Editor**: bind the existing waveform / pitch‑grid editors to the ARA model
   graph; keep the CLAP‑transport caret sync as the non‑ARA fallback.

Until step 4 lands, moving markers changes the waveform **inside the plug‑in
editor** (and the exported/processed audio); reflecting it on the host's own
track requires the ARA playback‑region mapping above.
