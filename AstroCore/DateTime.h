#pragma once

struct CalendarDate {
    long Year = 0;
    long Month = 0;
    long Day = 0;
};

enum class DayOfWeek {
    Sunday = 0,
    Monday = 1,
    Tuesday = 2,
    Wednesday = 3,
    Thursday = 4,
    Friday = 5,
    Saturday = 6
};

class DateTime final {
public:
    DateTime() noexcept;
    DateTime(long Year, long Month, double Day, bool bGregorianCalendar) noexcept;
    DateTime(long Year, long Month, double Day, double Hour, double Minute, double Second, bool bGregorianCalendar) noexcept;
    DateTime(double JD, bool bGregorianCalendar) noexcept;

    //Static Methods
    static double DateToJD(long Year, long Month, double Day, bool bGregorianCalendar) noexcept;
    static bool IsLeap(long Year, bool bGregorianCalendar) noexcept;
    static void DayOfYearToDayAndMonth(long DayOfYear, bool bLeap, long& DayOfMonth, long& Month) noexcept;
    static CalendarDate JulianToGregorian(long Year, long Month, long Day) noexcept;
    static CalendarDate GregorianToJulian(long Year, long Month, long Day) noexcept;
    [[nodiscard]] static DateTime Now();
    [[nodiscard]] static DateTime Today();

    constexpr static bool AfterPapalReform(long Year, long Month, double Day) {
        return ((Year > 1582) || ((Year == 1582) && (Month > 10)) || ((Year == 1582) && (Month == 10) && (Day >= 15)));
    }

    constexpr static bool AfterPapalReform(double JD) {
        return (JD >= 2299160.5);
    }

    static double DayOfYear(double JD, long Year, bool bGregorianCalendar) noexcept;
    static long DaysInMonth(long Month, bool bLeap) noexcept;

    [[nodiscard]] double Julian() const noexcept { return m_Julian; };
    operator double() const noexcept { return m_Julian; };
    [[nodiscard]] long Day() const noexcept;
    [[nodiscard]] long Month() const noexcept;
    [[nodiscard]] long Year() const noexcept;
    [[nodiscard]] long Hour() const noexcept;
    [[nodiscard]] long Minute() const noexcept;
    [[nodiscard]] double Second() const noexcept;
    void Set(long Year, long Month, double Day, double Hour, double Minute, double Second, bool bGregorianCalendar) noexcept;
    void Set(double JD, bool bGregorianCalendar) noexcept;
    void SetInGregorianCalendar(bool bGregorianCalendar) noexcept;
    void Get(long& Year, long& Month, long& Day, long& Hour, long& Minute, double& Second) const noexcept;
    [[nodiscard]] DayOfWeek GetDayOfWeek() const noexcept;
    [[nodiscard]] double DayOfYear() const noexcept;
    [[nodiscard]] long DaysInMonth() const noexcept;
    [[nodiscard]] long DaysInYear() const noexcept;
    [[nodiscard]] bool Leap() const noexcept;
    [[nodiscard]] bool InGregorianCalendar() const noexcept { return m_GregorianCalendar; };
    [[nodiscard]] double FractionalYear() const noexcept;
    [[nodiscard]] DateTime AddDays(double days) const;

protected:
    //Member variables
    double m_Julian; //Julian Day number for this date
    bool m_GregorianCalendar; //Is this date in the Gregorian calendar
};

