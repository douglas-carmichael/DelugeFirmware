/*
 * Copyright © 2026 Synthstrom Audible Limited
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

#include <cstdint>

namespace deluge::gm {

/// Bounds of the General MIDI percussion key map.
constexpr int32_t kFirstPercussionNote = 35;
constexpr int32_t kLastPercussionNote = 81;

/// The General MIDI drum-kit block: bass drums up to Ride Cymbal 2. Notes above this are latin percussion, which
/// most drum machines don't map, so a fill stops here.
constexpr int32_t kFirstDrumKitNote = 35;
constexpr int32_t kLastDrumKitNote = 59;

/// Name of the instrument General MIDI assigns to a percussion note, or nullptr if the note is outside the key map.
/// Left untranslated on purpose: these are standard industry terms, like note letters.
char const* percussionName(int32_t note);

} // namespace deluge::gm
