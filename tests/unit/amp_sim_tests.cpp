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

#include "CppUTest/TestHarness.h"
#include "dsp/amp/amp_sim.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <memory>
#include <vector>

namespace amp = deluge::dsp::amp;

namespace {
constexpr float kRate = 44100.0f;

// Two seconds of a decaying sawtooth power chord (E2, B2, E3), peak about -12 dBFS
std::vector<float> testSignal() {
	std::vector<float> x(2 * 44100);
	const double freqs[3] = {82.41, 123.47, 164.81};
	for (size_t i = 0; i < x.size(); i++) {
		const double t = static_cast<double>(i) / 44100.0;
		double v = 0.0;
		for (double f : freqs) {
			v += 2.0 * std::fmod(f * t, 1.0) - 1.0;
		}
		x[i] = static_cast<float>(0.25 / 3.0 * std::exp(-1.5 * t) * v);
	}
	return x;
}

// The design the AMP was voiced at: gain 8, bass 5, mid 7, treble 6, presence 6 (of 10), the 4x12 smooth cab
amp::Settings design(int oversample) {
	amp::Settings s;
	s.knobs = {0.8f, 0.5f, 0.7f, 0.6f, 0.6f, 0.0f};
	s.cab = 4;
	s.toneStack = amp::ToneStackType::BRITISH;
	s.oversample = oversample;
	return s;
}

// Runs the AMP over x in blocks of fixedBlock, or of random sizes 1-128 (like the Deluge's render windows) when 0
std::vector<float> runAmp(const amp::Settings& s, std::vector<float> x, size_t fixedBlock, bool neon) {
	auto a = std::make_unique<amp::AmpSim>();
	a->setNeon(neon);
	a->configure(s, kRate);
	uint32_t seed = 777u;
	for (size_t pos = 0; pos < x.size();) {
		size_t n = fixedBlock;
		if (n == 0) {
			seed = seed * 1664525u + 1013904223u;
			n = 1 + (seed >> 8) % amp::kMaxBlock;
		}
		n = std::min(n, x.size() - pos);
		a->process(x.data() + pos, n);
		pos += n;
	}
	return x;
}

// Error energy relative to the reference, in dB
double esrDb(const std::vector<float>& got, const std::vector<float>& ref) {
	double err = 0.0;
	double sig = 0.0;
	for (size_t i = 0; i < ref.size(); i++) {
		const double d = static_cast<double>(got[i]) - static_cast<double>(ref[i]);
		err += d * d;
		sig += static_cast<double>(ref[i]) * static_cast<double>(ref[i]);
	}
	return err == 0.0 ? -999.0 : 10.0 * std::log10(err / sig);
}

double rmsDb(const std::vector<float>& x) {
	double sum = 0.0;
	for (float v : x) {
		sum += static_cast<double>(v) * static_cast<double>(v);
	}
	return 10.0 * std::log10(sum / static_cast<double>(x.size()));
}

float peakOf(const std::vector<float>& x) {
	float peak = 0.0f;
	for (float v : x) {
		peak = std::max(peak, std::fabs(v));
	}
	return peak;
}

bool allFinite(const std::vector<float>& x) {
	return std::all_of(x.begin(), x.end(), [](float v) { return std::isfinite(v); });
}

double cabResponseDb(const amp::CabDesign& d, double hz) {
	const std::complex<double> z = std::polar(1.0, -2.0 * amp::kPi * hz / kRate);
	std::complex<double> h = 1.0;
	for (int s = 0; s < d.count; s++) {
		const amp::CabSection& c = d.sections[s];
		h *= (static_cast<double>(c.b0) + static_cast<double>(c.b1) * z + static_cast<double>(c.b2) * z * z)
		     / (1.0 + static_cast<double>(c.a1) * z + static_cast<double>(c.a2) * z * z);
	}
	return 20.0 * std::log10(std::abs(h));
}
} // namespace

TEST_GROUP(AmpSimTests){};

