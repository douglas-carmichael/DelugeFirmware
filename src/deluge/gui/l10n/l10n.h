#pragma once
#include "gui/l10n/language.h"
#include "util/misc.h"
#include <array>
#include <cstddef>
#include <initializer_list>
#include <optional>

namespace deluge::l10n {
std::string_view getView(const deluge::l10n::Language& language, deluge::l10n::String string);
std::string_view getView(deluge::l10n::String string);
const char* get(const deluge::l10n::Language& language, deluge::l10n::String string);
const char* get(deluge::l10n::String string);

/// The language chosen in the settings menu (kSelectableLanguages[FlashStorage::preferredLanguage]),
/// or english before the flash settings have been read.
Language const* getPreferredOledLanguage();

/// Point chosenLanguage at the preferred language. Only applies on an OLED display — the 7SEG
/// display always keeps its own abbreviated built-in language.
void applyPreferredOledLanguage();
} // namespace deluge::l10n
