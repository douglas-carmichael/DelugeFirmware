// Building blocks of the LXR drum voices: oscillator, state variable filter, envelopes, transient generator,
// waveshaper, one-pole smoother and LFO.
//
// Ported from the LXR firmware by Erica Synths (https://github.com/erica-synths/lxr-desktop-app, commit a43cd4f),
// files Core/{Inc,Src}/Soundengine/DSPAudio/{Oscillator,ResonantFilter,Decay,SlopeEg2,snapEg,transientGenerator,
// distortion,1PoleLp,lfo}.{h,c}. Originally written by Julian Schmidt (Sonic Potions).
//
// Modified for the Deluge: the C structs and functions became namespaced C++ with the same names and arithmetic,
// the global voice state is gone (everything works on the struct it is handed), the sample rate is the Deluge's
// 44.1kHz, Cortex-M intrinsics are replaced by the portable helpers below, and code paths the LXR compiles out
// (user samples, the "cheap" reverb-mode filter, the unused per-sample oscillator functions) are left out.
//
// Copyright (C) 2012-2026 Erica Synths
// SPDX-License-Identifier: GPL-3.0-only
//
// This file is derived from the LXR firmware. It is licensed under the GNU General Public License version 3,
// with the additional terms in NOTICE.md in this directory.

#pragma once

#include <cstdint>

