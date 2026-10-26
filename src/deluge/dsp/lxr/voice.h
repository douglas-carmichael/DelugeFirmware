// One playing voice of the LXR drum synth engine.
//
// Ported from the LXR firmware by Erica Synths (https://github.com/erica-synths/lxr-desktop-app, commit a43cd4f):
// the voices in Core/Src/Soundengine/DSPAudio/{DrumVoice,Snare,CymbalVoice,HiHat}.c, the per-block order of work
// in mixer.c (mixer_calcNextSampleBlock(), mixer_decimateBlock()), the parameter scaling in
// Core/Src/Soundengine/MIDI/MidiParser.c (midiParser_ccHandler()) and the velocity and LFO modulation in
// Core/Src/Soundengine/DSPAudio/modulationNode.c.
//
// Modified for the Deluge: a Voice object is one note of one LXR voice instead of the LXR's six global voices,
// so modulation targets stay inside the voice, the LFO restarts on its own hit only, the hardware fader and pan
// stages are left to the Deluge's own level and pan, and render() hands out any number of samples while still
// computing the LXR's 64-sample blocks internally. The Deluge's note and pitch modulation reach the oscillators
// as a frequency multiplier.
//
// Copyright (C) 2012-2026 Erica Synths
// SPDX-License-Identifier: GPL-3.0-only
//
// This file is derived from the LXR firmware. It is licensed under the GNU General Public License version 3,
// with the additional terms in NOTICE.md in this directory.

#pragma once

#include "dsp/lxr/components.h"
#include "dsp/lxr/patch.h"

namespace deluge::dsp::lxr {

class Voice {
public:
	/// Set up for a new note of the given voice circuit, loading every setting
	/// @param bpm current tempo, for tempo-synced LFO rates
	void init(Model model, const Values& values, uint32_t randomSeed, float bpm);

	/// Drum_trigger(), Snare_trigger(), Cymbal_trigger(), HiHat_trigger()
	/// @param sampleTime running count of audio samples, so a free-running LFO carries on from note to note
	void trigger(const Values& values, uint8_t velocity, uint32_t sampleTime);

	/// Render the next numSamples samples, each in 16-bit range, into out. Settings that changed since the last call
	/// take effect from the next 64-sample block, as they would on the LXR.
	/// @param pitchRatio frequency multiplier for the oscillators; 1 plays the voice at its own tuning
	/// @param bpm current tempo, for tempo-synced LFO rates
	/// @return false once the voice has died away; everything it renders after that is silence
	bool render(Model model, const Values& values, int32_t* out, int32_t numSamples, float pitchRatio, float bpm);

private:
	void applyAllValues(const Values& values);
	void applyValue(Param param, float value);
	/// The part of midiParser_ccHandler() for this voice: isModulator selects its isFloatModulator scaling
	void applyParam(Param param, uint8_t itg, float flt, bool isModulator);
	void applyBaseValue(const Values& values, uint8_t destination);
	/// modNode_updateValue(): move a destination around its stored value by amount * (value * 2 - 1)
	void applyModulation(const Values& values, uint8_t destination, float amount, float value);
	[[nodiscard]] bool isValidDestination(uint8_t destination) const;

	void computeBlock(const Values& values);
	void calcAsync();
	void calcSyncBlock(int16_t* buf);
	void decimateBlock(int16_t* buf);
	[[nodiscard]] bool envelopeFinished() const;

	Model model_ = Model::DRUM;

	OscInfo osc_{};        ///< the main (fm) oscillator
	OscInfo modOsc_{};     ///< drum: FM/mix modulator, cymbal and hihat: mod osc 1
	OscInfo modOsc2_{};    ///< cymbal and hihat: mod osc 2
	OscInfo noiseOsc_{};   ///< snare
	float fmModAmount_{};  ///< drum: fmModAmount, cymbal and hihat: fmModAmount1
	float fmModAmount2_{}; ///< cymbal and hihat
	float mix_{};          ///< snare: 0 = 100% osc, 1 = 100% noise
	float vol_{};
	float velo_{};

	DecayEg oscPitchEg_{};
	float egPitchModAmount_{};
	TransientGenerator transGen_{};
	Lfo lfo_{};
	SlopeEg2 oscVolEg_{};
	float egValueOscVol_{};
	Distortion distortion_{};
	ResonantFilter filter_{};
	uint8_t filterType_{};
	double hatDecay_{};       ///< hihat: decayClosed, which only reaches the envelope on the next hit
	double ampFilterInput_{}; ///< drum: amp envelope value at the end of the block
	double lastGain_{};       ///< drum: ... and at its start
	bool mixOscs_{};
	SnapEg snapEg_{};
	bool volumeMod_{}; ///< scale the level by velocity
	float combinedVolume_{};
	OnePoleFilter volumeSmoother_{}; ///< drum: MixerPageFilter[0]
	float pwm_{};

	// mixer_decimateBlock()
	float decimationRate_{};
	float decimationCount_{};
	int16_t decimationSample_{};

	// velocityModulators[] and the LFO's modTarget
	uint8_t veloDestination_{};
	float veloAmount_{};
	float veloValue_{};
	uint8_t lfoDestination_{};
	float lfoAmount_{};

	Random rng_{};
	Values applied_{}; ///< the values the voice's state was last set up from
	bool finished_{};

	int16_t block_[kBlockSize]{};
	int32_t blockPos_{};
};

} // namespace deluge::dsp::lxr
