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

#include "gui/midi_activity_indicator.h"
#include "definitions_cxx.hpp"
#include "gui/ui/ui.h"
#include "gui/ui_timer_manager.h"
#include "gui/views/arranger_view.h"
#include "gui/views/session_view.h"
#include "hid/display/display.h"
#include "hid/display/oled.h"
#include "hid/display/oled_canvas/canvas.h"
#include "model/midi/message.h"
#include "model/settings/runtime_feature_settings.h"
#include <algorithm>

namespace deluge::gui::midi_activity {

namespace {

/// How long a glyph stays lit after the last event in its lane. Events arriving while a
/// glyph is already lit extend the hold (via the retrigger flags) instead of causing
/// another redraw, so a dense stream costs two redraws total: on and off.
constexpr int32_t kHoldTimeMs = 150;

// 5x6-pixel glyphs, one column byte each, bit 0 = top row.
constexpr uint8_t kGatePulse[] = {0x20, 0x3F, 0x01, 0x3F, 0x20}; // CV/gate out: pulse shape
constexpr uint8_t kDownArrow[] = {0x08, 0x18, 0x3F, 0x18, 0x08}; // MIDI RX: into the Deluge
constexpr uint8_t kUpArrow[] = {0x04, 0x06, 0x3F, 0x06, 0x04};   // MIDI TX: out of the Deluge

constexpr int32_t kIconWidth = 5;
constexpr int32_t kIconHeight = 6;
constexpr int32_t kCellWidth = kIconWidth + 2; // 1px cleared padding each side

// Room for the tempo readout: six characters plus the metronome icon (see
// SessionView::displayTempoBPM, whose partial redraw clears exactly that rectangle).
constexpr int32_t kTempoReserveWidth = (kTextSpacingX * 6) + 7 + 3;

// The framebuffer's top OLED_MAIN_TOPMOST_PIXEL rows aren't visible on the panel; the
// extra 2px puts the glyph tops level with the tempo digits (drawn at TOPMOST + 3).
constexpr int32_t kMinY = OLED_MAIN_TOPMOST_PIXEL + 2;
constexpr int32_t kMaxY = kMinY + kIconHeight + 1;

/// One lane per traffic type, drawn right-aligned in array order.
struct Lane {
	const uint8_t* icon;
	bool visible = false;
	bool retrigger = false;
};

enum LaneIndex : int32_t { kLaneCV = 0, kLaneRx, kLaneTx, kNumLanes };

Lane lanes[kNumLanes] = {
    {kGatePulse}, // CV/gate out
    {kDownArrow}, // MIDI RX
    {kUpArrow},   // MIDI TX (rightmost)
};

int32_t preferredRightEdge() {
	// Song and arranger view render the tempo into the top-right of the main image (same
	// condition as PlaybackHandler::displayTempoBPM), so prefer the slot to its left
	// there; everywhere else prefer the corner.
	UI* currentUI = getCurrentUI();
	if (currentUI == &sessionView || currentUI == &arrangerView) {
		return OLED_MAIN_WIDTH_PIXELS - kTempoReserveWidth;
	}
	return OLED_MAIN_WIDTH_PIXELS;
}

/// True if nothing is drawn in the lane strip ending at rightEdge, checked with a 1px
/// margin on the left so the glyphs never sit flush against text.
bool stripBlank(hid::display::oled_canvas::Canvas& canvas, int32_t rightEdge) {
	int32_t minX = std::max<int32_t>(rightEdge - (kNumLanes * kCellWidth) - 1, 0);
	int32_t maxX = std::min<int32_t>(rightEdge - 1, OLED_MAIN_WIDTH_PIXELS - 1);
	auto& image = canvas.hackGetImageStore();

	for (int32_t page = kMinY >> 3; page <= kMaxY >> 3; page++) {
		int32_t pageTop = page << 3;
		uint8_t mask = static_cast<uint8_t>(0xFF << std::max<int32_t>(kMinY - pageTop, 0))
		               & static_cast<uint8_t>(0xFF >> (7 - std::min<int32_t>(kMaxY - pageTop, 7)));
		for (int32_t x = minX; x <= maxX; x++) {
			if ((image[page][x] & mask) != 0) {
				return false;
			}
		}
	}
	return true;
}

bool enabled() {
	return display->haveOLED() && runtimeFeatureSettings.isOn(RuntimeFeatureSettingType::ShowMidiActivity);
}

void notify(Lane& lane) {
	if (!enabled()) {
		return;
	}
	lane.retrigger = true;
	if (!lane.visible) {
		lane.visible = true;
		uiTimerManager.setTimer(TimerName::MIDI_ACTIVITY, kHoldTimeMs);
		renderUIsForOled();
	}
}

} // namespace

void notifySent(MIDIMessage message) {
	// Realtime clock would keep the arrow lit for as long as playback runs, hiding the
	// traffic the indicator is meant to surface.
	if (message.statusType == 0x0F && message.channel == 0x08) {
		return;
	}
	notify(lanes[kLaneTx]);
}

void notifyReceived(uint8_t statusType, uint8_t channel) {
	// Skip realtime clock and active sensing, which arrive continuously.
	if (statusType == 0x0F && (channel == 0x08 || channel == 0x0E)) {
		return;
	}
	notify(lanes[kLaneRx]);
}

void notifyCVOut() {
	notify(lanes[kLaneCV]);
}

void drawIndicator(hid::display::oled_canvas::Canvas& canvas) {
	bool anyVisible = false;
	for (Lane& lane : lanes) {
		anyVisible = anyVisible || lane.visible;
	}
	if (!anyVisible) {
		return;
	}

	// Popups are composited over the main image afterwards, so the blankness test below can't see them and the
	// glyphs end up crowding whatever box is up. A popup is transient, and so is the indicator, so yield to it.
	if (deluge::hid::display::OLED::isPopupPresent()) {
		return;
	}

	// Screens use their top rows very differently (titles, the tempo readout, parameter
	// graphics), so rather than knowing about each one, only ever draw into space that
	// is actually blank: the preferred slot first, then the other, else yield entirely
	// this frame rather than damage what the screen drew.
	constexpr int32_t kCorner = OLED_MAIN_WIDTH_PIXELS;
	constexpr int32_t kBesideTempo = OLED_MAIN_WIDTH_PIXELS - kTempoReserveWidth;
	int32_t preferred = preferredRightEdge();
	int32_t fallback = (preferred == kCorner) ? kBesideTempo : kCorner;

	int32_t rightEdge;
	if (stripBlank(canvas, preferred)) {
		rightEdge = preferred;
	}
	else if (stripBlank(canvas, fallback)) {
		rightEdge = fallback;
	}
	else {
		return;
	}

	for (int32_t i = 0; i < kNumLanes; i++) {
		if (lanes[i].visible) {
			int32_t minX = rightEdge - (kNumLanes - i) * kCellWidth;
			canvas.drawGraphicMultiLine(lanes[i].icon, minX + 1, kMinY + 1, kIconWidth, kIconHeight);
		}
	}
}

void timerEvent() {
	bool turnedOff = false;
	bool anyVisible = false;

	for (Lane& lane : lanes) {
		if (lane.retrigger) {
			lane.retrigger = false;
		}
		else if (lane.visible) {
			lane.visible = false;
			turnedOff = true;
		}
		anyVisible = anyVisible || lane.visible;
	}

	if (anyVisible) {
		uiTimerManager.setTimer(TimerName::MIDI_ACTIVITY, kHoldTimeMs);
	}
	if (turnedOff) {
		renderUIsForOled();
	}
}

} // namespace deluge::gui::midi_activity
