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

// The AMP's settings, apart from its DSP (amp_sim.h), so code that only stores them doesn't pull in the DSP

#pragma once

#include <cstdint>

namespace deluge::dsp::amp {

enum class ToneStackType : uint8_t { AMERICAN, BRITISH };

/// The knobs, 0..1 (0.5 is the middle), and the output level in dB. They can change while the AMP runs (setKnobs).
struct Knobs {
	float gain = 0.5f;
	float bass = 0.5f;
	float mid = 0.5f;
	float treble = 0.5f;
	float presence = 0.5f;
	float levelDb = 0.0f;
};

/// Everything; changing anything but the knobs needs configure(), which also clears the state
struct Settings {
	Knobs knobs;
	int cab = 4; ///< 0 = no cab, 1..kNumCabs = kCabDesigns[cab - 1]
	ToneStackType toneStack = ToneStackType::BRITISH;
	int oversample = 2; ///< 1 or 2
};

} // namespace deluge::dsp::amp
