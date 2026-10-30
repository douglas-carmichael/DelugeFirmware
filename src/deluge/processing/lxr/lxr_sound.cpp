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

#include "processing/lxr/lxr_sound.h"
#include "dsp/lxr/feature.h"
#include "dsp/lxr/voice.h"
#include "memory/fast_allocator.h"
#include "memory/object_pool.h"
#include "model/model_stack.h"
#include "model/song/song.h"
#include "modulation/automation/auto_param.h"
#include "modulation/params/param_set.h"
#include "playback/playback_handler.h"
#include "storage/storage_manager.h"
#include <algorithm>
#include <cstring>

namespace deluge::processing::lxr {

using deluge::dsp::lxr::Patch;
using deluge::dsp::lxr::Values;
using deluge::dsp::lxr::VoiceType;
#if FEATURE_LXR_ENGINE
using VoicePool = deluge::memory::ObjectPool<deluge::dsp::lxr::Voice, deluge::memory::fast_allocator>;
#endif

void getValues(const Patch& patch, UnpatchedParamSet& unpatchedParams, Values& values) {
	patch.getValues(values);
	for (int32_t i = 0; i < params::kNumLxrUnpatchedParams; i++) {
		values[static_cast<int32_t>(kAutomatedParams[i])] =
		    paramValueToValue(unpatchedParams.getValue(params::UNPATCHED_FIRST_LXR_PARAM + i));
	}
}

bool sustainsForever(const Patch& patch, UnpatchedParamSet& unpatchedParams) {
	Patch current = patch;
	for (int32_t i = 0; i < params::kNumLxrUnpatchedParams; i++) {
		current.set(kAutomatedParams[i],
		            paramValueToSetting(unpatchedParams.getValue(params::UNPATCHED_FIRST_LXR_PARAM + i)));
	}
	return current.sustainsForever();
}

void initParams(UnpatchedParamSet& unpatchedParams) {
	const Patch defaults;
	for (int32_t i = 0; i < params::kNumLxrUnpatchedParams; i++) {
		unpatchedParams.params[params::UNPATCHED_FIRST_LXR_PARAM + i].setCurrentValueBasicForSetup(
		    settingToParamValue(defaults.get(kAutomatedParams[i])));
	}
}

namespace {

char const* fileName(int32_t unpatchedParam) {
	return params::paramNameForFile(params::Kind::UNPATCHED_SOUND, params::UNPATCHED_START + unpatchedParam);
}

} // namespace

void writeParams(Serializer& writer, UnpatchedParamSet& unpatchedParams, bool writeAutomation) {
	const Patch defaults;
	for (int32_t i = 0; i < params::kNumLxrUnpatchedParams; i++) {
		const int32_t p = params::UNPATCHED_FIRST_LXR_PARAM + i;
		AutoParam& param = unpatchedParams.params[p];
		if (!param.isAutomated() && param.getCurrentValue() == settingToParamValue(defaults.get(kAutomatedParams[i]))) {
			continue;
		}
		unpatchedParams.writeParamAsAttribute(writer, fileName(p), p, writeAutomation);
	}
}

bool readParam(Deserializer& reader, char const* tagName, ParamCollectionSummary* unpatchedParamsSummary,
               int32_t readAutomationUpToPos) {
	for (int32_t i = 0; i < params::kNumLxrUnpatchedParams; i++) {
		const int32_t p = params::UNPATCHED_FIRST_LXR_PARAM + i;
		char const* name = fileName(p);
		if (!strcmp(tagName, name)) {
			static_cast<UnpatchedParamSet*>(unpatchedParamsSummary->paramCollection)
			    ->readParam(reader, unpatchedParamsSummary, p, readAutomationUpToPos);
			reader.exitTag(name);
			return true;
		}
	}
	return false;
}

void loadVoiceType(Patch& patch, VoiceType type, ModelStackWithThreeMainThings* modelStack) {
	patch.loadDefaults(type);
	for (int32_t i = 0; i < params::kNumLxrUnpatchedParams; i++) {
		ModelStackWithAutoParam* modelStackWithParam =
		    modelStack->getUnpatchedAutoParamFromId(params::UNPATCHED_FIRST_LXR_PARAM + i);
		if (modelStackWithParam->autoParam != nullptr) {
			modelStackWithParam->autoParam->setCurrentValueWithNoReversionOrRecording(
			    modelStackWithParam, settingToParamValue(patch.get(kAutomatedParams[i])));
		}
	}
}

gsl::owner<deluge::dsp::lxr::Voice*> acquireVoice() {
#if FEATURE_LXR_ENGINE
	try {
		return VoicePool::get().acquire().release();
	} catch (deluge::exception e) {
		return nullptr;
	}
#else
	return nullptr; // so a note of the engine fails to start
#endif
}

void releaseVoice(gsl::owner<deluge::dsp::lxr::Voice*> voice) {
#if FEATURE_LXR_ENGINE
	VoicePool::recycle(voice);
#endif
}

float currentBPM() {
	if (currentSong == nullptr) {
		return 120.f;
	}
	return playbackHandler.calculateBPM(playbackHandler.getTimePerInternalTickFloat());
}

} // namespace deluge::processing::lxr
