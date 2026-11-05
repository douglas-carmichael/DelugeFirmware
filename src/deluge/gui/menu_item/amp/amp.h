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

// The AMP menu (the amp and cab simulation): a horizontal menu in the FX menu of synths, drums, kits and audio tracks

#pragma once

#include "dsp/amp/amp_effect.h"
#include "gui/l10n/l10n.h"
#include "gui/menu_item/horizontal_menu.h"
#include "gui/menu_item/selection.h"
#include "gui/menu_item/toggle.h"
#include "gui/menu_item/unpatched_param.h"
#include "gui/ui/sound_editor.h"
#include "hid/display/display.h"
#include "model/instrument/kit.h"
#include "model/mod_controllable/mod_controllable_audio.h"
#include "model/settings/runtime_feature_settings.h"
#include "model/song/song.h"
#include "processing/sound/sound_drum.h"

namespace deluge::gui::menu_item::amp {

/// The AMP's menus show while the community feature is on, and never for the song: its master effects share the
/// kit's FX menu (in the song view and the performance view), and the song has no AMP
inline bool isRelevantFor(ModControllableAudio* modControllable) {
	return runtimeFeatureSettings.isOn(RuntimeFeatureSettingType::AmpSim) && modControllable != nullptr
	       && !modControllable->isSong();
}

/// Calls f on the AMP being edited, or on every drum's with AFFECT ENTIRE held on a kit row. False if any call
/// failed (an AMP that needed memory there wasn't enough of).
template <typename F>
bool forEachEditedAmp(F f) {
	if (currentUIMode == UI_MODE_HOLDING_AFFECT_ENTIRE_IN_SOUND_EDITOR && soundEditor.editingKitRow()) {
		bool ok = true;
		for (Drum* drum = getCurrentKit()->firstDrum; drum != nullptr; drum = drum->next) {
			if (drum->type == DrumType::SOUND) {
				ok = f(static_cast<SoundDrum*>(drum)->amp) && ok;
			}
		}
		return ok;
	}
	return f(soundEditor.currentModControllable->amp);
}

inline void showNoMemory() {
	display->displayPopup(l10n::get(l10n::String::STRING_FOR_ERROR_INSUFFICIENT_RAM));
}

/// The AMP submenu, a horizontal menu: AMP, GAIN, LEVEL, CAB / BASS, MID, TREBLE, PRESENCE / STACK, QUALITY, CHANNELS
class AmpMenu final : public HorizontalMenu {
public:
	using HorizontalMenu::HorizontalMenu;
	bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) const override {
		return isRelevantFor(modControllable);
	}
};

/// AMP: on or off. On takes the AMP's memory; if there isn't enough, it says so and stays off.
class Enabled final : public Toggle {
public:
	using Toggle::Toggle;
	void readCurrentValue() override { setValue(soundEditor.currentModControllable->amp.isEnabled()); }
	bool usesAffectEntire() override { return true; }
	void writeCurrentValue() override {
		const bool on = getValue();
		if (!forEachEditedAmp([on](dsp::amp::AmpEffect& a) { return a.setEnabled(on); })) {
			showNoMemory();
			readCurrentValue();
		}
	}
	bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) const override {
		return isRelevantFor(modControllable);
	}
};

/// GAIN, BASS, MID, TREBLE, PRESENCE and LEVEL: params, so each clip has its own values, with automation and MIDI
/// learn, like the other effects' knobs. Sounds (synths, drums) and GlobalEffectables (kits, audio tracks) number
/// them differently, so the param comes from what's being edited.
class Knob final : public UnpatchedParam {
public:
	Knob(l10n::String name, l10n::String title, l10n::String columnLabel, modulation::params::AmpKnob knob)
	    : UnpatchedParam(name, title, modulation::params::ampParam(modulation::params::Kind::UNPATCHED_SOUND, knob)),
	      knob_{knob}, columnLabel_{columnLabel} {}
	[[nodiscard]] uint8_t getP() const override {
		return modulation::params::ampParam(soundEditor.currentModControllable->getUnpatchedParamKind(), knob_);
	}
	void getColumnLabel(StringBuf& label) override { label.append(l10n::getView(columnLabel_)); }
	bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) const override {
		return isRelevantFor(modControllable);
	}

private:
	modulation::params::AmpKnob knob_;
	l10n::String columnLabel_;
};

