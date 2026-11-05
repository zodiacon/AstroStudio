#pragma once

#include "DateTime.h"
#include <optional>
#include <string>
#include <vector>

// An appointment for a calendar program: something that happens at a moment, or lasts from one moment to another.
struct CalendarEvent {
	DateTime Start;					// UT
	std::optional<DateTime> End;	// UT; none: it happens at Start and takes no time
	std::wstring Summary;			// the title the calendar shows
	std::wstring Description;		// more, if there is any (line breaks are kept)
	std::wstring Uid;				// empty: made from the summary and the start, so that importing the same event again updates it
};

// iCalendar (RFC 5545) text: what Outlook, Google Calendar and Apple Calendar import from an .ics file.
struct ICalendar abstract final {
	// The whole file - a VCALENDAR named name, with a VEVENT for each event - as UTF-8 with CRLF line ends and lines folded at
	// 75 bytes. Times are written in UTC (19980119T070000Z), to the second; stamp is the DTSTAMP of every event (when it was made).
	static std::string ToText(std::wstring const& name, std::vector<CalendarEvent> const& events, DateTime const& stamp);
	// a moment in UTC as iCalendar writes it
	static std::string FormatTime(DateTime const& ut);
	// text as a property value: UTF-8, with backslash, semicolon, comma and line breaks escaped
	static std::string EscapeText(std::wstring const& text);
	// a long line cut into lines of at most 75 bytes, each one after the first starting with a space (never inside a character)
	static std::string Fold(std::string const& line);
};
