#include "pch.h"
#include "DateTime.h"
#include <assert.h>
#include <array>

using namespace std;

DateTime::DateTime() noexcept : m_Julian(0), m_GregorianCalendar(false) {
}

DateTime::DateTime(long Year, long Month, double Day, bool bGregorianCalendar) noexcept : m_Julian(0), m_GregorianCalendar(false) {
	Set(Year, Month, Day, 0, 0, 0, bGregorianCalendar);
}

DateTime::DateTime(long Year, long Month, double Day, double Hour, double Minute, double Second, bool bGregorianCalendar) noexcept
	: m_Julian(0), m_GregorianCalendar(false) {
	Set(Year, Month, Day, Hour, Minute, Second, bGregorianCalendar);
}

DateTime::DateTime(double JD, bool bGregorianCalendar) noexcept : m_Julian(0), m_GregorianCalendar(false) {
	Set(JD, bGregorianCalendar);
}

double DateTime::DateToJD(long Year, long Month, double Day, bool bGregorianCalendar) noexcept {
	long Y = Year;
	long M = Month;
	if (M < 3) {
		Y = Y - 1;
		M = M + 12;
	}

	long B = 0;
	if (bGregorianCalendar) {
		const long A = INT(Y / 100.0);
		B = 2 - A + INT(A / 4.0);
	}

	return 0.0 + INT(365.25 * (Y + 4716.0)) + INT(30.6001 * (M + 1.0)) + Day + B - 1524.5;
}

bool DateTime::IsLeap(long Year, bool bGregorianCalendar) noexcept {
	if (bGregorianCalendar) {
		if ((Year % 100) == 0)
			return ((Year % 400) == 0) ? true : false;
		else
			return ((Year % 4) == 0) ? true : false;
	}
	else
		return ((Year % 4) == 0) ? true : false;
}

void DateTime::Set(long Year, long Month, double Day, double Hour, double Minute, double Second, bool bGregorianCalendar) noexcept {
	const double dblDay = Day + (Hour / 24) + (Minute / 1440) + (Second / 86400);
	Set(DateToJD(Year, Month, dblDay, bGregorianCalendar), bGregorianCalendar);
}

void DateTime::Get(long& Year, long& Month, long& Day, long& Hour, long& Minute, double& Second) const noexcept {
	const double JD = m_Julian + 0.5;
	double tempZ = 0;
	double F = modf(JD, &tempZ);
	const long Z = static_cast<long>(tempZ);
	long A = 0;

	if (m_GregorianCalendar) {
		const long alpha = INT((Z - 1867216.25) / 36524.25);
		A = Z + 1 + alpha - INT(INT(alpha) / 4.0);
	}
	else
		A = Z;

	const long B = A + 1524;
	long C = INT((B - 122.1) / 365.25);
	const long D = INT(365.25 * C);
	long E = INT((0.0 + B - D) / 30.6001);

	double dblDay = 0.0 + B - D - INT(30.6001 * E) + F;
	Day = static_cast<long>(dblDay);

	if (E < 14)
		Month = E - 1;
	else
		Month = E - 13;

	if (Month > 2)
		Year = C - 4716;
	else
		Year = C - 4715;

	F = modf(dblDay, &tempZ);
	Hour = INT(F * 24);
	Minute = INT((F - (Hour) / 24.0) * 1440.0);
	Second = (F - (Hour / 24.0) - (Minute / 1440.0)) * 86400.0;
}

void DateTime::Set(double JD, bool bGregorianCalendar) noexcept {
	m_Julian = JD;
	SetInGregorianCalendar(bGregorianCalendar);
}

void DateTime::SetInGregorianCalendar(bool bGregorianCalendar) noexcept {
	const bool bAfterPapalReform = AfterPapalReform(m_Julian);

#ifdef _DEBUG
	if (bGregorianCalendar) //We do not allow storage of proleptic Gregorian dates
		assert(bAfterPapalReform);
#endif //#ifdef _DEBUG

	m_GregorianCalendar = bGregorianCalendar && bAfterPapalReform;
}

long DateTime::Day() const noexcept {
	long Year = 0;
	long Month = 0;
	long Day = 0;
	long Hour = 0;
	long Minute = 0;
	double Second = 0;
	Get(Year, Month, Day, Hour, Minute, Second);
	return Day;
}

long DateTime::Month() const noexcept {
	long Year = 0;
	long Month = 0;
	long Day = 0;
	long Hour = 0;
	long Minute = 0;
	double Second = 0;
	Get(Year, Month, Day, Hour, Minute, Second);
	return Month;
}

long DateTime::Year() const noexcept {
	long Year = 0;
	long Month = 0;
	long Day = 0;
	long Hour = 0;
	long Minute = 0;
	double Second = 0;
	Get(Year, Month, Day, Hour, Minute, Second);
	return Year;
}

long DateTime::Hour() const noexcept {
	long Year = 0;
	long Month = 0;
	long Day = 0;
	long Hour = 0;
	long Minute = 0;
	double Second = 0;
	Get(Year, Month, Day, Hour, Minute, Second);
	return Hour;
}

long DateTime::Minute() const noexcept {
	long Year = 0;
	long Month = 0;
	long Day = 0;
	long Hour = 0;
	long Minute = 0;
	double Second = 0;
	Get(Year, Month, Day, Hour, Minute, Second);
	return Minute;
}