/// A menu setting: CAB, STACK, QUALITY or CHANNELS. A change fades the AMP out and back in.
class Setting : public Selection {
public:
	Setting(l10n::String name, l10n::String columnLabel) : Selection(name), columnLabel_{columnLabel} {}
	void readCurrentValue() override { setValue(get(soundEditor.currentModControllable->amp.settings())); }
	bool usesAffectEntire() override { return true; }
	void writeCurrentValue() override {
		const int32_t value = getValue();
		if (!forEachEditedAmp([&](dsp::amp::AmpEffect& a) {
			    dsp::amp::Settings s = a.settings();
			    set(s, value);
			    return a.setSettings(s);
		    })) {
			showNoMemory();
			readCurrentValue();
		}
	}
	void getColumnLabel(StringBuf& label) override { label.append(l10n::getView(columnLabel_)); }
	bool isRelevant(ModControllableAudio* modControllable, int32_t whichThing) const override {
		return isRelevantFor(modControllable);
	}

protected:
	[[nodiscard]] virtual int32_t get(const dsp::amp::Settings& s) const = 0;
	virtual void set(dsp::amp::Settings& s, int32_t value) const = 0;

private:
	l10n::String columnLabel_;
};

/// CAB: the speaker cab, or none
class Cab final : public Setting {
public:
	using Setting::Setting;
	deluge::vector<std::string_view> getOptions(OptType optType) override {
		using enum l10n::String;
		const bool shortNames = (optType == OptType::SHORT);
		return {
		    l10n::getView(STRING_FOR_OFF),
		    l10n::getView(shortNames ? STRING_FOR_CAB_1X12_OPEN_SHORT : STRING_FOR_CAB_1X12_OPEN),
		    l10n::getView(STRING_FOR_CAB_2X12),
		    l10n::getView(STRING_FOR_CAB_4X12),
		    l10n::getView(shortNames ? STRING_FOR_CAB_4X12_SMOOTH_SHORT : STRING_FOR_CAB_4X12_SMOOTH),
		};
	}

protected:
	[[nodiscard]] int32_t get(const dsp::amp::Settings& s) const override { return s.cab; }
	void set(dsp::amp::Settings& s, int32_t value) const override { s.cab = value; }
};

/// STACK: the tone stack, which sets how BASS, MID and TREBLE work
class ToneStack final : public Setting {
public:
	using Setting::Setting;
	deluge::vector<std::string_view> getOptions(OptType optType) override {
		using enum l10n::String;
		const bool shortNames = (optType == OptType::SHORT);
		return {
		    l10n::getView(shortNames ? STRING_FOR_AMERICAN_SHORT : STRING_FOR_AMERICAN),
		    l10n::getView(shortNames ? STRING_FOR_BRITISH_SHORT : STRING_FOR_BRITISH),
		};
	}

protected:
	[[nodiscard]] int32_t get(const dsp::amp::Settings& s) const override {
		return s.toneStack == dsp::amp::ToneStackType::AMERICAN ? 0 : 1;
	}
	void set(dsp::amp::Settings& s, int32_t value) const override {
		s.toneStack = (value == 0) ? dsp::amp::ToneStackType::AMERICAN : dsp::amp::ToneStackType::BRITISH;
	}
};

/// QUALITY: 1X, or 2X oversampling (smoother at high gain, and more CPU)
class Quality final : public Setting {
public:
	using Setting::Setting;
	deluge::vector<std::string_view> getOptions(OptType optType) override {
		using enum l10n::String;
		return {l10n::getView(STRING_FOR_1X), l10n::getView(STRING_FOR_2X)};
	}

protected:
	[[nodiscard]] int32_t get(const dsp::amp::Settings& s) const override { return s.oversample == 1 ? 0 : 1; }
	void set(dsp::amp::Settings& s, int32_t value) const override { s.oversample = (value == 0) ? 1 : 2; }
};

/// CHANNELS: MONO mixes the track to mono, like a real amp; STEREO runs an AMP per side (twice the CPU)
class Channels final : public Setting {
public:
	using Setting::Setting;
	deluge::vector<std::string_view> getOptions(OptType optType) override {
		using enum l10n::String;
		const bool shortNames = (optType == OptType::SHORT);
		return {
		    l10n::getView(STRING_FOR_MONO),
		    l10n::getView(shortNames ? STRING_FOR_STEREO_SHORT : STRING_FOR_STEREO),
		};
	}

protected:
	[[nodiscard]] int32_t get(const dsp::amp::Settings& s) const override {
		return s.channels == dsp::amp::Channels::STEREO ? 1 : 0;
	}
	void set(dsp::amp::Settings& s, int32_t value) const override {
		s.channels = (value == 1) ? dsp::amp::Channels::STEREO : dsp::amp::Channels::MONO;
	}
};

} // namespace deluge::gui::menu_item::amp
