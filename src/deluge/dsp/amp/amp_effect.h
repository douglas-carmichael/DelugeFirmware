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
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace deluge::dsp::amp {

/// The AMP's memory. The firmware takes it from fast internal RAM (amp_memory.cpp); the unit tests from the heap.
void* allocAmpMemory(std::size_t size);
void freeAmpMemory(void* memory);

/// The AMP on one track (a synth, a drum, a kit or an audio track): its settings, and the AmpSim it runs on (two in
/// STEREO), which live in fast internal RAM while the AMP is on. Converts to and from the Deluge's 32-bit audio.
///
/// - Switching on and choosing STEREO take memory: do them from the UI or while loading, never from the audio
///   routine. The audio routine only gives memory back (as the delay does).
/// - Switching on or off crossfades with the dry sound. Changing the cab, tone stack, quality or channels fades to
///   the dry sound, sets the AMP up again and fades back in. Each fade takes kFadeSamples.
/// - After kTailSamples of near silence in, the AMP sleeps: it clears its state and stops running (and costing CPU)
///   until sound comes in again.
class AmpEffect {
public:
	AmpEffect() = default;
	AmpEffect(const AmpEffect&) = delete;
	AmpEffect& operator=(const AmpEffect&) = delete;
	~AmpEffect() { release(); }

	/// How long the AMP rings on after its input goes quiet: its filters and cab have died away by then
	static constexpr int32_t kTailSamples = 4410; // 0.1 s
	/// The fades when the AMP is switched on or off, or a setting changes
	static constexpr int32_t kFadeSamples = 128; // 2.9 ms
	/// Input below this, in the Deluge's 32-bit audio, is near silence: about -102 dB below a track at full level
	static constexpr uint32_t kQuietLevel = 128;

	/// How hot the AMP's input is. SOUND: one sound (a synth, or a drum), after its voices. BUS: a whole track (a
	/// kit's drums together, or an audio track), which comes in about 12 dB hotter, so it gets 12 dB less input and
	/// 12 dB more output: the same GAIN distorts about as much, and the defaults stay about as loud as the AMP off.
	enum class Input : uint8_t { SOUND, BUS };

	// From the UI, or while loading

	/// The AMP setting, saved with the track. Whether the AMP runs also depends on the community feature.
	[[nodiscard]] bool isEnabled() const { return enabled_; }
	/// Switching on takes the AMP's memory: false (and it stays off) if there wasn't enough. Switching off gives the
	/// memory back, after the fade if the AMP is sounding.
	bool setEnabled(bool on);
	[[nodiscard]] const Settings& settings() const { return settings_; }
	/// The cab, tone stack, quality and channels: the knobs in s are ignored, they come from setKnobs(). False, and
	/// the old settings stay, if STEREO needed memory that wasn't free.
	bool setSettings(const Settings& s);
	/// Takes another AMP's settings, for a copied sound: false if there wasn't memory to switch it on (it stays off)
	bool copyFrom(const AmpEffect& other);

	// From the audio routine

	/// Whether process() has anything to do: the AMP is on, or fading off
	[[nodiscard]] bool isActive() const { return states_[0] != nullptr; }
	/// Whether the AMP may be making sound: active and not asleep (its tail may still be ringing)
	[[nodiscard]] bool isSounding() const { return isActive() && !asleep_; }
	/// The knobs, from the track's params, once per render window: cheap when they haven't moved
	void setKnobs(const Knobs& k);
	/// Mono, in place: the AMP's output replaces its input. In STEREO it runs only the left AMP; the right one catches
	/// up (takes the left one's state) when stereo comes in.
	void process(std::span<q31_t> mono);
	/// Stereo, in place: MONO mixes the sides and puts the AMP's output on both, STEREO runs an AMP per side
	void process(std::span<StereoSample> stereo, Input input = Input::SOUND);
	/// Clears the AMP's state (its tail), e.g. when its track stops rendering. A pending change applies at once.
	void reset();

private:
	struct State;
	static State* allocState();
	static void freeState(State*& state);

	/// Before a block: whether to run the AMP on it (it sleeps after kTailSamples of quiet input)
	bool prepareBlock(bool quiet, std::size_t n);
	/// After a block: once fully dry, applies a pending change or gives the memory back
	void afterBlock();
	void applyPending();
	void resetStates();
	/// The fade's step for this block (0 when the AMP is fully in or fully out)
	[[nodiscard]] float fadeStep() const;
	void release();

	std::array<State*, 2> states_{}; ///< [1] only in STEREO
	Settings settings_{};
	bool enabled_ = false;
	bool pending_ = false;    ///< a setting changed: fade out, configure(), fade in
	bool stereo_ = false;     ///< running as STEREO (settings_ may already say otherwise, until the change applies)
	bool rightStale_ = false; ///< STEREO: the right AMP missed mono blocks the left one ran
	bool asleep_ = false;
	int32_t quietSamples_ = 0;
	float wet_ = 0.0f;       ///< the AMP's share of the output: 0 dry, 1 the AMP only
	float wetTarget_ = 0.0f; ///< where the fade is heading
};

} // namespace deluge::dsp::amp
