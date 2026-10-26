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

#include "definitions_cxx.hpp"
#include "dsp/lxr/patch.h"
#include "gui/l10n/strings.h"
#include "processing/sound/sound.h"
#include "processing/source.h"
#include <array>

/// Menus for the drum synth engine (OscType::LXR). They edit the engine of oscillator 1, the only oscillator the
/// oscillator type menu offers it for.
namespace deluge::gui::menu_item::lxr {

/// The drum synth settings of a sound whose oscillator 1 plays the drum synth, otherwise nullptr
inline deluge::dsp::lxr::Patch* getPatch(ModControllableAudio* modControllable) {
	auto* sound = static_cast<Sound*>(modControllable);
	if (sound == nullptr || sound->getSynthMode() == SynthMode::FM || sound->sources[0].oscType != OscType::LXR) {
		return nullptr;
	}
	return sound->sources[0].lxrPatch;
}

/// Whether to offer a setting: only the settings the sound's voice circuit has
inline bool isAvailable(ModControllableAudio* modControllable, deluge::dsp::lxr::Param param) {
	const deluge::dsp::lxr::Patch* patch = getPatch(modControllable);
	return patch != nullptr && deluge::dsp::lxr::isRelevant(param, patch->model());
}

/// The name of each setting where it appears without its menu around it: as a menu title, as a modulation target,
/// and as the param name in the automation view
constexpr std::array<l10n::String, deluge::dsp::lxr::kNumParams> kTitles = {
    l10n::String::STRING_FOR_LXR_OSC_WAVE_TITLE,
    l10n::String::STRING_FOR_LXR_COARSE_TITLE,
    l10n::String::STRING_FOR_LXR_FINE_TITLE,
    l10n::String::STRING_FOR_LXR_PULSE_WIDTH_TITLE,
    l10n::String::STRING_FOR_LXR_NOISE_FREQ_TITLE,
    l10n::String::STRING_FOR_LXR_NOISE_MIX_TITLE,
    l10n::String::STRING_FOR_LXR_MOD1_FREQ_TITLE,
    l10n::String::STRING_FOR_LXR_MOD1_LEVEL_TITLE,
    l10n::String::STRING_FOR_LXR_MOD1_WAVE_TITLE,
    l10n::String::STRING_FOR_LXR_MOD2_FREQ_TITLE,
    l10n::String::STRING_FOR_LXR_MOD2_LEVEL_TITLE,
    l10n::String::STRING_FOR_LXR_MOD2_WAVE_TITLE,
    l10n::String::STRING_FOR_LXR_MOD_MODE_TITLE,
    l10n::String::STRING_FOR_LXR_PITCH_DECAY_TITLE,
    l10n::String::STRING_FOR_LXR_PITCH_AMOUNT_TITLE,
    l10n::String::STRING_FOR_LXR_PITCH_SLOPE_TITLE,
    l10n::String::STRING_FOR_LXR_ATTACK_TITLE,
    l10n::String::STRING_FOR_LXR_DECAY_TITLE,
    l10n::String::STRING_FOR_LXR_AMP_SLOPE_TITLE,
    l10n::String::STRING_FOR_LXR_REPEAT_TITLE,
    l10n::String::STRING_FOR_LXR_TRANSIENT_WAVE_TITLE,
    l10n::String::STRING_FOR_LXR_TRANSIENT_LEVEL_TITLE,
    l10n::String::STRING_FOR_LXR_TRANSIENT_PITCH_TITLE,
    l10n::String::STRING_FOR_LXR_FILTER_TYPE_TITLE,
    l10n::String::STRING_FOR_LXR_FILTER_FREQ_TITLE,
    l10n::String::STRING_FOR_LXR_FILTER_RESO_TITLE,
    l10n::String::STRING_FOR_LXR_FILTER_DRIVE_TITLE,
    l10n::String::STRING_FOR_LXR_DRIVE_TITLE,
    l10n::String::STRING_FOR_LXR_SAMPLE_RATE_TITLE,
    l10n::String::STRING_FOR_LXR_LEVEL_TITLE,
    l10n::String::STRING_FOR_LXR_VELO_TO_LEVEL_TITLE,
    l10n::String::STRING_FOR_LXR_VELO_AMOUNT_TITLE,
    l10n::String::STRING_FOR_LXR_VELO_DEST_TITLE,
    l10n::String::STRING_FOR_LXR_LFO_WAVE_TITLE,
    l10n::String::STRING_FOR_LXR_LFO_RATE_TITLE,
    l10n::String::STRING_FOR_LXR_LFO_SYNC_TITLE,
    l10n::String::STRING_FOR_LXR_LFO_DEPTH_TITLE,
    l10n::String::STRING_FOR_LXR_LFO_RETRIGGER_TITLE,
    l10n::String::STRING_FOR_LXR_LFO_PHASE_TITLE,
    l10n::String::STRING_FOR_LXR_LFO_DEST_TITLE,
};

constexpr l10n::String titleFor(deluge::dsp::lxr::Param param) {
	return kTitles[static_cast<int32_t>(param)];
}

} // namespace deluge::gui::menu_item::lxr
