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
#include "gui/menu_item/menu_item.h"
#include "hid/hid_sysex_exerciser.h"

namespace deluge::gui::menu_item::midi {

/// Runs the display exerciser's visual shape at 10 Hz for an eyeball hardware-vs-mirror check.
/// The menu stays open underneath; any button, pad or encoder input ends the test and repaints it.
class DisplayTest final : public MenuItem {
public:
	using MenuItem::MenuItem;

	MenuItem* selectButtonPress() override {
		HIDSysexExerciser::startLocalDemo();
		return NO_NAVIGATION;
	}

	bool shouldEnterSubmenu() override { return false; }
};
} // namespace deluge::gui::menu_item::midi
