#include "TestCommon.h"
#include "DateTime.h"

TEST_CASE("Julian day of well-known dates", "[DateTime]") {
	// J2000.0 and the examples from Meeus, Astronomical Algorithms, chapter 7
	CHECK(DateTime::DateToJD(2000, 1, 1.5, true) == Approx(2451545.0));
	CHECK(DateTime::DateToJD(1957, 10, 4.81, true) == Approx(2436116.31));
	CHECK(DateTime::DateToJD(333, 1, 27.5, false) == Approx(1842713.0));
	CHECK(DateTime::DateToJD(1970, 1, 1, true) == Approx(2440587.5));
}

TEST_CASE("The 1582 calendar reform", "[DateTime]") {
	// 4 Oct 1582 (Julian) is followed by 15 Oct 1582 (Gregorian): consecutive days
	auto last = DateTime::DateToJD(1582, 10, 4, false);
	auto first = DateTime::DateToJD(1582, 10, 15, true);
	CHECK(first - last == Approx(1.0));
	CHECK(first == Approx(2299160.5));

	CHECK(DateTime::AfterPapalReform(1582, 10, 15));
	CHECK_FALSE(DateTime::AfterPapalReform(1582, 10, 14));
	CHECK(DateTime::AfterPapalReform(2299160.5));
	CHECK_FALSE(DateTime::AfterPapalReform(2299160.4));

	CHECK(DateTime(1500, 1, 1, false).InGregorianCalendar() == false);
	CHECK(DateTime(2000, 1, 1, true).InGregorianCalendar() == true);
}

TEST_CASE("Components round-trip", "[DateTime]") {
	DateTime dt(2024, 2, 29, 13, 45, 30, true);
	CHECK(dt.Year() == 2024);
	CHECK(dt.Month() == 2);
	CHECK(dt.Day() == 29);
	CHECK(dt.Hour() == 13);
	CHECK(dt.Minute() == 45);
	CHECK(dt.Second() == Approx(30).margin(0.01));

	long y, m, d, h, min;
	double s;
	dt.Get(y, m, d, h, min, s);
	CHECK(y == 2024);
	CHECK(m == 2);
	CHECK(d == 29);
	CHECK(h == 13);
	CHECK(min == 45);
	CHECK(s == Approx(30).margin(0.01));
}

TEST_CASE("Julian calendar dates round-trip", "[DateTime]") {
	DateTime dt(1066, 10, 14, 9, 30, 0, false);
	CHECK_FALSE(dt.InGregorianCalendar());
	CHECK(dt.Year() == 1066);
	CHECK(dt.Month() == 10);
	CHECK(dt.Day() == 14);
	CHECK(dt.Hour() == 9);
	CHECK(dt.Minute() == 30);
}

TEST_CASE("Years before the common era", "[DateTime]") {
	// astronomical year numbering: 0 is 1 BC
	DateTime dt(-100, 3, 15, 12, 0, 0, false);
	CHECK(dt.Year() == -100);
	CHECK(dt.Month() == 3);
	CHECK(dt.Day() == 15);
	CHECK(dt.Hour() == 12);
}

TEST_CASE("Whole minutes come back as whole minutes", "[DateTime]") {
	// the fraction of the day must not land a hair under a whole minute: 07:00:00 is not 06:59:59.99999
	for (int hour : { 0, 1, 7, 12, 18, 23 })
		for (int minute : { 0, 1, 15, 30, 59 }) {
			DateTime dt(2026, 9, 24, hour, minute, 0, true);
			INFO("time " << hour << ":" << minute);
			CHECK(dt.Hour() == hour);
			CHECK(dt.Minute() == minute);
			CHECK(dt.Second() == Approx(0).margin(0.01));
		}
}

TEST_CASE("Leap years", "[DateTime]") {
	CHECK(DateTime::IsLeap(2024, true));
	CHECK(DateTime::IsLeap(2000, true));
	CHECK_FALSE(DateTime::IsLeap(1900, true));
	CHECK_FALSE(DateTime::IsLeap(2023, true));
	// every fourth year in the Julian calendar, centuries included
	CHECK(DateTime::IsLeap(1900, false));
	CHECK(DateTime::IsLeap(1500, false));

	CHECK(DateTime(2024, 6, 1).Leap());
	CHECK_FALSE(DateTime(2023, 6, 1).Leap());
	CHECK(DateTime(2024, 6, 1).DaysInYear() == 366);
	CHECK(DateTime(2023, 6, 1).DaysInYear() == 365);
}

