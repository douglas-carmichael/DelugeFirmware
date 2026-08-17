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

#include "definitions_cxx.hpp"
#include "gui/l10n/l10n.h"
#include "gui/menu_item/integer.h"
#include "gui/menu_item/midi_drum/selected_midi_drum.h"
#include "gui/ui/sound_editor.h"
#include "hid/display/display.h"
#include "hid/display/oled.h"
#include "model/drum/gm_percussion.h"
#include "model/drum/midi_drum.h"
#include "util/cfunctions.h"
#include "util/functions.h"
#include <cstring>

namespace deluge::gui::menu_item::midi_drum {

/// The note a MIDIDrum row sends. Also learnable: with this menu open, hold LEARN and hit the pad on the external
/// device, and the row takes that note and the channel it arrived on.
class Note final : public SelectedMIDIDrum<Integer> {
public:
	using SelectedMIDIDrum<Integer>::SelectedMIDIDrum;

	[[nodiscard]] int32_t getMaxValue() const override { return kMaxMIDIValue; }
	[[nodiscard]] RenderingStyle getRenderingStyle() const override { return NUMBER; }

	void readCurrentValue() override {
		if (MIDIDrum* drum = getMIDIDrum()) {
			this->setValue(drum->note);
		}
	}

	void writeCurrentValue() override {
		MIDIDrum* drum = getMIDIDrum();
		if (!drum) {
			return;
		}
		drum->killAllVoices(); // Don't strand a note-on over on the note we're leaving
		drum->note = this->getValue();
		getCurrentKit()->beenEdited();
	}

	bool allowsLearnMode() override { return true; }
	bool shouldBlinkLearnLed() override { return true; }

	bool learnNoteOn(MIDICable& cable, int32_t channel, int32_t noteCode) override {
		MIDIDrum* drum = getMIDIDrum();
		if (!drum) {
			return false;
		}

		drum->killAllVoices();
		drum->note = noteCode;
		// Take the channel too. On a drum machine the note and the channel it arrived on belong together, and this
		// saves setting the channel separately for gear that isn't on the kit's default.
		if (channel >= 0 && channel < 16) {
			drum->channel = channel;
		}
		getCurrentKit()->beenEdited();

		readValueAgain();
		return true;
	}

	void drawPixelsForOled() override {
		deluge::hid::display::oled_canvas::Canvas& canvas = deluge::hid::display::OLED::main;

		char buffer[16];
		writeNoteText(buffer);

		char const* gmName = deluge::gm::percussionName(this->getValue());
		if (gmName == nullptr) {
			canvas.drawStringCentredShrinkIfNecessary(buffer, 18 + OLED_MAIN_TOPMOST_PIXEL, kTextHugeSpacingX,
			                                          kTextHugeSizeY);
			return;
		}

		// Room for both lines only because the note drops to the big font. Anything larger collides with the title
		// separator above or runs off the bottom of the 48-pixel panel.
		canvas.drawStringCentredShrinkIfNecessary(buffer, 17 + OLED_MAIN_TOPMOST_PIXEL, kTextBigSpacingX,
		                                          kTextBigSizeY);
		canvas.drawStringCentredShrinkIfNecessary(gmName, 32 + OLED_MAIN_TOPMOST_PIXEL, kTextSpacingX,
		                                          kTextSizeYUpdated);
	}

private:
	/// Writes "36 C1" into a buffer of at least 16 chars.
	void writeNoteText(char* buffer) {
		intToString(this->getValue(), buffer, 1);
		strcat(buffer, " ");
		noteCodeToString(this->getValue(), &buffer[strlen(buffer)]);
	}
};
} // namespace deluge::gui::menu_item::midi_drum
