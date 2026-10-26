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
#pragma once

#include "gui/menu_item/submenu.h"
#include <algorithm>

namespace deluge::gui::menu_item::lxr {

/// A submenu of drum synth settings, shown only while it holds a setting the sound's voice circuit has
class Group final : public Submenu {
public:
	using Submenu::Submenu;

	bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) const override {
		return std::ranges::any_of(items,
		                           [&](MenuItem* item) { return item->isRelevant(modControllable, whichThing); });
	}
};

} // namespace deluge::gui::menu_item::lxr
