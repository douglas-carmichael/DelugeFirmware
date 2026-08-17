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
#include "gui/menu_item/integer.h"
#include "gui/ui/sound_editor.h"
#include "model/instrument/kit.h"
#include "model/settings/runtime_feature_settings.h"
#include "model/song/song.h"

namespace deluge::gui::menu_item::kit {

/// The channel MIDI rows added to this kit start on. Stored 0-15, shown 1-16. Existing rows keep their own channel
/// until Apply Channel To All is used.
class DefaultMIDIChannel final : public Integer {
public:
	using Integer::Integer;

	[[nodiscard]] int32_t getMaxValue() const override { return 15; }
	[[nodiscard]] RenderingStyle getRenderingStyle() const override { return NUMBER; }

	bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) const override {
		return runtimeFeatureSettings.isOn(RuntimeFeatureSettingType::MidiDrumRows)
		       && getCurrentOutputType() == OutputType::KIT;
	}

	void readCurrentValue() override {
		if (Kit* kit = getCurrentKit()) {
			this->setValue(kit->defaultMIDIChannel);
		}
	}

	void writeCurrentValue() override {
		if (Kit* kit = getCurrentKit()) {
			kit->defaultMIDIChannel = this->getValue();
			kit->beenEdited();
		}
	}

protected:
	[[nodiscard]] int32_t getDisplayValue() override { return this->getValue() + 1; }
};
} // namespace deluge::gui::menu_item::kit
