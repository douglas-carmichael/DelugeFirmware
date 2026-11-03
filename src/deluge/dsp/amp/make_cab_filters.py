#!/usr/bin/env python3
# Copyright © 2026 Douglas Carmichael
#
# This file is part of The Synthstrom Audible Deluge Firmware.
#
# The Synthstrom Audible Deluge Firmware is free software: you can redistribute it and/or modify it under the
# terms of the GNU General Public License as published by the Free Software Foundation,
# either version 3 of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
# without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with this program.
# If not, see <https://www.gnu.org/licenses/>.
"""Generate cab_filters.h: the AMP effect's speaker cabs as cascades of second-order filter sections.

Each cab is our own design, not a measured impulse response: a list of analog sections (a high-pass with the cab's
low resonance, peaks and dips for the speaker's character, low-passes for its roll-off). Each analog section becomes
one digital section at 44.1 kHz:
- high-passes (60-110 Hz): the bilinear transform, which keeps their exact double zero at DC and is accurate this far
  below Nyquist;
- low-passes and peaks: the bilinear transform would put zeros at Nyquist and squeeze them near it (up to 11 dB too
  dark at 12 kHz), so instead the analog poles map exactly (z = e^(sT)) and the numerator is fitted to the analog
  magnitude in dB from 20 Hz to 21 kHz.
The first section also carries the gain for 0 dB average over 80 Hz - 5 kHz. Each cab follows its design within
0.36 dB up to 12 kHz.

Usage: python3 make_cab_filters.py [out.h]   (default: cab_filters.h next to this script; needs numpy and scipy)"""

import os
import sys

import numpy as np
from scipy.optimize import least_squares

FS = 44100.0
# Leading sections at or below this run in double precision: in single precision their poles sit so close to z = 1
# that rounding noise near the cab's low resonance reaches about -80 dB (-97 to -122 dB this way)
DOUBLE_UP_TO_HZ = 250

# (kind, frequency Hz, Q) for "hp" / "lp", (kind, frequency Hz, gain dB, Q) for "pk"
CABS = [
    (
        "1x12 open",  # open back: looser low end, bright, extended top
        [
            ("hp", 110, 0.55),
            ("hp", 60, 0.7),
            ("pk", 180, 1.5, 1.2),
            ("pk", 450, -2.0, 1.0),
            ("pk", 1200, 1.5, 1.0),
            ("pk", 2900, 4.0, 1.6),
            ("pk", 4600, 2.5, 2.5),
            ("pk", 6800, -5.0, 6.0),
            ("pk", 7600, 3.0, 6.0),
            ("lp", 5600, 0.8),
            ("lp", 6500, 0.6),
            ("lp", 9000, 0.7),
        ],
    ),
    (
        "2x12",  # closed back: warm, mid-forward, earlier roll-off
        [
            ("hp", 85, 1.1),
            ("pk", 130, 1.5, 1.2),
            ("pk", 400, -1.0, 1.0),
            ("pk", 1000, 2.0, 0.9),
            ("pk", 2100, 4.5, 1.4),
            ("pk", 3400, -2.5, 2.5),
            ("pk", 4000, 2.0, 3.0),
            ("pk", 5900, 2.0, 5.0),
            ("pk", 6600, -4.0, 5.0),
            ("lp", 4600, 0.9),
            ("lp", 5200, 0.6),
            ("lp", 8000, 0.7),
        ],
    ),
    (
        "4x12",  # closed back, modern: tight low thump, scooped low mids, aggressive upper mids
        [
            ("hp", 90, 1.6),
            ("pk", 250, 1.0, 1.0),
            ("pk", 500, -3.0, 1.2),
            ("pk", 1400, 2.0, 1.2),
            ("pk", 2600, 6.0, 1.4),
            ("pk", 3600, 3.0, 2.0),
            ("pk", 4400, -6.0, 3.0),
            ("pk", 5100, 3.0, 3.0),
            ("pk", 6500, -4.0, 6.0),
            ("pk", 7300, 2.0, 6.0),
            ("pk", 8600, -3.0, 6.0),
            ("lp", 5400, 1.0),
            ("lp", 6000, 0.6),
            ("lp", 8500, 0.7),
        ],
    ),
    (
        "4x12 smooth",  # the 4x12 with its upper-mid peaks softened (2.6 kHz +6 -> +4 dB, 3.6 kHz +3 -> +1.5,
        # 5.1 kHz +3 -> +1) and the roll-off starting at 5 kHz instead of 5.4
        [
            ("hp", 90, 1.6),
            ("pk", 250, 1.0, 1.0),
            ("pk", 500, -3.0, 1.2),
            ("pk", 1400, 2.0, 1.2),
            ("pk", 2600, 4.0, 1.4),
            ("pk", 3600, 1.5, 2.0),
            ("pk", 4400, -6.0, 3.0),
            ("pk", 5100, 1.0, 3.0),
            ("pk", 6500, -4.0, 6.0),
            ("pk", 7300, 2.0, 6.0),
            ("pk", 8600, -3.0, 6.0),
            ("lp", 5000, 1.0),
            ("lp", 6000, 0.6),
            ("lp", 8500, 0.7),
        ],
    ),
]


