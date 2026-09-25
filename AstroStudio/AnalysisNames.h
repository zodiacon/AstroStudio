#pragma once

#include "Analysis.h"

// The words for analysis types and events, shared by the dialog and the list.
namespace AnalysisNames {
	inline PCWSTR TypeName(AnalysisType type) {
		switch (type) {
			case AnalysisType::TransitsToNatal: return L"Transits to natal";
			case AnalysisType::ProgressedToNatal: return L"Progressed to natal";
			case AnalysisType::SolarArcToNatal: return L"Solar arc to natal";
			case AnalysisType::TransitsToProgressed: return L"Transits to progressed";
			default: return L"Progressed to progressed";
		}
	}

	// what the moving planets are called
	inline PCWSTR MoverLabel(AnalysisType type) {
		switch (type) {
			case AnalysisType::TransitsToNatal:
			case AnalysisType::TransitsToProgressed: return L"Transit";
			case AnalysisType::SolarArcToNatal: return L"Directed";
			default: return L"Progressed";
		}
	}

	// ... and the ones they are compared with
	inline PCWSTR TargetLabel(AnalysisType type) {
		return type == AnalysisType::TransitsToProgressed || type == AnalysisType::ProgressedToProgressed ? L"progressed" : L"natal";
	}
}
