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

#include "dsp/lxr/patch.h"
#include "modulation/params/param.h"
#include <algorithm>
#include <array>
#include <gsl/gsl>

class Deserializer;
class ModelStackWithThreeMainThings;
class ParamCollectionSummary;
class Serializer;
class UnpatchedParamSet;
namespace deluge::dsp::lxr {
class Voice;
}

/// How a Sound plays the drum synth engine (OscType::LXR, the DSP in dsp/lxr/).
///
/// Every continuous setting of a drum synth voice is an unpatched param (params::UNPATCHED_LXR_*), so the Deluge can
/// automate it, lock it to a step, and learn it to a knob or a MIDI CC like any other param. The settings that pick
/// from a list (waveforms, filter type, modulation targets and such) live in the Source's lxr::Patch instead, the way
/// the oscillator type itself does, and the Patch's values for the automated settings go unused.
///
/// An automated setting stores the LXR's 0..127 value as the knob position value - 64. So a knob detent or a MIDI CC
/// step is one LXR step, and a CC value equals the LXR value. Automation between two steps reaches the engine as an
/// in-between value, which the LXR's own parameter handling accepts.
namespace deluge::processing::lxr {

namespace params = deluge::modulation::params;
using deluge::dsp::lxr::Param;

/// The LXR setting each UNPATCHED_LXR_* param holds, in the order of params::UnpatchedSound
constexpr std::array<Param, params::kNumLxrUnpatchedParams> kAutomatedParams = {
    Param::COARSE,
    Param::FINE,
    Param::PULSE_WIDTH,
    Param::NOISE_FREQ,
    Param::MIX,
    Param::MOD1_FREQ,
    Param::MOD1_GAIN,
    Param::MOD2_FREQ,
    Param::MOD2_GAIN,
    Param::PITCH_DECAY,
    Param::PITCH_AMOUNT,
    Param::PITCH_SLOPE,
    Param::ATTACK,
    Param::DECAY,
    Param::SLOPE,
    Param::REPEAT,
    Param::TRANSIENT_LEVEL,
    Param::TRANSIENT_PITCH,
    Param::FILTER_FREQ,
    Param::FILTER_RESO,
    Param::FILTER_DRIVE,
    Param::DRIVE,
    Param::SAMPLE_RATE,
    Param::LEVEL,
    Param::VELO_AMOUNT,
    Param::LFO_RATE,
    Param::LFO_DEPTH,
    Param::LFO_PHASE,
};

/// The LXR setting an UNPATCHED_LXR_* param holds
constexpr Param automatedParam(int32_t unpatchedParam) {
	return kAutomatedParams[unpatchedParam - params::UNPATCHED_FIRST_LXR_PARAM];
}

/// An LXR setting 0..127 as a param value
constexpr int32_t settingToParamValue(int32_t setting) {
	return (setting - 64) * (1 << 25);
}

/// The nearest LXR setting 0..127 for a param value
constexpr int32_t paramValueToSetting(int32_t value) {
	// Round to the nearest step; the top of the param range is knob position 64, which is past the LXR's range
	const int64_t knobPos = (static_cast<int64_t>(value) + (1 << 24)) >> 25;
	return std::clamp<int32_t>(static_cast<int32_t>(knobPos) + 64, 0, 127);
}

/// A param value as the 0..1 value the engine takes, with the in-between values automation passes through. For a
/// whole step this is exactly setting / 127, as the LXR's parameter handler computes it.
constexpr float paramValueToValue(int32_t value) {
	const float setting = std::clamp(value * (1.f / (1 << 25)) + 64.f, 0.f, 127.f);
	return setting / 127.f;
}

/// All settings of a voice: the Patch's, with the automated ones taken from the params
void getValues(const deluge::dsp::lxr::Patch& patch, UnpatchedParamSet& unpatchedParams,
               deluge::dsp::lxr::Values& values);

/// The default values for the automated settings (those of the default voice type), for Sound::initParams()
void initParams(UnpatchedParamSet& unpatchedParams);

/// Write the automated settings as param attributes: those that are automated or differ from what initParams()
/// gives, which is where every reader of the file starts from. Other sounds don't use these params at all.
void writeParams(Serializer& writer, UnpatchedParamSet& unpatchedParams, bool writeAutomation);

/// Read an automated setting's attribute, if tagName is one
/// @return whether tagName was one
bool readParam(Deserializer& reader, char const* tagName, ParamCollectionSummary* unpatchedParamsSummary,
               int32_t readAutomationUpToPos);

/// Pick a voice type: the Patch gets all of its default settings, and so do the automated settings in the
/// ParamManager in modelStack (the clip, or the kit row, being edited). Other clips keep their values, as they do
/// whenever a param changes. Automation of these settings stays in place.
void loadVoiceType(deluge::dsp::lxr::Patch& patch, deluge::dsp::lxr::VoiceType type,
                   ModelStackWithThreeMainThings* modelStack);

/// A voice for one unison part of one note, or nullptr if out of memory
gsl::owner<deluge::dsp::lxr::Voice*> acquireVoice();
void releaseVoice(gsl::owner<deluge::dsp::lxr::Voice*> voice);

/// The current tempo, for tempo-synced LFOs
float currentBPM();

} // namespace deluge::processing::lxr
