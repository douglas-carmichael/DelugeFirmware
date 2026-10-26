// The acid lowpass filter mode. See acid.h for where this code comes from and what changed.
//
// Copyright (C) 2012-2026 Erica Synths
// SPDX-License-Identifier: GPL-3.0-only
//
// This file is derived from the LXR firmware. It is licensed under the GNU General Public License version 3,
// with the additional terms in dsp/lxr/NOTICE.md.

#include "dsp/filter/acid.h"
#include "dsp/lxr/components.h"
#include <algorithm>
#include <cmath>

namespace deluge::dsp::filter {

namespace {

/// The level that stands for the LXR's full scale of 16-bit samples: about the level a full-scale sample reaches
/// in a voice
constexpr float kFullScale = 1 << 27;

/// The LXR's drive with the drive setting at 0 (SVF_setDrive())
constexpr float kMinDrive = 0.4f;

} // namespace

q31_t AcidFilter::setConfig(q31_t freq, q31_t res, FilterMode lpfMode, q31_t lpfMorph, q31_t filterGain) {
	(void)lpfMode;
	curveFrequency(freq);

	// tannedFrequency is tan(pi * cutoff / sample rate), with 1 as 2^28. The LXR takes the cutoff as a fraction of
	// the sample rate, up to 0.45 (SVF_directSetFilterValue()).
	constexpr float kPi = 3.14159265358979f;
	const float f = std::min(std::atan(tannedFrequency * (1.f / (1 << 28))) * (1 / kPi), 0.45f);
	coefficient = lxr::lp2::coefficient(f);

	// raw resonance is 0 - 536870896 (see SVFilter::setConfig()), the LXR's resonance 0 - 1 (SVF_setReso())
	const float resonance = std::clamp(res * (1.f / (1 << 29)), 0.f, 1.f);
	float q = 1 - resonance;
	if (q < 0.1f) {
		q = 0.02f;
	}
	feedback = lxr::lp2::feedback(q, coefficient);

	// morph is the LXR's drive setting (SVF_setDrive()), arriving as q31
	const float drive = lpfMorph * (1.f / ONE_Q31);
	inputScale = (kMinDrive + drive * drive * 6) / kFullScale;
	return filterGain;
}

[[gnu::always_inline]] inline q31_t AcidFilter::doLP2(q31_t input, State& state) {
	const float x = lxr::softClipTwo(input * inputScale);
	const float y = lxr::lp2::process(state.a, state.b, x, coefficient, feedback);
	// The LXR's filter turns its input down by kMinDrive before the soft clip; turn it back up, so that with no
	// drive, quiet signals keep their level as in the other modes. The output stays within +-2.5 * kFullScale.
	return static_cast<q31_t>(y * (kFullScale / kMinDrive));
}

[[gnu::hot]] void AcidFilter::doFilter(q31_t* startSample, q31_t* endSample, int32_t sampleIncrememt) {
	q31_t* currentSample = startSample;
	do {
		*currentSample = doLP2(*currentSample, l);
		currentSample += sampleIncrememt;
	} while (currentSample < endSample);
}

[[gnu::hot]] void AcidFilter::doFilterStereo(q31_t* startSample, q31_t* endSample) {
	q31_t* currentSample = startSample;
	do {
		*currentSample = doLP2(*currentSample, l);
		*(currentSample + 1) = doLP2(*(currentSample + 1), r);
		currentSample += 2;
	} while (currentSample < endSample);
}

} // namespace deluge::dsp::filter