TEST_CASE("Days in month", "[DateTime]") {
	CHECK(DateTime::DaysInMonth(2, true) == 29);
	CHECK(DateTime::DaysInMonth(2, false) == 28);
	CHECK(DateTime::DaysInMonth(1, false) == 31);
	CHECK(DateTime::DaysInMonth(4, false) == 30);
	CHECK(DateTime::DaysInMonth(12, true) == 31);

	CHECK(DateTime(2024, 2, 10).DaysInMonth() == 29);
	CHECK(DateTime(2023, 2, 10).DaysInMonth() == 28);
	CHECK(DateTime(2023, 9, 10).DaysInMonth() == 30);
}

TEST_CASE("Day of the week", "[DateTime]") {
	CHECK(DateTime(1970, 1, 1).GetDayOfWeek() == DayOfWeek::Thursday);
	CHECK(DateTime(2000, 1, 1).GetDayOfWeek() == DayOfWeek::Saturday);
	CHECK(DateTime(2024, 1, 1).GetDayOfWeek() == DayOfWeek::Monday);
	CHECK(DateTime(2024, 2, 29).GetDayOfWeek() == DayOfWeek::Thursday);
	CHECK(DateTime(2026, 9, 24).GetDayOfWeek() == DayOfWeek::Thursday);
	// Thursday 4 Oct 1582 (Julian) was followed by Friday 15 Oct 1582 (Gregorian)
	CHECK(DateTime(1582, 10, 4, false).GetDayOfWeek() == DayOfWeek::Thursday);
	CHECK(DateTime(1582, 10, 15, true).GetDayOfWeek() == DayOfWeek::Friday);
}

TEST_CASE("Day of the year", "[DateTime]") {
	CHECK(DateTime(2024, 1, 1).DayOfYear() == Approx(1));
	CHECK(DateTime(2024, 3, 1).DayOfYear() == Approx(61));	// leap year
	CHECK(DateTime(2023, 3, 1).DayOfYear() == Approx(60));
	CHECK(DateTime(2024, 12, 31).DayOfYear() == Approx(366));
	CHECK(DateTime(2023, 12, 31).DayOfYear() == Approx(365));
}

TEST_CASE("Fractional year", "[DateTime]") {
	CHECK(DateTime(2024, 1, 1).FractionalYear() == Approx(2024.0));
	CHECK(DateTime(2023, 7, 2).FractionalYear() == Approx(2023.5).margin(0.005));
}

TEST_CASE("AddDays", "[DateTime]") {
	auto dt = DateTime(2024, 2, 28, 12, 0, 0).AddDays(2);
	CHECK(dt.Year() == 2024);
	CHECK(dt.Month() == 3);
	CHECK(dt.Day() == 1);
	CHECK(dt.Hour() == 12);

	dt = DateTime(2024, 1, 1, 0, 0, 0).AddDays(-1);
	CHECK(dt.Year() == 2023);
	CHECK(dt.Month() == 12);
	CHECK(dt.Day() == 31);

	dt = DateTime(2024, 1, 1, 0, 0, 0).AddDays(0.5);
	CHECK(dt.Hour() == 12);

	// the calendar stays as it was
	CHECK(DateTime(1500, 1, 1, false).AddDays(10).InGregorianCalendar() == false);
	CHECK(DateTime(2000, 1, 1, true).AddDays(10).InGregorianCalendar() == true);
}

TEST_CASE("Conversion to a double is the Julian day", "[DateTime]") {
	DateTime dt(2000, 1, 1, 12, 0, 0);
	double jd = dt;
	CHECK(jd == Approx(2451545.0));
	CHECK(dt.Julian() == Approx(2451545.0));
	CHECK(DateTime(2451545.0).Year() == 2000);
	CHECK(DateTime(2451545.0) < DateTime(2451546.0));
}

TEST_CASE("Julian and Gregorian calendar conversion", "[DateTime]") {
	// in 2000 the calendars are 13 days apart
	auto g = DateTime::JulianToGregorian(2000, 1, 1);
	CHECK(g.Year == 2000);
	CHECK(g.Month == 1);
	CHECK(g.Day == 14);

	auto j = DateTime::GregorianToJulian(2000, 1, 1);
	CHECK(j.Year == 1999);
	CHECK(j.Month == 12);
	CHECK(j.Day == 19);
}