namespace deluge::dsp::lxr {

/// The LXR computes its envelopes, LFOs and filter coefficients once per block of this many samples
/// (OUTPUT_DMA_SIZE with USE_BIG_SAMPLE_BUFFER), and its envelope times are tuned to it.
constexpr int32_t kBlockSize = 64;
/// REAL_FS. The LXR runs at 44.14kHz and tunes its oscillators to that rate; the Deluge runs at 44.1kHz.
constexpr float kSampleRate = 44100.f;

// ---- Arithmetic helpers ----
// The LXR relies on how a Cortex-M7 converts and saturates. These spell that behaviour out so the port
// does the same thing on the Deluge's Cortex-A9 and in host-side tests.

/// NaN test that survives -ffast-math (the LXR builds its version of this with -O0 for the same reason)
inline bool isNan(float x) {
	uint32_t bits;
	__builtin_memcpy(&bits, &x, sizeof(bits));
	return (bits & 0x7F800000) == 0x7F800000 && (bits & 0x007FFFFF) != 0;
}

/// __SSAT(x, 16)
inline int16_t saturate16(int32_t x) {
	return x > 32767 ? 32767 : (x < -32768 ? -32768 : x);
}

/// One halfword of __QADD16 / __QSUB16
inline int16_t addSaturating16(int32_t a, int32_t b) {
	return saturate16(a + b);
}
inline int16_t subSaturating16(int32_t a, int32_t b) {
	return saturate16(a - b);
}

/// VCVT.U32.F32: truncates, negative values give 0, values beyond the range give 0xFFFFFFFF
inline uint32_t floatToU32(float x) {
	if (!(x > 0.f)) {
		return 0;
	}
	if (x >= 4294967296.f) {
		return 0xFFFFFFFF;
	}
	return static_cast<uint32_t>(x);
}

/// VCVT.S32.F32: truncates and saturates, NaN gives 0
inline int32_t floatToS32(float x) {
	if (isNan(x)) {
		return 0;
	}
	if (x >= 2147483648.f) {
		return 2147483647;
	}
	if (x <= -2147483648.f) {
		return -2147483647 - 1;
	}
	return static_cast<int32_t>(x);
}

/// Storing a float into an int16_t. The LXR truncates the float to 32 bits and keeps the low 16 bits, so
/// out-of-range values wrap around. This port saturates instead. The only LXR path that can go out of range
/// is the snare's mix of noise, oscillator and transient at full level.
inline int16_t floatToS16(float x) {
	return saturate16(floatToS32(x));
}
inline int16_t doubleToS16(double x) {
	if (x >= 32767.0) {
		return 32767;
	}
	if (x <= -32768.0) {
		return -32768;
	}
	return x != x ? 0 : static_cast<int16_t>(x);
}

/// Count leading zeros with the ARM result for zero (32)
inline uint32_t countLeadingZeros(uint32_t v) {
	return v ? static_cast<uint32_t>(__builtin_clz(v)) : 32;
}

/// Replaces the STM32 hardware random number generator (GetRngValue())
struct Random {
	uint32_t state = 0x12345678;
	void seed(uint32_t s) { state = s ? s : 0x12345678; }
	uint32_t next() {
		state ^= state << 13;
		state ^= state >> 17;
		state ^= state << 5;
		return state;
	}
};

// ---- Oscillator.h ----

enum OscWaveform : uint8_t {
	SINE,
	TRI,
	SAW,
	REC,
	NOISE,
	PULSE, ///< pulse wave, its width set by Param::PULSE_WIDTH
	NUM_OSC_WAVEFORMS
};

struct OscInfo {
	int16_t output;
	uint32_t phaseInc;
	uint32_t phase; ///< the current phase of the osc 8bit indexing 24 interpolation
	float freq;     ///< frequency in [Hz]
	uint8_t waveform;
	uint8_t tableOffset; ///< overtone table, 4..10 once the frequency has been set
	float pitchMod;      ///< modulation value for frequency
	float fmMod;
	float modNodeValue;
	uint16_t midiFreq; ///< upper 8 bit coarse, lower 8 bit fine
	uint8_t baseNote;  ///< the last played midi note
	/// Added for the Deluge: frequency multiplier carrying the Deluge's note, transpose and pitch modulation
	float externalPitch;
};

void calcNextOscSampleBlock(OscInfo* osc, int16_t* buf, uint8_t size, float gain, float pwm, Random& rng);
void calcNextOscSampleFmBlock(OscInfo* osc, const int16_t* modBuffer, int16_t* buf, uint8_t size, float gain, float pwm,
                              Random& rng);
void calcNoiseBlock(OscInfo* osc, int16_t* buf, uint8_t size, float gain, Random& rng);
void osc_setFreq(OscInfo* osc);
void osc_setBaseNote(OscInfo* osc, uint8_t baseNote);
void osc_recalcFreq(OscInfo* osc);
/// midiParser_calcDetune(): takes a value from 0 to 127 and returns a +/- half-semitone detune factor
float calcDetune(uint8_t value);

// ---- ResonantFilter.h ----

enum FilterType : uint8_t {
	FILTER_LP = 1,
	FILTER_HP,
	FILTER_BP,
	FILTER_UNITY_BP,
	FILTER_NOTCH,
	FILTER_PEAK,
	FILTER_NAIVE_2_POLE,
	FILTER_OFF
};

struct ResonantFilter {
	float f; ///< cutoff frequency, 0..0.5 of the sample rate
	float g; ///< embedded integrator gain
	float q; ///< q value calculated from setReso()
	float s1;
	float s2;
	float a, b;
	float drive;
};

void SVF_init(ResonantFilter* filter);
void SVF_reset(ResonantFilter* filter);
void SVF_setReso(ResonantFilter* filter, float feedback);
void SVF_setDrive(ResonantFilter* filter, float drive);
void SVF_directSetFilterValue(ResonantFilter* filter, float val);
void SVF_recalcFreq(ResonantFilter* filter);
void SVF_calcBlockZDF(ResonantFilter* filter, uint8_t type, int16_t* buf, uint8_t size);

/// Pade approximation of tanh(x) / x
inline float tanhXdX(float x) {
	const float a = x * x;
	return ((a + 105) * a + 945) / ((15 * a + 420) * a + 945);
}

/// The filters' input saturation, about 2 tanh(in / 2)
inline float softClipTwo(float in) {
	return in * tanhXdX(static_cast<float>(0.5 * in));
}

/// The LP2 filter type (FILTER_NAIVE_2_POLE): two one-pole lowpass stages with resonance feedback, each clamped to
/// +-1, which gives it its squelchy, 303-like sound.
namespace lp2 {

/// Stage coefficient for a cutoff of f, as a fraction of the sample rate (ResonantFilter::f)
inline float coefficient(float f) {
	return f * 2.21f;
}

/// Resonance feedback for a ResonantFilter::q, from SVF_setReso()
inline float feedback(float q, float coefficient) {
	return static_cast<float>((1 - q) * 1.4 + (1 - q) / (1.0 - coefficient));
}

/// One sample through the stages a and b
/// @param x the input, already through softClipTwo()
/// @return the output, -1..1
inline float process(float& a, float& b, float x, float coefficient, float feedback) {
	a += coefficient * ((x - a) + feedback * (a - b));
	if (a > 1) {
		a = 1;
	}
	else if (a < -1) {
		a = -1;
	}

	b += coefficient * (a - b);
	if (b > 1) {
		b = 1;
	}
	else if (b < -1) {
		b = -1;
	}
	return b;
}

} // namespace lp2

// ---- Decay.h ----

struct DecayEg {
	float decay;
	float slope;
	float value;
};

void DecayEg_init(DecayEg* eg);
void DecayEg_trigger(DecayEg* eg);
void DecayEg_setDecay(DecayEg* eg, uint8_t data2);
float DecayEg_calc(DecayEg* eg);
void DecayEg_setSlope(DecayEg* eg, uint8_t data2);

// ---- SlopeEg2.h ----

enum SlopeEgState : uint8_t { EG_STOPPED, EG_A, EG_D, EG_REPEAT };

struct SlopeEg2 {
	double attack;
	double decay;
	double slope;
	double invSlope;
	uint8_t repeat;    ///< number of repetitions of the attack phase
	uint8_t repeatCnt; ///< a counter for the already played repeats
	double value;
	uint8_t state;
};

void slopeEg2_init(SlopeEg2* eg);
void slopeEg2_trigger(SlopeEg2* eg);
double slopeEg2_calc(SlopeEg2* eg);
void slopeEg2_setAttack(SlopeEg2* eg, uint8_t data2, uint8_t isSync);
void slopeEg2_setDecay(SlopeEg2* eg, uint8_t data2, uint8_t isSync);
double slopeEg2_calcDecay(uint8_t data2);
void slopeEg2_setSlope(SlopeEg2* eg, uint8_t data2);

// ---- snapEg.h ----

struct SnapEg {
	float value;
};

void SnapEg_init(SnapEg* eg);
void SnapEg_trigger(SnapEg* eg);
float SnapEg_calc(SnapEg* eg, float time);

// ---- transientGenerator.h ----

/// Waveform 0 is "snap" (a pitch blip via SnapEg), 1 is "offset" (start phase), 2.. are the transient samples
constexpr uint8_t kNumTransientWaveforms = 2 + 12;

struct TransientGenerator {
	int16_t output;
	uint32_t phase;
	float pitch; ///< we need no accurate tuning for a transient, so we use a pitch factor [0.25f:4.f]
	uint8_t waveform;
	float volume;
};

void transient_init(TransientGenerator* transient);
void transient_trigger(TransientGenerator* transient);
void transient_calcBlock(TransientGenerator* transient, int16_t* buf, uint8_t size);
void transient_setWaveform(TransientGenerator* transient, uint8_t waveform);

// ---- 1PoleLp.h ----

struct OnePoleFilter {
	double r;
	double b;
};

void initOnePole(OnePoleFilter* filter);
float calcOnePole(OnePoleFilter* filter, double input);
void setOnePoleCoef(OnePoleFilter* filter, double val);

// ---- distortion.h ----

struct Distortion {
	float shape;
	float inv_shape;
	OnePoleFilter DriveParamFilter;
};

void setDistortionShape(Distortion* dist, float shape);
void calcDistBlock(const Distortion* dist, int16_t* buf, uint8_t size);
void calcDistBlockFiltered(Distortion* dist, int16_t* buf, uint8_t size);

// ---- valueShaper.h ----

/// shape is a value between -1 and 1
inline float valueShaperF2F(float val, float shape) {
	const float k = 2 * shape / (1.0001f - shape);
	return ((1 + k) * val / (1 + k * __builtin_fabsf(val)));
}

/// FILTER_SHAPER from globals.h
constexpr float kFilterShaper = -0.9f;

// ---- lfo.h ----

enum LfoWaveform : uint8_t {
	LFO_SINE,
	LFO_TRI,
	LFO_SAW_UP,
	LFO_SAW_DOWN,
	LFO_REC,
	LFO_NOISE,
	LFO_EXP_UP,
	LFO_EXP_DOWN,
	NUM_LFO_WAVEFORMS
};

/// Number of LFO sync settings: off, 4/1, 2/1, 1/1, 1/2, 1/3, 1/4, 1/6, 1/8, 1/12, 1/16, 1/32
constexpr uint8_t kNumLfoSyncRates = 12;

struct Lfo {
	uint32_t phase;
	uint32_t phaseInc;
	uint8_t waveform;
	uint8_t retrigger; ///< nonzero: the LFO restarts at phaseOffset whenever its own voice is triggered
	uint32_t phaseOffset;
	float rnd;
	uint8_t sync;
	float freq;
	float modNodeValue;
	float bpm; ///< added for the Deluge: tempo for synced rates, seq_getBpm() on the LXR
};

void lfo_init(Lfo* lfo);
float lfo_calc(Lfo* lfo, Random& rng);
void lfo_setFreq(Lfo* lfo, float f);
void lfo_setSync(Lfo* lfo, uint8_t sync);
/// Recalculate the phase increment for a new tempo (lfo_recalcSync())
void lfo_setBpm(Lfo* lfo, float bpm);

} // namespace deluge::dsp::lxr
