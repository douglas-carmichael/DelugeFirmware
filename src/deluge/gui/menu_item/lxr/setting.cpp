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

#include "gui/menu_item/lxr/setting.h"
#include "gui/l10n/l10n.h"
#include "gui/ui/sound_editor.h"

namespace deluge::gui::menu_item::lxr {

using deluge::dsp::lxr::Param;

namespace {

/// The values a modulation target setting can take for a voice circuit, in menu order: 0 (no target), then 1 + each
/// Param the LFO and velocity may modulate
deluge::vector<uint8_t> destinations(deluge::dsp::lxr::Model model) {
	deluge::vector<uint8_t> values = {0};
	for (int32_t p = 0; p < deluge::dsp::lxr::kNumParams; p++) {
		if (deluge::dsp::lxr::isModDestination(static_cast<Param>(p), model)) {
			values.push_back(p + 1);
		}
	}
	return values;
}

} // namespace

deluge::vector<std::string_view> Setting::getOptions(OptType optType) {
	(void)optType;
	using enum l10n::String;
	auto names = [](std::initializer_list<l10n::String> strings) {
		deluge::vector<std::string_view> options;
		for (l10n::String string : strings) {
			options.push_back(l10n::getView(string));
		}
		return options;
	};

	switch (param_) {
	case Param::OSC_WAVE:
	case Param::MOD1_WAVE:
	case Param::MOD2_WAVE:
		return names({STRING_FOR_SINE, STRING_FOR_TRIANGLE, STRING_FOR_SAW, STRING_FOR_SQUARE, STRING_FOR_LXR_NOISE,
		              STRING_FOR_LXR_PULSE});
	case Param::FM_MODE:
		return names({STRING_FOR_LXR_FM, STRING_FOR_LXR_MIX});
	case Param::TRANSIENT_WAVE:
		return names({STRING_FOR_LXR_SNAP, STRING_FOR_OFFSET, STRING_FOR_LXR_CLICK, STRING_FOR_LXR_CLICK_2,
		              STRING_FOR_LXR_TICK, STRING_FOR_LXR_KICK, STRING_FOR_LXR_RIM, STRING_FOR_LXR_DROP,
		              STRING_FOR_LXR_HAT, STRING_FOR_LXR_CLAP, STRING_FOR_LXR_KICK_2, STRING_FOR_LXR_SNARE,
		              STRING_FOR_LXR_TOM, STRING_FOR_LXR_SNAP_2});
	case Param::FILTER_TYPE:
		return names({STRING_FOR_LXR_LOWPASS, STRING_FOR_LXR_HIGHPASS, STRING_FOR_LXR_BANDPASS,
		              STRING_FOR_LXR_UNITY_BANDPASS, STRING_FOR_LXR_NOTCH, STRING_FOR_LXR_PEAK, STRING_FOR_ACID,
		              STRING_FOR_OFF});
	case Param::VELO_TO_LEVEL:
	case Param::LFO_RETRIGGER:
		return names({STRING_FOR_OFF, STRING_FOR_ON});
	case Param::LFO_WAVE:
		return names({STRING_FOR_SINE, STRING_FOR_TRIANGLE, STRING_FOR_LXR_SAW_UP, STRING_FOR_LXR_SAW_DOWN,
		              STRING_FOR_SQUARE, STRING_FOR_RANDOM, STRING_FOR_LXR_EXP_UP, STRING_FOR_LXR_EXP_DOWN});
	case Param::LFO_SYNC: {
		deluge::vector<std::string_view> options = {l10n::getView(STRING_FOR_OFF)};
		for (const char* rate : {"4/1", "2/1", "1/1", "1/2", "1/3", "1/4", "1/6", "1/8", "1/12", "1/16", "1/32"}) {
			options.push_back(rate);
		}
		return options;
	}
	case Param::VELO_DEST:
	case Param::LFO_DEST: {
		const deluge::dsp::lxr::Patch* patch = getPatch(soundEditor.currentModControllable);
		deluge::vector<std::string_view> options = {l10n::getView(STRING_FOR_NONE)};
		if (patch != nullptr) {
			for (uint8_t destination : destinations(patch->model())) {
				if (destination != 0) {
					options.push_back(l10n::getView(titleFor(static_cast<Param>(destination - 1))));
				}
			}
		}
		return options;
	}
	default:
		return {};
	}
}

void Setting::readCurrentValue() {
	const deluge::dsp::lxr::Patch* patch = getPatch(soundEditor.currentModControllable);
	if (patch == nullptr) {
		setValue(0);
		return;
	}
	const uint8_t value = patch->get(param_);
	if (!isDestination()) {
		setValue(value);
		return;
	}
	// A target the voice circuit doesn't have shows as no target, which is what it does
	const deluge::vector<uint8_t> values = destinations(patch->model());
	int32_t index = 0;
	for (int32_t i = 0; i < static_cast<int32_t>(values.size()); i++) {
		if (values[i] == value) {
			index = i;
		}
	}
	setValue(index);
}

void Setting::writeCurrentValue() {
	deluge::dsp::lxr::Patch* patch = getPatch(soundEditor.currentModControllable);
	if (patch == nullptr) {
		return;
	}
	int32_t value = getValue();
	if (isDestination()) {
		const deluge::vector<uint8_t> values = destinations(patch->model());
		value = (value >= 0 && value < static_cast<int32_t>(values.size())) ? values[value] : 0;
	}
	patch->set(param_, value);
}

} // namespace deluge::gui::menu_item::lxr
