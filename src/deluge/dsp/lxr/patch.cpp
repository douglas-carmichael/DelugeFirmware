// Settings of one LXR drum voice. See patch.h for where this comes from and what changed.
//
// Copyright (C) 2012-2026 Erica Synths
// SPDX-License-Identifier: GPL-3.0-only
//
// This file is derived from the LXR firmware. It is licensed under the GNU General Public License version 3,
// with the additional terms in NOTICE.md in this directory.

#include "dsp/lxr/patch.h"
#include <algorithm>

namespace deluge::dsp::lxr {

namespace {

struct VoiceDefaults {
	uint8_t oscWave, coarse, fine;
	uint8_t mod1Freq, mod1Gain, mod2Freq, mod2Gain;
	uint8_t pitchDecay, pitchAmount, pitchSlope;
	uint8_t decay, slope;
	uint8_t filterType, filterFreq;
};

/// The LXR init kit (Core/Src/Preset/initKit.c), voice by voice. Settings not listed here are the same for
/// every voice in that kit; see Patch::loadDefaults().
constexpr VoiceDefaults kInitKit[kNumVoiceTypes] = {
    // wave coarse fine | mod1 f/g, mod2 f/g | pitch decay/amount/slope | decay slope | filter type/freq
    {0, 31, 63, 0, 0, 0, 0, 26, 44, 22, 21, 46, 0, 127},    // DRUM_1
    {0, 41, 63, 0, 0, 0, 0, 93, 14, 10, 13, 11, 0, 127},    // DRUM_2
    {0, 58, 63, 0, 0, 0, 0, 7, 127, 14, 3, 4, 0, 127},      // DRUM_3
    {1, 45, 64, 0, 0, 0, 0, 14, 81, 15, 10, 10, 0, 127},    // SNARE
    {4, 127, 63, 0, 0, 0, 0, 0, 0, 0, 107, 0, 1, 68},       // CYMBAL
    {0, 50, 63, 56, 127, 127, 127, 0, 0, 0, 5, 6, 1, 104},  // CLOSED_HAT (PAR_VELOD6_CLOSED)
    {0, 50, 63, 56, 127, 127, 127, 0, 0, 0, 17, 6, 1, 104}, // OPEN_HAT (PAR_VELOD6_OPEN)
};

} // namespace

uint8_t maxValue(Param param) {
	switch (param) {
	case Param::OSC_WAVE:
	case Param::MOD1_WAVE:
	case Param::MOD2_WAVE:
		return 5;
	case Param::FM_MODE:
	case Param::VELO_TO_LEVEL:
	case Param::LFO_RETRIGGER:
		return 1;
	case Param::TRANSIENT_WAVE:
		return 13;
	case Param::FILTER_TYPE:
	case Param::LFO_WAVE:
		return 7;
	case Param::LFO_SYNC:
		return 11;
	case Param::VELO_DEST:
	case Param::LFO_DEST:
		return kNumParams;
	default:
		return 127;
	}
}

bool isRelevant(Param param, Model model) {
	switch (param) {
	case Param::NOISE_FREQ:
	case Param::MIX:
		return model == Model::SNARE;
	case Param::MOD1_FREQ:
	case Param::MOD1_GAIN:
	case Param::MOD1_WAVE:
		return model != Model::SNARE;
	case Param::MOD2_FREQ:
	case Param::MOD2_GAIN:
	case Param::MOD2_WAVE:
		return model == Model::CYMBAL || model == Model::HIHAT;
	case Param::FM_MODE:
		return model == Model::DRUM;
	case Param::PITCH_DECAY:
	case Param::PITCH_AMOUNT:
	case Param::PITCH_SLOPE:
		return model == Model::DRUM || model == Model::SNARE;
	case Param::REPEAT:
		return model == Model::SNARE || model == Model::CYMBAL;
	default:
		return true;
	}
}

bool isModDestination(Param param, Model model) {
	switch (param) {
	case Param::VELO_DEST:
	case Param::LFO_DEST:
	case Param::LFO_RETRIGGER:
		return false;
	default:
		return isRelevant(param, model);
	}
}

int32_t displayOffset(Param param) {
	switch (param) {
	case Param::COARSE:
	case Param::MOD1_FREQ:
	case Param::MOD2_FREQ:
		return 60;
	case Param::FINE:
		return 63;
	default:
		return 0;
	}
}

void Patch::loadDefaults(VoiceType type) {
	voiceType_ = type;
	const VoiceDefaults& d = kInitKit[static_cast<int32_t>(type)];

	params_.fill(0);
	auto put = [this](Param param, uint8_t value) { params_[static_cast<int32_t>(param)] = value; };
	put(Param::OSC_WAVE, d.oscWave);
	put(Param::COARSE, d.coarse);
	put(Param::FINE, d.fine);
	put(Param::NOISE_FREQ, 127);
	put(Param::MIX, 61);
	put(Param::MOD1_FREQ, d.mod1Freq);
	put(Param::MOD1_GAIN, d.mod1Gain);
	put(Param::MOD2_FREQ, d.mod2Freq);
	put(Param::MOD2_GAIN, d.mod2Gain);
	put(Param::PITCH_DECAY, d.pitchDecay);
	put(Param::PITCH_AMOUNT, d.pitchAmount);
	put(Param::PITCH_SLOPE, d.pitchSlope);
	put(Param::DECAY, d.decay);
	put(Param::SLOPE, d.slope);
	put(Param::FILTER_TYPE, d.filterType);
	put(Param::FILTER_FREQ, d.filterFreq);
	put(Param::DRIVE, 63);
	put(Param::SAMPLE_RATE, 127);
	put(Param::LEVEL, 127);
}

void Patch::set(Param param, int32_t value) {
	params_[static_cast<int32_t>(param)] = static_cast<uint8_t>(std::clamp<int32_t>(value, 0, maxValue(param)));
}

void Patch::getValues(Values& values) const {
	for (int32_t p = 0; p < kNumParams; p++) {
		values[p] = params_[p] / 127.f;
	}
}

bool Patch::sustainsForever() const {
	if (get(Param::DECAY) == 127) {
		return true;
	}
	return isRelevant(Param::REPEAT, model()) && get(Param::REPEAT) != 0 && get(Param::ATTACK) == 127;
}

void Patch::serialize(uint8_t (&out)[kSerializedSize]) const {
	out[0] = 1; // format version
	out[1] = static_cast<uint8_t>(voiceType_);
	std::copy(params_.begin(), params_.end(), &out[2]);
}

void Patch::deserialize(const uint8_t* data, int32_t length) {
	if (length < 2) {
		return;
	}
	loadDefaults(static_cast<VoiceType>(std::min<int32_t>(data[1], kNumVoiceTypes - 1)));
	const int32_t numParams = std::min<int32_t>(length - 2, kNumParams);
	for (int32_t p = 0; p < numParams; p++) {
		set(static_cast<Param>(p), data[2 + p]);
	}
}

} // namespace deluge::dsp::lxr