def hp2(f, fc, q):
    s = 1j * f / fc
    return s * s / (s * s + s / q + 1)


def lp2(f, fc, q):
    s = 1j * f / fc
    return 1 / (s * s + s / q + 1)


def peak(f, fc, db, q):
    a = 10 ** (db / 40)
    s = 1j * f / fc
    return (s * s + s * a / q + 1) / (s * s + s / (a * q) + 1)


def section_response(f, sec):
    if sec[0] == "hp":
        return hp2(f, sec[1], sec[2])
    if sec[0] == "lp":
        return lp2(f, sec[1], sec[2])
    return peak(f, sec[1], sec[2], sec[3])


def target(f, sections):
    h = np.ones_like(f, dtype=complex)
    for sec in sections:
        h *= section_response(f, sec)
    return np.abs(h)


def bilinear_biquad(sec):
    """(b0, b1, b2, a1, a2), a0 = 1: the RBJ-cookbook forms, which are the bilinear transforms of exactly the
    prototypes above, prewarped at the section's frequency."""
    kind, f = sec[0], sec[1]
    w0 = 2 * np.pi * f / FS
    cw, sw = np.cos(w0), np.sin(w0)
    if kind == "pk":
        a_lin = 10 ** (sec[2] / 40)
        alpha = sw / (2 * sec[3])
        b = [1 + alpha * a_lin, -2 * cw, 1 - alpha * a_lin]
        a = [1 + alpha / a_lin, -2 * cw, 1 - alpha / a_lin]
    else:
        alpha = sw / (2 * sec[2])
        if kind == "hp":
            b = [(1 + cw) / 2, -(1 + cw), (1 + cw) / 2]
        else:
            b = [(1 - cw) / 2, 1 - cw, (1 - cw) / 2]
        a = [1 + alpha, -2 * cw, 1 - alpha]
    return np.array([b[0] / a[0], b[1] / a[0], b[2] / a[0], a[1] / a[0], a[2] / a[0]])


def cascade_db(coefs, f):
    z = np.exp(-1j * 2 * np.pi * np.asarray(f, float) / FS)
    h = np.ones_like(z)
    for b0, b1, b2, a1, a2 in coefs:
        h *= (b0 + b1 * z + b2 * z * z) / (1 + a1 * z + a2 * z * z)
    return 20 * np.log10(np.abs(h) + 1e-12)


def section_biquad(sec):
    """One analog section as a digital biquad (see the module docstring for the method)."""
    if sec[0] == "hp":
        return bilinear_biquad(sec)
    wc = 2 * np.pi * sec[1]
    qd = (
        sec[2] if sec[0] == "lp" else 10 ** (sec[2] / 40) * sec[3]
    )  # the denominator's Q
    z = np.exp(np.roots([1, wc / qd, wc * wc]) / FS)
    a1, a2 = float(-(z[0] + z[1]).real), float((z[0] * z[1]).real)
    grid = np.concatenate([np.geomspace(20, 16000, 400), np.linspace(16000, 21000, 40)])
    want = 20 * np.log10(np.abs(section_response(grid, sec)))
    start = bilinear_biquad(sec)[:3]

    def err(b):
        return cascade_db([[b[0], b[1], b[2], a1, a2]], grid) - want

    b = least_squares(err, start, x_scale="jac").x
    zeros = np.roots(b)
    if np.any(
        np.abs(zeros) > 1
    ):  # keep it minimum phase: reflect zeros outside the unit circle (same magnitude)
        outside = np.abs(zeros) > 1
        g = b[0] * np.prod(np.abs(zeros[outside]))
        zeros[outside] = 1 / np.conj(zeros[outside])
        b = np.real(g * np.poly(zeros))
    if (
        np.sum(b) < 0
    ):  # a fit in dB can't see the sign: keep the analog section's positive gain at DC
        b = -b
    return np.array([b[0], b[1], b[2], a1, a2])


