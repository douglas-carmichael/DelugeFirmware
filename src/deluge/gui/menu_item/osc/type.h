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
#include "definitions_cxx.hpp"
#include "dsp/lxr/feature.h"
#include "gui/menu_item/formatted_title.h"
#include "gui/menu_item/lxr/group.h"
#include "gui/menu_item/selection.h"
#include "gui/menu_item/submenu.h"
#include "gui/ui/sound_editor.h"
#include "model/settings/runtime_feature_settings.h"
#include "model/song/song.h"
#include "processing/engines/audio_engine.h"
#include "processing/sound/sound.h"
#include "processing/source.h"
#include "util/comparison.h"

#include <hid/display/oled.h>

extern gui::menu_item::Submenu dxMenu;
extern gui::menu_item::lxr::Group lxrMenu;

namespace deluge::gui::menu_item::osc {
class Type final : public Selection, public FormattedTitle {
public:
	Type(l10n::String name, l10n::String title_format_str, uint8_t source_id)
	    : Selection(name), FormattedTitle(title_format_str, source_id + 1), sourceId_{source_id} {};
	void beginSession(MenuItem* navigatedBackwardFrom) override { Selection::beginSession(navigatedBackwardFrom); }

	bool mayUseDx() const { return sourceId_ == 0; }

	/// The drum synth is on offer once its community feature is on, and always for a sound that already uses it
	bool mayUseLxr() const {
		return sourceId_ == 0
		       && ((FEATURE_LXR_ENGINE
		            && runtimeFeatureSettings.get(RuntimeFeatureSettingType::EnableLxrEngine)
		                   == RuntimeFeatureStateToggle::On)
		           || soundEditor.currentSound->sources[sourceId_].oscType == OscType::LXR);
	}

	static bool anyInputPluggedIn() { return AudioEngine::micPluggedIn || AudioEngine::lineInPluggedIn; }

	/// The types on offer, in the order of getOptions()
	deluge::vector<OscType> getTypes() const {
		using enum OscType;
		deluge::vector<OscType> types = {SINE, TRIANGLE, SQUARE, ANALOG_SQUARE, SAW, ANALOG_SAW_2, SUPERSAW, WAVETABLE};
		if (soundEditor.currentSound->getSynthMode() == SynthMode::RINGMOD) {
			return types;
		}
		types.push_back(SAMPLE);
		if (mayUseDx()) {
			types.push_back(DX7);
		}
		if (mayUseLxr()) {
			types.push_back(LXR);
		}
		types.push_back(INPUT_L);
		if (anyInputPluggedIn()) {
			types.push_back(INPUT_R);
			types.push_back(INPUT_STEREO);
		}
		return types;
	}

	void readCurrentValue() override {
		OscType type = soundEditor.currentSound->sources[sourceId_].oscType;
		// With nothing plugged in, the one input option stands for all three input types
		if (!anyInputPluggedIn() && (type == OscType::INPUT_R || type == OscType::INPUT_STEREO)) {
			type = OscType::INPUT_L;
		}
		const deluge::vector<OscType> types = getTypes();
		const auto it = std::ranges::find(types, type);
		setValue(it != types.end() ? static_cast<int32_t>(it - types.begin()) : 0);
	}
	void writeCurrentValue() override {
		OscType oldValue = soundEditor.currentSound->sources[sourceId_].oscType;
		const deluge::vector<OscType> types = getTypes();
		const OscType newValue = types[std::clamp<int32_t>(getValue(), 0, static_cast<int32_t>(types.size()) - 1)];

		const auto needs_unassignment = {
		    OscType::INPUT_L,
		    OscType::INPUT_R,
		    OscType::INPUT_STEREO,
		    OscType::SAMPLE,
		    OscType::DX7,
		    OscType::LXR,

		    // Haven't actually really determined if this needs to be here - maybe not?
		    OscType::WAVETABLE,
		};

		if (util::one_of(oldValue, needs_unassignment) || util::one_of(newValue, needs_unassignment)) {
			soundEditor.currentSound->killAllVoices();
		}

		soundEditor.currentSound->sources[sourceId_].setOscType(newValue);

		if (oldValue == OscType::SQUARE || newValue == OscType::SQUARE) {
			soundEditor.currentSound->setupPatchingForAllParamManagers(currentSong);
		}

		// Leaving the drum synth can take it off offer, which moves the options after it
		if (oldValue == OscType::LXR) {
			readCurrentValue();
		}
	}

	[[nodiscard]] std::string_view getTitle() const override { return FormattedTitle::title(); }

