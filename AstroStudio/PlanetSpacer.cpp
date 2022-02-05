#include "pch.h"
#include "PlanetSpacer.h"

PlanetSpacer::PlanetSpacer(std::vector<PlanetPosition> const& positions) : m_positions(positions) {
}

void PlanetSpacer::Space(double minDegrees) {
	auto pos = m_positions;
	std::sort(pos.begin(), pos.end(), [](auto& p1, auto& p2) {
		return p1.Longitude < p2.Longitude;
		});

	bool done;
	do {
		done = true;
		int size = (int)pos.size();
		for (int i = 0; i < size; i++) {
			int j = (i + 1) % size;
			double diff = fabs(pos[i].Longitude - pos[j].Longitude);
			if (diff > 180)
				diff = 360 - diff;
			if (diff < minDegrees) {
				pos[i].Longitude.Value -= (minDegrees - diff) / 2;
				pos[j].Longitude.Value += (minDegrees - diff) / 2;
				done = false;
				pos[i].Longitude.Normalize();
				pos[j].Longitude.Normalize();
			}
		}
	} while (!done);

	m_newPositions = std::move(pos);
}

std::vector<PlanetPosition> const& PlanetSpacer::NewPositions() const {
	return m_newPositions;
}
