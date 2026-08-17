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

#include "gui/menu_item/integer.h"
#include "gui/menu_item/midi_drum/selected_midi_drum.h"
#include "model/drum/midi_drum.h"
#include "model/instrument/kit.h"

namespace deluge::gui::menu_item::midi_drum {

/// The MIDI channel a MIDIDrum row sends on. Stored 0-15, shown 1-16 to match how the row's name reads.
class Channel final : public SelectedMIDIDrum<Integer> {
public:
	using SelectedMIDIDrum<Integer>::SelectedMIDIDrum;

	[[nodiscard]] int32_t getMaxValue() const override { return 15; }
	[[nodiscard]] RenderingStyle getRenderingStyle() const override { return NUMBER; }

	void readCurrentValue() override {
		if (MIDIDrum* drum = getMIDIDrum()) {
			this->setValue(drum->channel);
		}
	}

	void writeCurrentValue() override {
		MIDIDrum* drum = getMIDIDrum();
		if (!drum) {
			return;
		}
		drum->killAllVoices(); // Don't strand a note-on over on the channel we're leaving
		drum->channel = this->getValue();
		getCurrentKit()->beenEdited();
	}

protected:
	[[nodiscard]] int32_t getDisplayValue() override { return this->getValue() + 1; }
};
} // namespace deluge::gui::menu_item::midi_drum
