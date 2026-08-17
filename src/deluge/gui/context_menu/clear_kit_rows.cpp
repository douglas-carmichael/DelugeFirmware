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

#include "gui/context_menu/clear_kit_rows.h"
#include "gui/l10n/l10n.h"
#include "gui/views/instrument_clip_view.h"
#include "hid/display/display.h"

namespace deluge::gui::context_menu {

ClearKitRows clearKitRows{};

char const* ClearKitRows::getTitle() {
	using enum l10n::String;
	return l10n::get(STRING_FOR_CLEAR_KIT_ROWS_QMARK);
}

std::span<char const*> ClearKitRows::getOptions() {
	using enum l10n::String;
	static char const* options[] = {l10n::get(STRING_FOR_OK)};
	return {options, 1};
}

bool ClearKitRows::acceptCurrentOption() {
	instrumentClipView.clearAllKitRows();
	return false; // Exit the UI - the user wants to see the emptied kit
}

} // namespace deluge::gui::context_menu
