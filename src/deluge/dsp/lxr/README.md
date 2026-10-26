# Drum synth engine

The DSP of the drum synth engine, oscillator type `OscType::LXR`, which plays the drum voices of the LXR-02
drum machine. It is ported from the LXR firmware by Erica Synths
(<https://github.com/erica-synths/lxr-desktop-app>, commit `a43cd4f`). See [NOTICE.md](NOTICE.md) for the licence
of this code, and for which other files are derived from the LXR firmware. This project is not made, approved or
endorsed by, or affiliated with, Erica Synths.

## What's here

| File | What | From the LXR firmware |
|---|---|---|
| `components.h/.cpp` | Oscillators, filters, envelopes, transient generator, distortion, LFO | `Core/Src/Soundengine/DSPAudio/`: `Oscillator.c`, `ResonantFilter.c`, `Decay.c`, `SlopeEg2.c`, `snapEg.c`, `transientGenerator.c`, `distortion.c`, `1PoleLp.c`, `lfo.c`; `Core/Inc/Soundengine/MIDI/valueShaper.h` |
| `voice.h/.cpp` | One playing voice: the four voice circuits and the per-block order of work | `DrumVoice.c`, `Snare.c`, `CymbalVoice.c`, `HiHat.c`, `mixer.c`, `modulationNode.c`, `BufferTools.c`, and the parameter scaling in `Core/Src/Soundengine/MIDI/MidiParser.c` |
| `patch.h/.cpp` | The settings of one voice, their ranges, and the init kit's sounds | `Core/Inc/Parameters.h`, `Core/Src/menu/`, `Core/Src/Preset/initKit.c` |
| `tables.h/.cpp` | Wavetables, transient samples, MIDI note frequencies | `wavetable.c`, `transientTables.c`, `Core/Inc/Soundengine/MIDI/MidiNoteNumbers.h` |
| `feature.h` | The `FEATURE_LXR_ENGINE` build switch | |

The LXR renders its six voices into one kit. Here, a `Voice` is one note of one voice circuit, so the Deluge can
play the engine polyphonically, in any number of kit rows and synths at once. The LXR's six voices map onto four
voice circuits (`Model`): the drum (LXR voices 1 to 3), snare, cymbal and hi-hat. A `VoiceType` is one of the init
kit's seven sounds (the hi-hat as both closed and open hat); picking one picks the circuit and loads its settings.

The engine computes the LXR's 64-sample blocks at 44.1 kHz, as the LXR does, and `Voice::render()` hands out any
number of samples, so the result doesn't depend on the Deluge's render window sizes.

## How the Deluge uses it

- `processing/lxr/lxr_sound.h` connects it to a `Sound`: 28 continuous settings are unpatched params
  (`params::UNPATCHED_LXR_*`), so they can be automated, locked to steps in the automation view, and learnt to
  gold knobs and MIDI CCs. They store the LXR's 0..127 value as knob position - 64: a knob detent or a CC step
  is one LXR step. The 12 settings that pick from a list live in the `Source`'s `Patch`, like the oscillator type.
- `model/voice/voice.cpp` triggers a voice on every note and renders it, at the level of a 16-bit sample. The
  note and the Deluge's pitch params set a frequency multiplier for the oscillators; middle C plays a voice at its
  own tuning, as on the LXR.
- `Sound::allowNoteTails()` treats a voice like a one-shot sample: it plays until its own envelope has died away,
  unless its settings make it sound for ever (decay at 127, or a repeating attack phase that never decays).
- Files: a source of type `lxr` saves its `Patch` as the `lxrPatch` hex attribute (a format version, the voice
  type, then one byte per `Param`; only ever add to the end). The params are saved as `lxrCoarse` and so on, where
  they differ from the defaults that `Sound::initParams()` gives. Older firmware loads such a sound as
  a triangle oscillator.
- Menus are in `gui/menu_item/lxr/`, under SOUND > DRUM SYNTH and SELECT on oscillator 1's TYPE. The type is on
  offer once the community feature "Drum Synth Engine" is on.

## Differences from the LXR

Besides the changes each file describes, the port departs from the LXR's output in three edge cases, all of which
make the LXR produce garbage:

- Out-of-range float samples saturate when stored as 16 bits; the LXR wraps them around. Only the snare's mix of
  noise, oscillator and transient at full level gets there.
- A pitch envelope slope of 127 makes the LXR divide by zero; the port uses a very steep slope instead.
- A pitch envelope slope of 0 divides 0 by 0 at the envelope's start; the port gives 0, the value of the curve
  there.

## Verification

During development, the port was compared with the LXR firmware's own C code, both compiled for the host: the
init kit's seven sounds at three velocities for 2000 blocks each, and 3000 random patches for each voice type
(random settings, velocity, tempo, noise seed and modulation targets) for 600 blocks each. With the three changes
above switched back to the LXR's behaviour, every sample matched exactly. Settings changing during a note were
covered only through the LFO and velocity modulation. `tests/unit/lxr_tests.cpp` covers the engine's behaviour (every voice type sounds and
ends, render sizes don't matter, pitch, velocity, sustaining patches, settings round trips).

## Size

The engine adds about 120 kB to the firmware image, 78 kB of that the tables. Building with
`./dbt configure -DFEATURE_LXR_ENGINE=OFF` leaves the DSP and the tables out, about 100 kB; sounds that use the
engine then still load and save, but play silently.
