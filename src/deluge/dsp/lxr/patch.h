// Settings of one LXR drum voice.
//
// The parameter set, value ranges and default sounds follow the LXR firmware by Erica Synths
// (https://github.com/erica-synths/lxr-desktop-app, commit a43cd4f): Core/Inc/Parameters.h, the CC handler in
// Core/Src/Soundengine/MIDI/MidiParser.c, the menu tables in Core/Src/menu/ and the init kit in
// Core/Src/Preset/initKit.c. Modified for the Deluge: one voice per patch instead of a 6-voice kit, without the
// LXR's pan, audio routing, FX bus and cross-voice LFO targets.
//
// Copyright (C) 2012-2026 Erica Synths
// SPDX-License-Identifier: GPL-3.0-only
//
// This file is derived from the LXR firmware. It is licensed under the GNU General Public License version 3,
// with the additional terms in NOTICE.md in this directory.

#pragma once

#include <array>
#include <cstdint>

/// The LXR drum synth engine: one Deluge oscillator plays one voice of the LXR drum machine.
///
/// A Patch holds the settings of one such voice as 0..127 values, the same way the LXR stores them in its kits,
/// so the defaults below are the LXR's own init kit. The DSP lives in voice.h.
namespace deluge::dsp::lxr {

/// The four voice circuits of the LXR
enum class Model : uint8_t {
	DRUM,   ///< voices 1-3: oscillator with FM/mix modulator and pitch envelope (kicks, toms)
	SNARE,  ///< voice 4: tonal oscillator plus filtered noise (snares, claps)
	CYMBAL, ///< voice 5: 3-oscillator FM
	HIHAT,  ///< voice 6: 3-oscillator FM; the LXR plays it as closed and open hat
};

/// The seven sounds of the LXR init kit, one per LXR trigger channel. Picking one sets the Model and loads its
/// default settings.
enum class VoiceType : uint8_t {
	DRUM_1,
	DRUM_2,
	DRUM_3,
	SNARE,
	CYMBAL,
	CLOSED_HAT,
	OPEN_HAT,
};
constexpr int32_t kNumVoiceTypes = 7;

constexpr Model modelOf(VoiceType type) {
	switch (type) {
	case VoiceType::SNARE:
		return Model::SNARE;
	case VoiceType::CYMBAL:
		return Model::CYMBAL;
	case VoiceType::CLOSED_HAT:
	case VoiceType::OPEN_HAT:
		return Model::HIHAT;
	default:
		return Model::DRUM;
	}
}

/// Settings of one voice. The comment names the LXR parameter of voice 1 (or the voice that has it).
/// The order is part of the file format (Patch::serialize()), so only ever add to the end.
enum class Param : uint8_t {
	OSC_WAVE,        ///< PAR_OSC_WAVE_DRUM1: sine, tri, saw, rec, noise, pwm
	COARSE,          ///< PAR_COARSE1: MIDI note of the oscillator
	FINE,            ///< PAR_FINE1: +-half a semitone around 63.5
	PULSE_WIDTH,     ///< PAR_PWM_1
	NOISE_FREQ,      ///< PAR_NOISE_FREQ1 (snare)
	MIX,             ///< PAR_MIX1 (snare): oscillator / noise balance
	MOD1_FREQ,       ///< PAR_FM_FREQ1 (drum), PAR_MOD_OSC_F1_CYM (cymbal, hihat): MIDI note of the modulator
	MOD1_GAIN,       ///< PAR_FMAMNT1 (drum), PAR_MOD_OSC_GAIN1_CYM (cymbal, hihat)
	MOD1_WAVE,       ///< PAR_MOD_WAVE_DRUM1 (drum), PAR_WAVE2_CYM (cymbal, hihat)
	MOD2_FREQ,       ///< PAR_MOD_OSC_F2_CYM (cymbal, hihat)
	MOD2_GAIN,       ///< PAR_MOD_OSC_GAIN2_CYM (cymbal, hihat)
	MOD2_WAVE,       ///< PAR_WAVE3_CYM (cymbal, hihat)
	FM_MODE,         ///< PAR_MIX_MOD_1 (drum): 0 = modulator does FM, 1 = modulator is mixed in
	PITCH_DECAY,     ///< PAR_MOD_EG1 (drum, snare)
	PITCH_AMOUNT,    ///< PAR_MODAMNT1 (drum, snare)
	PITCH_SLOPE,     ///< PAR_PITCH_SLOPE1 (drum, snare)
	ATTACK,          ///< PAR_VELOA1
	DECAY,           ///< PAR_VELOD1; for the hihat PAR_VELOD6_CLOSED or PAR_VELOD6_OPEN
	SLOPE,           ///< PAR_VOL_SLOPE1: amp envelope curve
	REPEAT,          ///< PAR_REPEAT4 (snare, cymbal): retriggers of the attack phase, for claps
	TRANSIENT_WAVE,  ///< PAR_TRANS1_WAVE: snap, offset, then the 12 samples
	TRANSIENT_LEVEL, ///< PAR_TRANS1_VOL
	TRANSIENT_PITCH, ///< PAR_TRANS1_FREQ
	FILTER_TYPE,     ///< PAR_FILTER_TYPE_1: LP, HP, BP, unity BP, notch, peak, LP2, off
	FILTER_FREQ,     ///< PAR_FILTER_FREQ_1
	FILTER_RESO,     ///< PAR_RESO_1
	FILTER_DRIVE,    ///< PAR_FILTER_DRIVE_1
	DRIVE,           ///< PAR_DRIVE1: waveshaper after the amp envelope
	SAMPLE_RATE,     ///< PAR_VOICE_DECIMATION1: 127 = full rate
	LEVEL,           ///< PAR_VOL1
	VELO_TO_LEVEL,   ///< PAR_VOLUME_MOD_ON_OFF1: velocity scales the level inside the voice
	VELO_AMOUNT,     ///< PAR_VELO_MOD_AMT_1
	VELO_DEST,       ///< PAR_VEL_DEST_1: 0 = none, otherwise 1 + the Param velocity modulates
	LFO_WAVE,        ///< PAR_WAVE_LFO1
	LFO_RATE,        ///< PAR_FREQ_LFO1
	LFO_SYNC,        ///< PAR_SYNC_LFO1: off, 4/1, 2/1, 1/1, 1/2, 1/3, 1/4, 1/6, 1/8, 1/12, 1/16, 1/32
	LFO_DEPTH,       ///< PAR_AMOUNT_LFO1
	LFO_RETRIGGER,   ///< PAR_RETRIGGER_LFO1: restart the LFO on every hit. The LXR can pick the voice to follow.
	LFO_PHASE,       ///< PAR_OFFSET_LFO1: start phase on retrigger
	LFO_DEST,        ///< PAR_TARGET_LFO1: 0 = none, otherwise 1 + the Param the LFO modulates
};
constexpr int32_t kNumParams = static_cast<int32_t>(Param::LFO_DEST) + 1;

/// The value of every Param as 0..1, the way the LXR's parameter handler receives it: a stored setting n is n / 127.
/// Values between those steps come from the Deluge's automation of the settings it can automate.
using Values = std::array<float, kNumParams>;

/// Highest value a Param can take (the lowest is always 0)
uint8_t maxValue(Param param);
/// Whether the given voice circuit uses a Param
bool isRelevant(Param param, Model model);
/// Whether the LFO and velocity may modulate a Param of this voice circuit
bool isModDestination(Param param, Model model);
/// LXR shows these offset (COARSE and the modulator frequencies as semitones from middle C, FINE around 0)
int32_t displayOffset(Param param);

struct Patch {
	Patch() { loadDefaults(VoiceType::DRUM_1); }

