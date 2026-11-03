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
#include "memory/general_memory_allocator.h"
#include <algorithm>
#include <new>

namespace deluge::dsp::amp {
namespace {

// The Deluge mixes in 32-bit integers with about 42 dB of headroom: a track at full output is about 2^24 of 2^31.
// The AMP was voiced on a guitar input at about -34 dBFS RMS in float. Measured on a Deluge with three synth presets
// (a bell, a lead and a bass, whose levels spread over 17 dB): kInputGain (+33.1 dB) drives the middle one, the bass
// at -24 dB RMS re 2^24, about as hard as that guitar; kOutputGain (-57.1 dB) brings the AMP back to within 1 dB of
// the bass's own level at the AMP's design settings. Quieter sounds come out a little louder and louder ones a little
// quieter (+5.8 and -5.4 dB for the bell and the lead): the AMP compresses, as distortion does.
constexpr float kInputGain = 45.4f;
constexpr float kOutputGain = 1.39e-3f;
constexpr float kSampleRate = 44100.0f;

} // namespace

/// Everything the AMP needs while it runs, in one allocation: the DSP and the float buffer it works in (kept off the
/// stack, which is deep by the time a track renders)
struct AmpEffect::State {
	AmpSim sim;
	alignas(16) float buf[kMaxBlock];
};

bool AmpEffect::setOn(bool on) {
	if (!on) {
		release();
		return true;
	}
	if (state_ == nullptr) {
		memory_ = GeneralMemoryAllocator::get().allocMaxSpeed(sizeof(State) + 15);
		if (memory_ == nullptr) {
			return false;
		}
		const auto aligned = (reinterpret_cast<uintptr_t>(memory_) + 15) & ~uintptr_t{15};
		state_ = new (reinterpret_cast<void*>(aligned)) State();
		state_->sim.configure(settings_, kSampleRate);
	}
	return true;
}

void AmpEffect::setSettings(const Settings& s) {
	settings_ = s;
	if (state_ != nullptr) {
		state_->sim.configure(settings_, kSampleRate);
	}
}

void AmpEffect::process(std::span<q31_t> mono) {
	if (state_ == nullptr) {
		return;
	}
	float* buf = state_->buf;
	for (size_t pos = 0; pos < mono.size(); pos += kMaxBlock) {
		const size_t n = std::min(kMaxBlock, mono.size() - pos);
		q31_t* q = mono.data() + pos;
		for (size_t i = 0; i < n; i++) {
			buf[i] = q31_to_float(q[i]) * kInputGain;
		}
		state_->sim.process(buf, n);
		for (size_t i = 0; i < n; i++) {
			q[i] = q31_from_float(buf[i] * kOutputGain);
		}
	}
}

void AmpEffect::process(std::span<StereoSample> stereo) {
	if (state_ == nullptr) {
		return;
	}
	float* buf = state_->buf;
	for (size_t pos = 0; pos < stereo.size(); pos += kMaxBlock) {
		const size_t n = std::min(kMaxBlock, stereo.size() - pos);
		StereoSample* s = stereo.data() + pos;
		for (size_t i = 0; i < n; i++) {
			buf[i] = (q31_to_float(s[i].l) + q31_to_float(s[i].r)) * (0.5f * kInputGain);
		}
		state_->sim.process(buf, n);
		for (size_t i = 0; i < n; i++) {
			const q31_t v = q31_from_float(buf[i] * kOutputGain);
			s[i].l = v;
			s[i].r = v;
		}
	}
}

void AmpEffect::reset() {
	if (state_ != nullptr) {
		state_->sim.reset();
	}
}

void AmpEffect::release() {
	if (state_ != nullptr) {
		state_->~State();
		state_ = nullptr;
	}
	if (memory_ != nullptr) {
		GeneralMemoryAllocator::get().dealloc(memory_);
		memory_ = nullptr;
	}
}

} // namespace deluge::dsp::amp
