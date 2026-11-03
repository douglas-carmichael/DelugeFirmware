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

// AmpEffect: the AMP on a track, in the Deluge's 32-bit audio. Its DSP (AmpSim) has its own tests; these check the
// switching, fades, sleep, STEREO and memory.

#include "CppUTest/TestHarness.h"
#include "dsp/amp/amp_effect.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace amp = deluge::dsp::amp;

namespace {
int allocations = 0;
int frees = 0;
bool failAllocations = false;
} // namespace

// The firmware takes the AMP's memory from fast internal RAM (amp_memory.cpp); here it comes from the heap
namespace deluge::dsp::amp {
void* allocAmpMemory(std::size_t size) {
	if (failAllocations) {
		return nullptr;
	}
	allocations++;
	return std::malloc(size);
}
void freeAmpMemory(void* memory) {
	frees++;
	std::free(memory);
}
} // namespace deluge::dsp::amp

namespace {

constexpr int32_t kFade = amp::AmpEffect::kFadeSamples;

/// A sawtooth chord at about -12 dB re 2^24 (a track at full level), decaying a little; L and R can differ
std::vector<StereoSample> testSignal(size_t n, bool sidesDiffer = false) {
	std::vector<StereoSample> x(n);
	for (size_t i = 0; i < n; i++) {
		const double t = static_cast<double>(i) / 44100.0;
		const double l = (2.0 * std::fmod(110.0 * t, 1.0) - 1.0) + (2.0 * std::fmod(164.81 * t, 1.0) - 1.0);
		const double r = sidesDiffer ? (2.0 * std::fmod(146.83 * t, 1.0) - 1.0) : l;
		const double g = 0.125 * 16777216.0 * std::exp(-t);
		x[i].l = static_cast<q31_t>(g * l);
		x[i].r = static_cast<q31_t>(g * r);
	}
	return x;
}

std::vector<StereoSample> silence(size_t n) {
	return std::vector<StereoSample>(n, StereoSample{0, 0});
}

/// Runs an AMP over x in blocks of `block` samples (the last one may be shorter)
void runAmp(amp::AmpEffect& a, std::vector<StereoSample>& x, size_t from = 0, size_t to = SIZE_MAX,
            size_t block = 128) {
	to = std::min(to, x.size());
	for (size_t pos = from; pos < to; pos += block) {
		a.process(std::span<StereoSample>(x.data() + pos, std::min(block, to - pos)));
	}
}

bool same(const StereoSample& a, const StereoSample& b) {
	return a.l == b.l && a.r == b.r;
}

/// The largest difference between two signals over [from, to)
int64_t maxDiff(const std::vector<StereoSample>& a, const std::vector<StereoSample>& b, size_t from, size_t to) {
	int64_t worst = 0;
	for (size_t i = from; i < to; i++) {
		worst = std::max(
		    {worst, std::abs(static_cast<int64_t>(a[i].l) - b[i].l), std::abs(static_cast<int64_t>(a[i].r) - b[i].r)});
	}
	return worst;
}

} // namespace

TEST_GROUP(AmpEffectTests){void setup() override{allocations = 0;
frees = 0;
failAllocations = false;
}
}
;

TEST(AmpEffectTests, offLeavesTheAudioAlone) {
	amp::AmpEffect a;
	const auto dry = testSignal(1000);
	auto x = dry;
	runAmp(a, x);
	CHECK_FALSE(a.isEnabled());
	CHECK_FALSE(a.isActive());
	LONGS_EQUAL(0, maxDiff(x, dry, 0, x.size()));
	LONGS_EQUAL(0, allocations);
}

TEST(AmpEffectTests, switchingOnFadesInFromTheDrySound) {
	amp::AmpEffect a;
	CHECK(a.setEnabled(true));
	amp::AmpEffect ref; // the same AMP, fully in from the start
	CHECK(ref.copyFrom(a));

	const auto dry = testSignal(2000);
	auto x = dry;
	auto y = dry;
	runAmp(a, x);
	runAmp(ref, y);

	// The first sample is almost all dry, then the fade takes kFadeSamples, then the two are the same
	CHECK(std::abs(static_cast<int64_t>(x[0].l) - dry[0].l) <= std::abs(static_cast<int64_t>(y[0].l) - dry[0].l));
	CHECK(maxDiff(x, y, 0, kFade) > 0);
	LONGS_EQUAL(0, maxDiff(x, y, kFade, x.size()));
	CHECK(maxDiff(y, dry, 0, y.size()) > 1000000); // and the AMP changes the sound
}

TEST(AmpEffectTests, switchingOffFadesToDryAndGivesTheMemoryBack) {
	amp::AmpEffect a;
	CHECK(a.setEnabled(true));
	const auto dry = testSignal(3000);
	auto x = dry;
	runAmp(a, x, 0, 1024);
	CHECK(a.isActive());

	CHECK(a.setEnabled(false));
	CHECK(a.isActive()); // still fading
	runAmp(a, x, 1024);
	CHECK_FALSE(a.isActive());
	LONGS_EQUAL(allocations, frees);
	// After the fade, dry
	CHECK(maxDiff(x, dry, 1024, 1024 + kFade) > 0);
	LONGS_EQUAL(0, maxDiff(x, dry, 1024 + kFade, x.size()));
}