double DateTime::Second() const noexcept {
	long Year = 0;
	long Month = 0;
	long Day = 0;
	long Hour = 0;
	long Minute = 0;
	double Second = 0;
	Get(Year, Month, Day, Hour, Minute, Second);
	return Second;
}

DayOfWeek DateTime::GetDayOfWeek() const noexcept {
	return static_cast<DayOfWeek>((static_cast<long>(m_Julian + 1.5) % 7));
}

long DateTime::DaysInMonth(long Month, bool bLeap) noexcept {
	//Validate our parameters
	assert(Month >= 1 && Month <= 12);
#ifdef _MSC_VER
	__analysis_assume(Month >= 1 && Month <= 12);
#endif //#ifdef _MSC_VER

	static constexpr array<int, 12> g_NonLeapMonths{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	static constexpr array<int, 12> g_LeapMonths{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

	if (bLeap) {
#ifdef _MSC_VER
#pragma warning(suppress : 26446 26472 26482)
#endif //#ifdef _MSC_VER
		return g_LeapMonths[static_cast<size_t>(Month) - 1];
	}
	else {
#ifdef _MSC_VER
#pragma warning(suppress : 26446 26472 26482)
#endif //#ifdef _MSC_VER
		return g_NonLeapMonths[static_cast<size_t>(Month) - 1];
	}
}

long DateTime::DaysInMonth() const noexcept {
	long Year = 0;
	long Month = 0;
	long Day = 0;
	long Hour = 0;
	long Minute = 0;
	double Second = 0;
	Get(Year, Month, Day, Hour, Minute, Second);

	return DaysInMonth(Month, IsLeap(Year, m_GregorianCalendar));
}

long DateTime::DaysInYear() const noexcept {
	long Year = 0;
	long Month = 0;
	long Day = 0;
	long Hour = 0;
	long Minute = 0;
	double Second = 0;
	Get(Year, Month, Day, Hour, Minute, Second);

	if (IsLeap(Year, m_GregorianCalendar))
		return 366;
	else
		return 365;
}

double DateTime::DayOfYear() const noexcept {
	long Year = 0;
	long Month = 0;
	long Day = 0;
	long Hour = 0;
	long Minute = 0;
	double Second = 0;
	Get(Year, Month, Day, Hour, Minute, Second);

	return DayOfYear(m_Julian, Year, AfterPapalReform(Year, 1, 1));
}

double DateTime::DayOfYear(double JD, long Year, bool bGregorianCalendar) noexcept {
	return JD - DateToJD(Year, 1, 1, bGregorianCalendar) + 1;
}

double DateTime::FractionalYear() const noexcept {
	long Year = 0;
	long Month = 0;
	long Day = 0;
	long Hour = 0;
	long Minute = 0;
	double Second = 0;
	Get(Year, Month, Day, Hour, Minute, Second);

	long DaysInYear = 0;
	if (IsLeap(Year, m_GregorianCalendar))
		DaysInYear = 366;
	else
		DaysInYear = 365;

	return Year + ((m_Julian - DateToJD(Year, 1, 1, AfterPapalReform(Year, 1, 1))) / DaysInYear);
}

bool DateTime::Leap() const noexcept {
	return IsLeap(Year(), m_GregorianCalendar);
}

void DateTime::DayOfYearToDayAndMonth(long DayOfYear, bool bLeap, long& DayOfMonth, long& Month) noexcept {
	long K = bLeap ? 1 : 2;

	Month = INT(9 * (0.0 + K + DayOfYear) / 275.0 + 0.98);
	if (DayOfYear < 32)
		Month = 1;

	DayOfMonth = DayOfYear - INT((275.0 * Month) / 9.0) + (K * INT((Month + 9.0) / 12.0)) + 30;
}

CalendarDate DateTime::JulianToGregorian(long Year, long Month, long Day) noexcept {
	DateTime date(Year, Month, Day, false);
	date.SetInGregorianCalendar(true);

	CalendarDate GregorianDate;
	long Hour = 0;
	long Minute = 0;
	double Second = 0;
	date.Get(GregorianDate.Year, GregorianDate.Month, GregorianDate.Day, Hour, Minute, Second);

	return GregorianDate;
}

CalendarDate DateTime::GregorianToJulian(long Year, long Month, long Day) noexcept {
	DateTime date(Year, Month, Day, true);
	date.SetInGregorianCalendar(false);

	CalendarDate JulianDate;
	long Hour = 0;
	long Minute = 0;
	double Second = 0;
	date.Get(JulianDate.Year, JulianDate.Month, JulianDate.Day, Hour, Minute, Second);

	return JulianDate;
}


DateTime DateTime::AddDays(double days) const {
	return DateTime(m_Julian + days, m_GregorianCalendar);
}

DateTime DateTime::Today(bool local) {
	SYSTEMTIME st;
	if (local)
		::GetLocalTime(&st);
	else
		::GetSystemTime(&st);
	return DateTime(st.wYear, st.wMonth, st.wDay, 0, 0, 0, true);
}

DateTime DateTime::Now(bool local) {
	SYSTEMTIME st;
	if (local)
		::GetLocalTime(&st);
	else
		::GetSystemTime(&st);
	return DateTime(st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond + st.wMilliseconds / 1000.0, true);
}

