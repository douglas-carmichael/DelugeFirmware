/*
 * Copyright © 2026 Synthstrom Audible Limited
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

#include <cstdint>

class MIDICable;

// Display MIDI link stress exerciser.
//
// Renders synthetic OLED frames through the production display-push path (lease check, delta
// comparison, RLE packing, sendBufferSpace() throttle) at a commanded rate, so the whole MIDI
// link can be load-tested and soaked without anyone playing the instrument. Offered load is the
// render schedule; delivered load is what survives the throttle and the cable.
//
// Controlled via display-SysEx command 0x03 (alongside 0x00 OLED / 0x01 7-seg / 0x02 read block):
// F0 00 21 7B 01 02 03 <sub> ... F7. Legacy F0 7D framing is accepted by the dispatcher and
// replies are converted back in-kind by the USB cable layer.
//
//   sub 00: stop
//   sub 01: start; payload bytes = shape, interval, p1, p2, seed[4], capSecs[2]
//           shape    0 baseline / 1 message rate / 2 bandwidth / 3 burst / 4 soak / 5 ramp / 6 visual
//           interval frame interval in 10 ms units (1..127 -> 10 ms..1.27 s); 0 = free-run
//                    (next frame as soon as the previous push was accepted; shape 5 ignores this)
//           p1, p2   shape 3: on/off window lengths in 100 ms units (both nonzero);
//                    shape 5: p1 = seconds per ramp step (nonzero)
//           seed     28-bit xorshift32 seed, 4x7 bits LSB-first (0 -> built-in default)
//           capSecs  hard runtime cap in seconds, 14 bits LSB-first; 0 -> default 600
//   sub 02: query stats -> F0 .. 43 02 <layoutVersion> <pack_8bit_to_7bit(payload)> F7
//   sub 03: reset stats counters (no reply)
//
// Subs 00/01 are acked with F0 00 21 7B 01 02 43 <sub> <status> F7, status: 0 ok / 1 bad param /
// 2 refused (sequencer playing or recording, or no live display-push lease for a remote start) /
// 3 no OLED (reserved: 7-seg-only units degrade to a walking-dot rendering instead of refusing).
//
// Stats payload layout version 1, all little-endian:
//   0..23  u32 framesRendered, framesSkippedBudget, deltaSent, fullSent, pushDeferredBuffer,
//          bytesQueued (wrapping; deltaSent/fullSent/pushDeferredBuffer/bytesQueued count all
//          display SysEx, so reset them via sub 03 before a run)
//   24..27 u32 audioSampleTimer
//   28..29 u16 sendBufferSpace of the push cable (0 if none)
//   30..31 u16 push lease remaining, ms
//   32     u8  active shape (0x7F when idle)
//   33     u8  current interval in 10 ms units (ramp: current step; 0 = free-run)
//
// Every OLED frame is stamped in framebuffer bytes 762..767 (bottom-right 6x8 px):
//   762..764 frame sequence number, 24-bit little-endian
//   765..766 CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) over bytes 0..761, little-endian
//   767      0x5A marker
// Sand content is xorshift32 (x ^= x<<13; x ^= x>>17; x ^= x<<5) seeded from the start command,
// one state advance per framebuffer byte 0..767, taking the low 8 bits of each state.
namespace HIDSysexExerciser {

/// Display-SysEx command family 0x03; data/len exactly as passed to HIDSysex::sysexReceived
/// (data[1] == 3, sub-command in data[2], parameters from data[3]).
void commandReceived(MIDICable& cable, uint8_t* data, int32_t len);

/// SETTINGS > MIDI > DISPLAY TEST: run the visual shape at 10 Hz for an eyeball
/// hardware-vs-mirror check. No push lease required; any input exits.
void startLocalDemo();

/// Handler for TimerName::DISPLAY_EXERCISER.
void timerRoutine();

/// Called from the button/pad/encoder input funnels: any local input aborts an active run
/// instantly and restores the normal UI.
void notifyLocalInput();

bool isRunning();

// Counter taps from the display-push send path in hid_sysex.cpp.
void onDeltaSent(int32_t bytes);
void onFullSent(int32_t bytes);
void onPushDeferred();

} // namespace HIDSysexExerciser
