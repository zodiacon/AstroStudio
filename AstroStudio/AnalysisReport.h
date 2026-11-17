#pragma once

#include "ChartAnalysis.h"
#include "Helpers.h"
#include "Printing.h"
#include <optional>
#include <vector>

// The analysis of a chart (ChartAnalysis) in words, as sections of items - which the Chart Analysis tab shows as a tree - and the
// same as a report: pages for the printer (Printing::Report), a web page, and a flat table for CSV.
namespace AnalysisReport {
	struct Item {
		CString Text;
		// the planets the item is about (highlighted on the wheel when it is selected)
		std::vector<Planet> Planets;
		std::vector<Item> Children;
		// a remark under a table (the report shows it after the table, where the other items of the section are the table's rows)
		bool Note{ false };
	};

	struct Section {
		CString Title;
		std::vector<Item> Items;
		// how the report shows the section, if a table says it better than lines (the tree always has the items)
		std::optional<Helpers::TableSource> Table;
	};

	PCWSTR PatternName(PatternKind kind);
	PCWSTR ElementName(Element element);
	PCWSTR ModalityName(Modality modality);
	PCWSTR DignityName(Dignity dignity);
	// a group of a division of the chart: "Fire", "Above the horizon", "Quadrant 2 (houses 4-6)"...
	CString GroupName(BalanceKind kind, int group);

	std::vector<Section> Build(ChartData const& chart, ChartAnalysisResult const& result, ChartAnalysisOptions const& options);

	// the pages (the picture is a packed DIB, ChartImage::ToDib)
	Printing::Report ToReport(std::vector<Section> const& sections, CString const& title, std::vector<CString> const& lines, std::vector<BYTE> picture);
	// a page that stands alone: its style inside, the picture (PNG bytes) as a data: URL
	CString ToHtml(std::vector<Section> const& sections, CString const& title, std::vector<CString> const& lines, std::vector<BYTE> const& png);
	// every item as a row (Section, Item), the items under another indented
	Helpers::TableSource ToTable(std::vector<Section> const& sections);
}
