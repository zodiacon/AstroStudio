#include "pch.h"
#include "TimeStep.h"
#include <cmath>

namespace {
	DateTime FromJulian(double jd) {
		return DateTime(jd, DateTime::AfterPapalReform(jd));
	}

	bool InRange(long year) {
		return year >= MinChartYear && year <= MaxChartYear;
	}

	long FloorDiv(long a, long b) {
		long q = a / b;
		return (a % b != 0 && (a < 0) != (b < 0)) ? q - 1 : q;
	}
}

bool TimeStep::Step(DateTime& ut, TimeZoneInfo& tz, StepUnit unit, int count) {
	if (count == 0)
		return true;

	TimeZoneInfo zone = tz;
	DateTime result;

	if (unit == StepUnit::Minute || unit == StepUnit::Hour) {
		double seconds = (double)count * (unit == StepUnit::Minute ? 60 : 3600);
		// snapped to a whole second so repeated steps don't drift
		double jd = std::round((ut.Julian() * 86400.0) + seconds) / 86400.0;
		result = FromJulian(jd);

		int offset;
		auto local = TimeZones::UtToLocal(result, zone, &offset);
		if (!InRange(local.Year))
			return false;
		zone.OffsetUT = offset;
	}
	else {
		auto local = TimeZones::UtToLocal(ut, zone);

		if (unit == StepUnit::Day || unit == StepUnit::Week) {
			// Julian day arithmetic also walks straight over the days the calendar reform skipped
			double jd = TimeZones::FieldsToDateTime(local).Julian() + (double)count * (unit == StepUnit::Week ? 7 : 1);
			local = TimeZones::DateTimeToFields(FromJulian(jd));
		}
		else {
			long months = (long)count * (unit == StepUnit::Year ? 12 : 1);
			long total = local.Year * 12 + (local.Month - 1) + months;
			local.Year = FloorDiv(total, 12);
			local.Month = total - local.Year * 12 + 1;

			bool gregorian = DateTime::AfterPapalReform(local.Year, local.Month, 1);
			local.Day = std::min(local.Day, DateTime::DaysInMonth(local.Month, DateTime::IsLeap(local.Year, gregorian)));
			if (local.Year == 1582 && local.Month == 10 && local.Day > 4 && local.Day < 15)
				local.Day = 15;
		}

		if (!InRange(local.Year))
			return false;
		result = TimeZones::LocalToUt(local, zone);
	}

	ut = result;
	tz = std::move(zone);
	return true;
}
