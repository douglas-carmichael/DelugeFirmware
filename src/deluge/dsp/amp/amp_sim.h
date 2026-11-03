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

// The AMP effect: a guitar amp and speaker cab. Mono, float, blocks of up to 128 samples, no allocation. Per block:
//
//   tight low cut -> input voicing (6.5 kHz low-pass, +4 dB at 800 Hz) -> [2x upsample] -> preamp stage 1 ->
//   coupling and Miller filters -> stage 2 -> coupling and Miller filters -> tone stack -> makeup gain -> presence
//   -> power amp -> [2x downsample] -> output transformer (30 Hz high-pass) -> cab -> level
//
// The three clippers are f(v) = v / sqrt(1 + v^2) with first-order antiderivative anti-aliasing (ADAA): with
// F(v) = sqrt(1 + v^2), the ADAA output (F(v) - F(v')) / (v - v') is exactly (v + v') / (F(v) + F(v')), so there is no
// division by a small difference and nothing to special-case. Measured on a Deluge in blocks of 128 with NEON: 6.8 %
// of the CPU at 1x, 11.1 % at 2x.

#pragma once

#include "dsp/amp/cab_filters.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <utility>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define DELUGE_AMP_SIM_HAVE_NEON 1
#else
#define DELUGE_AMP_SIM_HAVE_NEON 0
#endif

