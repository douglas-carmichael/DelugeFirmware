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

#pragma once

#include "dsp/amp/amp_settings.h"
#include "dsp/stereo_sample.h"
#include "util/fixedpoint.h"
#include <cstdint>
#include <span>

namespace deluge::dsp::amp {

/// The AMP on one track (a synth, a drum, a kit or an audio track): its settings, and the AmpSim it runs on, which
/// lives in fast internal RAM while the AMP is on. Converts to and from the Deluge's 32-bit integer audio.
///
/// Switching on and changing settings allocate and set up; do them from the UI or while loading, never from the
/// audio routine. process() never allocates: until the AMP is on and set up, it leaves the audio as it is.
class AmpEffect {
public:
	AmpEffect() = default;
	AmpEffect(const AmpEffect&) = delete;
	AmpEffect& operator=(const AmpEffect&) = delete;
	~AmpEffect() { release(); }

	/// How long the AMP rings on after its input stops: its filters and cab have died away by then
	static constexpr int32_t kTailSamples = 4410; // 0.1 s

	[[nodiscard]] bool isOn() const { return state_ != nullptr; }
	/// Switching on takes the AMP's memory; returns false (and stays off) if none was free. Off gives it back.
	bool setOn(bool on);
	[[nodiscard]] const Settings& settings() const { return settings_; }
	/// New settings; if the AMP is on, it starts again from silence
	void setSettings(const Settings& s);

	/// Mono, in place: the AMP's output replaces its input
	void process(std::span<q31_t> mono);
	/// Stereo in, mixed to mono, and the AMP's output on both sides
	void process(std::span<StereoSample> stereo);
	/// Silences the AMP's state (its tail), e.g. when its track stops rendering
	void reset();

private:
	struct State;
	void release();

	State* state_ = nullptr;
	void* memory_ = nullptr;
	Settings settings_{};
};

} // namespace deluge::dsp::amp
