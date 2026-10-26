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

#include "gui/l10n/l10n.h"
#include "gui/menu_item/lxr/common.h"
#include "gui/menu_item/selection.h"
#include "gui/ui/sound_editor.h"
#include "model/model_stack.h"
#include "processing/lxr/lxr_sound.h"
#include "util/misc.h"

namespace deluge::gui::menu_item::lxr {

/// Picks one of the LXR init kit's seven sounds. This picks the voice circuit and loads that sound's settings,
/// replacing every drum synth setting of the sound (in the clip being edited, for the settings that are params).
class VoiceTypeSelection final : public Selection {
public:
	using Selection::Selection;

	bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) const override {
		return getPatch(modControllable) != nullptr;
	}

	deluge::vector<std::string_view> getOptions(OptType optType) override {
		(void)optType;
		using enum l10n::String;
		return {
		    l10n::getView(STRING_FOR_LXR_DRUM_1),   l10n::getView(STRING_FOR_LXR_DRUM_2),
		    l10n::getView(STRING_FOR_LXR_DRUM_3),   l10n::getView(STRING_FOR_LXR_SNARE),
		    l10n::getView(STRING_FOR_LXR_CYMBAL),   l10n::getView(STRING_FOR_LXR_CLOSED_HAT),
		    l10n::getView(STRING_FOR_LXR_OPEN_HAT),
		};
	}

	void readCurrentValue() override {
		const deluge::dsp::lxr::Patch* patch = getPatch(soundEditor.currentModControllable);
		setValue(patch != nullptr ? util::to_underlying(patch->voiceType()) : 0);
	}

	void writeCurrentValue() override {
		deluge::dsp::lxr::Patch* patch = getPatch(soundEditor.currentModControllable);
		if (patch == nullptr) {
			return;
		}
		// The voice circuit may change, which playing voices can't follow
		soundEditor.currentSound->killAllVoices();

		char modelStackMemory[MODEL_STACK_MAX_SIZE];
		ModelStackWithThreeMainThings* modelStack = soundEditor.getCurrentModelStack(modelStackMemory);
		processing::lxr::loadVoiceType(*patch, getValue<deluge::dsp::lxr::VoiceType>(), modelStack);
	}
};

} // namespace deluge::gui::menu_item::lxr
