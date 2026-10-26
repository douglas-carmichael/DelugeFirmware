// Tests for the drum synth engine (OscType::LXR, dsp/lxr/) and its params (processing/lxr/lxr_sound.h).

#include "CppUTest/TestHarness.h"
#include "dsp/lxr/patch.h"
#include "dsp/lxr/voice.h"
#include "processing/lxr/lxr_sound.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace lxr = deluge::dsp::lxr;
namespace lxr_sound = deluge::processing::lxr;
namespace params = deluge::modulation::params;
using lxr::Param;
using lxr::VoiceType;

namespace {

struct Hit {
	std::vector<int32_t> samples;
	bool finished = false;
};

/// Play one hit of a patch for numSamples, rendering numSamplesPerCall at a time
Hit renderHit(const lxr::Patch& patch, int32_t numSamples, int32_t numSamplesPerCall = 128, float pitchRatio = 1,
              uint8_t velocity = 100) {
	lxr::Values values;
	patch.getValues(values);
	lxr::Voice voice;
	voice.init(patch.model(), values, 1234, 120);
	voice.trigger(values, velocity, 0);

	Hit hit;
	std::vector<int32_t> buffer(numSamplesPerCall);
	while (static_cast<int32_t>(hit.samples.size()) < numSamples) {
		const int32_t numThisTime = std::min<int32_t>(numSamplesPerCall, numSamples - hit.samples.size());
		// Once finished, the voice renders silence
		const bool active = voice.render(patch.model(), values, buffer.data(), numThisTime, pitchRatio, 120);
		hit.samples.insert(hit.samples.end(), buffer.begin(), buffer.begin() + numThisTime);
		hit.finished = hit.finished || !active;
	}
	return hit;
}

int32_t peak(const std::vector<int32_t>& samples, size_t from = 0, size_t to = SIZE_MAX) {
	int32_t result = 0;
	for (size_t i = from; i < std::min(to, samples.size()); i++) {
		result = std::max(result, std::abs(samples[i]));
	}
	return result;
}

int32_t zeroCrossings(const std::vector<int32_t>& samples, size_t from, size_t to) {
	int32_t count = 0;
	for (size_t i = from + 1; i < to; i++) {
		if ((samples[i - 1] < 0) != (samples[i] < 0)) {
			count++;
		}
	}
	return count;
}

} // namespace

TEST_GROUP(LxrPatchTest){};

TEST(LxrPatchTest, defaultsAreTheInitKit) {
	lxr::Patch patch;
	CHECK(patch.voiceType() == VoiceType::DRUM_1);
	CHECK(patch.model() == lxr::Model::DRUM);
	CHECK_EQUAL(31, patch.get(Param::COARSE));
	CHECK_EQUAL(63, patch.get(Param::FINE));
	CHECK_EQUAL(21, patch.get(Param::DECAY));
	CHECK_EQUAL(127, patch.get(Param::LEVEL));
	CHECK_EQUAL(127, patch.get(Param::SAMPLE_RATE));
	CHECK_EQUAL(0, patch.get(Param::LFO_DEST));

	patch.loadDefaults(VoiceType::CYMBAL);
	CHECK(patch.model() == lxr::Model::CYMBAL);
	CHECK_EQUAL(107, patch.get(Param::DECAY));
	CHECK_EQUAL(1, patch.get(Param::FILTER_TYPE));

	patch.loadDefaults(VoiceType::OPEN_HAT);
	CHECK(patch.model() == lxr::Model::HIHAT);
	CHECK_EQUAL(17, patch.get(Param::DECAY));
}

TEST(LxrPatchTest, setKeepsValuesInRange) {
	lxr::Patch patch;
	patch.set(Param::OSC_WAVE, 9);
	CHECK_EQUAL(5, patch.get(Param::OSC_WAVE));
	patch.set(Param::LFO_SYNC, -3);
	CHECK_EQUAL(0, patch.get(Param::LFO_SYNC));
	patch.set(Param::LFO_DEST, 200);
	CHECK_EQUAL(lxr::kNumParams, patch.get(Param::LFO_DEST));
}