TEST(AmpEffectTests, aSettingChangeFadesOutAndBackIn) {
	amp::AmpEffect a;
	CHECK(a.setEnabled(true));
	const auto dry = testSignal(4096);
	auto x = dry;
	runAmp(a, x, 0, 1024);

	amp::Settings s = a.settings();
	s.cab = 1;
	s.toneStack = amp::ToneStackType::AMERICAN;
	CHECK(a.setSettings(s));
	runAmp(a, x, 1024); // fades out over 1024-1151 (fully dry at 1151), sets up, fades in over 1152-1279

	LONGS_EQUAL(dry[1151].l, x[1151].l);
	// From 1152 the AMP runs with the new settings from a clean start: the same as a new AMP started there
	amp::AmpEffect ref;
	CHECK(ref.copyFrom(a));
	auto y = dry;
	runAmp(ref, y, 1152);
	CHECK(maxDiff(x, y, 1152, 1152 + kFade) > 0);
	LONGS_EQUAL(0, maxDiff(x, y, 1152 + kFade, x.size()));
}

TEST(AmpEffectTests, sleepsOnSilenceAndWakesClean) {
	amp::AmpEffect a;
	CHECK(a.setEnabled(true));
	const size_t tail = amp::AmpEffect::kTailSamples;
	auto x = testSignal(1024);
	auto quiet = silence(((tail + 1024) / 128 + 1) * 128); // so the signal comes back at the start of a block
	x.insert(x.end(), quiet.begin(), quiet.end());
	const size_t wake = x.size();
	auto more = testSignal(2048);
	x.insert(x.end(), more.begin(), more.end());

	auto y = x;
	runAmp(a, y);
	// The tail rings on into the silence, then the AMP sleeps: exact silence out
	CHECK(maxDiff(y, x, 1024, 1024 + 512) > 0);
	LONGS_EQUAL(0, maxDiff(y, x, wake - 1024, wake));
	// It wakes from a clean state, the same as a new AMP
	amp::AmpEffect ref;
	CHECK(ref.copyFrom(a));
	auto z = x;
	runAmp(ref, z, wake);
	LONGS_EQUAL(0, maxDiff(y, z, wake, x.size()));
}

TEST(AmpEffectTests, monoMixesTheSidesStereoKeepsThem) {
	const auto dry = testSignal(2048, true);

	amp::AmpEffect mono;
	amp::AmpEffect stereo;
	amp::Settings s;
	s.channels = amp::Channels::STEREO;
	CHECK(stereo.setSettings(s));
	CHECK(mono.setEnabled(true));
	CHECK(stereo.setEnabled(true));
	LONGS_EQUAL(3, allocations); // one AMP, then two

	auto x = dry;
	auto y = dry;
	runAmp(mono, x);
	runAmp(stereo, y);
	for (size_t i = kFade; i < x.size(); i++) { // (during the fade-in, the dry sides differ)
		LONGS_EQUAL(x[i].l, x[i].r);
	}
	CHECK(maxDiff(y, x, kFade, y.size()) > 0);

	// STEREO's left side is an AMP on the left alone (a mono AMP fed the left side)
	std::vector<q31_t> l(dry.size());
	for (size_t i = 0; i < dry.size(); i++) {
		l[i] = dry[i].l;
	}
	amp::AmpEffect leftRef;
	CHECK(leftRef.copyFrom(mono));
	for (size_t pos = 0; pos < l.size(); pos += 128) {
		leftRef.process(std::span<q31_t>(l.data() + pos, 128));
	}
	for (size_t i = kFade; i < dry.size(); i++) {
		LONGS_EQUAL(l[i], y[i].l);
	}
}

TEST(AmpEffectTests, stereoCatchesUpAfterMonoBlocks) {
	amp::AmpEffect a;
	amp::Settings s;
	s.channels = amp::Channels::STEREO;
	CHECK(a.setSettings(s));
	CHECK(a.setEnabled(true));
	const auto dry = testSignal(2048);

	// A synth whose voices render in mono: the left AMP runs alone
	std::vector<q31_t> m(1024);
	for (size_t i = 0; i < m.size(); i++) {
		m[i] = dry[i].l;
	}
	for (size_t pos = 0; pos < m.size(); pos += 128) {
		a.process(std::span<q31_t>(m.data() + pos, 128));
	}
	// Then stereo with both sides the same: the right AMP took the left one's state, so the sides stay the same
	auto x = dry;
	runAmp(a, x, 1024);
	for (size_t i = 1024; i < x.size(); i++) {
		LONGS_EQUAL(x[i].l, x[i].r);
	}
}

