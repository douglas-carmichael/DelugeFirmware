/*
 * Copyright © 2017-2023 Synthstrom Audible Limited
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
#include "gui/l10n/l10n.h"
#include "gui/menu_item/selection.h"
#include "storage/flash_storage.h"

namespace deluge::gui::menu_item {

// UI language for the OLED display. Options are each language's own name, so they are legible
// whatever language is currently active. The 7SEG display keeps its built-in abbreviations, but
// the setting still persists and applies whenever an OLED display is in use.
class Language final : public Selection {
public:
	using Selection::Selection;

	void readCurrentValue() override { this->setValue(FlashStorage::preferredLanguage); }

	void writeCurrentValue() override {
		FlashStorage::preferredLanguage = static_cast<uint8_t>(this->getValue());
		l10n::applyPreferredOledLanguage();
	}

	deluge::vector<std::string_view> getOptions(OptType optType) override {
		(void)optType;
		deluge::vector<std::string_view> options;
		options.reserve(l10n::kSelectableLanguages.size());
		for (l10n::Language const* language : l10n::kSelectableLanguages) {
			options.push_back(language->name());
		}
		return options;
	}
};

} // namespace deluge::gui::menu_item
