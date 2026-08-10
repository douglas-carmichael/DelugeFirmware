#pragma once
#include "gui/l10n/l10n.h"
#include "util/functions.h"

namespace deluge::gui::menu_item {

// Mixin for a formatted title
class FormattedTitle {
public:
	FormattedTitle(l10n::String format_str, std::optional<int32_t> arg = std::nullopt)
	    : format_str_(format_str), arg_{arg} {}

	/// Update the substitution argument for dynamic titles (e.g. the selected CV channel, DX
	/// operator or gate number). The formatted string is (re)built lazily in title(), so this
	/// only records the value; callers keep rendering via getTitle()/getName() afterwards.
	void format(int32_t arg) const { arg_ = arg; }

	[[nodiscard]] std::string_view title() const {
		// Re-resolve on every call so the title follows a live language switch. Previously the
		// formatted string was cached on first use and never refreshed, so a title first shown in
		// one language stayed in that language after switching - and, because the OSC-mixer column
		// labels are derived from this string, a column would keep reading e.g. a stale "Nive".
		title_ = l10n::get(format_str_);
		if (arg_.has_value()) {
			asterixToInt(title_.data(), arg_.value());
		}
		return title_;
	}

private:
	l10n::String format_str_;
	mutable std::string title_;
	mutable std::optional<int32_t> arg_;
};
} // namespace deluge::gui::menu_item
