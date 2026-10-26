// Lookup tables for the LXR drum synth engine, copied from the LXR firmware by Erica Synths.
// See tables.cpp for where each table comes from.
//
// Copyright (C) 2012-2026 Erica Synths
// SPDX-License-Identifier: GPL-3.0-only
//
// This file is derived from the LXR firmware. It is licensed under the GNU General Public License
// version 3, with the additional terms in NOTICE.md in this directory.

#pragma once

#include <cstdint>

namespace deluge::dsp::lxr {

constexpr int32_t kSineTableSize = 4096;
/// 12-bit indexed sine table (starts at -32767), plus one guard sample for interpolation
extern const int16_t sineTable[kSineTableSize + 1];

constexpr int32_t kWavetableSize = 1024;
/// The LXR has 11 band-limited tables per waveform (one per octave), but only ever plays 4..10
constexpr int32_t kFirstWavetable = 4;
constexpr int32_t kLastWavetable = 10;
constexpr int32_t kNumWavetables = kLastWavetable - kFirstWavetable + 1;
extern const int16_t sawTable[kNumWavetables][kWavetableSize + 1];
extern const int16_t recTable[kNumWavetables][kWavetableSize + 1];
extern const int16_t triTable[kNumWavetables][kWavetableSize + 1];

/// Short one-shot attack samples, signed 8-bit, about 50ms long at 44.1kHz
constexpr int32_t kNumTransients = 12;
constexpr int32_t kTransientLength = 2205;
extern const int8_t transientData[kNumTransients][kTransientLength];

/// Frequency in Hz of each MIDI note
extern const float midiNoteFrequencies[128];

} // namespace deluge::dsp::lxr
