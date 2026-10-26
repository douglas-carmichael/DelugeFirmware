// One playing voice of the LXR drum synth engine. See voice.h for where this code comes from and what changed.
//
// Copyright (C) 2012-2026 Erica Synths
// SPDX-License-Identifier: GPL-3.0-only
//
// This file is derived from the LXR firmware. It is licensed under the GNU General Public License version 3,
// with the additional terms in NOTICE.md in this directory.

#include "dsp/lxr/voice.h"
#include "dsp/lxr/feature.h"
#include <algorithm>

#if FEATURE_LXR_ENGINE

namespace deluge::dsp::lxr {

namespace {

/// SEQ_DEFAULT_NOTE: the LXR plays a voice at its own tuning on this note. The Deluge's note reaches the
/// oscillators through OscInfo::externalPitch instead.
constexpr uint8_t kDefaultNote = 60;

float calcPitchModAmount(uint8_t data2) {
	constexpr float kPitchAmountFactor = 32;
	const float val = data2 / 127.f;
	return val * val * kPitchAmountFactor;
}

void initOsc(OscInfo* osc, uint8_t waveform, float fmMod, float pitchMod) {
	osc->freq = 440;
	osc->waveform = waveform;
	osc->fmMod = fmMod;
	osc->midiFreq = 70 << 8 | 64;
	osc->pitchMod = pitchMod;
	osc->modNodeValue = 1;
	osc->externalPitch = 1;
}

// ---- BufferTools.h ----

void addBuffersSaturating(int16_t* buf1, const int16_t* buf2) {
	for (int32_t i = 0; i < kBlockSize; i++) {
		buf1[i] = addSaturating16(buf1[i], buf2[i]);
	}
}

void addGain(int16_t* buf, float gain) {
	for (int32_t i = 0; i < kBlockSize; i++) {
		buf[i] = floatToS16(buf[i] * gain);
	}
}

void addGainInterpolated(int16_t* buf, float gain, float lastGain) {
	for (int32_t i = 0; i < kBlockSize; i++) {
		const double frac = i / (kBlockSize - 1.f);
		const double currentGain = lastGain + frac * (gain - lastGain);
		buf[i] = doubleToS16(buf[i] * currentGain);
	}
}

} // namespace

void Voice::init(Model model, const Values& values, uint32_t randomSeed, float bpm) {
	*this = Voice{};
	model_ = model;
	rng_.seed(randomSeed);

	// initDrumVoice(), Snare_init(), Cymbal_init(), HiHat_init()
	SnapEg_init(&snapEg_);
	vol_ = 0.8f;
	volumeMod_ = true;
	transient_init(&transGen_);
	lfo_init(&lfo_);
	lfo_.bpm = bpm;
	slopeEg2_init(&oscVolEg_);
	SVF_init(&filter_);
	setDistortionShape(&distortion_, 0.f);
	initOnePole(&distortion_.DriveParamFilter);
	setOnePoleCoef(&distortion_.DriveParamFilter, 0.01);
	initOsc(&noiseOsc_, TRI, 0, 1.0f);

	switch (model_) {
	case Model::DRUM:
		initOnePole(&volumeSmoother_);
		setOnePoleCoef(&volumeSmoother_, 0.01);
		fmModAmount_ = 0.5f;
		filterType_ = 0x01;
		initOsc(&modOsc_, TRI, 0, 1.0f);
		initOsc(&osc_, TRI, 0, 0);
		DecayEg_init(&oscPitchEg_);
		egPitchModAmount_ = 0.5f;
		mixOscs_ = true;
		break;

	case Model::SNARE:
		initOsc(&osc_, TRI, 0, 0);
		DecayEg_init(&oscPitchEg_);
		egPitchModAmount_ = 0.5f;
		break;

	case Model::CYMBAL:
	case Model::HIHAT:
		fmModAmount_ = 0.5f;
		fmModAmount2_ = 0.5f;
		initOsc(&modOsc_, SINE, 0, 1.0f);
		initOsc(&modOsc2_, NOISE, 0, 1.0f);
		initOsc(&osc_, TRI, 1, 1.0f);
		break;
	}

	// mixer_init()
	decimationRate_ = 1;

	applyAllValues(values);

	// The LXR's smoothers have long since settled on these values by the time a voice plays
	volumeSmoother_.b = vol_;
	distortion_.DriveParamFilter.b = distortion_.shape;

	blockPos_ = kBlockSize;
}

void Voice::applyAllValues(const Values& values) {
	for (int32_t p = 0; p < kNumParams; p++) {
		const auto param = static_cast<Param>(p);
		// The voices share some state (the amp envelope's repeat count, say), and the LXR has no such setting
		// for a voice that doesn't use it
		if (!isRelevant(param, model_)) {
			continue;
		}
		applyValue(param, values[p]);
	}
	applied_ = values;
}

void Voice::applyValue(Param param, float value) {
	// For a stored setting n this hands the parameter handler n and n / 127, exactly like the LXR
	applyParam(param, static_cast<uint8_t>(floatToU32(value * 127 + 0.5f)), value, false);
}

void Voice::applyParam(Param param, uint8_t itg, float flt, bool isModulator) {
	// The LXR's isFloatModulator scaling for parameters that pick from a list
	auto pick = [&](uint8_t numOptions) -> uint8_t {
		if (!isModulator) {
			return itg;
		}
		const auto option = static_cast<uint8_t>(floatToU32(numOptions * flt));
		return option >= numOptions ? numOptions - 1 : option;
	};
	auto setCoarse = [itg](OscInfo* osc) {
		osc->midiFreq &= 0x00ff;
		osc->midiFreq |= itg << 8;
		osc_recalcFreq(osc);
	};

	switch (param) {
	case Param::OSC_WAVE:
		osc_.waveform = pick(NUM_OSC_WAVEFORMS);
		break;
	case Param::COARSE:
		setCoarse(&osc_);
		break;
	case Param::FINE:
		osc_.midiFreq &= 0xff00;
		osc_.midiFreq |= itg;
		osc_recalcFreq(&osc_);
		break;
	case Param::PULSE_WIDTH:
		pwm_ = flt;
		break;
	case Param::NOISE_FREQ:
		noiseOsc_.freq = flt * 22000;
		break;
	case Param::MIX:
		mix_ = flt;
		break;
	case Param::MOD1_FREQ:
		setCoarse(&modOsc_);
		break;
	case Param::MOD1_GAIN:
		fmModAmount_ = flt;
		break;
	case Param::MOD1_WAVE:
		modOsc_.waveform = pick(NUM_OSC_WAVEFORMS);
		break;
	case Param::MOD2_FREQ:
		setCoarse(&modOsc2_);
		break;
	case Param::MOD2_GAIN:
		fmModAmount2_ = flt;
		break;
	case Param::MOD2_WAVE:
		modOsc2_.waveform = pick(NUM_OSC_WAVEFORMS);
		break;
	case Param::FM_MODE:
		mixOscs_ = itg != 0;
		break;
	case Param::PITCH_DECAY:
		DecayEg_setDecay(&oscPitchEg_, itg);
		break;
	case Param::PITCH_AMOUNT:
		egPitchModAmount_ = calcPitchModAmount(itg);
		break;
	case Param::PITCH_SLOPE:
		DecayEg_setSlope(&oscPitchEg_, itg);
		break;
	case Param::ATTACK:
		slopeEg2_setAttack(&oscVolEg_, itg, false);
		break;
	case Param::DECAY:
		if (model_ == Model::HIHAT) {
			hatDecay_ = slopeEg2_calcDecay(itg);
		}
		else {
			slopeEg2_setDecay(&oscVolEg_, itg, false);
		}
		break;
	case Param::SLOPE:
		slopeEg2_setSlope(&oscVolEg_, itg);
		break;
	case Param::REPEAT:
		oscVolEg_.repeat = itg;
		break;
	case Param::TRANSIENT_WAVE:
		// modulation reaches the samples' range only, as on the LXR
		transient_setWaveform(&transGen_, isModulator ? pick(kNumTransientWaveforms - 2) : itg);
		break;
	case Param::TRANSIENT_LEVEL:
		transGen_.volume = flt;
		break;
	case Param::TRANSIENT_PITCH:
		transGen_.pitch = 1.f + ((itg / 33.9f) - 0.75f); // range about 0.25 to 4
		break;
	case Param::FILTER_TYPE:
		if (isModulator) {
			const auto type = static_cast<uint8_t>(floatToU32(static_cast<float>(FILTER_NAIVE_2_POLE) * flt + 1));
			filterType_ = type >= FILTER_OFF ? FILTER_OFF - 1 : type;
		}
		else {
			filterType_ = itg + 1; // +1 because 0 is filter off which results in silence
		}
		break;
	case Param::FILTER_FREQ:
		// exponential full range freq
		SVF_directSetFilterValue(&filter_, valueShaperF2F(flt, kFilterShaper));
		break;
	case Param::FILTER_RESO:
		SVF_setReso(&filter_, flt);
		break;
	case Param::FILTER_DRIVE:
		SVF_setDrive(&filter_, flt);
		break;
	case Param::DRIVE:
		setDistortionShape(&distortion_, flt);
		break;
	case Param::SAMPLE_RATE:
		decimationRate_ = valueShaperF2F(flt, -0.7f);
		break;
	case Param::LEVEL:
		vol_ = flt;
		break;
	case Param::VELO_TO_LEVEL:
		volumeMod_ = itg != 0;
		break;
	case Param::VELO_AMOUNT:
		veloAmount_ = flt;
		break;
	case Param::VELO_DEST:
		veloDestination_ = itg;
		break;
	case Param::LFO_WAVE:
		lfo_.waveform = pick(NUM_LFO_WAVEFORMS);
		break;
	case Param::LFO_RATE:
		lfo_setFreq(&lfo_, flt);
		break;
	case Param::LFO_SYNC:
		lfo_setSync(&lfo_, itg);
		break;
	case Param::LFO_DEPTH:
		lfoAmount_ = flt;
		break;
	case Param::LFO_RETRIGGER:
		lfo_.retrigger = itg;
		break;
	case Param::LFO_PHASE:
		lfo_.phaseOffset = floatToU32(flt * 4294967296.f);
		break;
	case Param::LFO_DEST:
		lfoDestination_ = itg;
		break;
	}
}

bool Voice::isValidDestination(uint8_t destination) const {
	return destination >= 1 && destination <= kNumParams
	       && isModDestination(static_cast<Param>(destination - 1), model_);
}

void Voice::applyBaseValue(const Values& values, uint8_t destination) {
	if (!isValidDestination(destination)) {
		return;
	}
	const auto param = static_cast<Param>(destination - 1);
	applyValue(param, values[destination - 1]);
}

void Voice::applyModulation(const Values& values, uint8_t destination, float amount, float value) {
	if (!isValidDestination(destination)) {
		return;
	}
	const auto param = static_cast<Param>(destination - 1);
	const float originalValue = values[destination - 1];
	// value is [0:1], make it [-1:1] for bipolar modulation around the original value
	float newValue = originalValue + amount * (value * 2.f - 1.f);
	newValue = newValue < 0.f ? 0 : newValue > 1.f ? 1.f : newValue;
	applyParam(param, static_cast<uint8_t>(floatToU32(newValue * 127)), newValue, true);
}

void Voice::trigger(const Values& values, uint8_t velocity, uint32_t sampleTime) {
	// lfo_retrigger(). Without retrigger the LXR's LFO runs continuously, so pick up the phase it would have now.
	if (lfo_.retrigger) {
		lfo_.phase = lfo_.phaseOffset;
	}
	else {
		lfo_.phase = (sampleTime / kBlockSize) * floatToU32(lfo_.phaseInc * lfo_.modNodeValue);
	}

	// update velocity modulation
	veloValue_ = velocity / 127.f;
	applyModulation(values, veloDestination_, veloAmount_, veloValue_);

	float offset = 1;
	if (transGen_.waveform == 1) { // offset mode
		offset -= transGen_.volume;
	}

	switch (model_) {
	case Model::DRUM:
		if (osc_.waveform == SINE) {
			osc_.phase = floatToU32(1024 + ((0x3ff << 20) - 1024) * offset);
		}
		else if (osc_.waveform > SINE && osc_.waveform <= REC) {
			osc_.phase = floatToU32((0xff << 22) * offset);
		}
		else {
			osc_.phase = 0;
		}

		// reset mod osc phase
		if (modOsc_.waveform == SINE) {
			modOsc_.phase = 1024 + ((0x3ff << 20) - 1024);
		}
		else if (modOsc_.waveform > SINE && modOsc_.waveform <= REC) {
			modOsc_.phase = (0xff << 22);
		}
		else {
			modOsc_.phase = 0;
		}

		osc_setBaseNote(&osc_, kDefaultNote);
		osc_setBaseNote(&modOsc_, kDefaultNote);

		DecayEg_trigger(&oscPitchEg_);
		slopeEg2_trigger(&oscVolEg_);
		velo_ = velocity / 127.f;

		transient_trigger(&transGen_);
		SnapEg_trigger(&snapEg_);

		// reset filter coeffs to prevent wrong transient
		SVF_reset(&filter_);
		break;

	case Model::SNARE:
		if (osc_.waveform == SINE) {
			osc_.phase = floatToU32((0x3ff << 20) * offset);
		}
		else if (osc_.waveform > SINE && osc_.waveform <= REC) {
			osc_.phase = floatToU32((0xff << 22) * offset);
		}
		else {
			osc_.phase = 0;
		}

		DecayEg_trigger(&oscPitchEg_);
		slopeEg2_trigger(&oscVolEg_);
		velo_ = velocity / 127.f;

		osc_setBaseNote(&osc_, kDefaultNote);

		transient_trigger(&transGen_);
		SnapEg_trigger(&snapEg_);
		break;

	case Model::CYMBAL:
	case Model::HIHAT:
		if (osc_.waveform == SINE) {
			osc_.phase = floatToU32((0x3ff << 20) * offset);
		}
		else if (osc_.waveform > SINE && osc_.waveform <= REC) {
			osc_.phase = floatToU32((0xff << 22) * offset);
		}
		else {
			osc_.phase = 0;
		}

		modOsc_.phase = 0;
		modOsc2_.phase = 0;

		osc_setBaseNote(&osc_, kDefaultNote);
		osc_setBaseNote(&modOsc_, kDefaultNote);
		osc_setBaseNote(&modOsc2_, kDefaultNote);

		if (model_ == Model::HIHAT) {
			// The LXR picks decayClosed or decayOpen here; a Deluge hihat row has just the one
			oscVolEg_.decay = hatDecay_;
		}

		slopeEg2_trigger(&oscVolEg_);
		velo_ = velocity / 127.f;

		transient_trigger(&transGen_);
		SnapEg_trigger(&snapEg_);
		break;
	}

	finished_ = false;
	blockPos_ = kBlockSize;
}

bool Voice::render(Model model, const Values& values, int32_t* out, int32_t numSamples, float pitchRatio, float bpm) {
	// The voice circuit can't change under a playing voice. The sound editor stops voices when it changes,
	// but stay silent rather than render one model's state as another.
	if (model != model_) {
		finished_ = true;
	}
	else {
		for (int32_t p = 0; p < kNumParams; p++) {
			if (values[p] != applied_[p]) {
				applied_[p] = values[p];
				if (isRelevant(static_cast<Param>(p), model_)) {
					applyValue(static_cast<Param>(p), values[p]);
				}
			}
		}
	}

	if (bpm != lfo_.bpm) {
		lfo_setBpm(&lfo_, bpm);
	}

	osc_.externalPitch = pitchRatio;
	modOsc_.externalPitch = pitchRatio;
	modOsc2_.externalPitch = pitchRatio;

	while (numSamples > 0) {
		if (blockPos_ >= kBlockSize) {
			if (finished_) {
				std::fill(out, out + numSamples, 0);
				return false;
			}
			computeBlock(values);
			blockPos_ = 0;
		}
		const int32_t numThisTime = std::min(numSamples, kBlockSize - blockPos_);
		for (int32_t i = 0; i < numThisTime; i++) {
			*(out++) = block_[blockPos_++];
		}
		numSamples -= numThisTime;
	}
	return !finished_;
}

// mixer_calcNextSampleBlock(), for one voice
void Voice::computeBlock(const Values& values) {
	// modNode_resetTargets(): undo last block's modulation
	applyBaseValue(values, veloDestination_);
	applyBaseValue(values, lfoDestination_);

	// modNode_reassignVeloMod()
	applyModulation(values, veloDestination_, veloAmount_, veloValue_);

	// lfo_dispatchNextValue()
	const float lfoValue = lfo_calc(&lfo_, rng_);
	applyModulation(values, lfoDestination_, lfoAmount_, lfoValue);

	// update filter frequencies
	SVF_recalcFreq(&filter_);

	calcAsync();
	calcSyncBlock(block_);

	const bool silent = std::all_of(block_, block_ + kBlockSize, [](int16_t sample) { return sample == 0; });

	decimateBlock(block_);

	// Once the amp envelope has closed and the decimator holds silence, the voice can only render silence from
	// here on. At a sample rate of 0 the decimator never lets go of its sample; the LXR keeps playing that as DC
	// until the rate moves again, which only the LFO can do now.
	const bool decimatorFrozen =
	    decimationRate_ <= 0.f && !(lfoDestination_ == static_cast<uint8_t>(Param::SAMPLE_RATE) + 1 && lfoAmount_ > 0);
	if (silent && envelopeFinished() && (decimationSample_ == 0 || decimatorFrozen)) {
		finished_ = true;
	}
}

bool Voice::envelopeFinished() const {
	// The drum ramps its gain from one block's envelope value to the next, and the envelope can dip below 0 for
	// a block before it settles there
	if (model_ == Model::DRUM && ampFilterInput_ != 0) {
		return false;
	}
	switch (oscVolEg_.state) {
	case EG_STOPPED:
		return true;
	case EG_D:
		return oscVolEg_.value <= 0;
	case EG_A:
		// With attack at 127 the envelope never opens, unless the LFO moves the attack time
		return oscVolEg_.attack <= 0 && oscVolEg_.value <= 0
		       && !(lfoDestination_ == static_cast<uint8_t>(Param::ATTACK) + 1 && lfoAmount_ > 0);
	default:
		return false;
	}
}

// calcDrumVoiceAsync(), Snare_calcAsync(), Cymbal_calcAsync(), HiHat_calcAsync()
void Voice::calcAsync() {
	switch (model_) {
	case Model::DRUM: {
		// add modulation eg to osc freq (1 = no change. a+eg = original freq + modulation
		const float egPitchVal = DecayEg_calc(&oscPitchEg_);
		const float pitchEgValue = egPitchVal * egPitchModAmount_;
		osc_.pitchMod = 1 + pitchEgValue;

		// calc snap EG if transient sample 0 is activated
		if (transGen_.waveform == 0) {
			const float snapVal = SnapEg_calc(&snapEg_, transGen_.pitch);
			osc_.pitchMod += snapVal * transGen_.volume;
		}

		// fm amount with pitch eg
		osc_.fmMod = fmModAmount_ * egPitchVal;

		// check if in attack phase
		if ((oscVolEg_.attack == 1) && ((oscVolEg_.state == EG_A) || (oscVolEg_.state == EG_REPEAT))) {
			// if attack is set to 0 -> no interpolation
			ampFilterInput_ = slopeEg2_calc(&oscVolEg_);
			lastGain_ = ampFilterInput_;
		}
		else {
			lastGain_ = ampFilterInput_;
			ampFilterInput_ = slopeEg2_calc(&oscVolEg_);
		}

		// update osc phaseInc
		osc_setFreq(&osc_);
		osc_setFreq(&modOsc_);
	} break;

	case Model::SNARE: {
		const float egPitchVal = DecayEg_calc(&oscPitchEg_);
		const float pitchEgValue = egPitchVal * egPitchModAmount_;
		osc_.pitchMod = 1 + pitchEgValue;

		// calc the osc vol eg
		egValueOscVol_ = static_cast<float>(slopeEg2_calc(&oscVolEg_));

		if (transGen_.waveform == 0) {
			const float snapVal = SnapEg_calc(&snapEg_, transGen_.pitch);
			osc_.pitchMod += snapVal * transGen_.volume;
		}

		osc_setFreq(&osc_);
		osc_setFreq(&noiseOsc_);
	} break;

	case Model::CYMBAL:
	case Model::HIHAT:
		egValueOscVol_ = static_cast<float>(slopeEg2_calc(&oscVolEg_));

		if (transGen_.waveform == 0) {
			const float snapVal = SnapEg_calc(&snapEg_, transGen_.pitch);
			osc_.pitchMod = 1 + snapVal * transGen_.volume;
		}

		osc_setFreq(&osc_);
		osc_setFreq(&modOsc_);
		osc_setFreq(&modOsc2_);
		break;
	}
}

// calcDrumVoiceSyncBlock(), Snare_calcSyncBlock(), Cymbal_calcSyncBlock(), HiHat_calcSyncBlock()
void Voice::calcSyncBlock(int16_t* buf) {
	constexpr uint8_t size = kBlockSize;

	switch (model_) {
	case Model::DRUM: {
		int16_t modBuf[size];

		// calc next mod osc sampleBlock
		calcNextOscSampleBlock(&modOsc_, modBuf, size, fmModAmount_, pwm_, rng_);

		if (mixOscs_) {
			calcNextOscSampleBlock(&osc_, buf, size, (1.f - fmModAmount_), pwm_, rng_);
			addBuffersSaturating(buf, modBuf);
		}
		else {
			calcNextOscSampleFmBlock(&osc_, modBuf, buf, size, 1.0f, pwm_, rng_);
		}

		// calc transient sample and mix it in
		transient_calcBlock(&transGen_, modBuf, size);
		addBuffersSaturating(buf, modBuf);

		SVF_calcBlockZDF(&filter_, filterType_, buf, size);

		// attenuate main OSCs by amp EG
		addGainInterpolated(buf, static_cast<float>(ampFilterInput_), static_cast<float>(lastGain_));

		// MIDI velocity
		if (volumeMod_) {
			addGain(buf, velo_);
		}

		calcDistBlockFiltered(&distortion_, buf, size);

		// channel volume (the LXR also multiplies in its hardware fader here)
		for (int32_t i = 0; i < size; i++) {
			const float mixVol = calcOnePole(&volumeSmoother_, vol_);
			combinedVolume_ = mixVol;
			buf[i] = floatToS16(buf[i] * combinedVolume_);
		}
	} break;

	case Model::SNARE: {
		int16_t transBuf[size];
		int16_t oscBuf[size];

		calcNoiseBlock(&noiseOsc_, buf, size, 0.9f, rng_);
		SVF_calcBlockZDF(&filter_, filterType_, buf, size);

		// calc next osc sample
		calcNextOscSampleBlock(&osc_, oscBuf, size, (1.f - mix_), pwm_, rng_);

		// calc transient sample
		transient_calcBlock(&transGen_, transBuf, size);

		if (volumeMod_) {
			combinedVolume_ = vol_ * velo_ * egValueOscVol_;
		}
		else {
			combinedVolume_ = vol_ * egValueOscVol_;
		}

		for (int32_t j = 0; j < size; j++) {
			// noise * mix + osc, plus the transient (fmaf() on the LXR)
			float tmp = buf[j] * mix_ + oscBuf[j];
			tmp += transBuf[j];
			buf[j] = floatToS16(combinedVolume_ * tmp);
		}

		calcDistBlock(&distortion_, buf, size);
	} break;

	case Model::CYMBAL:
	case Model::HIHAT: {
		int16_t mod[size];
		int16_t mod2[size];

		// calc next mod osc samples, scaled with mod amount
		calcNextOscSampleBlock(&modOsc_, mod, size, fmModAmount_, pwm_, rng_);
		calcNextOscSampleBlock(&modOsc2_, mod2, size, fmModAmount2_, pwm_, rng_);

		// combine both mod oscs to 1 modulation signal
		addBuffersSaturating(mod, mod2);

		calcNextOscSampleFmBlock(&osc_, mod, buf, size, model_ == Model::HIHAT ? 0.5f : 1.f, pwm_, rng_);

		SVF_calcBlockZDF(&filter_, filterType_, buf, size);

		// calc transient sample
		transient_calcBlock(&transGen_, mod, size);

		if (volumeMod_) {
			combinedVolume_ = vol_ * velo_ * egValueOscVol_;
		}
		else {
			combinedVolume_ = vol_ * egValueOscVol_;
		}

		// add the transient, then the volume
		addBuffersSaturating(buf, mod);
		for (int32_t j = 0; j < size; j++) {
			buf[j] = floatToS16(buf[j] * combinedVolume_);
		}

		calcDistBlock(&distortion_, buf, size);
	} break;
	}
}

// mixer_decimateBlock()
void Voice::decimateBlock(int16_t* buf) {
	// the LXR multiplies in a global decimation rate here too, which this port doesn't have
	const float phaseInc = decimationRate_;
	for (int32_t i = 0; i < kBlockSize; i++) {
		decimationCount_ += phaseInc;
		if (decimationCount_ >= 1.f) {
			decimationCount_ -= 1.f;
			decimationSample_ = buf[i];
		}
		buf[i] = decimationSample_;
	}
}

} // namespace deluge::dsp::lxr

#endif // FEATURE_LXR_ENGINE
