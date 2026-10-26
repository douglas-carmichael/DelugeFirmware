// The acid lowpass filter mode: the LP2 filter of the LXR drum synth engine (see dsp/lxr/components.h), in the
// Deluge's filter framework so any sound can use it.
//
// The filter is from the LXR firmware by Erica Synths (https://github.com/erica-synths/lxr-desktop-app,
// commit a43cd4f), Core/Src/Soundengine/DSPAudio/ResonantFilter.c. Modified for the Deluge: it takes the Deluge's
// frequency, resonance and morph (as drive) params and 32-bit samples, and keeps the level of the other modes.
//
// Copyright (C) 2012-2026 Erica Synths
// SPDX-License-Identifier: GPL-3.0-only
//
// This file is derived from the LXR firmware. It is licensed under the GNU General Public License version 3,
// with the additional terms in dsp/lxr/NOTICE.md.

#pragma once

#include "dsp/filter/filter.h"
#include "util/fixedpoint.h"

namespace deluge::dsp::filter {

class AcidFilter : public Filter<AcidFilter> {
public:
	AcidFilter() = default;
	// returns a compensatory gain value
	q31_t setConfig(q31_t frequency, q31_t resonance, FilterMode lpfMode, q31_t lpfMorph, q31_t filterGain);
	void doFilter(q31_t* startSample, q31_t* endSample, int32_t sampleIncrememt);
	void doFilterStereo(q31_t* startSample, q31_t* endSample);
	void resetFilter() {
		l = State{};
		r = State{};
	}

private:
	struct State {
		float a;
		float b;
	};
	[[gnu::always_inline]] inline q31_t doLP2(q31_t input, State& state);
	State l;
	State r;

	float coefficient;
	float feedback;
	float inputScale;
};

} // namespace deluge::dsp::filter
