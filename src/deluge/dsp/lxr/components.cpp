// Building blocks of the LXR drum voices. See components.h for where this code comes from and what changed.
//
// Copyright (C) 2012-2026 Erica Synths
// SPDX-License-Identifier: GPL-3.0-only
//
// This file is derived from the LXR firmware. It is licensed under the GNU General Public License version 3,
// with the additional terms in NOTICE.md in this directory.

#include "dsp/lxr/components.h"
#include "dsp/lxr/feature.h"
#include "dsp/lxr/tables.h"
#include <cmath>
#include <cstring>

#if FEATURE_LXR_ENGINE

namespace deluge::dsp::lxr {

namespace {

// ---- Oscillator.c ----

/// The LXR's table selection, freqToTableIndex(): an integer log2 of f/440Hz that wraps to table 4 below 440Hz,
/// which is why tables 0..3 are never played
uint8_t fast_log2(uint32_t val) {
	return static_cast<uint8_t>(31 - countLeadingZeros(val));
}

int32_t freqToMidiNote(float f) {
	return (12 * fast_log2(floatToU32(f / 440.f))) + 70;
}

uint8_t freqToTableIndex(float f) {
	return static_cast<uint8_t>(freqToMidiNote(f) / 12);
}

uint32_t freq2PhaseIncr(float f) {
	return floatToU32(((kSineTableSize * f) / kSampleRate) * 1048576); // 1048576 <-> (<<20)
}

uint32_t freq2PhaseIncr1024(float f) {
	return floatToU32(((1024 * f) / kSampleRate) * 4194304); // (<<22)
}

const int16_t* tableRow(const int16_t (*table)[kWavetableSize + 1], uint8_t tableOffset) {
	int32_t row = static_cast<int32_t>(tableOffset) - kFirstWavetable;
	if (row < 0) {
		row = 0;
	}
	else if (row >= kNumWavetables) {
		row = kNumWavetables - 1;
	}
	return table[row];
}

int16_t interpolate20(const int16_t* table, uint32_t index) {
	uint32_t itg = index >> 20; // 12 msb as itg index
	int16_t oscOut = table[itg++];
	uint32_t frac = index & 0xFFFFF;
	return static_cast<int16_t>(oscOut
	                            + static_cast<int16_t>((static_cast<int64_t>(table[itg] - oscOut) * frac) >> 20));
}

int16_t interpolate22(const int16_t* table, uint32_t index) {
	uint32_t itg = index >> 22; // 10 bit index, 22 bit fractional part
	int16_t oscOut = table[itg++];
	uint32_t frac = index & 0x3FFFFF;
	return static_cast<int16_t>(oscOut
	                            + static_cast<int16_t>((static_cast<int64_t>(table[itg] - oscOut) * frac) >> 22));
}

/// phase + pwm * (UINT32_MAX/2), evaluated in float like the LXR (so the second saw clips instead of wrapping)
uint32_t pwmPhase(uint32_t phase, float pwm) {
	return floatToU32(static_cast<float>(phase) + pwm * 2147483648.f);
}

void calcSineBlock(OscInfo* osc, int16_t* buf, uint8_t size, float gain) {
	for (uint8_t i = 0; i < size; i++) {
		const uint32_t oscPhase = osc->phase;
		const int16_t oscOut = interpolate20(sineTable, oscPhase);
		osc->phase = oscPhase + osc->phaseInc;
		buf[i] = floatToS16(oscOut * gain);
	}
}

void calcFmSineBlock(OscInfo* osc, const int16_t* modBuffer, int16_t* buf, uint8_t size, float gain) {
	for (uint8_t i = 0; i < size; i++) {
		const uint32_t oscPhase = osc->phase;
		// A negative modulator converts to 0 here on ARM, so only its positive half moves the phase
		const uint32_t index = oscPhase + (floatToU32(modBuffer[i] * osc->fmMod) << 17);
		const int16_t oscOut = interpolate20(sineTable, index);
		osc->phase = oscPhase + osc->phaseInc;
		buf[i] = floatToS16(oscOut * gain);
	}
}

void calcFmBlock(OscInfo* osc, const int16_t (*table)[kWavetableSize + 1], const int16_t* modBuffer, int16_t* buf,
                 uint8_t size, float gain) {
	const int16_t* row = tableRow(table, osc->tableOffset);
	for (uint8_t i = 0; i < size; i++) {
		const uint32_t oscPhase = osc->phase;
		const uint32_t index = oscPhase + (floatToU32(modBuffer[i] * osc->fmMod) << 19);
		const int16_t oscOut = interpolate22(row, index);
		osc->phase = oscPhase + osc->phaseInc;
		osc->output = oscOut;
		buf[i] = floatToS16(oscOut * gain);
	}
}

void calcPwmFmBlock(OscInfo* osc, const int16_t* modBuffer, int16_t* buf, uint8_t size, float gain, float pwm) {
	const int16_t* row = tableRow(sawTable, osc->tableOffset);
	for (uint8_t i = 0; i < size; i++) {
		const uint32_t oscPhase = osc->phase;
		const uint32_t index = oscPhase + (floatToU32(modBuffer[i] * osc->fmMod) << 19);
		const uint32_t index2 = pwmPhase(index, pwm);
		const int16_t oscOut = subSaturating16(interpolate22(row, index), interpolate22(row, index2));
		osc->phase = oscPhase + osc->phaseInc;
		osc->output = oscOut;
		buf[i] = floatToS16(oscOut * gain);
	}
}

void calcWavetableOscBlock(OscInfo* osc, const int16_t (*table)[kWavetableSize + 1], int16_t* buf, uint8_t size,
                           float gain) {
	const int16_t* row = tableRow(table, osc->tableOffset);
	for (uint8_t i = 0; i < size; i++) {
		const uint32_t oscPhase = osc->phase;
		const int16_t oscOut = interpolate22(row, oscPhase);
		osc->phase = oscPhase + osc->phaseInc;
		buf[i] = floatToS16(oscOut * gain);
	}
}

void calcPwmBlock(OscInfo* osc, int16_t* buf, uint8_t size, float gain, float pwm) {
	const int16_t* row = tableRow(sawTable, osc->tableOffset);
	for (uint8_t i = 0; i < size; i++) {
		const uint32_t oscPhase = osc->phase;
		const uint32_t oscPhase2 = pwmPhase(osc->phase, pwm);
		const int16_t oscOut = subSaturating16(interpolate22(row, oscPhase), interpolate22(row, oscPhase2));
		osc->phase = oscPhase + osc->phaseInc;
		osc->output = oscOut;
		buf[i] = floatToS16(oscOut * gain);
	}
}

void osc_calcSineFreq(OscInfo* osc) {
	osc->phaseInc = freq2PhaseIncr(osc->freq * osc->pitchMod * osc->modNodeValue * osc->externalPitch);
}

void osc_calcWavetableFreq(OscInfo* osc) {
	const float currentFreq = osc->freq * osc->pitchMod * osc->modNodeValue * osc->externalPitch;
	osc->phaseInc = freq2PhaseIncr1024(currentFreq);

	const uint8_t overtoneIndex = freqToTableIndex(currentFreq);
	osc->tableOffset = overtoneIndex > kLastWavetable ? kLastWavetable : overtoneIndex;
}

// ---- ResonantFilter.c ----

float fastTanh(float var) {
	if (var < -1.95f) {
		return -1.0f;
	}
	if (var > 1.95f) {
		return 1.0f;
	}
	return 4.15f * var / (4.29f + var * var);
}

float fastTan(float x) {
	const float x2 = x * x;
	const float A = -15 * x + x * x2;
	const float B = 3 * (-5 + 2 * x2);
	return A / B;
}

float removeMaliciousSample(float in) {
	return isNan(in) ? 0.f : in;
}

// ---- Decay.c ----

float DecayEg_calcSlopeValue(float val, float slope) {
	const float denominator = 1 + slope * std::fabs(val);
	// The LXR divides by zero here for slope -1 at |val| 1; the curve is flat at 0 there
	if (denominator == 0.f) {
		return 0.f;
	}
	return (1 + slope) * val / denominator;
}

constexpr float kDecayTimeK = 2 * 0.99f / (1.f - 0.99f);

float DecayEg_calcTime(uint8_t data2) {
	const float val = (data2) / 127.f;
	// routine is called only half as often -> *2
	const float ret = (1.f - ((1.f + kDecayTimeK) * val / (1.f + kDecayTimeK * std::fabs(val)))) * 2.f;
	return ret > 1.f ? 1.f : ret;
}

// ---- SlopeEg2.c ----

double slopeEg2_calcSlopeValue(double val, double slope) {
	return (1.0 + slope) * val / (1.0 + slope * std::fabs(val));
}

constexpr double kTimeAmountDecay = 0.999;
constexpr double kTimeAmountAttack = 0.99;
constexpr double kTimeKAttack = (2 * kTimeAmountAttack / (1.0 - kTimeAmountAttack));
constexpr double kTimeKDecay = (2 * kTimeAmountDecay / (1.0 - kTimeAmountDecay));

double slopeEg2_calcTime(uint8_t data2, double amount) {
	const double val = (data2) / 127.0;
	// routine is called only half as often -> *2
	const float ret = static_cast<float>((1.0 - ((1.0 + amount) * val / (1.0 + amount * std::fabs(val)))) * 2.f);
	return ret > 1.f ? 1.f : ret;
}

// ---- lfo.c ----

constexpr float kLfoMaxFreq = 200; // [Hz]
constexpr float kLfoSampleRate = kSampleRate / static_cast<float>(kBlockSize);

uint32_t lfo_calcPhaseInc(float freq, uint8_t sync, float bpm) {
	if (sync == 0) { // no sync
		return floatToU32(freq / (kLfoSampleRate) * 4294967296.f);
	}

	const float tempoAsFrequency = bpm > 0.0 ? bpm / 60.f / 4 : 0.0f; // bpm/60 = beat duration /4 = bar duration

	float scaler;
	switch (sync) {
	default: // no sync
		return floatToU32(freq / (kLfoSampleRate) * 4294967296.f);
	case 1: // 4/1
		scaler = 0.25f;
		break;
	case 2: // 2/1
		scaler = 0.5f;
		break;
	case 3: // 1/1
		scaler = 1;
		break;
	case 4: // 1/2
		scaler = 2;
		break;
	case 5: // 1/3
		scaler = 3;
		break;
	case 6: // 1/4
		scaler = 4;
		break;
	case 7: // 1/6
		scaler = 6;
		break;
	case 8: // 1/8
		scaler = 8;
		break;
	case 9: // 1/12
		scaler = 12;
		break;
	case 10: // 1/16
		scaler = 16;
		break;
	case 11: // 1/32
		scaler = 32;
		break;
	}

	const float lfoSyncfreq = tempoAsFrequency * scaler;
	return floatToU32((lfoSyncfreq / (kLfoSampleRate)) * 4294967296.f);
}

} // namespace

// ---- Oscillator.c ----

void calcNoiseBlock(OscInfo* osc, int16_t* buf, uint8_t size, float gain, Random& rng) {
	for (uint8_t i = 0; i < size; i++) {
		const uint32_t lastPhase = osc->phase;
		osc->phase += osc->phaseInc;

		if (lastPhase > osc->phase) {
			// overflow happened -> phaseWrapped
			osc->output = static_cast<int16_t>(rng.next()); // normal pitched white noise
		}

		buf[i] = floatToS16(osc->output * gain);
	}
}

void calcNextOscSampleFmBlock(OscInfo* osc, const int16_t* modBuffer, int16_t* buf, uint8_t size, float gain, float pwm,
                              Random& rng) {
	switch (osc->waveform) {
	case SINE:
		calcFmSineBlock(osc, modBuffer, buf, size, gain);
		break;
	case SAW:
		calcFmBlock(osc, sawTable, modBuffer, buf, size, gain);
		break;
	case TRI:
		calcFmBlock(osc, triTable, modBuffer, buf, size, gain);
		break;
	case REC:
		calcFmBlock(osc, recTable, modBuffer, buf, size, gain);
		break;
	case NOISE:
		calcNoiseBlock(osc, buf, size, gain, rng);
		break;
	case PULSE:
		calcPwmFmBlock(osc, modBuffer, buf, size, gain, pwm);
		break;
	default: // user samples on the LXR, which this port doesn't have
		memset(buf, 0, size * sizeof(int16_t));
		break;
	}
}

void calcNextOscSampleBlock(OscInfo* osc, int16_t* buf, uint8_t size, float gain, float pwm, Random& rng) {
	switch (osc->waveform) {
	case SINE:
		calcSineBlock(osc, buf, size, gain);
		break;
	case SAW:
		calcWavetableOscBlock(osc, sawTable, buf, size, gain);
		break;
	case TRI:
		calcWavetableOscBlock(osc, triTable, buf, size, gain);
		break;
	case REC:
		calcWavetableOscBlock(osc, recTable, buf, size, gain);
		break;
	case NOISE:
		calcNoiseBlock(osc, buf, size, gain, rng);
		break;
	case PULSE:
		calcPwmBlock(osc, buf, size, gain, pwm);
		break;
	default:
		memset(buf, 0, size * sizeof(int16_t));
		break;
	}
}

void osc_setFreq(OscInfo* osc) {
	switch (osc->waveform) {
	case SINE:
	case NOISE:
		osc_calcSineFreq(osc);
		break;
	default:
		osc_calcWavetableFreq(osc);
		break;
	}
}

float calcDetune(uint8_t value) {
	constexpr float kSemitoneUp = 1.0594630943592952645618252949463f;
	// linear interpolation between 1 (no change) and a semitone up/down
	const float frac = (value / 127.f - 0.5f);
	float cent = 1;
	cent += frac * (kSemitoneUp - 1);
	return cent;
}

void osc_setBaseNote(OscInfo* osc, uint8_t baseNote) {
	osc->baseNote = baseNote;
	osc_recalcFreq(osc);
}

void osc_recalcFreq(OscInfo* osc) {
	constexpr int32_t kDefaultNote = 60; // SEQ_DEFAULT_NOTE
	// get fine tune
	const float cent = calcDetune(osc->midiFreq & 0xff);
	// calc coarse tune
	int16_t note = static_cast<int16_t>((osc->midiFreq >> 8) + (osc->baseNote - kDefaultNote));
	if (note > 127) {
		note = 127;
	}
	if (note < 0) {
		note = 0;
	}
	osc->freq = midiNoteFrequencies[note] * cent;
}

// ---- ResonantFilter.c ----

void SVF_setReso(ResonantFilter* filter, float feedback) {
	filter->q = 1 - feedback;
	if (filter->q < 0.1f) {
		filter->q = 0.02f;
	}
}

void SVF_init(ResonantFilter* filter) {
	filter->s1 = 0;
	filter->s2 = 0;
	filter->a = filter->b = 0;
	filter->f = 0.20f;
	filter->q = 0.9f;
	filter->drive = 0.315f;
	SVF_directSetFilterValue(filter, 0.25f);
}

void SVF_reset(ResonantFilter* filter) {
	filter->s1 = 0;
	filter->s2 = 0;
	filter->a = filter->b = 0;
}

void SVF_recalcFreq(ResonantFilter* filter) {
	filter->g = fastTan(static_cast<float>(M_PI * filter->f));
}

void SVF_setDrive(ResonantFilter* filter, float drive) {
	filter->drive = 0.4f + (drive) * (drive) * 6;
}

void SVF_directSetFilterValue(ResonantFilter* filter, float val) {
	filter->f = val * (0.5f * 0.90f);
	filter->g = fastTan(static_cast<float>(M_PI * filter->f));
}

void SVF_calcBlockZDF(ResonantFilter* filter, uint8_t type, int16_t* buf, uint8_t size) {
	constexpr float kFilterGain = 0x7fff;

	const float f = filter->g;
	// fix unstable filter for high f and r settings
	const float R = filter->f >= 0.4499f ? 1 : filter->q;
	const float ff = f * f;

	if (type == FILTER_OFF) {
		return;
	}
	if (type == FILTER_NAIVE_2_POLE) {
		// alternative 2Pole LP filter to fix the kick transient problems with the nonlinear ZDF LP
		const float coefficient = lp2::coefficient(filter->f);
		const float feedback = lp2::feedback(filter->q, coefficient);
		for (uint8_t i = 0; i < size; i++) {
			const float x = softClipTwo((buf[i] / (static_cast<float>(0x7fff))) * filter->drive);
			const float y = lp2::process(filter->a, filter->b, x, coefficient, feedback);
			buf[i] = saturate16(floatToS32(y * kFilterGain));
		}
		return;
	}

	filter->s1 = removeMaliciousSample(filter->s1);
	filter->s2 = removeMaliciousSample(filter->s2);
	for (uint8_t i = 0; i < size; i++) {
		const float x = softClipTwo((buf[i] / (static_cast<float>(0x7fff))) * filter->drive);

		// the nonlinear integrator gains, which the LXR compiles out
		const float t0 = 1;
		const float t1 = 1;

		// g# the denominators for solutions of individual stages
		const float g0 = 1.f / (1.f + f * t0 * 2 * R);
		const float s1 = filter->s1;
		const float s2 = filter->s2;

		// solve feedback
		const float f1 = ff * g0 * t0 * t1;
		const float y1 = (f1 * x + s2 + f * g0 * t1 * s1) / (f1 + 1);

		// solve the remaining stages with nonlinear gain
		const float xx = t0 * (x - y1);
		const float y0 = (softClipTwo(s1) + f * xx) * g0;

		filter->s1 = softClipTwo(filter->s1) + 2 * f * (xx - t0 * 2 * R * y0);
		filter->s2 = (filter->s2) + 2 * f * t1 * y0;

		switch (type) {
		default:
			return;
		case FILTER_LP:
			buf[i] = saturate16(floatToS32(fastTanh(y1) * 0x7fff));
			break;
		case FILTER_HP: {
			const float ugb = 2 * R * y0;
			const float h = x - ugb - y1;
			buf[i] = saturate16(floatToS32(h * kFilterGain));
		} break;
		case FILTER_BP:
			buf[i] = saturate16(floatToS32(y0 * kFilterGain));
			break;
		case FILTER_UNITY_BP: {
			const float ugb = 2 * R * y0;
			buf[i] = saturate16(floatToS32(ugb * kFilterGain));
		} break;
		case FILTER_NOTCH: {
			const float ugb = 2 * R * y0;
			buf[i] = saturate16(floatToS32((x - ugb) * kFilterGain));
		} break;
		case FILTER_PEAK: {
			const float ugb = 2 * R * y0;
			const float h = x - ugb - y1;
			buf[i] = saturate16(floatToS32((y1 - h) * kFilterGain));
		} break;
		}
	}
}

// ---- Decay.c ----

void DecayEg_init(DecayEg* eg) {
	eg->decay = 0.01f;
	eg->value = 0;
}

void DecayEg_trigger(DecayEg* eg) {
	eg->value = 1.f;
}

float DecayEg_calc(DecayEg* eg) {
	float val = eg->value;
	if (val > 0) {
		val -= eg->decay;
	}
	else {
		val = 0;
	}
	eg->value = val;
	return DecayEg_calcSlopeValue(val, eg->slope);
}

void DecayEg_setDecay(DecayEg* eg, uint8_t data2) {
	eg->decay = DecayEg_calcTime(data2);
}

void DecayEg_setSlope(DecayEg* eg, uint8_t data2) {
	const float amount = ((data2 / 127.f) - 0.5f) * 2;
	// At 127 the LXR divides by zero and the pitch envelope turns into NaN. Use a slope steep enough to be
	// indistinguishable from the limit instead.
	eg->slope = amount >= 1.f ? 1.0e6f : 2 * amount / (1 - amount);
}

// ---- SlopeEg2.c ----

void slopeEg2_init(SlopeEg2* eg) {
	eg->attack = 0.01;
	eg->decay = 0.01;
	eg->value = 0;
	eg->state = EG_STOPPED;
	eg->repeat = 0;
	slopeEg2_setSlope(eg, 0); // the LXR passes 0.5f to this uint8_t parameter, which truncates to 0
}

void slopeEg2_trigger(SlopeEg2* eg) {
	// if no repeat is selected begin in attack state
	if (!eg->repeat) {
		eg->state = EG_A;
	}
	// if repeat is active we want to repeat the decay stage during the attack time
	else {
		eg->state = EG_REPEAT;
		// since we have no attack we start with eg->value = max
		// we use the attack time as a repeat phase decay time
		eg->value = 1.0;
	}
	eg->repeatCnt = eg->repeat;
}

double slopeEg2_calc(SlopeEg2* eg) {
	double val = eg->value;
	switch (eg->state) {
	case EG_STOPPED:
		return 0.0;

	case EG_REPEAT:
		// if the repeat mode is active we use the attack time as a 2nd decay time for the loop part
		// the attack phase is looped repeatCnt times
		if (val > 0) {
			val -= eg->attack;
		}
		else {
			eg->repeatCnt--;
			if (eg->repeatCnt > 0) {
				// retrigger the decay phase and decrease repeat counter
				val = 1.0;
			}
			else {
				// repeat counter reached zero -> go to real decay stage
				eg->state = EG_D;
				val = 1.0;
			}
		}
		eg->value = val;
		// to get a analogue style exp eg we need to invert the shape
		return slopeEg2_calcSlopeValue(val, eg->slope);

	case EG_A:
		val += eg->attack;
		if (val >= 1.0) {
			val = 1.0;
			eg->state = EG_D;
		}
		eg->value = val;
		// to get a analogue style exp eg we need to invert the shape
		return slopeEg2_calcSlopeValue(val, eg->invSlope);

	case EG_D:
		if (val > 0) {
			val -= eg->decay;
			eg->value = val;
			return slopeEg2_calcSlopeValue(val, eg->slope);
		}
		eg->value = 0;
		return 0;
	}
	return 0;
}

void slopeEg2_setAttack(SlopeEg2* eg, uint8_t data2, uint8_t isSync) {
	eg->attack = slopeEg2_calcTime(data2, kTimeKAttack);
	if (isSync) {
		eg->attack /= 16;
	}
}

void slopeEg2_setDecay(SlopeEg2* eg, uint8_t data2, uint8_t isSync) {
	eg->decay = slopeEg2_calcTime(data2, kTimeKDecay);
	if (isSync) {
		eg->decay /= 16;
	}
}

double slopeEg2_calcDecay(uint8_t data2) {
	return slopeEg2_calcTime(data2, kTimeKDecay);
}

void slopeEg2_setSlope(SlopeEg2* eg, uint8_t data2) {
	const double amount = ((data2 / 127.f) - 0.5f) * 1.999f;
	// negate the amount
	const double invAmount = -amount;

	eg->slope = 2 * amount / (1 - amount);
	eg->invSlope = 2 * invAmount / (1 - invAmount);
}

// ---- snapEg.c ----

void SnapEg_init(SnapEg* eg) {
	eg->value = 0;
}

void SnapEg_trigger(SnapEg* eg) {
	eg->value = 1;
}

float SnapEg_calc(SnapEg* eg, float time) {
	constexpr float kSnapMaxValue = 24.f;
	constexpr float kSnapReduction = 0.2f;
	float ret = 0;
	if (eg->value > 0) {
		ret = eg->value * eg->value * kSnapMaxValue;
		eg->value -= kSnapReduction * time;
	}
	return ret;
}

// ---- transientGenerator.c ----

void transient_init(TransientGenerator* transient) {
	transient->pitch = 1.f;
	transient->output = 0;
	transient->phase = 0;
	transient->waveform = 0;
	transient->volume = 1.f;
}

void transient_setWaveform(TransientGenerator* transient, uint8_t waveform) {
	if (waveform < kNumTransientWaveforms) {
		transient->waveform = waveform;
	}
	else {
		transient->waveform = 0;
	}
}

void transient_trigger(TransientGenerator* transient) {
	// reset the phase to the beginning when the generator is triggered
	transient->phase = 0;
}

void transient_calcBlock(TransientGenerator* transient, int16_t* buf, uint8_t size) {
	constexpr uint32_t kEndPhase = 2311061504u; // 2204<<20
	// no sample for snap and offset, no sample data if phase >= sample length
	if ((transient->waveform <= 1) || (transient->phase >= kEndPhase)) {
		memset(buf, 0, size * sizeof(int16_t));
		return;
	}

	const int8_t* data = transientData[transient->waveform - 2];
	for (uint8_t i = 0; i < size; i++) {
		if (transient->phase < kEndPhase) {
			const uint32_t phase = transient->phase >> 20;
			buf[i] = floatToS16(transient->volume * (data[phase] * 256) * (phase < kTransientLength));
			transient->phase = floatToU32(static_cast<float>(transient->phase) + (transient->pitch * (1 << 20)));
		}
		else {
			buf[i] = 0;
		}
	}
}

// ---- 1PoleLp.c ----

void initOnePole(OnePoleFilter* filter) {
	filter->b = 0.f;
	filter->r = 0.01f;
}

float calcOnePole(OnePoleFilter* filter, double input) {
	// The LXR uses fmaf() here, a fused multiply-add on its Cortex-M7. The Deluge's Cortex-A9 has no fused
	// multiply-add instruction, so this is a plain multiply and add.
	filter->b = static_cast<float>(filter->r) * static_cast<float>(input - filter->b) + static_cast<float>(filter->b);
	return static_cast<float>(filter->b);
}

void setOnePoleCoef(OnePoleFilter* filter, double val) {
	filter->r = val;
}

// ---- distortion.c ----

void setDistortionShape(Distortion* dist, float shape) {
	shape = static_cast<float>(shape - 0.0078125); // to prevent division by 0
	dist->shape = 2 * (shape) / (1 - (shape));
}

void calcDistBlock(const Distortion* dist, int16_t* buf, uint8_t size) {
	for (uint8_t i = 0; i < size; i++) {
		float x = buf[i] / 32767.f;
		x = (1 + dist->shape) * x / (1 + dist->shape * std::fabs(x));
		buf[i] = floatToS16(x * 32767);
	}
}

void calcDistBlockFiltered(Distortion* dist, int16_t* buf, uint8_t size) {
	for (uint8_t i = 0; i < size; i++) {
		const float shape = calcOnePole(&dist->DriveParamFilter, dist->shape);
		float x = buf[i] / 32767.f;
		x = (1 + shape) * x / (1 + shape * std::fabs(x));
		buf[i] = floatToS16(x * 32767);
	}
}

// ---- lfo.c ----

void lfo_init(Lfo* lfo) {
	lfo->phase = 0;
	lfo->phaseInc = 1;
	lfo->phaseOffset = 0;
	lfo->retrigger = 0;
	lfo->waveform = LFO_SINE;
	lfo->sync = 0;
	lfo->freq = 1;
	lfo->modNodeValue = 1;
	lfo->rnd = 0;
	lfo->bpm = 120;
}

float lfo_calc(Lfo* lfo, Random& rng) {
	const uint32_t oldPhase = lfo->phase;
	const float inc = lfo->phaseInc * lfo->modNodeValue;
	const uint32_t incInt = floatToU32(inc);
	lfo->phase += incInt;
	const bool overflow = oldPhase > lfo->phase;

	constexpr float kPhaseRange = 4294967296.f; // (float)0xffffffff

	switch (lfo->waveform) {
	case LFO_SINE:
		return ((sineTable[(lfo->phase >> 20)] / 32767.f) + 1) / 2.f;
	case LFO_TRI:
		return (1.f - std::fabs((lfo->phase / kPhaseRange) * 2 - 1));
	case LFO_SAW_UP:
		return lfo->phase / kPhaseRange;
	case LFO_REC:
		return lfo->phase > 0x7fffffff ? 1.f : 0.f;
	case LFO_NOISE:
		if (overflow) {
			lfo->rnd = static_cast<float>(rng.next());
			lfo->rnd = lfo->rnd / kPhaseRange;
		}
		return lfo->rnd;
	case LFO_SAW_DOWN:
		return (0xffffffff - (lfo->phase)) / kPhaseRange;
	case LFO_EXP_UP: {
		const float x = lfo->phase / kPhaseRange;
		return x * x * x;
	}
	case LFO_EXP_DOWN: {
		const float x = (0xffffffff - (lfo->phase)) / kPhaseRange;
		return x * x * x;
	}
	default:
		return 0;
	}
}

void lfo_setFreq(Lfo* lfo, float f) {
	f = 0.0078125f + f * (1.f - 0.0078125f); // to prevent f from being zero
	f = f * f * f;
	lfo->freq = f * kLfoMaxFreq;
	lfo->phaseInc = lfo_calcPhaseInc(lfo->freq, lfo->sync, lfo->bpm);
}

void lfo_setSync(Lfo* lfo, uint8_t sync) {
	lfo->sync = sync;
	lfo->phaseInc = lfo_calcPhaseInc(lfo->freq, lfo->sync, lfo->bpm);
}

void lfo_setBpm(Lfo* lfo, float bpm) {
	lfo->bpm = bpm;
	lfo->phaseInc = lfo_calcPhaseInc(lfo->freq, lfo->sync, lfo->bpm);
}

} // namespace deluge::dsp::lxr

#endif // FEATURE_LXR_ENGINE