TEST(AmpSimTests, toneStackIsPassiveAndStable) {
	for (const amp::ToneStackParts* parts : {&amp::kAmericanToneStack, &amp::kBritishToneStack}) {
		for (float fs : {44100.0f, 88200.0f}) {
			for (int b = 0; b <= 2; b++) {
				for (int m = 0; m <= 2; m++) {
					for (int t = 0; t <= 2; t++) {
						amp::ToneStack ts;
						ts.configure(*parts, b / 2.0f, m / 2.0f, t / 2.0f, fs);
						double worst = 0.0;
						for (int i = 0; i < 100; i++) {
							worst = std::max(worst, ts.magnitude(20.0 * std::pow(1000.0, i / 99.0)));
						}
						CHECK(worst <= 1.0 + 1e-6); // a passive network never gains
						float y = ts.process(1.0f);
						const int len = static_cast<int>(fs * 3.0f);
						for (int i = 0; i < len; i++) {
							y = ts.process(0.0f);
						}
						CHECK(std::fabs(y) < 1e-6f); // the impulse response has died away after 3 s
					}
				}
			}
		}
	}
}

TEST(AmpSimTests, oversamplerPassbandAndImages) {
	for (double hz : {100.0, 1000.0, 5000.0, 10000.0, 16000.0}) {
		amp::Oversampler os;
		os.setFactor(2);
		std::vector<float> in(amp::kMaxBlock);
		std::vector<float> out(amp::kMaxBlock);
		double sumIn = 0.0;
		double sumOut = 0.0;
		for (int blk = 0; blk < 400; blk++) {
			for (size_t i = 0; i < in.size(); i++) {
				in[i] = static_cast<float>(std::sin(2.0 * amp::kPi * hz * (blk * 128.0 + i) / kRate));
			}
			float* x = os.up(in.data(), in.size());
			os.down(x, out.data(), out.size());
			if (blk >= 100) {
				for (size_t i = 0; i < in.size(); i++) {
					sumIn += static_cast<double>(in[i]) * in[i];
					sumOut += static_cast<double>(out[i]) * out[i];
				}
			}
		}
		DOUBLES_EQUAL(0.0, 10.0 * std::log10(sumOut / sumIn), 0.01); // dB
	}
	// Upsampling a 15 kHz tone leaves its first image at 44.1 - 15 = 29.1 kHz
	amp::Oversampler os;
	os.setFactor(2);
	std::vector<float> in(amp::kMaxBlock);
	std::vector<float> up;
	for (int blk = 0; blk < 200; blk++) {
		for (size_t i = 0; i < in.size(); i++) {
			in[i] = static_cast<float>(std::sin(2.0 * amp::kPi * 15000.0 * (blk * 128.0 + i) / kRate));
		}
		float* x = os.up(in.data(), in.size());
		if (blk >= 100) {
			up.insert(up.end(), x, x + 2 * in.size());
		}
	}
	auto bin = [&](double hz) {
		std::complex<double> acc = 0.0;
		for (size_t i = 0; i < up.size(); i++) {
			const double w = 0.5 - 0.5 * std::cos(2.0 * amp::kPi * i / (up.size() - 1));
			acc += static_cast<double>(up[i]) * w * std::polar(1.0, -2.0 * amp::kPi * hz * i / (2.0 * kRate));
		}
		return std::abs(acc);
	};
	CHECK(20.0 * std::log10(bin(kRate - 15000.0) / bin(15000.0)) < -100.0);
}

TEST(AmpSimTests, neonHalfbandsMatchPlainC) {
	amp::Oversampler plain;
	amp::Oversampler neon;
	plain.setFactor(2);
	neon.setFactor(2);
	plain.setNeon(false);
	neon.setNeon(true);
	std::vector<float> in(amp::kMaxBlock);
	std::vector<float> outPlain(amp::kMaxBlock);
	std::vector<float> outNeon(amp::kMaxBlock);
	uint32_t seed = 12345u;
	float worst = 0.0f;
	for (int blk = 0; blk < 400; blk++) {
		seed = seed * 1664525u + 1013904223u;
		const size_t n = 1 + (seed >> 8) % amp::kMaxBlock;
		for (size_t i = 0; i < n; i++) {
			seed = seed * 1664525u + 1013904223u;
			in[i] = static_cast<float>(static_cast<int32_t>(seed)) / 2147483648.0f;
		}
		float* xp = plain.up(in.data(), n);
		float* xn = neon.up(in.data(), n);
		for (size_t i = 0; i < 2 * n; i++) {
			worst = std::max(worst, std::fabs(xp[i] - xn[i]));
		}
		plain.down(xp, outPlain.data(), n);
		neon.down(xn, outNeon.data(), n);
		for (size_t i = 0; i < n; i++) {
			worst = std::max(worst, std::fabs(outPlain[i] - outNeon[i]));
		}
	}
	// Identical without fused multiply-adds (as on the Deluge); a host compiler may fuse the scalar loop
	CHECK(worst <= 1e-6f);
}