TEST(LxrPatchTest, serializeRoundTrip) {
	lxr::Patch patch;
	patch.loadDefaults(VoiceType::SNARE);
	for (int32_t p = 0; p < lxr::kNumParams; p++) {
		patch.set(static_cast<Param>(p), (p * 37 + 11) % (lxr::maxValue(static_cast<Param>(p)) + 1));
	}
	uint8_t data[lxr::Patch::kSerializedSize];
	patch.serialize(data);

	lxr::Patch copy;
	copy.deserialize(data, sizeof(data));
	CHECK(copy.voiceType() == VoiceType::SNARE);
	for (int32_t p = 0; p < lxr::kNumParams; p++) {
		CHECK_EQUAL(patch.get(static_cast<Param>(p)), copy.get(static_cast<Param>(p)));
	}
}

TEST(LxrPatchTest, shorterDataKeepsTheVoiceTypeDefaults) {
	lxr::Patch patch;
	patch.loadDefaults(VoiceType::CLOSED_HAT);
	uint8_t data[lxr::Patch::kSerializedSize];
	patch.serialize(data);
	data[2 + static_cast<int32_t>(Param::COARSE)] = 99;

	lxr::Patch copy;
	copy.deserialize(data, 2 + static_cast<int32_t>(Param::COARSE) + 1);
	CHECK(copy.voiceType() == VoiceType::CLOSED_HAT);
	CHECK_EQUAL(99, copy.get(Param::COARSE));
	CHECK_EQUAL(patch.get(Param::DECAY), copy.get(Param::DECAY));

	// Too short for even the voice type: nothing changes
	copy.deserialize(data, 1);
	CHECK_EQUAL(99, copy.get(Param::COARSE));
}

TEST(LxrPatchTest, sustainsForever) {
	lxr::Patch patch;
	CHECK_FALSE(patch.sustainsForever());
	patch.set(Param::DECAY, 127);
	CHECK(patch.sustainsForever());

	// A repeating attack phase that never decays, which only the snare and cymbal have
	patch.loadDefaults(VoiceType::SNARE);
	patch.set(Param::REPEAT, 3);
	patch.set(Param::ATTACK, 127);
	CHECK(patch.sustainsForever());
	patch.loadDefaults(VoiceType::DRUM_1);
	patch.set(Param::REPEAT, 3);
	patch.set(Param::ATTACK, 127);
	CHECK_FALSE(patch.sustainsForever());
}

TEST_GROUP(LxrParamTest){};

TEST(LxrParamTest, everySettingSurvivesTheParamValue) {
	for (int32_t setting = 0; setting <= 127; setting++) {
		const int32_t value = lxr_sound::settingToParamValue(setting);
		CHECK_EQUAL(setting, lxr_sound::paramValueToSetting(value));
		// Exactly what the LXR's parameter handler gets for the setting
		CHECK(lxr_sound::paramValueToValue(value) == setting / 127.f);
	}
}

TEST(LxrParamTest, paramValuesRoundToTheNearestSetting) {
	const int32_t value = lxr_sound::settingToParamValue(10);
	CHECK_EQUAL(10, lxr_sound::paramValueToSetting(value + (1 << 24) - 1));
	CHECK_EQUAL(11, lxr_sound::paramValueToSetting(value + (1 << 24)));
	CHECK_EQUAL(0, lxr_sound::paramValueToSetting(INT32_MIN));
	CHECK_EQUAL(127, lxr_sound::paramValueToSetting(INT32_MAX));
	CHECK(lxr_sound::paramValueToValue(INT32_MIN) == 0.f);
	CHECK(lxr_sound::paramValueToValue(INT32_MAX) == 1.f);
	// In between two settings, the engine gets an in-between value
	const float between = lxr_sound::paramValueToValue(value + (1 << 24));
	CHECK(between > 10 / 127.f && between < 11 / 127.f);
}

