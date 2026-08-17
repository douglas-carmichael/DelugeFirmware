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
#include "gui/ui/sound_editor.h"
#include "model/drum/midi_drum.h"
#include "model/instrument/kit.h"
#include "model/settings/runtime_feature_settings.h"
#include "model/song/song.h"

namespace deluge::gui::menu_item::midi_drum {

/// Shared plumbing for menu items that edit the kit's selected MIDIDrum. Mixed into a menu item base so both the
/// Integer-valued items can use it.
template <typename Base>
class SelectedMIDIDrum : public Base {
public:
	using Base::Base;

	bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) const override {
		return runtimeFeatureSettings.isOn(RuntimeFeatureSettingType::MidiDrumRows) && soundEditor.editingMidiDrumRow();
	}

protected:
	/// The MIDIDrum being edited, or nullptr if the selection moved somewhere else under us.
	static MIDIDrum* getMIDIDrum() {
		Kit* kit = getCurrentKit();
		if (kit == nullptr || kit->selectedDrum == nullptr || kit->selectedDrum->type != DrumType::MIDI) {
			return nullptr;
		}
		return static_cast<MIDIDrum*>(kit->selectedDrum);
	}
};

} // namespace deluge::gui::menu_item::midi_drum
