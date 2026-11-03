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

// The AMP's memory on the Deluge, apart from amp_effect.cpp so the unit tests can give it heap memory instead

#include "dsp/amp/amp_effect.h"
#include "memory/general_memory_allocator.h"

namespace deluge::dsp::amp {

void* allocAmpMemory(std::size_t size) {
	return GeneralMemoryAllocator::get().allocMaxSpeed(size);
}

void freeAmpMemory(void* memory) {
	GeneralMemoryAllocator::get().dealloc(memory);
}

} // namespace deluge::dsp::amp
