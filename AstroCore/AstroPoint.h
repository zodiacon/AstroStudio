#pragma once

#include <string>
#include <vector>

enum class ZodiacSign;

enum class PlanetType {
	Sun,
	Moon,
	Mercury,
	Venus,
	Mars,
	Jupiter,
	Saturn,
	Uranus,
	Neptune,
	Pluto,
	MeanNode,
	TrueNode,
	Lilith,
	OscuApog,
	Earth,
	Chiron,
	Pholus,
	Ceres,
	Pallas,
	Juno,
	Vesta,
	NumPlanets,
};

struct PlanetInfo {
	PlanetType Type;
	std::wstring Name;
};

enum class ZodiacSign {
	Aries,
	Taurus,
	Gemini,
	Cancer,
	Leo,
	Virgo,
	Libra,
	Scorpio,
	Sagittarius,
	Capricorn,
	Aquarius,
	Pisces,
};

struct ZodiacSignInfo {
	ZodiacSign Sign;
	std::wstring Name;
};

enum class AstroPointFlags {
	None = 0,
	Retro = 1,
	Direct = 2,
	Stationary = 4
};
DEFINE_ENUM_FLAG_OPERATORS(AstroPointFlags);

struct AstroPoint final {
	AstroPoint(double value, AstroPointFlags flags = AstroPointFlags::None);
	AstroPoint() = default;
	AstroPoint& operator=(double value);

	operator double() const {
		return Value;
	}

	ZodiacSign Sign() const;
	double DegreeInSign() const;
	double Minutes() const;
	double Seconds() const;
	AstroPoint NextSign() const;
	AstroPoint ZeroSign() const;
	AstroPoint Opposite() const;
	AstroPoint& Normalize();
	[[nodiscard]] AstroPoint Normalize() const;
	bool IsBetween(AstroPoint const& start, AstroPoint const& end) const;

	static double Diff(AstroPoint const& p1, AstroPoint const& p2);
	static AstroPoint MidPoint(AstroPoint const& p1, AstroPoint const& p2);

	double Value;
	AstroPointFlags Flags;
};



