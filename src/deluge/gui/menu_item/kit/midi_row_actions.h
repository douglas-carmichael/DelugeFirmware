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
#include "gui/context_menu/clear_kit_rows.h"
#include "gui/l10n/l10n.h"
#include "gui/menu_item/menu_item.h"
#include "gui/menu_item/submenu.h"
#include "gui/ui/sound_editor.h"
#include "gui/views/instrument_clip_view.h"
#include "hid/display/display.h"
#include "model/settings/runtime_feature_settings.h"
#include "model/song/song.h"

namespace deluge::gui::menu_item::kit {

/// Base for the kit-wide MIDI row actions. They fire on select-button press rather than holding a value.
class MIDIRowAction : public MenuItem {
public:
	using MenuItem::MenuItem;

	bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) const override {
		return runtimeFeatureSettings.isOn(RuntimeFeatureSettingType::MidiDrumRows)
		       && getCurrentOutputType() == OutputType::KIT;
	}

	bool shouldEnterSubmenu() override { return false; }
};

/// Points every MIDI row in the kit at the kit's default channel.
class ApplyMIDIChannel final : public MIDIRowAction {
public:
	using MIDIRowAction::MIDIRowAction;

	MenuItem* selectButtonPress() override {
		using enum l10n::String;
		int32_t numMidiRows = 0;
		instrumentClipView.applyDefaultMIDIChannelToRows(&numMidiRows);
		// Rows that were already on the channel change nothing, but the kit still ends up how the user asked, so
		// that's an OK - only say there are no MIDI rows when there really aren't any.
		display->displayPopup(l10n::get(numMidiRows ? STRING_FOR_OK : STRING_FOR_NO_MIDI_ROWS));
		return NO_NAVIGATION;
	}
};

/// Adds a MIDI row for each General MIDI drum-kit note the kit doesn't already send.
class GMDrumFill final : public MIDIRowAction {
public:
	using MIDIRowAction::MIDIRowAction;

	MenuItem* selectButtonPress() override {
		using enum l10n::String;
		int32_t numAdded = instrumentClipView.fillWithGMDrumRows();
		if (numAdded < 0) {
			display->displayError(Error::INSUFFICIENT_RAM);
		}
		else if (numAdded == 0) {
			// Every GM note is already covered, so there was nothing to add.
			display->displayPopup(l10n::get(STRING_FOR_NONE));
		}
		// A successful fill needs no popup: the new rows are right there on the grid.
		return NO_NAVIGATION;
	}
};

/// Empties the kit so its rows can be rebuilt. Confirms first, because it can't be undone.
class ClearRows final : public MIDIRowAction {
public:
	using MIDIRowAction::MIDIRowAction;

	MenuItem* selectButtonPress() override {
		if (context_menu::clearKitRows.setupAndCheckAvailability()) {
			openUI(&context_menu::clearKitRows);
		}
		return NO_NAVIGATION;
	}
};

/// The kit's MIDI Rows submenu. Hidden as a whole when the community feature is off, so the kit Actions menu reads
/// exactly as it did before rather than showing an empty entry.
class MIDIRowsSubmenu final : public Submenu {
public:
	using Submenu::Submenu;

	bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) const override {
		return runtimeFeatureSettings.isOn(RuntimeFeatureSettingType::MidiDrumRows)
		       && getCurrentOutputType() == OutputType::KIT;
	}
};

} // namespace deluge::gui::menu_item::kit