	[[nodiscard]] VoiceType voiceType() const { return voiceType_; }
	[[nodiscard]] Model model() const { return modelOf(voiceType_); }
	[[nodiscard]] uint8_t get(Param param) const { return params_[static_cast<int32_t>(param)]; }

	/// Picks one of the LXR init kit's sounds, replacing every setting
	void loadDefaults(VoiceType type);
	void set(Param param, int32_t value);

	/// Every setting, as Values
	void getValues(Values& values) const;

	/// A voice that can sound for ever, because its amp envelope never decays (DECAY at 127, or a repeating
	/// attack phase that never decays). Such a voice is ended by note-off instead. This reads the Patch's own
	/// values; on the Deluge, DECAY, REPEAT and ATTACK are params: see processing::lxr::sustainsForever().
	[[nodiscard]] bool sustainsForever() const;

	/// File format: a format version byte, the voice type, then one byte per Param
	static constexpr int32_t kSerializedSize = 2 + kNumParams;
	void serialize(uint8_t (&out)[kSerializedSize]) const;
	/// Accepts shorter data from older versions of the format; missing settings keep the voice type's defaults
	void deserialize(const uint8_t* data, int32_t length);

private:
	VoiceType voiceType_ = VoiceType::DRUM_1;
	std::array<uint8_t, kNumParams> params_{};
};

} // namespace deluge::dsp::lxr
