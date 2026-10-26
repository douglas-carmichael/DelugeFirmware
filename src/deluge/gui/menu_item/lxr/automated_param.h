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
#include "gui/menu_item/unpatched_param.h"
#include "gui/ui/sound_editor.h"
#include "modulation/params/param_manager.h"
#include "modulation/params/param_set.h"
#include "processing/lxr/lxr_sound.h"

namespace deluge::gui::menu_item::lxr {

/// A drum synth setting that is a Deluge param (one of the UNPATCHED_LXR_* params), so it can be automated, locked
/// to a step and learnt like any other. Shows the LXR's own 0..127 value, or the offset version the LXR shows for
/// the note-based settings.
class AutomatedParam final : public UnpatchedParam {
public:
	AutomatedParam(l10n::String name, int32_t p)
	    : UnpatchedParam(name, titleFor(processing::lxr::automatedParam(p)), p) {}

	bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) const override {
		return isAvailable(modControllable, setting());
	}

	void readCurrentValue() override {
		const int32_t value = soundEditor.currentParamManager->getUnpatchedParamSet()->getValue(getP());
		setValue(processing::lxr::paramValueToSetting(value) - offset());
	}

protected:
	[[nodiscard]] int32_t getMaxValue() const override { return 127 - offset(); }
	[[nodiscard]] int32_t getMinValue() const override { return -offset(); }
	int32_t getFinalValue() override { return processing::lxr::settingToParamValue(getValue() + offset()); }

private:
	[[nodiscard]] deluge::dsp::lxr::Param setting() const { return processing::lxr::automatedParam(p); }
	[[nodiscard]] int32_t offset() const { return deluge::dsp::lxr::displayOffset(setting()); }
};

} // namespace deluge::gui::menu_item::lxr
