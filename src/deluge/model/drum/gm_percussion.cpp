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

#include "model/drum/gm_percussion.h"

namespace deluge::gm {

namespace {
/// Indexed by (note - kFirstPercussionNote).
const char* const percussionNames[kLastPercussionNote - kFirstPercussionNote + 1] = {
    "Acoustic Bass Drum", // 35
    "Bass Drum 1",        // 36
    "Side Stick",         // 37
    "Acoustic Snare",     // 38
    "Hand Clap",          // 39
    "Electric Snare",     // 40
    "Low Floor Tom",      // 41
    "Closed Hi-Hat",      // 42
    "High Floor Tom",     // 43
    "Pedal Hi-Hat",       // 44
    "Low Tom",            // 45
    "Open Hi-Hat",        // 46
    "Low-Mid Tom",        // 47
    "Hi-Mid Tom",         // 48
    "Crash Cymbal 1",     // 49
    "High Tom",           // 50
    "Ride Cymbal 1",      // 51
    "Chinese Cymbal",     // 52
    "Ride Bell",          // 53
    "Tambourine",         // 54
    "Splash Cymbal",      // 55
    "Cowbell",            // 56
    "Crash Cymbal 2",     // 57
    "Vibraslap",          // 58
    "Ride Cymbal 2",      // 59
    "Hi Bongo",           // 60
    "Low Bongo",          // 61
    "Mute Hi Conga",      // 62
    "Open Hi Conga",      // 63
    "Low Conga",          // 64
    "High Timbale",       // 65
    "Low Timbale",        // 66
    "High Agogo",         // 67
    "Low Agogo",          // 68
    "Cabasa",             // 69
    "Maracas",            // 70
    "Short Whistle",      // 71
    "Long Whistle",       // 72
    "Short Guiro",        // 73
    "Long Guiro",         // 74
    "Claves",             // 75
    "Hi Wood Block",      // 76
    "Low Wood Block",     // 77
    "Mute Cuica",         // 78
    "Open Cuica",         // 79
    "Mute Triangle",      // 80
    "Open Triangle",      // 81
};
} // namespace

char const* percussionName(int32_t note) {
	if (note < kFirstPercussionNote || note > kLastPercussionNote) {
		return nullptr;
	}
	return percussionNames[note - kFirstPercussionNote];
}

} // namespace deluge::gm