TEST(LxrParamTest, automatedParamsMatchTheirIds) {
	CHECK(lxr_sound::automatedParam(params::UNPATCHED_LXR_COARSE) == Param::COARSE);
	CHECK(lxr_sound::automatedParam(params::UNPATCHED_LXR_FINE) == Param::FINE);
	CHECK(lxr_sound::automatedParam(params::UNPATCHED_LXR_PULSE_WIDTH) == Param::PULSE_WIDTH);
	CHECK(lxr_sound::automatedParam(params::UNPATCHED_LXR_NOISE_FREQ) == Param::NOISE_FREQ);
	CHECK(lxr_sound::automatedParam(params::UNPATCHED_LXR_MIX) == Param::MIX);
	CHECK(lxr_sound::automatedParam(params::UNPATCHED_LXR_MOD1_FREQ) == Param::MOD1_FREQ);
	CHECK(lxr_sound::automatedParam(params::UNPATCHED_LXR_MOD1_GAIN) == Param::MOD1_GAIN);
	CHECK(lxr_sound::automatedParam(params::UNPATCHED_LXR_MOD2_FREQ) == Param::MOD2_FREQ);
	CHECK(lxr_sound::automatedParam(params::UNPATCHED_LXR_MOD2_GAIN) == Param::MOD2_GAIN);
	CHECK(lxr_sound::automatedParam(params::UNPATCHED_LXR_PITCH_DECAY) == Param::PITCH_DECAY);
	CHECK(lxr_sound::automatedParam(params::UNPATCHED_LXR_PITCH_AMOUNT) == Param::PITCH_AMOUNT);
	CHECK(lxr_sound::automatedParam(params::UNPATCHED_LXR_PITCH_SLOPE) == Param::PITCH_SLOPE);
	CHECK(lxr_sound::automatedParam(params::UNPATCHED_LXR_ATTACK) == Param::ATTACK);
	CHECK(lxr_sound::automatedParam(params::UNPATCHED_LXR_DECAY) == Param::DECAY);
	CHECK(lxr_sound::automatedParam(params::UNPATCHED_LXR_SLOPE) == Param::SLOPE);
	CHECK(lxr_sound::automatedParam(params::UNPATCHED_LXR_REPEAT) == Param::REPEAT);
	CHECK(lxr_sound::automatedParam(params::UNPATCHED_LXR_TRANSIENT_LEVEL) == Param::TRANSIENT_LEVEL);
	CHECK(lxr_sound::automatedParam(params::UNPATCHED_LXR_TRANSIENT_PITCH) == Param::TRANSIENT_PITCH);
	CHECK(lxr_sound::automatedParam(params::UNPATCHED_LXR_FILTER_FREQ) == Param::FILTER_FREQ);
	CHECK(lxr_sound::automatedParam(params::UNPATCHED_LXR_FILTER_RESO) == Param::FILTER_RESO);
	CHECK(lxr_sound::automatedParam(params::UNPATCHED_LXR_FILTER_DRIVE) == Param::FILTER_DRIVE);
	CHECK(lxr_sound::automatedParam(params::UNPATCHED_LXR_DRIVE) == Param::DRIVE);
	CHECK(lxr_sound::automatedParam(params::UNPATCHED_LXR_SAMPLE_RATE) == Param::SAMPLE_RATE);
	CHECK(lxr_sound::automatedParam(params::UNPATCHED_LXR_LEVEL) == Param::LEVEL);
	CHECK(lxr_sound::automatedParam(params::UNPATCHED_LXR_VELO_AMOUNT) == Param::VELO_AMOUNT);
	CHECK(lxr_sound::automatedParam(params::UNPATCHED_LXR_LFO_RATE) == Param::LFO_RATE);
	CHECK(lxr_sound::automatedParam(params::UNPATCHED_LXR_LFO_DEPTH) == Param::LFO_DEPTH);
	CHECK(lxr_sound::automatedParam(params::UNPATCHED_LXR_LFO_PHASE) == Param::LFO_PHASE);
}

