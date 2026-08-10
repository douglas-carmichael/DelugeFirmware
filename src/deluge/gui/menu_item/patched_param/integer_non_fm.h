/*
 * Copyright (c) 2014-2023 Synthstrom Audible Limited
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
#include "integer.h"
#include "processing/sound/sound.h"

namespace deluge::gui::menu_item::patched_param {
// Currently only instantiated as the oscillator-mixer "Noise level" menu.
class IntegerNonFM : public Integer {
public:
	using Integer::Integer;
	bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) const override {
		Sound* sound = static_cast<Sound*>(modControllable);
		return (sound->synthMode != SynthMode::FM);
	}

	[[nodiscard]] RenderingStyle getRenderingStyle() const override { return BAR; }

	// Dedicated short label so the mixer column reads "Noise"/"Bruit"/... instead of the truncated
	// localized title ("Niveau de bruit" would otherwise render as "Nivea").
	void getColumnLabel(StringBuf& label) override { label.append(l10n::get(l10n::String::STRING_FOR_NOISE_SHORT)); }
};
} // namespace deluge::gui::menu_item::patched_param