	deluge::vector<std::string_view> getOptions(OptType optType) override {
		(void)optType;
		using enum l10n::String;
		deluge::vector options = {
		    l10n::getView(STRING_FOR_SINE),          //<
		    l10n::getView(STRING_FOR_TRIANGLE),      //<
		    l10n::getView(STRING_FOR_SQUARE),        //<
		    l10n::getView(STRING_FOR_ANALOG_SQUARE), //<
		    l10n::getView(STRING_FOR_SAW),           //<
		    l10n::getView(STRING_FOR_ANALOG_SAW),    //<
		    l10n::getView(STRING_FOR_SUPERSAW),      //<
		    l10n::getView(STRING_FOR_WAVETABLE),     //<
		};

		if (soundEditor.currentSound->getSynthMode() == SynthMode::RINGMOD) {
			return options;
		}

		options.emplace_back(l10n::getView(STRING_FOR_SAMPLE));

		if (mayUseDx()) {
			options.emplace_back(l10n::getView(STRING_FOR_DX7));
		}

		if (mayUseLxr()) {
			options.emplace_back(l10n::getView(STRING_FOR_LXR_ENGINE));
		}

		if (anyInputPluggedIn()) {
			options.emplace_back(l10n::getView(STRING_FOR_INPUT_LEFT));
			options.emplace_back(l10n::getView(STRING_FOR_INPUT_RIGHT));
			options.emplace_back(l10n::getView(STRING_FOR_INPUT_STEREO));
		}
		else {
			options.emplace_back(l10n::getView(STRING_FOR_INPUT));
		}

		return options;
	}

	bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) const override {
		Sound* sound = static_cast<Sound*>(modControllable);
		return (sound->getSynthMode() != SynthMode::FM);
	}

	MenuItem* selectButtonPress() override {
		switch (soundEditor.currentSound->sources[sourceId_].oscType) {
		case OscType::DX7:
			return &dxMenu;
		case OscType::LXR:
			return &lxrMenu;
		default:
			return nullptr;
		}
	}

	[[nodiscard]] bool showColumnLabel() const override { return false; }

	void renderInHorizontalMenu(const SlotPosition& slot) override {
		oled_canvas::Canvas& image = OLED::main;

		const OscType osc_type = soundEditor.currentSound->sources[sourceId_].oscType;
		if (osc_type == OscType::DX7) {
			const auto option = getOptions(OptType::FULL)[getValue()].data();
			return image.drawStringCentered(option, slot.start_x, slot.start_y + kHorizontalMenuSlotYOffset + 5,
			                                kTextTitleSpacingX, kTextTitleSizeY, slot.width);
		}
		if (osc_type == OscType::LXR) {
			// Cut to fit, the name would read SYN in French and SIN in Spanish, so every language shows one short
			// label. drawStringCentered() cuts what comes within 3 px of the width, which would take the period
			// off "DRU." (29 px) in this 31 px slot, so the label is centred here without that cut. A label at most
			// 2 px narrower than the slot stays inside the selected slot's highlight; a wider one is cut as before.
			const char* label = l10n::get(l10n::String::STRING_FOR_LXR_ENGINE_SHORT);
			const int32_t label_width = image.getStringWidthInPixels(label, kTextTitleSizeY);
			const int32_t y = slot.start_y + kHorizontalMenuSlotYOffset + 5;
			if (label_width > slot.width - 2) {
				return image.drawStringCentered(label, slot.start_x, y, kTextTitleSpacingX, kTextTitleSizeY,
				                                slot.width);
			}
			return image.drawString(label, slot.start_x + (slot.width - label_width + 1) / 2, y, kTextTitleSpacingX,
			                        kTextTitleSizeY);
		}

		const Icon& icon = [&] {
			switch (osc_type) {
			case OscType::SINE:
				return OLED::sineIcon;
			case OscType::TRIANGLE:
				return OLED::triangleIcon;
			case OscType::SQUARE:
			case OscType::ANALOG_SQUARE:
				return OLED::squareIcon;
			case OscType::SAW:
			case OscType::ANALOG_SAW_2:
			case OscType::SUPERSAW:
				return OLED::sawIcon;
			case OscType::SAMPLE:
				return OLED::sampleIcon;
			case OscType::INPUT_STEREO:
			case OscType::INPUT_L:
			case OscType::INPUT_R:
				return AudioEngine::lineInPluggedIn ? OLED::inputIcon : OLED::micIcon;
			case OscType::WAVETABLE:
				return OLED::wavetableIcon;
			default:
				return OLED::sineIcon;
			}
		}();

		image.drawIconCentered(icon, slot.start_x, slot.width, slot.start_y + kHorizontalMenuSlotYOffset + 2);

		if (osc_type == OscType::ANALOG_SQUARE || osc_type == OscType::ANALOG_SAW_2 || osc_type == OscType::SUPERSAW) {
			const int32_t x = slot.start_x + 4;
			constexpr int32_t y = OLED_MAIN_HEIGHT_PIXELS - kTextSpacingY - 8;
			image.clearAreaExact(x - 1, y - 1, x + kTextSpacingX + 1, y + kTextSpacingY + 1);
			image.drawChar(osc_type == OscType::SUPERSAW ? 'S' : 'A', x, y, kTextSpacingX, kTextSpacingY);
		}
	}

	bool wrapAround() override {
		return parent != nullptr && parent->renderingStyle() == Submenu::RenderingStyle::HORIZONTAL;
	}

private:
	uint8_t sourceId_;
};

} // namespace deluge::gui::menu_item::osc
