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

#include "hid/hid_sysex_exerciser.h"
#include "definitions_cxx.hpp"
#include "gui/l10n/l10n.h"
#include "gui/ui/ui.h"
#include "gui/ui_timer_manager.h"
#include "hid/display/display.h"
#include "hid/display/oled.h"
#include "hid/display/screensaver.h"
#include "hid/hid_sysex.h"
#include "io/midi/midi_device.h"
#include "playback/playback_handler.h"
#include "processing/engines/audio_engine.h"
#include "util/pack.h"

namespace HIDSysexExerciser {

enum class Shape : uint8_t {
	BASELINE = 0,
	MESSAGE_RATE = 1,
	BANDWIDTH = 2,
	BURST = 3,
	SOAK = 4,
	RAMP = 5,
	VISUAL = 6,
};

namespace {

constexpr uint32_t kDefaultCapSecs = 600;
constexpr uint32_t kFrameBytes = (OLED_MAIN_HEIGHT_PIXELS >> 3) * OLED_MAIN_WIDTH_PIXELS;
constexpr int32_t kStampOffset = kFrameBytes - 6;
constexpr int32_t kCrcCoverage = kFrameBytes - 6;
// Message-rate shape blinks its dot next to the stamp block so the delta stays minimal
// (one 8-byte-aligned span covering bytes 760..767).
constexpr int32_t kNearStampTickByte = kFrameBytes - 8;
constexpr int32_t kFreeRunPollMs = 3;
constexpr int32_t kIdlePollMs = 500;

struct Counters {
	uint32_t framesRendered;
	uint32_t framesSkippedBudget;
	uint32_t deltaSent;
	uint32_t fullSent;
	uint32_t pushDeferredBuffer;
	uint32_t bytesQueued;
};
Counters counters = {};

struct RunState {
	bool active;
	bool localOnly; // started from the settings menu: no push lease required
	Shape shape;
	uint8_t currentIntervalCs; // effective frame interval, 10 ms units; 0 = free-run
	uint8_t p1;
	uint8_t p2;
	uint32_t frameSeq;
	uint32_t endTime;          // audioSampleTimer at which the hard runtime cap expires
	uint32_t nextRampStepTime; // ramp: audioSampleTimer of the next interval halving
	uint32_t burstWindowEnd;   // burst: audioSampleTimer at which the current window flips
	bool burstOn;
	bool firstFrameDone;
	uint32_t sentAtLastRender; // deltaSent+fullSent snapshot, for free-run pacing
	int32_t marqueeOffset;
};
RunState run = {};

uint32_t sandState;

uint8_t nextSandByte() {
	uint32_t x = sandState;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	sandState = x;
	return x & 0xFF;
}

uint16_t crc16Ccitt(const uint8_t* data, int32_t len) {
	uint16_t crc = 0xFFFF;
	for (int32_t i = 0; i < len; i++) {
		crc ^= static_cast<uint16_t>(data[i]) << 8;
		for (int32_t bit = 0; bit < 8; bit++) {
			crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
		}
	}
	return crc;
}

uint8_t* frameBuffer() {
	return deluge::hid::display::OLED::main.hackGetImageStore()[0];
}

/// Sequence number, CRC and marker into the bottom-right 6x8 px corner block. The CRC covers
/// everything before the stamp, so it includes both tick cells.
void applyStamp(uint8_t* buf) {
	uint32_t seq = run.frameSeq & 0xFFFFFF;
	buf[kStampOffset + 0] = seq & 0xFF;
	buf[kStampOffset + 1] = (seq >> 8) & 0xFF;
	buf[kStampOffset + 2] = (seq >> 16) & 0xFF;
	uint16_t crc = crc16Ccitt(buf, kCrcCoverage);
	buf[kStampOffset + 3] = crc & 0xFF;
	buf[kStampOffset + 4] = crc >> 8;
	buf[kStampOffset + 5] = 0x5A;
}

/// Incompressible xorshift32 field, laid down once per run and never redrawn. Every subsequent
/// delta spans from the top-left tick cell to the stamp, so each frame moves ~900 packed bytes
/// while the physical screen stays still. The caption band is cleared so a unit under test reads
/// as deliberately busy, not faulty.
void layDownField() {
	uint8_t* buf = frameBuffer();
	for (uint32_t i = 0; i < kFrameBytes; i++) {
		buf[i] = nextSandByte();
	}
	auto& canvas = deluge::hid::display::OLED::main;
	canvas.clearAreaExact(26, 17, 102, 29);
	canvas.drawStringCentred("STRESS TEST", 19, kTextSpacingX, kTextSizeYUpdated);
}

/// Gentle eyeball shape: marquee scrolling 2 px/frame plus a 4-phase spinner. No blinking
/// elements, constant average luminance.
void renderVisual() {
	auto& canvas = deluge::hid::display::OLED::main;
	canvas.clear();
	canvas.drawStringCentred("STRESS TEST", 6, kTextSpacingX, kTextSizeYUpdated);
	static const char* kMarqueeText = "DISPLAY STRESS TEST";
	int32_t textWidth = canvas.getStringWidthInPixels(kMarqueeText, kTextSizeYUpdated);
	int32_t cycle = textWidth + OLED_MAIN_WIDTH_PIXELS;
	int32_t pos = run.marqueeOffset % cycle;
	if (pos < OLED_MAIN_WIDTH_PIXELS) {
		canvas.drawString(kMarqueeText, OLED_MAIN_WIDTH_PIXELS - pos, 21, kTextSpacingX, kTextSizeYUpdated);
	}
	else {
		canvas.drawString(kMarqueeText, 0, 21, kTextSpacingX, kTextSizeYUpdated, pos - OLED_MAIN_WIDTH_PIXELS);
	}
	run.marqueeOffset += 2;

	int32_t cx = 8, cy = 40;
	switch (run.frameSeq & 3) {
	case 0:
		canvas.drawVerticalLine(cx, cy - 3, cy + 3);
		break;
	case 1:
		canvas.drawLine(cx - 3, cy + 3, cx + 3, cy - 3);
		break;
	case 2:
		canvas.drawHorizontalLine(cy, cx - 3, cx + 3);
		break;
	default:
		canvas.drawLine(cx - 3, cy - 3, cx + 3, cy + 3);
		break;
	}
}

/// 7-seg degrade: the sequence number (mod 10000) in the digits plus a walking decimal dot.
/// Inherently calm - no full-field capability exists. No CRC.
void render7Seg(uint32_t seq) {
	char text[5];
	uint32_t num = seq % 10000;
	text[0] = '0' + (num / 1000) % 10;
	text[1] = '0' + (num / 100) % 10;
	text[2] = '0' + (num / 10) % 10;
	text[3] = '0' + num % 10;
	text[4] = 0;
	display->setText(text, true, seq % 4);
}

void doRenderFrame() {
	// Snapshot before pushing: the push below can complete synchronously, and free-run pacing
	// needs to see the counters move relative to this.
	run.sentAtLastRender = counters.deltaSent + counters.fullSent;
	if (display->haveOLED()) {
		using deluge::hid::display::OLED;
		// Keep the screensaver from replacing the frames partway through a long soak.
		deluge::hid::display::Screensaver::noteActivity();
		uint8_t* buf = frameBuffer();
		switch (run.shape) {
		case Shape::BASELINE:
			// One frame, then static: only the initial stamp is ever drawn.
			break;
		case Shape::MESSAGE_RATE:
			buf[kNearStampTickByte] = (run.frameSeq & 1) ? 0x01 : 0x02;
			break;
		case Shape::VISUAL:
			renderVisual();
			break;
		default:
			// Bandwidth-type shapes: 1 px dot in framebuffer byte 0, alternating between the two
			// topmost visible rows (the panel hides rows 0..4), so the first and last words of the
			// frame differ every time and the delta spans the whole frozen field.
			buf[0] = (run.frameSeq & 1) ? 0x20 : 0x40;
			break;
		}
		applyStamp(buf);
		// The production push path: panel refresh plus HIDSysex::sendDisplayIfChanged() with its
		// lease check, delta comparison, RLE packing and sendBufferSpace() throttle.
		OLED::markChanged();
		OLED::sendMainImage();
	}
	else {
		render7Seg(run.frameSeq); // SevenSegment::render() ends in sendDisplayIfChanged()
	}
	run.frameSeq++;
	run.firstFrameDone = true;
	counters.framesRendered++;
}

void armTimer(int32_t ms) {
	uiTimerManager.setTimer(TimerName::DISPLAY_EXERCISER, ms);
}

void armForInterval() {
	armTimer(run.currentIntervalCs ? run.currentIntervalCs * 10 : kFreeRunPollMs);
}

void stopRun(bool restoreUi) {
	if (!run.active) {
		return;
	}
	run.active = false;
	uiTimerManager.unsetTimer(TimerName::DISPLAY_EXERCISER);
	if (restoreUi) {
		// The next normal render differs from the synthetic frame nearly everywhere, so the
		// following delta covers the whole screen and subscribers self-heal.
		UI* ui = getCurrentUI();
		if (ui) {
			ui->displayOrLanguageChanged();
			if (!display->haveOLED()) {
				ui->focusRegained();
			}
		}
		doAnyPendingUIRendering();
	}
}

bool playingOrRecording() {
	return playbackHandler.isEitherClockActive() || playbackHandler.recording != RecordingMode::OFF;
}

/// Start (or restart) a run. Returns the protocol ack status.
uint8_t tryStart(uint8_t shapeByte, uint8_t intervalCs, uint8_t p1, uint8_t p2, uint32_t seed, uint32_t capSecs,
                 bool localOnly) {
	if (shapeByte > static_cast<uint8_t>(Shape::VISUAL)) {
		return 1;
	}
	Shape shape = static_cast<Shape>(shapeByte);
	if (shape == Shape::BURST && (p1 == 0 || p2 == 0)) {
		return 1;
	}
	if (shape == Shape::RAMP && p1 == 0) {
		return 1;
	}
	if (playingOrRecording()) {
		return 2;
	}
	if (!localOnly && !HIDSysex::pushLeaseActive()) {
		return 2;
	}

	stopRun(false);
	run = RunState{};
	run.active = true;
	run.localOnly = localOnly;
	run.shape = shape;
	run.p1 = p1;
	run.p2 = p2;
	run.currentIntervalCs = (shape == Shape::RAMP) ? 50 : intervalCs; // ramp always starts at 500 ms
	sandState = seed ? seed : 0x6C078965;

	uint32_t now = AudioEngine::audioSampleTimer;
	if (capSecs == 0) {
		capSecs = kDefaultCapSecs;
	}
	run.endTime = now + capSecs * kSampleRate;
	if (shape == Shape::RAMP) {
		run.nextRampStepTime = now + p1 * kSampleRate;
	}
	if (shape == Shape::BURST) {
		run.burstOn = true;
		run.burstWindowEnd = now + p1 * (kSampleRate / 10);
	}

	if (display->haveOLED()) {
		using deluge::hid::display::OLED;
		if (OLED::isPopupPresent()) {
			OLED::removePopup();
		}
		display->removeWorkingAnimation();
		// Also stops any scrolling/blinking animation, which would otherwise keep redrawing over
		// the synthetic frames from its own timer.
		OLED::clearMainImage();
		if (shape != Shape::VISUAL) {
			layDownField();
		}
	}
	doRenderFrame();
	armForInterval();
	return 0;
}

void sendAck(MIDICable& cable, uint8_t sub, uint8_t status) {
	uint8_t reply[10] = {0xF0, 0x00, 0x21, 0x7B, 0x01, 0x02, 0x43, sub, status, 0xF7};
	cable.sendSysex(reply, sizeof(reply));
}

void sendStats(MIDICable& cable) {
	uint8_t payload[34];
	auto put32 = [&](int32_t offset, uint32_t value) {
		payload[offset] = value & 0xFF;
		payload[offset + 1] = (value >> 8) & 0xFF;
		payload[offset + 2] = (value >> 16) & 0xFF;
		payload[offset + 3] = (value >> 24) & 0xFF;
	};
	auto put16 = [&](int32_t offset, uint32_t value) {
		if (value > 0xFFFF) {
			value = 0xFFFF;
		}
		payload[offset] = value & 0xFF;
		payload[offset + 1] = (value >> 8) & 0xFF;
	};
	put32(0, counters.framesRendered);
	put32(4, counters.framesSkippedBudget);
	put32(8, counters.deltaSent);
	put32(12, counters.fullSent);
	put32(16, counters.pushDeferredBuffer);
	put32(20, counters.bytesQueued);
	put32(24, AudioEngine::audioSampleTimer);
	MIDICable* pushCable = HIDSysex::pushCable();
	put16(28, pushCable ? static_cast<uint32_t>(pushCable->sendBufferSpace()) : 0);
	int32_t leaseMs = HIDSysex::pushLeaseRemainingMs();
	put16(30, leaseMs > 0 ? static_cast<uint32_t>(leaseMs) : 0);
	payload[32] = run.active ? static_cast<uint8_t>(run.shape) : 0x7F;
	payload[33] = run.active ? run.currentIntervalCs : 0;

	uint8_t reply[64] = {0xF0, 0x00, 0x21, 0x7B, 0x01, 0x02, 0x43, 0x02, 0x01 /* layout version */};
	int32_t packed = pack_8bit_to_7bit(reply + 9, sizeof(reply) - 10, payload, sizeof(payload));
	if (packed < 0) {
		return;
	}
	reply[9 + packed] = 0xF7;
	cable.sendSysex(reply, 9 + packed + 1);
}

} // namespace

void timerRoutine() {
	if (!run.active) {
		return;
	}
	uint32_t now = AudioEngine::audioSampleTimer;

	if (static_cast<int32_t>(now - run.endTime) >= 0) { // hard runtime cap
		stopRun(true);
		return;
	}
	if (!run.localOnly && !HIDSysex::pushLeaseActive()) { // client vanished, lease lapsed
		stopRun(true);
		return;
	}
	if (playingOrRecording()) { // playback started mid-run, e.g. by an external MIDI start
		stopRun(true);
		return;
	}

	// Baseline sent its single frame at start; just keep polling the lease and the cap.
	if (run.shape == Shape::BASELINE && run.firstFrameDone) {
		armTimer(kIdlePollMs);
		return;
	}

	if (run.shape == Shape::BURST) {
		if (static_cast<int32_t>(now - run.burstWindowEnd) >= 0) {
			run.burstOn = !run.burstOn;
			run.burstWindowEnd = now + (run.burstOn ? run.p1 : run.p2) * (kSampleRate / 10);
		}
		if (!run.burstOn) {
			// Silent window: no offered load, but keep ticking to catch lease lapse and the cap.
			int32_t remainingMs = static_cast<int32_t>(run.burstWindowEnd - now) / (kSampleRate / 1000) + 1;
			armTimer(remainingMs < kIdlePollMs ? remainingMs : kIdlePollMs);
			return;
		}
	}

	if (run.shape == Shape::RAMP && run.currentIntervalCs > 0
	    && static_cast<int32_t>(now - run.nextRampStepTime) >= 0) {
		run.currentIntervalCs >>= 1; // 500 ms, halving each step, until free-run
		run.nextRampStepTime = now + run.p1 * kSampleRate;
	}

	// Free-run offers the next frame as soon as the previous push was accepted; until then,
	// rendering again would only coalesce frames the link never saw as distinct.
	if (run.currentIntervalCs == 0 && counters.deltaSent + counters.fullSent == run.sentAtLastRender) {
		armTimer(kFreeRunPollMs);
		return;
	}

	// A frame is due; yield to the audio engine rather than contending with its deadlines.
	if (AudioEngine::cpuDireness > 12) {
		counters.framesSkippedBudget++;
		armForInterval();
		return;
	}

	doRenderFrame();
	armForInterval();
}

void commandReceived(MIDICable& cable, uint8_t* data, int32_t len) {
	// data[0] = 0x02 (HID), data[1] = 0x03 (exerciser), data[2] = sub-command.
	if (len < 3) {
		return;
	}
	switch (data[2]) {
	case 0: // stop
		stopRun(true);
		sendAck(cable, 0, 0);
		break;

	case 1: // start: shape, interval, p1, p2, seed[4], capSecs[2]
	{
		if (len < 14) {
			sendAck(cable, 1, 1);
			break;
		}
		uint32_t seed = data[7] | (data[8] << 7) | (data[9] << 14) | (static_cast<uint32_t>(data[10]) << 21);
		uint32_t capSecs = data[11] | (data[12] << 7);
		sendAck(cable, 1, tryStart(data[3], data[4], data[5], data[6], seed, capSecs, false));
		break;
	}

	case 2: // query stats
		sendStats(cable);
		break;

	case 3: // reset stats
		counters = Counters{};
		break;

	default:
		break;
	}
}

void startLocalDemo() {
	if (playingOrRecording()) {
		display->displayPopup(deluge::l10n::get(deluge::l10n::String::STRING_FOR_CANT_START_DISPLAY_TEST));
		return;
	}
	tryStart(static_cast<uint8_t>(Shape::VISUAL), 10, 0, 0, 0, 0, true);
}

void notifyLocalInput() {
	if (run.active) {
		stopRun(true);
	}
}

bool isRunning() {
	return run.active;
}

void onDeltaSent(int32_t bytes) {
	counters.deltaSent++;
	counters.bytesQueued += bytes;
}

void onFullSent(int32_t bytes) {
	counters.fullSent++;
	counters.bytesQueued += bytes;
}

void onPushDeferred() {
	counters.pushDeferredBuffer++;
}

} // namespace HIDSysexExerciser