TEST_CASE("Day of the year to day and month", "[DateTime]") {
	long day, month;
	DateTime::DayOfYearToDayAndMonth(60, false, day, month);
	CHECK(month == 3);
	CHECK(day == 1);
	DateTime::DayOfYearToDayAndMonth(60, true, day, month);
	CHECK(month == 2);
	CHECK(day == 29);
	DateTime::DayOfYearToDayAndMonth(1, false, day, month);
	CHECK(month == 1);
	CHECK(day == 1);
	DateTime::DayOfYearToDayAndMonth(365, false, day, month);
	CHECK(month == 12);
	CHECK(day == 31);
}

TEST_CASE("SetDate and SetTime keep the other part", "[DateTime]") {
	DateTime dt(2024, 5, 10, 8, 30, 0);
	dt.SetDate(2025, 6, 11);
	CHECK(dt.Year() == 2025);
	CHECK(dt.Month() == 6);
	CHECK(dt.Day() == 11);
	CHECK(dt.Hour() == 8);
	CHECK(dt.Minute() == 30);

	dt.SetTime(21, 15, 0);
	CHECK(dt.Year() == 2025);
	CHECK(dt.Month() == 6);
	CHECK(dt.Day() == 11);
	CHECK(dt.Hour() == 21);
	CHECK(dt.Minute() == 15);
}

TEST_CASE("Just before midnight stays on the same day", "[DateTime]") {
	DateTime dt(2024, 12, 31, 23, 59, 59);
	CHECK(dt.Year() == 2024);
	CHECK(dt.Month() == 12);
	CHECK(dt.Day() == 31);
	CHECK(dt.Hour() == 23);
	CHECK(dt.Minute() == 59);
	CHECK(dt.Second() == Approx(59).margin(0.01));

	auto next = dt.AddDays(1.0 / 86400);
	CHECK(next.Year() == 2025);
	CHECK(next.Month() == 1);
	CHECK(next.Day() == 1);
	CHECK(next.Hour() == 0);
	CHECK(next.Minute() == 0);
}

TEST_CASE("Whole seconds come back as whole seconds", "[DateTime]") {
	for (int second : { 0, 1, 29, 30, 45, 59 }) {
		DateTime dt(2026, 9, 24, 14, 50, second);
		INFO("second " << second);
		CHECK(dt.Minute() == 50);
		CHECK(dt.Second() == Approx(second).margin(0.002));
		CHECK((int)dt.Second() == second);
	}
}

TEST_CASE("Conversion to a SYSTEMTIME", "[DateTime]") {
	DateTime dt(2024, 3, 5, 18, 30, 45);
	auto st = dt.AsSystemTime();
	CHECK(st.wYear == 2024);
	CHECK(st.wMonth == 3);
	CHECK(st.wDay == 5);
	CHECK(st.wHour == 18);
	CHECK(st.wMinute == 30);
	CHECK(st.wSecond == 45);
	CHECK(st.wDayOfWeek == 2);		// Tuesday
}

TEST_CASE("Setting a date from a SYSTEMTIME", "[DateTime]") {
	SYSTEMTIME st{};
	st.wYear = 2024; st.wMonth = 3; st.wDay = 5;
	DateTime dt;
	dt.Set(st);
	CHECK(dt.Year() == 2024);
	CHECK(dt.Month() == 3);
	CHECK(dt.Day() == 5);
}

TEST_CASE("Setting a time from a SYSTEMTIME keeps the time of day", "[DateTime]") {
	SYSTEMTIME st{};
	st.wYear = 2024; st.wMonth = 3; st.wDay = 5; st.wHour = 18; st.wMinute = 30; st.wSecond = 45; st.wMilliseconds = 500;
	DateTime dt;
	dt.Set(st);
	CHECK(dt.Hour() == 18);
	CHECK(dt.Minute() == 30);
	CHECK(dt.Second() == Approx(45.5).margin(0.002));
}

TEST_CASE("Now and Today", "[DateTime]") {
	auto today = DateTime::Today();
	auto now = DateTime::Now();
	CHECK(today.Hour() == 0);
	CHECK(today.Minute() == 0);
	CHECK(now.Julian() >= today.Julian());
	CHECK(now.Julian() - today.Julian() < 1.0 + 1e-6);
	CHECK(now.Year() >= 2024);
}
