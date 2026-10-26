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

#include "gui/menu_item/lxr/common.h"
#include "gui/menu_item/selection.h"

namespace deluge::gui::menu_item::lxr {

/// A drum synth setting that picks from a list (a waveform, the filter type, a modulation target and such). These
/// live in the sound's lxr::Patch, the way the oscillator type does, so they are the same in every clip.
class Setting final : public Selection {
public:
	Setting(l10n::String name, deluge::dsp::lxr::Param param) : Selection(name, titleFor(param)), param_(param) {}

	bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) const override {
		return isAvailable(modControllable, param_);
	}
	bool isToggle() override {
		return param_ == deluge::dsp::lxr::Param::VELO_TO_LEVEL || param_ == deluge::dsp::lxr::Param::LFO_RETRIGGER;
	}

	deluge::vector<std::string_view> getOptions(OptType optType) override;
	void readCurrentValue() override;
	void writeCurrentValue() override;

private:
	[[nodiscard]] bool isDestination() const {
		return param_ == deluge::dsp::lxr::Param::VELO_DEST || param_ == deluge::dsp::lxr::Param::LFO_DEST;
	}

	deluge::dsp::lxr::Param param_;
};

} // namespace deluge::gui::menu_item::lxr
