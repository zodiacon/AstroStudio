#include "TestCommon.h"
#include "Calendar.h"

namespace {
	size_t Count(std::string const& text, std::string const& what) {
		size_t count = 0;
		for (auto at = text.find(what); at != std::string::npos; at = text.find(what, at + what.size()))
			count++;
		return count;
	}
}

TEST_CASE("Times are written in UTC to the second", "[Calendar]") {
	CHECK(ICalendar::FormatTime(DateTime(2026, 3, 20, 14, 46, 0)) == "20260320T144600Z");
	CHECK(ICalendar::FormatTime(DateTime(1999, 12, 31, 23, 59, 59.4)) == "19991231T235959Z");
	// half a second up rounds into the next minute, hour and day
	CHECK(ICalendar::FormatTime(DateTime(1999, 12, 31, 23, 59, 59.6)) == "20000101T000000Z");
	CHECK(ICalendar::FormatTime(DateTime(2026, 1, 2, 3, 4, 5)) == "20260102T030405Z");
}

TEST_CASE("Text is escaped as iCalendar wants it", "[Calendar]") {
	CHECK(ICalendar::EscapeText(L"Moon void of course; in Leo, after a square") == "Moon void of course\\; in Leo\\, after a square");
	CHECK(ICalendar::EscapeText(L"a\\b") == "a\\\\b");
	CHECK(ICalendar::EscapeText(L"one\r\ntwo\nthree") == "one\\ntwo\\nthree");
	// UTF-8, surrogate pairs included
	CHECK(ICalendar::EscapeText(L"10\u00b0 \u264c") == "10\xc2\xb0 \xe2\x99\x8c");
	CHECK(ICalendar::EscapeText(L"\U0001F319") == "\xf0\x9f\x8c\x99");
}

TEST_CASE("Long lines are folded at 75 bytes, never inside a character", "[Calendar]") {
	CHECK(ICalendar::Fold("short") == "short");
	std::string line(200, 'x');
	auto folded = ICalendar::Fold(line);
	size_t start = 0, longest = 0;
	std::string joined;
	while (true) {
		auto end = folded.find("\r\n", start);
		auto part = folded.substr(start, end == std::string::npos ? std::string::npos : end - start);
		longest = std::max(longest, part.size());
		joined += start == 0 ? part : part.substr(1);
		if (start != 0)
			CHECK(part[0] == ' ');
		if (end == std::string::npos)
			break;
		start = end + 2;
	}
	CHECK(longest <= 75);
	CHECK(joined == line);

	// two-byte characters across the cut stay whole
	std::string accents;
	for (int i = 0; i < 60; i++)
		accents += "\xc3\xa9";
	folded = ICalendar::Fold("SUMMARY:" + accents);
	auto cut = folded.find("\r\n ");
	REQUIRE(cut != std::string::npos);
	CHECK(cut <= 75);
	CHECK(((unsigned char)folded[cut + 3] & 0xC0) != 0x80);
}

TEST_CASE("A calendar of events", "[Calendar]") {
	std::vector<CalendarEvent> events(2);
	events[0].Start = DateTime(2026, 5, 1, 10, 0, 0);
	events[0].End = DateTime(2026, 5, 1, 13, 30, 0);
	events[0].Summary = L"Moon void of course (in Gemini)";
	events[0].Description = L"Last aspect: square Saturn";
	events[1].Start = DateTime(2026, 6, 2, 8, 15, 0);
	events[1].Summary = L"Exact: Transit Saturn square natal Sun";
	auto text = ICalendar::ToText(L"Void of course", events, DateTime(2026, 4, 30, 0, 0, 0));

	CHECK(text.starts_with("BEGIN:VCALENDAR\r\nVERSION:2.0\r\n"));
	CHECK(text.ends_with("END:VCALENDAR\r\n"));
	CHECK(Count(text, "BEGIN:VEVENT") == 2);
	CHECK(Count(text, "END:VEVENT") == 2);
	CHECK(Count(text, "DTSTAMP:20260430T000000Z") == 2);
	CHECK(text.find("DTSTART:20260501T100000Z\r\nDTEND:20260501T133000Z\r\n") != std::string::npos);
	CHECK(text.find("SUMMARY:Moon void of course (in Gemini)\r\n") != std::string::npos);
	CHECK(text.find("DESCRIPTION:Last aspect: square Saturn\r\n") != std::string::npos);
	CHECK(text.find("X-WR-CALNAME:Void of course\r\n") != std::string::npos);
	// a moment has no end
	CHECK(Count(text, "DTEND:") == 1);
	// every line ends with CRLF
	CHECK(Count(text, "\n") == Count(text, "\r\n"));

	SECTION("the same event gets the same id each time, another one another") {
		auto again = ICalendar::ToText(L"Void of course", events, DateTime(2027, 1, 1, 0, 0, 0));
		auto uid = [](std::string const& text, size_t which) {
			size_t at = 0;
			for (size_t i = 0; i <= which; i++)
				at = text.find("UID:", at) + 4;
			return text.substr(at, text.find("\r\n", at) - at);
		};
		CHECK(uid(text, 0) == uid(again, 0));
		CHECK(uid(text, 0) != uid(text, 1));
		CHECK(uid(text, 0).ends_with("@astrostudio"));
	}
	SECTION("an id given is used") {
		events[1].Uid = L"mine-1";
		CHECK(ICalendar::ToText(L"", events, DateTime(2026, 4, 30, 0, 0, 0)).find("UID:mine-1\r\n") != std::string::npos);
	}
}