TEST(AmpEffectTests, switchingChannelsTakesAndGivesBackTheSecondAmp) {
	amp::AmpEffect a;
	CHECK(a.setEnabled(true));
	LONGS_EQUAL(1, allocations);
	auto x = testSignal(4096, true);
	runAmp(a, x, 0, 512);

	amp::Settings s = a.settings();
	s.channels = amp::Channels::STEREO;
	CHECK(a.setSettings(s));
	LONGS_EQUAL(2, allocations); // taken from the UI
	runAmp(a, x, 512, 1024);

	s.channels = amp::Channels::MONO;
	CHECK(a.setSettings(s));
	runAmp(a, x, 1024, 2048); // given back once the fade is out
	LONGS_EQUAL(1, frees);
	for (size_t i = 1024 + 2 * kFade; i < 2048; i++) {
		LONGS_EQUAL(x[i].l, x[i].r);
	}
}

TEST(AmpEffectTests, noMemoryLeavesItOff) {
	amp::AmpEffect a;
	failAllocations = true;
	CHECK_FALSE(a.setEnabled(true));
	CHECK_FALSE(a.isEnabled());
	CHECK_FALSE(a.isActive());
	const auto dry = testSignal(512);
	auto x = dry;
	runAmp(a, x);
	LONGS_EQUAL(0, maxDiff(x, dry, 0, x.size()));

	// STEREO needs a second AMP: without memory the setting stays MONO
	failAllocations = false;
	CHECK(a.setEnabled(true));
	failAllocations = true;
	amp::Settings s = a.settings();
	s.channels = amp::Channels::STEREO;
	CHECK_FALSE(a.setSettings(s));
	CHECK(a.settings().channels == amp::Channels::MONO);
}

TEST(AmpEffectTests, knobsChangeTheSound) {
	amp::AmpEffect a;
	amp::AmpEffect b;
	CHECK(a.setEnabled(true));
	CHECK(b.copyFrom(a));
	amp::Knobs k;
	k.gain = 0.9f;
	k.treble = 0.2f;
	b.setKnobs(k);
	b.setKnobs(k); // no change
	CHECK(b.settings().knobs == k);

	const auto dry = testSignal(2048);
	auto x = dry;
	auto y = dry;
	runAmp(a, x);
	runAmp(b, y);
	CHECK(maxDiff(x, y, 2 * kFade, x.size()) > 100000);
}

TEST(AmpEffectTests, resetAppliesAPendingChangeAndClearsTheTail) {
	amp::AmpEffect a;
	CHECK(a.setEnabled(true));
	auto x = testSignal(1024);
	runAmp(a, x);
	amp::Settings s = a.settings();
	s.oversample = 1;
	CHECK(a.setSettings(s));
	a.reset(); // e.g. the track stops rendering
	// Silence in now gives exact silence out, and switching off needs no fade
	auto q = silence(512);
	runAmp(a, q);
	LONGS_EQUAL(0, maxDiff(q, silence(512), 0, q.size()));
	CHECK(a.setEnabled(false));
	CHECK_FALSE(a.isActive());
	LONGS_EQUAL(allocations, frees);
}

TEST(AmpEffectTests, theDestructorGivesTheMemoryBack) {
	{
		amp::AmpEffect a;
		amp::Settings s;
		s.channels = amp::Channels::STEREO;
		CHECK(a.setSettings(s));
		CHECK(a.setEnabled(true));
		LONGS_EQUAL(2, allocations);
	}
	LONGS_EQUAL(2, frees);
}

TEST(AmpEffectTests, aBusIsDrivenLikeASound12dBQuieter) {
	// A kit or an audio track (Input::BUS) gets 12 dB less input and 12 dB more output: a signal 4x hotter there
	// drives the AMP exactly as the quieter one does on a sound, and comes out 4x as loud
	amp::AmpEffect source;
	CHECK(source.setEnabled(true));
	amp::AmpEffect sound;
	amp::AmpEffect bus;
	CHECK(sound.copyFrom(source));
	CHECK(bus.copyFrom(source));

	const auto quiet = testSignal(2048);
	auto hot = quiet;
	for (StereoSample& s : hot) {
		s.l *= 4;
		s.r *= 4;
	}
	auto x = quiet;
	auto y = hot;
	for (size_t pos = 0; pos < x.size(); pos += 128) {
		sound.process(std::span<StereoSample>(x.data() + pos, 128));
		bus.process(std::span<StereoSample>(y.data() + pos, 128), amp::AmpEffect::Input::BUS);
	}
	int64_t worst = 0;
	for (size_t i = 0; i < x.size(); i++) {
		worst = std::max({worst, std::abs(static_cast<int64_t>(y[i].l) - 4 * static_cast<int64_t>(x[i].l)),
		                  std::abs(static_cast<int64_t>(y[i].r) - 4 * static_cast<int64_t>(x[i].r))});
	}
	CHECK(worst <= 4);                               // only the conversion back to integers rounds differently
	CHECK(maxDiff(x, quiet, 0, x.size()) > 1000000); // and the AMP changed the sound
}