TEST(LxrParamTest, theAutomatedParamsAreTheContinuousSettings) {
	for (int32_t p = 0; p < lxr::kNumParams; p++) {
		const auto param = static_cast<Param>(p);
		const auto count = std::count(lxr_sound::kAutomatedParams.begin(), lxr_sound::kAutomatedParams.end(), param);
		CHECK_EQUAL(lxr::maxValue(param) == 127 ? 1 : 0, count);
	}
}

TEST_GROUP(LxrVoiceTest){};

TEST(LxrVoiceTest, everyVoiceTypeSoundsAndEnds) {
	for (int32_t type = 0; type < lxr::kNumVoiceTypes; type++) {
		lxr::Patch patch;
		patch.loadDefaults(static_cast<VoiceType>(type));
		const Hit hit = renderHit(patch, 30 * kSampleRate);
		CHECK(hit.finished);
		CHECK_EQUAL(0, peak(hit.samples, hit.samples.size() - kSampleRate));
		CHECK(peak(hit.samples) > 1000);
		CHECK(peak(hit.samples) <= 32767);
	}
}

TEST(LxrVoiceTest, theLengthOfEachRenderDoesNotMatter) {
	lxr::Patch patch;
	patch.loadDefaults(VoiceType::SNARE);
	const Hit inBlocks = renderHit(patch, kSampleRate, 64);
	const Hit inOddSizes = renderHit(patch, kSampleRate, 37);
	const Hit inLargeSizes = renderHit(patch, kSampleRate, 128);
	CHECK(inBlocks.samples == inOddSizes.samples);
	CHECK(inBlocks.samples == inLargeSizes.samples);
}

TEST(LxrVoiceTest, decayAt127SustainsForever) {
	lxr::Patch patch;
	patch.set(Param::DECAY, 127);
	const Hit hit = renderHit(patch, 3 * kSampleRate);
	CHECK_FALSE(hit.finished);
	CHECK(peak(hit.samples, 2 * kSampleRate) > 1000);
}

TEST(LxrVoiceTest, pitchRatioSetsThePitch) {
	// A steady sine: no pitch envelope, modulator, transient or decay
	lxr::Patch patch;
	patch.set(Param::PITCH_AMOUNT, 0);
	patch.set(Param::MOD1_GAIN, 0);
	patch.set(Param::TRANSIENT_LEVEL, 0);
	patch.set(Param::DECAY, 127);
	const Hit low = renderHit(patch, 2 * kSampleRate, 128, 1);
	const Hit high = renderHit(patch, 2 * kSampleRate, 128, 2);
	const int32_t lowCrossings = zeroCrossings(low.samples, kSampleRate / 2, 2 * kSampleRate);
	const int32_t highCrossings = zeroCrossings(high.samples, kSampleRate / 2, 2 * kSampleRate);
	CHECK(lowCrossings > 50);
	CHECK(std::abs(highCrossings - 2 * lowCrossings) <= 4);
}

TEST(LxrVoiceTest, velocityToLevel) {
	lxr::Patch patch;
	patch.set(Param::VELO_TO_LEVEL, 1);
	const Hit loud = renderHit(patch, kSampleRate / 2, 128, 1, 127);
	const Hit quiet = renderHit(patch, kSampleRate / 2, 128, 1, 32);
	CHECK(peak(quiet.samples) * 2 < peak(loud.samples));

	// Without it, the voice ignores velocity (the Deluge applies velocity itself)
	patch.set(Param::VELO_TO_LEVEL, 0);
	CHECK(renderHit(patch, kSampleRate / 2, 128, 1, 127).samples
	      == renderHit(patch, kSampleRate / 2, 128, 1, 32).samples);
}
