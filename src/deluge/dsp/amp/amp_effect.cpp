/*
 * Copyright © 2026 Douglas Carmichael
 *
 * This file is part of The Synthstrom Audible Deluge Firmware.
 *
 * The Synthstrom Audible Deluge Firmware is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <https://www.gnu.org/licenses/>.
 */

#include "dsp/amp/amp_effect.h"
#include "dsp/amp/amp_sim.h"
#include <algorithm>
#include <new>

namespace deluge::dsp::amp {
namespace {

// The Deluge mixes in 32-bit integers with about 42 dB of headroom: a track at full output is about 2^24 of 2^31.
// The AMP was voiced on a guitar input at about -34 dBFS RMS in float. Measured on a Deluge with three synth presets
// (a bell, a lead and a bass, whose levels spread over 17 dB): kInputGain (+33.1 dB) drives the middle one, the bass
// at -24 dB RMS re 2^24, about as hard as that guitar. At GAIN 8 (of 10) the AMP then came out at -0.8 dB for the
// bass, +5.8 for the bell and -5.4 for the lead, against their own levels, with an output gain of -57.1 dB. With the
// knobs at their defaults (all in the middle) the AMP is 4.9 dB quieter on average over the three (worked out on the
// host from those measurements), so kOutputGain (-52.2 dB) makes that up: at the defaults, LEVEL in the middle is
// about as loud as the AMP off. The AMP compresses, as distortion does: quiet sounds come out a little louder.
constexpr float kInputGain = 45.4f;
constexpr float kOutputGain = 2.44e-3f;
// A whole kit came in about 14 dB hotter than a synth on the Deluge (the TR-909 kit, four rows hit together: -10.5 dB
// RMS re 2^24, against -24.0 for the bass), so at the defaults the AMP drove it far harder and it came out 10 dB
// quieter than the kit without the AMP. Input::BUS (kits and audio tracks) gets 12 dB less input and 12 dB more
// output. Audio tracks weren't measured; they get the kits' trim.
constexpr float kBusTrim = 0.25f; // -12 dB: a power of two, so the trim adds no rounding
constexpr float kSampleRate = 44100.0f;
constexpr float kFadeStepSize = 1.0f / static_cast<float>(AmpEffect::kFadeSamples);

/// Whether a sample is within +-kQuietLevel (unsigned wraparound makes this one compare)
[[gnu::always_inline]] inline bool quiet(q31_t v) {
	return static_cast<uint32_t>(v) + AmpEffect::kQuietLevel < 2 * AmpEffect::kQuietLevel;
}

bool isQuiet(std::span<const q31_t> block) {
	return std::ranges::all_of(block, [](q31_t v) { return quiet(v); });
}

bool isQuiet(std::span<const StereoSample> block) {
	return std::ranges::all_of(block, [](const StereoSample& s) { return quiet(s.l) && quiet(s.r); });
}

/// out = dry + (amp - dry) * wet, with wet moving by step each sample (clamped to 0..1); returns the last wet
[[gnu::always_inline]] inline float mix(q31_t& out, float amp, float& wet, float step) {
	wet = std::clamp(wet + step, 0.0f, 1.0f);
	const float dry = q31_to_float(out);
	out = q31_from_float(dry + (amp - dry) * wet);
	return wet;
}

} // namespace

/// One AMP's DSP and the float buffer it works in (kept off the stack, which is deep by the time a track renders)
struct AmpEffect::State {
	AmpSim sim;
	alignas(16) float buf[kMaxBlock];
	void* memory; ///< what allocAmpMemory() returned, before aligning
};

AmpEffect::State* AmpEffect::allocState() {
	void* memory = allocAmpMemory(sizeof(State) + 15);
	if (memory == nullptr) {
		return nullptr;
	}
	const auto aligned = (reinterpret_cast<uintptr_t>(memory) + 15) & ~uintptr_t{15};
	auto* state = new (reinterpret_cast<void*>(aligned)) State();
	state->memory = memory;
	return state;
}

void AmpEffect::freeState(State*& state) {
	if (state == nullptr) {
		return;
	}
	void* memory = state->memory;
	state->~State();
	freeAmpMemory(memory);
	state = nullptr;
}

bool AmpEffect::setEnabled(bool on) {
	if (!on) {
		enabled_ = false;
		wetTarget_ = 0.0f;
		if (asleep_ || wet_ == 0.0f) {
			release(); // nothing to fade
		}
		return true;
	}
	if (!isActive()) {
		states_[0] = allocState();
		if (settings_.channels == Channels::STEREO) {
			states_[1] = allocState();
		}
		if (states_[0] == nullptr || (settings_.channels == Channels::STEREO && states_[1] == nullptr)) {
			release();
			return false;
		}
		for (State* s : states_) {
			if (s != nullptr) {
				s->sim.configure(settings_, kSampleRate);
			}
		}
		wet_ = 0.0f; // fade in from the dry sound
		stereo_ = (settings_.channels == Channels::STEREO);
		pending_ = false;
		rightStale_ = false;
		asleep_ = false;
		quietSamples_ = 0;
	}
	enabled_ = true;
	if (!pending_) {
		wetTarget_ = 1.0f; // (a pending change fades back in once it's applied)
	}
	return true;
}

bool AmpEffect::setSettings(const Settings& s) {
	Settings next = s;
	next.knobs = settings_.knobs;
	if (next == settings_) {
		return true;
	}
	if (isActive() && next.channels == Channels::STEREO && states_[1] == nullptr) {
		states_[1] = allocState(); // set up when the change applies
		if (states_[1] == nullptr) {
			return false;
		}
	}
	settings_ = next;
	if (isActive()) {
		pending_ = true;
		wetTarget_ = 0.0f;
		if (asleep_) {
			applyPending(); // nothing to fade
			wet_ = wetTarget_ = enabled_ ? 1.0f : 0.0f;
		}
	}
	return true;
}

bool AmpEffect::copyFrom(const AmpEffect& other) {
	release();
	enabled_ = false;
	settings_ = other.settings_;
	if (!other.enabled_) {
		return true;
	}
	if (!setEnabled(true)) {
		return false;
	}
	wet_ = 1.0f; // a new sound: nothing to fade from
	return true;
}

void AmpEffect::setKnobs(const Knobs& k) {
	if (k == settings_.knobs) {
		return;
	}
	settings_.knobs = k;
	for (State* s : states_) {
		if (s != nullptr) {
			s->sim.setKnobs(k);
		}
	}
}

bool AmpEffect::prepareBlock(bool quiet, std::size_t n) {
	if (!quiet) {
		quietSamples_ = 0;
		asleep_ = false;
		return true;
	}
	if (!asleep_) {
		quietSamples_ += static_cast<int32_t>(n);
		if (quietSamples_ <= kTailSamples) {
			return true; // the tail rings on
		}
		resetStates();
		asleep_ = true;
	}
	// Asleep: nothing is audible, so any fade can finish at once
	wet_ = 0.0f;
	afterBlock();
	wet_ = wetTarget_;
	return false;
}

void AmpEffect::afterBlock() {
	if (wet_ > 0.0f) {
		return;
	}
	// Fully dry: the moment to change things
	if (!enabled_) {
		release();
		return;
	}
	if (pending_) {
		applyPending();
	}
	wetTarget_ = 1.0f;
}

void AmpEffect::applyPending() {
	pending_ = false;
	if (settings_.channels == Channels::MONO) {
		freeState(states_[1]);
	}
	for (State* s : states_) {
		if (s != nullptr) {
			s->sim.configure(settings_, kSampleRate);
		}
	}
	stereo_ = (states_[1] != nullptr);
	rightStale_ = false;
}

void AmpEffect::resetStates() {
	for (State* s : states_) {
		if (s != nullptr) {
			s->sim.reset();
		}
	}
	rightStale_ = false;
}

float AmpEffect::fadeStep() const {
	if (wet_ == wetTarget_) {
		return 0.0f;
	}
	return (wetTarget_ > wet_) ? kFadeStepSize : -kFadeStepSize;
}

void AmpEffect::process(std::span<q31_t> mono) {
	for (std::size_t pos = 0; pos < mono.size() && isActive(); pos += kMaxBlock) {
		const std::span<q31_t> block = mono.subspan(pos, std::min(kMaxBlock, mono.size() - pos));
		if (!prepareBlock(isQuiet(block), block.size())) {
			continue; // asleep: the near-silence passes through
		}
		State& left = *states_[0];
		float* buf = left.buf;
		for (std::size_t i = 0; i < block.size(); i++) {
			buf[i] = q31_to_float(block[i]) * kInputGain;
		}
		left.sim.process(buf, block.size());
		if (stereo_) {
			rightStale_ = true;
		}

		const float step = fadeStep();
		if (step == 0.0f && wet_ == 1.0f) {
			for (std::size_t i = 0; i < block.size(); i++) {
				block[i] = q31_from_float(buf[i] * kOutputGain);
			}
		}
		else {
			float wet = wet_;
			for (std::size_t i = 0; i < block.size(); i++) {
				mix(block[i], buf[i] * kOutputGain, wet, step);
			}
			wet_ = wet;
		}
		afterBlock();
	}
}

void AmpEffect::process(std::span<StereoSample> stereo, Input input) {
	const float inputGain = (input == Input::BUS) ? kInputGain * kBusTrim : kInputGain;
	const float outputGain = (input == Input::BUS) ? kOutputGain / kBusTrim : kOutputGain;
	for (std::size_t pos = 0; pos < stereo.size() && isActive(); pos += kMaxBlock) {
		const std::span<StereoSample> block = stereo.subspan(pos, std::min(kMaxBlock, stereo.size() - pos));
		if (!prepareBlock(isQuiet(block), block.size())) {
			continue; // asleep: the near-silence passes through
		}
		const std::size_t n = block.size();
		const float step = fadeStep();
		const bool steady = (step == 0.0f && wet_ == 1.0f);
		State& left = *states_[0];

		if (!stereo_) {
			// One AMP: mix to mono, and its output on both sides
			float* buf = left.buf;
			for (std::size_t i = 0; i < n; i++) {
				buf[i] = (q31_to_float(block[i].l) + q31_to_float(block[i].r)) * (0.5f * inputGain);
			}
			left.sim.process(buf, n);
			if (steady) {
				for (std::size_t i = 0; i < n; i++) {
					const q31_t v = q31_from_float(buf[i] * outputGain);
					block[i].l = v;
					block[i].r = v;
				}
			}
			else {
				float wet = wet_;
				for (std::size_t i = 0; i < n; i++) {
					const float amp = buf[i] * outputGain;
					float wetR = wet;
					mix(block[i].l, amp, wet, step);
					mix(block[i].r, amp, wetR, step);
				}
				wet_ = wet;
			}
		}
		else {
			// STEREO: an AMP per side. If the left one ran alone on mono blocks, the right one takes its state, as
			// both sides were the same.
			State& right = *states_[1];
			if (rightStale_) {
				right.sim = left.sim;
				rightStale_ = false;
			}
			for (std::size_t i = 0; i < n; i++) {
				left.buf[i] = q31_to_float(block[i].l) * inputGain;
				right.buf[i] = q31_to_float(block[i].r) * inputGain;
			}
			left.sim.process(left.buf, n);
			right.sim.process(right.buf, n);
			if (steady) {
				for (std::size_t i = 0; i < n; i++) {
					block[i].l = q31_from_float(left.buf[i] * outputGain);
					block[i].r = q31_from_float(right.buf[i] * outputGain);
				}
			}
			else {
				float wet = wet_;
				for (std::size_t i = 0; i < n; i++) {
					float wetR = wet;
					mix(block[i].l, left.buf[i] * outputGain, wet, step);
					mix(block[i].r, right.buf[i] * outputGain, wetR, step);
				}
				wet_ = wet;
			}
		}
		afterBlock();
	}
}

void AmpEffect::reset() {
	if (!isActive()) {
		return;
	}
	if (!enabled_) {
		release(); // it was fading off
		return;
	}
	if (pending_) {
		applyPending(); // configures, which clears the state too
	}
	else {
		resetStates();
	}
	wet_ = wetTarget_ = 1.0f;
	asleep_ = true; // until sound comes in, so switching off now needs no fade
	quietSamples_ = 0;
}

void AmpEffect::release() {
	freeState(states_[0]);
	freeState(states_[1]);
	pending_ = false;
	stereo_ = false;
	rightStale_ = false;
	asleep_ = false;
	quietSamples_ = 0;
	wet_ = 0.0f;
	wetTarget_ = 0.0f;
}

} // namespace deluge::dsp::amp