def cab_filters(sections, band):
    """The cab as float32 biquads, the first carrying the gain for 0 dB average over `band`, returned as float64
    values of the float32 coefficients (what the firmware computes with)."""
    coefs = np.array([section_biquad(s) for s in sections])
    gain = np.sqrt(np.mean(10 ** (cascade_db(coefs, band) / 10)))
    coefs[0, :3] /= gain
    coefs = coefs.astype(np.float32).astype(np.float64)
    for b0, b1, b2, a1, a2 in coefs:  # every pole strictly inside the unit circle
        assert np.all(np.abs(np.roots([1, a1, a2])) < 1)
    return coefs


HEADER = """/*
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

// Generated by make_cab_filters.py - do not edit. The AMP effect's speaker cabs as cascades of second-order sections
// at 44100 Hz, one per analog section of each design. The first section also carries the gain for 0 dB average over
// 80 Hz - 5 kHz.

#pragma once

namespace deluge::dsp::amp {

/// y = b0 x + b1 x[-1] + b2 x[-2] - a1 y[-1] - a2 y[-2]
struct CabSection {
	float b0, b1, b2, a1, a2;
};
"""


def write_header(path, all_coefs):
    most = max(len(c) for c in all_coefs)
    with open(path, "w") as f:
        f.write(HEADER)
        f.write(f"\nconstexpr int kMaxCabSections = {most};\n")
        f.write(f"constexpr int kNumCabs = {len(CABS)};\n\n")
        f.write("struct CabDesign {\n\tint count;\n")
        f.write(
            f"\tint doubleCount; ///< the first doubleCount sections (at or below {DOUBLE_UP_TO_HZ} Hz) run in double "
            "precision\n"
        )
        f.write("\tCabSection sections[kMaxCabSections];\n};\n\n")
        f.write("// clang-format off\n")
        f.write("inline constexpr CabDesign kCabDesigns[kNumCabs] = {\n")
        for (name, sections), coefs in zip(CABS, all_coefs):
            low = 0
            while low < len(sections) and sections[low][1] <= DOUBLE_UP_TO_HZ:
                low += 1
            f.write(f"    // {name}\n    {{{len(coefs)}, {low},\n     {{\n")
            for sec, c in zip(sections, coefs):
                label = " ".join(str(v) for v in sec)
                f.write(
                    "         {"
                    + ", ".join(f"{v: .9e}f" for v in c)
                    + f"}}, // {label}\n"
                )
            f.write("     }},\n")
        f.write("};\n// clang-format on\n\n} // namespace deluge::dsp::amp\n")


def main():
    out = (
        sys.argv[1]
        if len(sys.argv) > 1
        else os.path.join(os.path.dirname(os.path.abspath(__file__)), "cab_filters.h")
    )
    band = np.geomspace(80, 5000, 200)
    fine = np.geomspace(80, 6000, 400)
    hi = np.geomspace(6000, 12000, 200)
    all_coefs = []
    for name, sections in CABS:
        coefs = cab_filters(sections, band)
        all_coefs.append(coefs)
        tgt_gain = np.sqrt(np.mean(target(band, sections) ** 2))
        dev_lo = cascade_db(coefs, fine) - 20 * np.log10(
            target(fine, sections) / tgt_gain
        )
        dev_hi = cascade_db(coefs, hi) - 20 * np.log10(target(hi, sections) / tgt_gain)
        print(
            f"{name}: {len(coefs)} sections, max deviation from the design 80 Hz-6 kHz {np.max(np.abs(dev_lo)):.2f} dB, "
            f"6-12 kHz {np.max(np.abs(dev_hi)):.2f} dB"
        )
    write_header(out, all_coefs)
    print("wrote", out)


if __name__ == "__main__":
    main()
