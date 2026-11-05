#include "pch.h"
#include "Calendar.h"
#include <cmath>
#include <cstdio>

namespace {
	void AppendUtf8(std::string& out, char32_t ch) {
		if (ch < 0x80)
			out += (char)ch;
		else if (ch < 0x800) {
			out += (char)(0xC0 | (ch >> 6));
			out += (char)(0x80 | (ch & 0x3F));
		}
		else if (ch < 0x10000) {
			out += (char)(0xE0 | (ch >> 12));
			out += (char)(0x80 | ((ch >> 6) & 0x3F));
			out += (char)(0x80 | (ch & 0x3F));
		}
		else {
			out += (char)(0xF0 | (ch >> 18));
			out += (char)(0x80 | ((ch >> 12) & 0x3F));
			out += (char)(0x80 | ((ch >> 6) & 0x3F));
			out += (char)(0x80 | (ch & 0x3F));
		}
	}

	// FNV-1a: the same text gives the same id on every run and computer
	unsigned long long Hash(std::string const& text) {
		unsigned long long hash = 14695981039346656037ull;
		for (unsigned char ch : text) {
			hash ^= ch;
			hash *= 1099511628211ull;
		}
		return hash;
	}
}

std::string ICalendar::FormatTime(DateTime const& ut) {
	// to the whole second (rounded), in the Gregorian calendar, which is what iCalendar knows
	double seconds = std::round(ut.Julian() * 86400.0);
	DateTime t(seconds / 86400.0, true);
	char text[32];
	int second = (int)std::floor(t.Second() + 0.5);
	sprintf_s(text, "%04ld%02ld%02ldT%02ld%02ld%02dZ", t.Year(), t.Month(), t.Day(), t.Hour(), t.Minute(), second > 59 ? 59 : second);
	return text;
}

std::string ICalendar::EscapeText(std::wstring const& text) {
	std::string out;
	for (size_t i = 0; i < text.size(); i++) {
		char32_t ch = text[i];
		if (ch >= 0xD800 && ch < 0xDC00 && i + 1 < text.size() && text[i + 1] >= 0xDC00 && text[i + 1] < 0xE000)
			ch = 0x10000 + ((ch - 0xD800) << 10) + (text[++i] - 0xDC00);
		switch (ch) {
			case L'\\': out += "\\\\"; break;
			case L';': out += "\\;"; break;
			case L',': out += "\\,"; break;
			case L'\r':
				if (i + 1 < text.size() && text[i + 1] == L'\n')
					i++;
				out += "\\n";
				break;
			case L'\n': out += "\\n"; break;
			default: AppendUtf8(out, ch); break;
		}
	}
	return out;
}

std::string ICalendar::Fold(std::string const& line) {
	std::string out;
	size_t start = 0, limit = 75;
	while (line.size() - start > limit) {
		size_t cut = start + limit;
		while (cut > start && ((unsigned char)line[cut] & 0xC0) == 0x80)
			cut--;		// (not inside a UTF-8 character)
		out.append(line, start, cut - start);
		out += "\r\n ";
		start = cut;
		limit = 74;		// (the space counts)
	}
	out.append(line, start, std::string::npos);
	return out;
}

std::string ICalendar::ToText(std::wstring const& name, std::vector<CalendarEvent> const& events, DateTime const& stamp) {
	std::string out;
	auto line = [&](std::string const& text) {
		out += Fold(text);
		out += "\r\n";
	};
	line("BEGIN:VCALENDAR");
	line("VERSION:2.0");
	line("PRODID:-//Astro Studio//Astro Studio//EN");
	line("CALSCALE:GREGORIAN");
	line("METHOD:PUBLISH");
	if (!name.empty())
		line("X-WR-CALNAME:" + EscapeText(name));
	auto stampText = FormatTime(stamp);
	for (auto const& event : events) {
		auto start = FormatTime(event.Start);
		auto summary = EscapeText(event.Summary);
		std::string uid;
		if (event.Uid.empty()) {
			char text[40];
			sprintf_s(text, "%016llx", Hash(start + "|" + summary));
			uid = std::string(text) + "@astrostudio";
		}
		else
			uid = EscapeText(event.Uid);
		line("BEGIN:VEVENT");
		line("UID:" + uid);
		line("DTSTAMP:" + stampText);
		line("DTSTART:" + start);
		if (event.End && event.End->Julian() > event.Start.Julian())
			line("DTEND:" + FormatTime(*event.End));
		line("SUMMARY:" + summary);
		if (!event.Description.empty())
			line("DESCRIPTION:" + EscapeText(event.Description));
		line("TRANSP:TRANSPARENT");		// (it doesn't make anyone busy)
		line("END:VEVENT");
	}
	line("END:VCALENDAR");
	return out;
}