namespace deluge::dsp::amp {

constexpr std::size_t kMaxBlock = 128; ///< the Deluge renders at most 128 samples per window
constexpr double kPi = 3.14159265358979323846;
/// Filter states below this (-300 dB) are set to zero once per block, so they never decay into denormals
constexpr float kFlush = 1e-15f;

inline float dbToGain(float db) {
	return std::pow(10.0f, db / 20.0f);
}

/// Trapezoidal (TPT) one-pole: stable and accurate down to very low cutoffs in single precision
class OnePole {
public:
	void setCutoff(float hz, float sampleRate) {
		const float g = std::tan(static_cast<float>(kPi) * std::min(hz, 0.45f * sampleRate) / sampleRate);
		g_ = g / (1.0f + g);
	}
	float lowpass(float x) {
		const float v = (x - s_) * g_;
		const float lp = v + s_;
		s_ = lp + v;
		return lp;
	}
	float highpass(float x) { return x - lowpass(x); }
	void reset() { s_ = 0.0f; }
	void flush() {
		if (std::fabs(s_) < kFlush) {
			s_ = 0.0f;
		}
	}

private:
	float g_ = 0.0f;
	float s_ = 0.0f;
};

/// First-order high shelf: unity below hz, gainDb above
class HighShelf {
public:
	void set(float hz, float gainDb, float sampleRate) {
		hp_.setCutoff(hz, sampleRate);
		k_ = dbToGain(gainDb) - 1.0f;
	}
	float process(float x) { return x + k_ * hp_.highpass(x); }
	void reset() { hp_.reset(); }
	void flush() { hp_.flush(); }

private:
	OnePole hp_;
	float k_ = 0.0f;
};

/// RBJ-cookbook peaking EQ, transposed direct form II
class Peak {
public:
	void set(float hz, float gainDb, float q, float sampleRate) {
		const double a = std::pow(10.0, gainDb / 40.0);
		const double w0 = 2.0 * kPi * hz / sampleRate;
		const double alpha = std::sin(w0) / (2.0 * q);
		const double cw = std::cos(w0);
		const double a0 = 1.0 + alpha / a;
		b0_ = static_cast<float>((1.0 + alpha * a) / a0);
		b1_ = static_cast<float>(-2.0 * cw / a0);
		b2_ = static_cast<float>((1.0 - alpha * a) / a0);
		a1_ = b1_;
		a2_ = static_cast<float>((1.0 - alpha / a) / a0);
	}
	float process(float x) {
		const float y = b0_ * x + z1_;
		z1_ = b1_ * x - a1_ * y + z2_;
		z2_ = b2_ * x - a2_ * y;
		return y;
	}
	void reset() { z1_ = z2_ = 0.0f; }
	void flush() {
		if (std::fabs(z1_) < kFlush && std::fabs(z2_) < kFlush) {
			z1_ = z2_ = 0.0f;
		}
	}

private:
	float b0_ = 1.0f, b1_ = 0.0f, b2_ = 0.0f, a1_ = 0.0f, a2_ = 0.0f;
	float z1_ = 0.0f, z2_ = 0.0f;
};

/// y = f(x + bias) - f(bias), f(v) = v / sqrt(1 + v^2), anti-aliased as described at the top. A nonzero bias makes one
/// side clip earlier, which adds the even harmonics of a single-ended tube stage.
class Clipper {
public:
	void configure(float bias) {
		bias_ = bias;
		offset_ = bias / std::sqrt(1.0f + bias * bias);
		reset();
	}
	float process(float x) {
		const float v = x + bias_;
		const float f = std::sqrt(1.0f + v * v);
		const float y = (v + lastV_) / (f + lastF_);
		lastV_ = v;
		lastF_ = f;
		return y - offset_;
	}
	/// Silence in gives exactly zero out from here: (bias + bias) / (F(bias) + F(bias)) - offset is exactly 0
	void reset() {
		lastV_ = bias_;
		lastF_ = std::sqrt(1.0f + bias_ * bias_);
	}

private:
	float bias_ = 0.0f;
	float offset_ = 0.0f;
	float lastV_ = 0.0f;
	float lastF_ = 1.0f;
};

struct ToneStackParts {
	double r1, r2, r3, r4, c1, c2, c3; ///< R1 treble pot, R2 bass pot, R3 mid pot, R4 slope resistor
};
/// Component values of the 5F6-A tweed circuit (1959)
inline constexpr ToneStackParts kAmericanToneStack{250e3, 1e6, 25e3, 56e3, 250e-12, 20e-9, 20e-9};
/// Component values of the 2203 circuit (1981)
inline constexpr ToneStackParts kBritishToneStack{220e3, 1e6, 22e3, 33e3, 470e-12, 22e-9, 22e-9};

/// The passive Bass/Mid/Treble network of classic American and British amps, from Yeh and Smith's third-order analog
/// model (DAFx-06, "Discretization of the '59 Fender Bassman Tone Stack"), bilinear transform, transposed direct form
/// II in double precision: its poles sit close to z = 1, too close for single precision at oversampled rates.
/// configure() keeps the state, so the knobs can move while it runs.
class ToneStack {
public:
	/// Knobs 0..1; the bass pot is log taper
	void configure(const ToneStackParts& p, float bass, float mid, float treble, float sampleRate) {
		const double t = treble;
		const double m = mid;
		const double l = std::exp((bass - 1.0) * 3.4);
		const double R1 = p.r1, R2 = p.r2, R3 = p.r3, R4 = p.r4, C1 = p.c1, C2 = p.c2, C3 = p.c3;

		const double b1 = t * C1 * R1 + m * C3 * R3 + l * (C1 * R2 + C2 * R2) + (C1 * R3 + C2 * R3);
		const double b2 = t * (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4) - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
		                  + m * (C1 * C3 * R1 * R3 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
		                  + l * (C1 * C2 * R1 * R2 + C1 * C2 * R2 * R4 + C1 * C3 * R2 * R4)
		                  + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
		                  + (C1 * C2 * R1 * R3 + C1 * C2 * R3 * R4 + C1 * C3 * R3 * R4);
		const double b3 = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
		                  - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
		                  + m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
		                  + t * C1 * C2 * C3 * R1 * R3 * R4 - t * m * C1 * C2 * C3 * R1 * R3 * R4
		                  + t * l * C1 * C2 * C3 * R1 * R2 * R4;
		const double a0 = 1.0;
		const double a1 = (C1 * R1 + C1 * R3 + C2 * R3 + C2 * R4 + C3 * R4) + m * C3 * R3 + l * (C1 * R2 + C2 * R2);
		const double a2 = m * (C1 * C3 * R1 * R3 - C2 * C3 * R3 * R4 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
		                  + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
		                  - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
		                  + l * (C1 * C2 * R2 * R4 + C1 * C2 * R1 * R2 + C1 * C3 * R2 * R4 + C2 * C3 * R2 * R4)
		                  + (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4 + C1 * C2 * R3 * R4 + C1 * C2 * R1 * R3
		                     + C1 * C3 * R3 * R4 + C2 * C3 * R3 * R4);
		const double a3 =
		    l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
		    - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
		    + m * (C1 * C2 * C3 * R3 * R3 * R4 + C1 * C2 * C3 * R1 * R3 * R3 - C1 * C2 * C3 * R1 * R3 * R4)
		    + l * C1 * C2 * C3 * R1 * R2 * R4 + C1 * C2 * C3 * R1 * R3 * R4;

		// H(s) = (b1 s + b2 s^2 + b3 s^3) / (a0 + a1 s + a2 s^2 + a3 s^3), s = c (1 - 1/z) / (1 + 1/z)
		const double c = 2.0 * sampleRate;
		const double c2 = c * c;
		const double c3 = c2 * c;
		const double B[4] = {b1 * c + b2 * c2 + b3 * c3, b1 * c - b2 * c2 - 3 * b3 * c3,
		                     -b1 * c - b2 * c2 + 3 * b3 * c3, -b1 * c + b2 * c2 - b3 * c3};
		const double A[4] = {a0 + a1 * c + a2 * c2 + a3 * c3, 3 * a0 + a1 * c - a2 * c2 - 3 * a3 * c3,
		                     3 * a0 - a1 * c - a2 * c2 + 3 * a3 * c3, a0 - a1 * c + a2 * c2 - a3 * c3};
		for (int i = 0; i < 4; i++) {
			b_[i] = B[i] / A[0];
			a_[i] = A[i] / A[0];
		}
		sampleRate_ = sampleRate;
	}

	float process(float in) {
		const double x = in;
		const double y = b_[0] * x + s_[0];
		s_[0] = b_[1] * x - a_[1] * y + s_[1];
		s_[1] = b_[2] * x - a_[2] * y + s_[2];
		s_[2] = b_[3] * x - a_[3] * y;
		return static_cast<float>(y);
	}

	/// |H| at hz, for the makeup gain and for tests
	[[nodiscard]] double magnitude(double hz) const {
		const std::complex<double> zInv = std::polar(1.0, -2.0 * kPi * hz / sampleRate_);
		const std::complex<double> num = b_[0] + zInv * (b_[1] + zInv * (b_[2] + zInv * b_[3]));
		const std::complex<double> den = 1.0 + zInv * (a_[1] + zInv * (a_[2] + zInv * a_[3]));
		return std::abs(num / den);
	}

	/// Power-average gain over 100 Hz - 5 kHz (log spaced)
	[[nodiscard]] double averageGain() const {
		double sum = 0.0;
		constexpr int kPoints = 64;
		for (int i = 0; i < kPoints; i++) {
			const double hz = 100.0 * std::pow(50.0, i / static_cast<double>(kPoints - 1));
			const double g = magnitude(hz);
			sum += g * g;
		}
		return std::sqrt(sum / kPoints);
	}

	void reset() { s_[0] = s_[1] = s_[2] = 0.0; }
	void flush() {
		if (std::fabs(s_[0]) < kFlush && std::fabs(s_[1]) < kFlush && std::fabs(s_[2]) < kFlush) {
			reset();
		}
	}

private:
	double b_[4]{};
	double a_[4]{1.0};
	double s_[3]{};
	double sampleRate_ = 44100.0;
};

/// The even taps h[0], h[2] ... h[46] of the 47-tap halfband FIR (Kaiser window, beta 8): they sum to 0.5, and the
/// centre tap h[23] is 0.5, so the gain at DC is 1
alignas(16) inline constexpr std::array<float, 24> kHalfbandEvenTaps = {
    -3.236778866e-05f, 2.146022744e-04f,  -6.899972796e-04f, 1.690635458e-03f,  -3.539435333e-03f, 6.670786068e-03f,
    -1.168527640e-02f, 1.951150224e-02f,  -3.190591559e-02f, 5.323911086e-02f,  -9.953366965e-02f, 3.160600364e-01f,
    3.160600364e-01f,  -9.953366965e-02f, 5.323911086e-02f,  -3.190591559e-02f, 1.951150224e-02f,  -1.168527640e-02f,
    6.670786068e-03f,  -3.539435333e-03f, 1.690635458e-03f,  -6.899972796e-04f, 2.146022744e-04f,  -3.236778866e-05f,
};

#if DELUGE_AMP_SIM_HAVE_NEON
/// acc[m] += h[I] * x[m - I] for 4 outputs in a and the next 4 in b. The coefficient comes from a lane of a NEON
/// register (vmlaq_lane_f32), so nothing moves between the core and NEON registers per tap.
template <int I>
[[gnu::always_inline]] inline void halfbandTap(float32x4_t& a, float32x4_t& b, const float* x,
                                               const float32x4_t (&h)[6]) {
	const float32x2_t hh = (I % 4 < 2) ? vget_low_f32(h[I / 4]) : vget_high_f32(h[I / 4]);
	a = vmlaq_lane_f32(a, vld1q_f32(x - I), hh, I % 2);
	b = vmlaq_lane_f32(b, vld1q_f32(x + 4 - I), hh, I % 2);
}
template <int... I>
[[gnu::always_inline]] inline void halfbandTaps(float32x4_t& a, float32x4_t& b, const float* x,
                                                const float32x4_t (&h)[6], std::integer_sequence<int, I...>) {
	(halfbandTap<I>(a, b, x, h), ...); // taps in order 0, 1, 2 ...: the same sum order as the scalar loop
}
#endif

/// 2x up/down sampling with the 47-tap halfband FIR. The centre tap sits on an odd index, so every other upsampler
/// output is a plain delay, and the downsampler needs only the even taps plus the centre: it keeps its input split
/// into the even phase (for the even taps) and the odd phase (for the centre tap). The NEON path does 8 outputs per
/// pass; each lane sums the taps in the scalar loop's order and vmla does not fuse, so both give the same results.
class Halfband2x {
public:
	static constexpr int kHalf = 23; ///< centre index (odd)
	static constexpr int kEvenTaps = kHalf + 1;

	void setNeon(bool on) { neon_ = on && DELUGE_AMP_SIM_HAVE_NEON; }

	/// n inputs -> 2n outputs, n <= kMaxBlock
	void upsample(const float* in, float* out, std::size_t n) {
		std::copy(in, in + n, up_.begin() + kHalf);
		std::size_t m = 0;
#if DELUGE_AMP_SIM_HAVE_NEON
		if (neon_) {
			m = upsampleNeon(out, n);
		}
#endif
		for (; m < n; m++) {
			const float* x = &up_[kHalf + m]; // x[0] newest, x[-i] older
			float acc = 0.0f;
			for (int i = 0; i < kEvenTaps; i++) {
				acc += kHalfbandEvenTaps[i] * x[-i];
			}
			out[2 * m] = 2.0f * acc;
			out[2 * m + 1] = x[-(kHalf / 2)]; // 2 * centre tap * x[m - 11]
		}
		std::copy(up_.begin() + n, up_.begin() + n + kHalf, up_.begin());
	}

	/// 2n inputs -> n outputs, n <= kMaxBlock: out[m] = 0.5 * odd[m - 12] + sum of h[i] * evenPhase[m - i], centre tap
	/// first
	void downsample(const float* in, float* out, std::size_t n) {
		std::size_t k = 0;
#if DELUGE_AMP_SIM_HAVE_NEON
		if (neon_) {
			for (; k + 4 <= n; k += 4) {
				const float32x4x2_t v = vld2q_f32(in + 2 * k);
				vst1q_f32(&evenIn_[kHalf + k], v.val[0]);
				vst1q_f32(&oddIn_[kHalf + k], v.val[1]);
			}
		}
#endif
		for (; k < n; k++) {
			evenIn_[kHalf + k] = in[2 * k];
			oddIn_[kHalf + k] = in[2 * k + 1];
		}
		std::size_t m = 0;
#if DELUGE_AMP_SIM_HAVE_NEON
		if (neon_) {
			m = downsampleNeon(out, n);
		}
#endif
		for (; m < n; m++) {
			const float* e = &evenIn_[kHalf + m];
			float acc = 0.5f * oddIn_[kHalf / 2 + m];
			for (int i = 0; i < kEvenTaps; i++) {
				acc += kHalfbandEvenTaps[i] * e[-i];
			}
			out[m] = acc;
		}
		std::copy(evenIn_.begin() + n, evenIn_.begin() + n + kHalf, evenIn_.begin());
		std::copy(oddIn_.begin() + n, oddIn_.begin() + n + kHalf, oddIn_.begin());
	}

	void reset() {
		up_.fill(0.0f);
		evenIn_.fill(0.0f);
		oddIn_.fill(0.0f);
	}

private:
#if DELUGE_AMP_SIM_HAVE_NEON
	static void loadTaps(float32x4_t (&h)[6]) {
		for (int k = 0; k < 6; k++) {
			h[k] = vld1q_f32(&kHalfbandEvenTaps[4 * k]);
		}
	}

	std::size_t upsampleNeon(float* out, std::size_t n) const {
		float32x4_t h[6];
		loadTaps(h);
		const float32x4_t two = vdupq_n_f32(2.0f);
		const float* x = &up_[kHalf];
		std::size_t m = 0;
		for (; m + 8 <= n; m += 8) {
			float32x4_t a = vdupq_n_f32(0.0f);
			float32x4_t b = a;
			halfbandTaps(a, b, x + m, h, std::make_integer_sequence<int, kEvenTaps>{});
			const float32x4x2_t lo = {{vmulq_f32(a, two), vld1q_f32(x + m - kHalf / 2)}};
			const float32x4x2_t hi = {{vmulq_f32(b, two), vld1q_f32(x + m + 4 - kHalf / 2)}};
			vst2q_f32(out + 2 * m, lo); // interleaved: filtered, delayed, filtered, delayed ...
			vst2q_f32(out + 2 * m + 8, hi);
		}
		return m;
	}

	std::size_t downsampleNeon(float* out, std::size_t n) const {
		float32x4_t h[6];
		loadTaps(h);
		const float32x4_t half = vdupq_n_f32(0.5f);
		const float* e = &evenIn_[kHalf];
		const float* o = &oddIn_[kHalf / 2];
		std::size_t m = 0;
		for (; m + 8 <= n; m += 8) {
			float32x4_t a = vmulq_f32(vld1q_f32(o + m), half);
			float32x4_t b = vmulq_f32(vld1q_f32(o + m + 4), half);
			halfbandTaps(a, b, e + m, h, std::make_integer_sequence<int, kEvenTaps>{});
			vst1q_f32(out + m, a);
			vst1q_f32(out + m + 4, b);
		}
		return m;
	}
#endif

	std::array<float, kHalf + kMaxBlock> up_{};
	std::array<float, kHalf + kMaxBlock> evenIn_{}; ///< the downsampler's input, even phase
	std::array<float, kHalf + kMaxBlock> oddIn_{};  ///< and odd phase
	bool neon_ = DELUGE_AMP_SIM_HAVE_NEON;
};

/// 1x or 2x
class Oversampler {
public:
	void setFactor(int factor) { factor_ = (factor >= 2) ? 2 : 1; }
	[[nodiscard]] int factor() const { return factor_; }
	void setNeon(bool on) { halfband_.setNeon(on); }

	/// 2x only: returns the upsampled block of 2n samples
	float* up(const float* in, std::size_t n) {
		halfband_.upsample(in, work_.data(), n);
		return work_.data();
	}
	/// 2x only: 2n samples in, n out
	void down(const float* in, float* out, std::size_t n) { halfband_.downsample(in, out, n); }
	void reset() { halfband_.reset(); }

private:
	int factor_ = 1;
	Halfband2x halfband_;
	std::array<float, 2 * kMaxBlock> work_{};
};

/// The built-in cabs as cascades of second-order sections (cab_filters.h), at about 5 multiply-adds per section per
/// sample. Direct form I (its state is just the last two inputs and outputs), one section at a time over the block.
/// The sections at or below 250 Hz (the first 2-3) run in double precision: in single precision their poles sit so
/// close to z = 1 that the rounding noise near the cab's low resonance reaches -80 dB. The NEON path makes 4 outputs
/// of a float section at once: unrolling the recurrence over 4 samples makes each output a fixed mix of the 4 new
/// inputs, the 2 inputs before them and the 2 outputs before them (8 columns of 4 numbers, computed in setCab). It
/// rounds differently from the scalar loop, so the two agree to float precision, not bit for bit.
class CabFilters {
public:
	/// 0-based design index, or -1 for none
	void setCab(int index) {
		design_ = (index >= 0 && index < kNumCabs) ? &kCabDesigns[index] : nullptr;
		if (design_ != nullptr) {
			for (int s = 0; s < design_->count; s++) {
				makeColumns(design_->sections[s], cols_[s]);
			}
		}
		reset();
	}
	[[nodiscard]] bool active() const { return design_ != nullptr; }
	void setNeon(bool on) { neon_ = on && DELUGE_AMP_SIM_HAVE_NEON; }

	void process(float* buf, std::size_t n) {
		for (int s = 0; s < design_->doubleCount; s++) {
			processDouble(design_->sections[s], doubleState_[s], buf, n);
		}
		for (int s = design_->doubleCount; s < design_->count; s++) {
			const CabSection& c = design_->sections[s];
			State& st = state_[s];
			std::size_t i = 0;
#if DELUGE_AMP_SIM_HAVE_NEON
			if (neon_) {
				i = processNeon(cols_[s], st, buf, n);
			}
#endif
			float x1 = st.x1, x2 = st.x2, y1 = st.y1, y2 = st.y2;
			for (; i < n; i++) {
				const float x = buf[i];
				// The term with the newest output last: the next sample waits only for one multiply-add
				const float y = (c.b0 * x + c.b1 * x1 + c.b2 * x2 - c.a2 * y2) - c.a1 * y1;
				x2 = x1;
				x1 = x;
				y2 = y1;
				y1 = y;
				buf[i] = y;
			}
			st = {x1, x2, y1, y2};
		}
	}

	void reset() {
		for (State& st : state_) {
			st = State{};
		}
		for (DoubleState& st : doubleState_) {
			st = DoubleState{};
		}
	}

	void flush() {
		for (State& st : state_) {
			if (std::fabs(st.y1) < kFlush && std::fabs(st.y2) < kFlush && std::fabs(st.x1) < kFlush
			    && std::fabs(st.x2) < kFlush) {
				st = State{};
			}
		}
		for (DoubleState& st : doubleState_) {
			if (std::fabs(st.y1) < kFlush && std::fabs(st.y2) < kFlush && std::fabs(st.x1) < kFlush
			    && std::fabs(st.x2) < kFlush) {
				st = DoubleState{};
			}
		}
	}

private:
	struct State {
		float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f; ///< x[-1], x[-2], y[-1], y[-2]
	};
	struct DoubleState {
		double x1 = 0.0, x2 = 0.0, y1 = 0.0, y2 = 0.0;
	};

	static void processDouble(const CabSection& c, DoubleState& st, float* buf, std::size_t n) {
		const double b0 = c.b0, b1 = c.b1, b2 = c.b2, a1 = c.a1, a2 = c.a2;
		double x1 = st.x1, x2 = st.x2, y1 = st.y1, y2 = st.y2;
		for (std::size_t i = 0; i < n; i++) {
			const double x = buf[i];
			const double y = (b0 * x + b1 * x1 + b2 * x2 - a2 * y2) - a1 * y1;
			x2 = x1;
			x1 = x;
			y2 = y1;
			y1 = y;
			buf[i] = static_cast<float>(y);
		}
		st = {x1, x2, y1, y2};
	}

	/// Output k of a 4-sample block = sum over j of cols[j][k] * u[j], u = {x[-2], x[-1], x[0], x[1], x[2], x[3],
	/// y[-2], y[-1]}: the recurrence run in double for each unit input, from the float coefficients
	static void makeColumns(const CabSection& c, float (&cols)[8][4]) {
		for (int j = 0; j < 8; j++) {
			double u[8] = {};
			u[j] = 1.0;
			const double x[6] = {u[0], u[1], u[2], u[3], u[4], u[5]};
			double y[6] = {u[6], u[7], 0.0, 0.0, 0.0, 0.0};
			for (int k = 0; k < 4; k++) {
				y[k + 2] = static_cast<double>(c.b0) * x[k + 2] + static_cast<double>(c.b1) * x[k + 1]
				           + static_cast<double>(c.b2) * x[k] - static_cast<double>(c.a1) * y[k + 1]
				           - static_cast<double>(c.a2) * y[k];
				cols[j][k] = static_cast<float>(y[k + 2]);
			}
		}
	}

#if DELUGE_AMP_SIM_HAVE_NEON
	static std::size_t processNeon(const float (&cols)[8][4], State& st, float* buf, std::size_t n) {
		const float32x4_t cxm2 = vld1q_f32(cols[0]), cxm1 = vld1q_f32(cols[1]);
		const float32x4_t cx0 = vld1q_f32(cols[2]), cx1 = vld1q_f32(cols[3]);
		const float32x4_t cx2 = vld1q_f32(cols[4]), cx3 = vld1q_f32(cols[5]);
		const float32x4_t cym2 = vld1q_f32(cols[6]), cym1 = vld1q_f32(cols[7]);
		const float xs[2] = {st.x2, st.x1};
		const float ys[2] = {st.y2, st.y1};
		float32x2_t xp = vld1_f32(xs); // {x[-2], x[-1]}
		float32x2_t yp = vld1_f32(ys); // {y[-2], y[-1]}
		std::size_t i = 0;
		for (; i + 4 <= n; i += 4) {
			const float32x4_t x = vld1q_f32(buf + i);
			const float32x2_t xlo = vget_low_f32(x);
			const float32x2_t xhi = vget_high_f32(x);
			// Two accumulators, the old outputs last, so the next block waits only for those two multiply-adds
			float32x4_t a = vmulq_lane_f32(cx0, xlo, 0);
			float32x4_t b = vmulq_lane_f32(cx1, xlo, 1);
			a = vmlaq_lane_f32(a, cx2, xhi, 0);
			b = vmlaq_lane_f32(b, cx3, xhi, 1);
			a = vmlaq_lane_f32(a, cxm2, xp, 0);
			b = vmlaq_lane_f32(b, cxm1, xp, 1);
			a = vmlaq_lane_f32(a, cym2, yp, 0);
			b = vmlaq_lane_f32(b, cym1, yp, 1);
			const float32x4_t y = vaddq_f32(a, b);
			vst1q_f32(buf + i, y);
			xp = xhi; // x[2], x[3] are the next block's x[-2], x[-1]
			yp = vget_high_f32(y);
		}
		st = {vget_lane_f32(xp, 1), vget_lane_f32(xp, 0), vget_lane_f32(yp, 1), vget_lane_f32(yp, 0)};
		return i;
	}
#endif

	const CabDesign* design_ = nullptr;
	bool neon_ = DELUGE_AMP_SIM_HAVE_NEON;
	State state_[kMaxCabSections]{};
	DoubleState doubleState_[kMaxCabSections]{};
	alignas(16) float cols_[kMaxCabSections][8][4]{};
};

enum class ToneStackType : uint8_t { AMERICAN, BRITISH };

/// The knobs, 0..1 (0.5 is the middle), and the output level in dB. They can change while the AMP runs (setKnobs).
struct Knobs {
	float gain = 0.5f;
	float bass = 0.5f;
	float mid = 0.5f;
	float treble = 0.5f;
	float presence = 0.5f;
	float levelDb = 0.0f;
};

/// Everything; changing anything but the knobs needs configure(), which also clears the state
struct Settings {
	Knobs knobs;
	int cab = 4; ///< 0 = no cab, 1..kNumCabs = kCabDesigns[cab - 1]
	ToneStackType toneStack = ToneStackType::BRITISH;
	int oversample = 2; ///< 1 or 2
};

class AmpSim {
public:
	/// Coefficients for these settings and a clean state: for switching the AMP on, and for a new cab, tone stack or
	/// oversampling factor
	void configure(const Settings& s, float sampleRate) {
		sampleRate_ = sampleRate;
		os_.setFactor(s.oversample);
		fsOs_ = sampleRate * static_cast<float>(os_.factor());

		inputLowpass_.setCutoff(6500.0f, sampleRate);
		midHump_.set(800.0f, 4.0f, 0.7f, sampleRate);
		stage1_.configure(0.25f);
		stage2_.configure(-0.15f); // tube stages invert, so the harder-clipping side alternates
		power_.configure(0.0f);    // push-pull: symmetric
		miller1_.setCutoff(6000.0f, fsOs_);
		couple2_.setCutoff(25.0f, fsOs_);
		miller2_.setCutoff(4500.0f, fsOs_);

		toneStackParts_ = (s.toneStack == ToneStackType::AMERICAN) ? &kAmericanToneStack : &kBritishToneStack;
		ToneStack noon;
		noon.configure(*toneStackParts_, 0.5f, 0.5f, 0.5f, fsOs_);
		makeup_ = static_cast<float>(1.0 / noon.averageGain()); // knobs at noon average 0 dB over 100 Hz - 5 kHz

		transformer_.setCutoff(30.0f, sampleRate); // the power amp clips an asymmetric wave: block the DC it makes
		cab_.setCab(s.cab - 1);
		reset();
		setKnobs(s.knobs);
		drive1_ = drive1Target_; // no glide after configure()
		drive2_ = drive2Target_;
		level_ = levelTarget_;
	}

	/// New knob values: the filters follow at once, the gains glide to them over the next block. Keeps the state.
	void setKnobs(const Knobs& k) {
		const float t = std::clamp(k.gain, 0.0f, 1.0f);
		tight_.setCutoff(30.0f + 150.0f * t * t, sampleRate_); // more gain, tighter low end: 30 Hz at 0, 180 Hz at 1
		drive1Target_ = dbToGain(6.0f + 18.0f * t);            // +6 .. +24 dB
		drive2Target_ = dbToGain(24.0f * t);                   //  0 .. +24 dB
		couple1_.setCutoff(20.0f + 100.0f * t, fsOs_);
		tone_.configure(*toneStackParts_, std::clamp(k.bass, 0.0f, 1.0f), std::clamp(k.mid, 0.0f, 1.0f),
		                std::clamp(k.treble, 0.0f, 1.0f), fsOs_);
		presence_.set(2500.0f, 6.0f * std::clamp(k.presence, 0.0f, 1.0f), fsOs_);
		levelTarget_ = dbToGain(k.levelDb);
	}

	/// Mono, in place, n <= kMaxBlock
	void process(float* buf, std::size_t n) {
		if (n == 0) {
			return;
		}
		for (std::size_t i = 0; i < n; i++) {
			const float v = tight_.highpass(buf[i]);
			buf[i] = midHump_.process(inputLowpass_.lowpass(v));
		}
		const std::size_t m = n * static_cast<std::size_t>(os_.factor());
		const float step1 = (drive1Target_ - drive1_) / static_cast<float>(m); // 0 when the knob didn't move
		const float step2 = (drive2Target_ - drive2_) / static_cast<float>(m);
		float d1 = drive1_;
		float d2 = drive2_;
		if (os_.factor() == 1) {
			for (std::size_t i = 0; i < n; i++) {
				d1 += step1;
				d2 += step2;
				buf[i] = transformer_.highpass(powerAmp(preamp(buf[i], d1, d2)));
			}
		}
		else {
			float* x = os_.up(buf, n);
			for (std::size_t i = 0; i < m; i++) {
				d1 += step1;
				d2 += step2;
				x[i] = powerAmp(preamp(x[i], d1, d2));
			}
			os_.down(x, buf, n);
			for (std::size_t i = 0; i < n; i++) {
				buf[i] = transformer_.highpass(buf[i]);
			}
		}
		drive1_ = drive1Target_;
		drive2_ = drive2Target_;
		if (cab_.active()) {
			cab_.process(buf, n);
		}
		const float stepLevel = (levelTarget_ - level_) / static_cast<float>(n);
		float level = level_;
		for (std::size_t i = 0; i < n; i++) {
			level += stepLevel;
			buf[i] *= level;
		}
		level_ = levelTarget_;
		flush();
	}

	/// Silence everywhere: after this, silence in gives exactly zero out
	void reset() {
		tight_.reset();
		inputLowpass_.reset();
		midHump_.reset();
		stage1_.reset();
		stage2_.reset();
		power_.reset();
		couple1_.reset();
		miller1_.reset();
		couple2_.reset();
		miller2_.reset();
		tone_.reset();
		presence_.reset();
		os_.reset();
		transformer_.reset();
		cab_.reset();
	}

	/// The NEON paths are on wherever the CPU has NEON; tests switch them off to compare with plain C
	void setNeon(bool on) {
		os_.setNeon(on);
		cab_.setNeon(on);
	}

private:
	/// The two preamp stages with their coupling and Miller filters
	float preamp(float x, float drive1, float drive2) {
		float v = stage1_.process(drive1 * x);
		v = miller1_.lowpass(couple1_.highpass(v));
		v = stage2_.process(drive2 * v);
		return miller2_.lowpass(couple2_.highpass(v));
	}

	/// Tone stack with its makeup gain, presence, power amp
	float powerAmp(float v) {
		v = makeup_ * tone_.process(v);
		return power_.process(presence_.process(v));
	}

	void flush() {
		tight_.flush();
		inputLowpass_.flush();
		midHump_.flush();
		couple1_.flush();
		miller1_.flush();
		couple2_.flush();
		miller2_.flush();
		tone_.flush();
		presence_.flush();
		transformer_.flush();
		cab_.flush();
	}

	float sampleRate_ = 44100.0f;
	float fsOs_ = 44100.0f;
	OnePole tight_;
	OnePole inputLowpass_;
	Peak midHump_;
	Clipper stage1_, stage2_, power_;
	OnePole couple1_, miller1_, couple2_, miller2_;
	const ToneStackParts* toneStackParts_ = &kBritishToneStack;
	ToneStack tone_;
	HighShelf presence_;
	Oversampler os_;
	OnePole transformer_;
	CabFilters cab_;
	float makeup_ = 1.0f;
	float drive1_ = 1.0f, drive1Target_ = 1.0f;
	float drive2_ = 1.0f, drive2Target_ = 1.0f;
	float level_ = 1.0f, levelTarget_ = 1.0f;
};

} // namespace deluge::dsp::amp