TEST(AmpSimTests, clipperMatchesItsCurve) {
	// On a slow sine the anti-aliased output is the curve at the midpoint of each step, to second order
	amp::Clipper c;
	c.configure(0.25f);
	double worst = 0.0;
	float last = 0.0f;
	for (int i = 0; i < 44100; i++) {
		const auto x = static_cast<float>(3.0 * std::sin(2.0 * amp::kPi * 50.0 * i / kRate));
		const float y = c.process(x);
		if (i > 0) {
			const double v = 0.5 * (static_cast<double>(x) + last) + 0.25;
			const double ref = v / std::sqrt(1.0 + v * v) - 0.25 / std::sqrt(1.0 + 0.0625);
			worst = std::max(worst, std::fabs(y - ref));
		}
		last = x;
	}
	CHECK(worst < 1e-4);
	// A constant input gives the curve's value: no division by a small difference
	amp::Clipper d;
	d.configure(0.0f);
	float y = 0.0f;
	for (int i = 0; i < 10; i++) {
		y = d.process(2.0f);
	}
	DOUBLES_EQUAL(2.0 / std::sqrt(5.0), y, 1e-6);
}

TEST(AmpSimTests, cabsFollowTheirDesigns) {
	// Each design's analog response (make_cab_filters.py's sections), normalized to 0 dB average over 80 Hz - 5 kHz
	const double hz[9] = {100, 200, 500, 1000, 2000, 3000, 5000, 8000, 12000};
	const double designDb[amp::kNumCabs][9] = {
	    {-6.56, -1.06, -1.96, 0.75, 1.84, 3.59, -1.50, -13.62, -31.22}, // 1x12 open
	    {0.86, -0.50, -1.63, 0.73, 2.93, -1.50, -7.49, -23.64, -41.25}, // 2x12
	    {1.75, -0.96, -4.99, -1.60, 2.34, 3.75, -4.59, -19.28, -36.02}, // 4x12
	    {2.66, -0.06, -4.13, -0.93, 1.79, 1.92, -7.20, -20.26, -36.71}, // 4x12 smooth
	};
	for (int cab = 0; cab < amp::kNumCabs; cab++) {
		const amp::CabDesign& d = amp::kCabDesigns[cab];
		for (int s = 0; s < d.count; s++) {
			// Poles inside the unit circle: the stability triangle of a second-order section
			const amp::CabSection& c = d.sections[s];
			CHECK(std::fabs(c.a2) < 1.0f);
			CHECK(std::fabs(c.a1) < 1.0f + c.a2);
		}
		for (int i = 0; i < 9; i++) {
			DOUBLES_EQUAL(designDb[cab][i], cabResponseDb(d, hz[i]), 0.4);
		}
		double sum = 0.0;
		for (int i = 0; i < 200; i++) {
			const double g = std::pow(10.0, cabResponseDb(d, 80.0 * std::pow(5000.0 / 80.0, i / 199.0)) / 20.0);
			sum += g * g;
		}
		DOUBLES_EQUAL(0.0, 10.0 * std::log10(sum / 200.0), 0.01); // 0 dB average over 80 Hz - 5 kHz
	}
}

