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

struct MIDIMessage;

namespace deluge::hid::display::oled_canvas {
class Canvas;
}

/// MIDI TX/RX and CV/gate-output activity glyphs in the top-right corner of the OLED,
/// gated behind the "Show MIDI/CV Activity" community feature. No 7-segment equivalent
/// exists (no spare segments), so everything here no-ops when the OLED isn't the active
/// display. CV/gate has no receive direction — the hardware only has outputs.
namespace deluge::gui::midi_activity {

/// Call for every non-SysEx MIDI message given to the output funnel (TX).
void notifySent(MIDIMessage message);

/// Call for every channel/system message arriving from DIN or USB (RX).
void notifyReceived(uint8_t statusType, uint8_t channel);

/// Call when the CV engine changes a gate or CV output. Trigger-clock ticks are excluded
/// at the call sites for the same always-on reason as MIDI clock.
void notifyCVOut();

/// Overlay the active glyphs into blank space near the top-right of the main image
/// (yielding entirely if none is free). Called after the UI stack has rendered.
void drawIndicator(deluge::hid::display::oled_canvas::Canvas& canvas);

/// TimerName::MIDI_ACTIVITY handler: turns arrows off once traffic stops.
void timerEvent();

} // namespace deluge::gui::midi_activity