TEST(AmpSimTests, neonCabMatchesPlainC) {
	const std::vector<float> in = testSignal();
	for (int cab = 0; cab < amp::kNumCabs; cab++) {
		std::vector<float> plain = in;
		std::vector<float> neon = in;
		amp::CabFilters a;
		amp::CabFilters b;
		a.setNeon(false);
		b.setNeon(true);
		a.setCab(cab);
		b.setCab(cab);
		for (size_t pos = 0; pos < in.size(); pos += amp::kMaxBlock) {
			const size_t n = std::min(amp::kMaxBlock, in.size() - pos);
			a.process(plain.data() + pos, n);
			b.process(neon.data() + pos, n);
		}
		// The NEON path unrolls each section's recurrence over 4 samples, which rounds about 8 dB worse than the plain
		// loop. This loud, low chord straight into the cab is the hard case: the 2x12 reaches -98 dB (in the whole
		// chain, after the amp's high-passes, it is -104 dB)
		CHECK(esrDb(neon, plain) < -95.0);
	}
}

TEST(AmpSimTests, blockSizesDontChangeTheOutput) {
	const std::vector<float> in = testSignal();
	for (int os : {1, 2}) {
		const std::vector<float> plain128 = runAmp(design(os), in, 128, false);
		CHECK(runAmp(design(os), in, 0, false) == plain128); // plain C: identical in any block sizes
		CHECK(esrDb(runAmp(design(os), in, 128, true), plain128) < -100.0);
		CHECK(esrDb(runAmp(design(os), in, 0, true), plain128) < -100.0);
	}
}

TEST(AmpSimTests, outputLevelMatchesThePrototype) {
	// The Mac prototype's output for this signal at the design settings, measured when the AMP was ported
	const std::vector<float> in = testSignal();
	DOUBLES_EQUAL(-6.9461, rmsDb(runAmp(design(1), in, 128, false)), 0.01);
	DOUBLES_EQUAL(-6.9547, rmsDb(runAmp(design(2), in, 128, false)), 0.01);
}

TEST(AmpSimTests, extremeInputStaysFinite) {
	std::vector<float> square(44100);
	for (size_t i = 0; i < square.size(); i++) {
		square[i] = ((i / 220) % 2 != 0u) ? 1.0f : -1.0f;
	}
	amp::Settings hot;
	hot.knobs = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f};
	for (int os : {1, 2}) {
		hot.oversample = os;
		const std::vector<float> y = runAmp(hot, square, 0, true);
		CHECK(allFinite(y));
		CHECK(peakOf(y) < 4.0f);
	}
}

TEST(AmpSimTests, silenceEndsInExactZero) {
	// The filters' states are cleared once they fall below -300 dB, so they never decay into denormals
	std::vector<float> x = testSignal();
	x.resize(x.size() / 4); // half a second of sound
	x.resize(x.size() + 3 * 44100, 0.0f);
	for (int os : {1, 2}) {
		const std::vector<float> y = runAmp(design(os), x, 0, true);
		CHECK(std::all_of(y.end() - 22050, y.end(), [](float v) { return v == 0.0f; }));
	}
}

TEST(AmpSimTests, resetClearsTheState) {
	auto a = std::make_unique<amp::AmpSim>();
	a->configure(design(2), kRate);
	std::vector<float> x = testSignal();
	for (size_t pos = 0; pos < 22050; pos += amp::kMaxBlock) {
		a->process(x.data() + pos, amp::kMaxBlock);
	}
	a->reset();
	std::vector<float> zeros(amp::kMaxBlock, 0.0f);
	a->process(zeros.data(), zeros.size());
	CHECK(std::all_of(zeros.begin(), zeros.end(), [](float v) { return v == 0.0f; }));
}

TEST(AmpSimTests, movingKnobsStayFiniteAndBounded) {
	auto a = std::make_unique<amp::AmpSim>();
	a->configure(design(2), kRate);
	std::vector<float> x = testSignal();
	uint32_t seed = 99u;
	auto random01 = [&seed]() {
		seed = seed * 1664525u + 1013904223u;
		return static_cast<float>(seed >> 8) / 16777216.0f;
	};
	for (size_t pos = 0; pos < x.size(); pos += amp::kMaxBlock) {
		a->setKnobs({random01(), random01(), random01(), random01(), random01(), -12.0f + 24.0f * random01()});
		a->process(x.data() + pos, std::min(amp::kMaxBlock, x.size() - pos));
	}
	CHECK(allFinite(x));
	CHECK(peakOf(x) < 16.0f); // up to +12 dB of level on a peak of about 4
}
