#include "pch.h"
#include "AnalysisReport.h"
#include <wincrypt.h>
#include <algorithm>
#include <functional>
#include <map>
#include <set>

#pragma comment(lib, "crypt32.lib")

namespace AnalysisReport {
	namespace {
		constexpr wchar_t Degree = 0xb0;

		CString Planets(std::vector<Planet> const& planets) {
			CString text;
			for (auto p : planets)
				text += (text.IsEmpty() ? L"" : L", ") + CString(Helpers::GetPlanetName(p));
			return text;
		}

		CString PointName(BalancePoint const& point) {
			switch (point.Kind) {
				case PointKind::Ascendant: return L"Ascendant";
				case PointKind::Midheaven: return L"Midheaven";
				default: return Helpers::GetPlanetName(point.Body);
			}
		}

		CString Points(BalanceGroup const& group) {
			CString text;
			for (auto const& p : group.Points)
				text += (text.IsEmpty() ? L"" : L", ") + PointName(p);
			return text;
		}

		std::vector<Planet> PlanetsOf(BalanceGroup const& group) {
			std::vector<Planet> result;
			for (auto const& p : group.Points)
				if (p.Kind == PointKind::Planet)
					result.push_back(p.Body);
			return result;
		}

		CString Score(double value) {
			CString text;
			text.Format(L"%g", value);
			return text;
		}

		CString Orb(double orb) {
			CString text;
			text.Format(L"%.2f%c", orb, Degree);
			return text;
		}

		CString Names(BalanceSet const& set, std::vector<int> const& groups) {
			CString text;
			for (int g : groups)
				text += (text.IsEmpty() ? L"" : L", ") + GroupName(set.Kind, g);
			return text;
		}

		CString SignOf(std::map<Planet, ZodiacSign> const& signs, Planet planet) {
			auto it = signs.find(planet);
			return it == signs.end() ? CString(Helpers::GetPlanetName(planet))
				: CString(Helpers::GetPlanetName(planet)) + L" in " + Helpers::GetZodiacSignName(it->second);
		}

		Item PatternItem(AspectPattern const& p) {
			Item item;
			item.Planets = p.Planets;
			CString name = PatternName(p.Kind);
			CString text;
			if (p.Kind == PatternKind::Stellium) {
				if (p.Sign) {
					text.Format(L"%s in %s", (PCWSTR)name, (PCWSTR)Helpers::GetZodiacSignName(*p.Sign));
					if (p.House)
						text.AppendFormat(L" (house %d)", p.House);
				}
				else
					text.Format(L"%s in house %d", (PCWSTR)name, p.House);
				item.Text = text + L": " + Planets(p.Planets);
				return item;
			}
			text = name;
			if (p.Kind == PatternKind::Kite)
				text += p.Element ? CString(L" (on a grand trine in ") + ElementName(*p.Element) + L")" : CString(L" (on a grand trine out of sign)");
			else if (p.Element)
				text.AppendFormat(L" in %s", ElementName(*p.Element));
			else if (p.Modality)
				text.AppendFormat(L" in %s signs", ModalityName(*p.Modality));
			else if (p.OutOfSign)
				text += L" (out of sign)";
			text += L": " + Planets(p.Planets);
			if (p.Apex)
				text.AppendFormat(L"; %s %s", p.Kind == PatternKind::Kite ? L"head" : L"apex", Helpers::GetPlanetName(*p.Apex));
			text.AppendFormat(L"  (widest orb %s)", (PCWSTR)Orb(p.MaxOrb));
			item.Text = text;
			for (auto const& a : p.Aspects) {
				Item child;
				child.Text.Format(L"%s %s %s  %s", Helpers::GetPlanetName(a.Planet1.Planet), (PCWSTR)CString(Helpers::GetAspectName(a.Type)).MakeLower(),
					Helpers::GetPlanetName(a.Planet2.Planet), (PCWSTR)Orb(a.Orb));
				child.Planets = { a.Planet1.Planet, a.Planet2.Planet };
				item.Children.push_back(std::move(child));
			}
			return item;
		}

		// a section of one or more divisions of the chart: an item per group, a table of them, and remarks on the empty and lone ones
		Section BalanceSection(CString title, std::initializer_list<BalanceSet const*> sets, bool weighted, CString divisionHeader) {
			Section section{ std::move(title) };
			std::vector<std::vector<CString>> rows;
			for (auto set : sets) {
				for (int g = 0; g < static_cast<int>(set->Groups.size()); g++) {
					auto const& group = set->Groups[g];
					bool dominant = std::ranges::find(set->Dominant, g) != set->Dominant.end();
					Item item;
					item.Text.Format(L"%s: %s%s%s", (PCWSTR)GroupName(set->Kind, g), (PCWSTR)Score(group.Score),
						group.Points.empty() ? L"" : L"  -  ", (PCWSTR)Points(group));
					if (dominant)
						item.Text += L"  (dominant)";
					item.Planets = PlanetsOf(group);
					section.Items.push_back(std::move(item));
					CString planets;
					planets.Format(L"%d", group.Planets());
					rows.push_back({ GroupName(set->Kind, g) + (dominant ? L" (dominant)" : L""), Score(group.Score), planets, Points(group) });
				}
			}
			for (auto set : sets) {
				auto remark = [&](std::vector<int> const& groups, PCWSTR what) {
					if (groups.empty())
						return;
					Item note;
					note.Note = true;
					note.Text.Format(L"%s: %s", what, (PCWSTR)Names(*set, groups));
					for (int g : groups)
						for (auto p : PlanetsOf(set->Groups[g]))
							note.Planets.push_back(p);
					section.Items.push_back(std::move(note));
				};
				// (a house with one planet in it is nothing to remark on)
				remark(set->Missing, set->Kind == BalanceKind::House ? L"Empty" : L"No planets in");
				if (set->Kind != BalanceKind::House)
					remark(set->Singletons, L"A single planet in");
			}
			section.Table = Printing::TableFromRows({ divisionHeader, weighted ? L"Score" : L"Count", L"Planets", L"Points" }, std::move(rows));
			return section;
		}
	}

	PCWSTR PatternName(PatternKind kind) {
		switch (kind) {
			case PatternKind::GrandCross: return L"Grand Cross";
			case PatternKind::Kite: return L"Kite";
			case PatternKind::MysticRectangle: return L"Mystic Rectangle";
			case PatternKind::GrandTrine: return L"Grand Trine";
			case PatternKind::TSquare: return L"T-Square";
			case PatternKind::Yod: return L"Yod";
			case PatternKind::Stellium: return L"Stellium";
		}
		return L"";
	}

	PCWSTR ElementName(Element element) {
		static PCWSTR names[] = { L"Fire", L"Earth", L"Air", L"Water" };
		return names[static_cast<int>(element)];
	}

	PCWSTR ModalityName(Modality modality) {
		static PCWSTR names[] = { L"Cardinal", L"Fixed", L"Mutable" };
		return names[static_cast<int>(modality)];
	}

	PCWSTR DignityName(Dignity dignity) {
		static PCWSTR names[] = { L"domicile", L"exaltation", L"triplicity", L"term", L"face", L"detriment", L"fall" };
		static_assert(_countof(names) == static_cast<int>(Dignity::Fall) + 1);
		return names[static_cast<int>(dignity)];
	}

	PCWSTR ShapeName(ChartShape shape) {
		static PCWSTR names[] = { L"Bundle", L"Bowl", L"Bucket", L"Locomotive", L"Seesaw", L"Splash", L"Splay" };
		static_assert(_countof(names) == static_cast<int>(ChartShape::Splay) + 1);
		return names[static_cast<int>(shape)];
	}

	PCWSTR AngleName(AngleKind angle) {
		static PCWSTR names[] = { L"Ascendant", L"Midheaven", L"Descendant", L"IC" };
		return names[static_cast<int>(angle)];
	}

	CString GroupName(BalanceKind kind, int group) {
		switch (kind) {
			case BalanceKind::Element: return ElementName(static_cast<Element>(group));
			case BalanceKind::Modality: return ModalityName(static_cast<Modality>(group));
			case BalanceKind::Hemisphere: return group == 0 ? L"Above the horizon" : L"Below the horizon";
			case BalanceKind::Side: return group == 0 ? L"Eastern" : L"Western";
			case BalanceKind::Quadrant: {
				CString text;
				text.Format(L"Quadrant %d (houses %d-%d)", group + 1, group * 3 + 1, group * 3 + 3);
				return text;
			}
			case BalanceKind::Angularity: {
				static PCWSTR names[] = { L"Angular (houses 1, 4, 7, 10)", L"Succedent (2, 5, 8, 11)", L"Cadent (3, 6, 9, 12)" };
				return names[group];
			}
			case BalanceKind::House: {
				CString text;
				text.Format(L"House %d", group + 1);
				return text;
			}
		}
		return L"";
	}

	std::vector<Section> Build(ChartData const& chart, ChartAnalysisResult const& r, ChartAnalysisOptions const& options) {
		std::vector<Section> sections;
		std::map<Planet, ZodiacSign> signs;
		for (auto const& p : chart.AllPlanets())
			signs[p.Planet] = p.Longitude.Sign();

		// the summary
		Section summary{ L"Summary" };
		auto add = [](Section& section, CString text, std::vector<Planet> planets = {}) {
			section.Items.push_back({ std::move(text), std::move(planets) });
		};
		if (r.Ruler) {
			CString text;
			text.Format(L"Chart ruler: %s (the Ascendant is in %s)", Helpers::GetPlanetName(r.Ruler->Body), (PCWSTR)Helpers::GetZodiacSignName(r.Ruler->AscendantSign));
			if (r.Ruler->Sign) {
				text.AppendFormat(L", in %s", (PCWSTR)Helpers::GetZodiacSignName(*r.Ruler->Sign));
				if (r.Ruler->House)
					text.AppendFormat(L", house %d", r.Ruler->House);
			}
			else
				text += L", not in the chart";
			add(summary, text, { r.Ruler->Body });
		}
		if (r.DayChart)
			add(summary, *r.DayChart ? L"Day chart (the Sun above the horizon)" : L"Night chart (the Sun below the horizon)");
		if (r.Shape) {
			CString text;
			text.Format(L"Shape: %s", ShapeName(r.Shape->Shape));
			std::vector<Planet> planets;
			if (r.Shape->Handle) {
				text.AppendFormat(L", handle %s", Helpers::GetPlanetName(*r.Shape->Handle));
				planets.push_back(*r.Shape->Handle);
			}
			if (r.Shape->Leading) {
				text.AppendFormat(L", led by %s", Helpers::GetPlanetName(*r.Shape->Leading));
				planets.push_back(*r.Shape->Leading);
			}
			add(summary, text, planets);
		}
		auto dominant = [&](BalanceSet const& set, PCWSTR what) {
			if (set.Dominant.empty())
				return;
			std::vector<Planet> planets;
			for (int g : set.Dominant)
				for (auto p : PlanetsOf(set.Groups[g]))
					planets.push_back(p);
			CString text;
			text.Format(L"%s: %s (%s)", what, (PCWSTR)Names(set, set.Dominant), (PCWSTR)Score(set.Groups[set.Dominant[0]].Score));
			add(summary, text, planets);
		};
		dominant(r.Elements, L"Dominant element");
		dominant(r.Modalities, L"Dominant modality");
		if (r.HasHouses) {
			CString text;
			for (auto set : { &r.Hemispheres, &r.Sides })
				if (set->Dominant.size() == 1)
					text += (text.IsEmpty() ? L"" : L", ") + GroupName(set->Kind, set->Dominant[0]);
			if (!text.IsEmpty())
				add(summary, L"Emphasis: " + text);
		}
		if (!r.Elements.Missing.empty())
			add(summary, L"No planets in: " + Names(r.Elements, r.Elements.Missing));
		if (r.Dispositors.Final)
			add(summary, CString(L"Final dispositor: ") + Helpers::GetPlanetName(*r.Dispositors.Final), { *r.Dispositors.Final });
		else if (!r.Dispositors.Ends.empty()) {
			CString text;
			std::vector<Planet> planets;
			for (auto const& end : r.Dispositors.Ends) {
				CString names;
				for (auto p : end) {
					names += (names.IsEmpty() ? L"" : L" and ") + CString(Helpers::GetPlanetName(p));
					planets.push_back(p);
				}
				text += (text.IsEmpty() ? L"" : L"; ") + names;
			}
			add(summary, L"No final dispositor; the chains end in: " + text, planets);
		}
		{
			std::map<PatternKind, int> counts;
			for (auto const& p : r.Patterns)
				counts[p.Kind]++;
			CString text;
			for (auto [kind, count] : counts) {
				text += text.IsEmpty() ? L"" : L", ";
				text += PatternName(kind);
				if (count > 1)
					text.AppendFormat(L" (%d)", count);
			}
			add(summary, text.IsEmpty() ? CString(L"No aspect patterns") : L"Aspect patterns: " + text);
		}
		if (!r.OnAngles.empty()) {
			CString text;
			std::vector<Planet> planets;
			for (auto const& a : r.OnAngles) {
				text += (text.IsEmpty() ? L"" : L", ") + CString(Helpers::GetPlanetName(a.Body)) + L" (" + AngleName(a.Angle) + L")";
				planets.push_back(a.Body);
			}
			add(summary, L"On the angles: " + text, planets);
		}
		if (!r.Unaspected.empty())
			add(summary, L"Unaspected: " + Planets(r.Unaspected), r.Unaspected);
		{
			CString text;
			text.Format(L"Retrograde: %d", static_cast<int>(r.Retrograde.size()));
			if (!r.Retrograde.empty())
				text += L" (" + Planets(r.Retrograde) + L")";
			add(summary, text, r.Retrograde);
		}
		{
			CString text;
			text.Format(L"%s rulers; %s", options.Rulers == RulerSystem::Modern ? L"Modern" : L"Traditional",
				options.Weighted ? L"the Sun, the Moon, the Ascendant and the Midheaven count twice" : L"every point counts once");
			add(summary, text);
		}
		sections.push_back(std::move(summary));

		// the patterns
		Section patterns{ L"Aspect Patterns" };
		for (auto const& p : r.Patterns)
			patterns.Items.push_back(PatternItem(p));
		if (r.Patterns.empty())
			add(patterns, L"None found");
		sections.push_back(std::move(patterns));

		// the shape
		Section shape{ L"Chart Shape" };
		if (auto const& s = r.Shape) {
			CString text;
			switch (s->Shape) {
				case ChartShape::Bundle: text.Format(L"all the planets within %.0f%c, a trine or less", s->Span, Degree); break;
				case ChartShape::Bowl: text.Format(L"all the planets within %.0f%c, half the wheel or less", s->Span, Degree); break;
				case ChartShape::Bucket: text = L"the planets in half the wheel, with one alone in the empty half as the handle"; break;
				case ChartShape::Locomotive: text.Format(L"the planets take up %.0f%c, leaving an empty space of %.0f%c", s->Span, Degree, s->LargestGap, Degree); break;
				case ChartShape::Seesaw: text = L"two groups facing each other across two empty spaces of 60\x00b0 or more"; break;
				case ChartShape::Splash: text = L"the planets spread round the whole wheel"; break;
				case ChartShape::Splay: text = L"the planets in irregular clusters"; break;
			}
			std::vector<Planet> all;
			for (auto const& p : chart.AllPlanets())
				if (std::ranges::find(options.Bodies, p.Planet) != options.Bodies.end())
					all.push_back(p.Planet);
			add(shape, CString(ShapeName(s->Shape)) + L": " + text, all);
			if (s->Handle)
				add(shape, CString(L"Handle: ") + Helpers::GetPlanetName(*s->Handle), { *s->Handle });
			if (s->Leading)
				add(shape, CString(L"Leading planet (rising first, after the empty space): ") + Helpers::GetPlanetName(*s->Leading), { *s->Leading });
		}
		else
			add(shape, L"Too few planets to tell (seven are needed)");
		sections.push_back(std::move(shape));

		// the conditions of the planets
		Section conditions{ L"Planetary Conditions" };
		{
			Item angles{ L"On the angles" };
			for (auto const& a : r.OnAngles) {
				Item child;
				child.Text.Format(L"%s on the %s  %s", Helpers::GetPlanetName(a.Body), AngleName(a.Angle), (PCWSTR)Orb(a.Orb));
				child.Planets = { a.Body };
				angles.Planets.push_back(a.Body);
				angles.Children.push_back(std::move(child));
			}
			if (!r.HasHouses)
				angles.Text += L": the chart has no houses";
			else if (r.OnAngles.empty())
				angles.Text.AppendFormat(L": none within %.0f%c", options.AngleOrb, Degree);
			conditions.Items.push_back(std::move(angles));
			add(conditions, r.Unaspected.empty() ? CString(L"Unaspected: none (every planet makes a major aspect)")
				: L"Unaspected (no major aspect): " + Planets(r.Unaspected), r.Unaspected);
			CString retro;
			retro.Format(L"Retrograde: %d", static_cast<int>(r.Retrograde.size()));
			if (!r.Retrograde.empty())
				retro += L" - " + Planets(r.Retrograde);
			add(conditions, retro, r.Retrograde);
		}
		sections.push_back(std::move(conditions));

		// the balance
		sections.push_back(BalanceSection(L"Elements", { &r.Elements }, options.Weighted, L"Element"));
		sections.push_back(BalanceSection(L"Modalities", { &r.Modalities }, options.Weighted, L"Modality"));
		if (r.HasHouses) {
			sections.push_back(BalanceSection(L"Hemispheres and Quadrants", { &r.Hemispheres, &r.Sides, &r.Quadrants }, options.Weighted, L"Part of the chart"));
			sections.push_back(BalanceSection(L"Houses", { &r.Angularity, &r.Houses }, options.Weighted, L"Houses"));
		}
		else {
			for (PCWSTR title : { L"Hemispheres and Quadrants", L"Houses" }) {
				Section houses{ title };
				add(houses, L"The chart has no houses");
				sections.push_back(std::move(houses));
			}
		}

		// the dignities
		Section dignities{ L"Essential Dignities" };
		std::vector<std::vector<CString>> rows;
		std::map<Planet, AstroPoint> longitudes;
		for (auto const& p : chart.AllPlanets())
			longitudes[p.Planet] = p.Longitude;
		const bool minor = std::ranges::any_of(r.Dignities, [](auto const& d) { return d.Body <= Planet::Saturn; });
		for (auto const& d : r.Dignities) {
			const bool seven = d.Body <= Planet::Saturn;
			CString words;
			for (auto x : d.Dignities)
				words += (words.IsEmpty() ? L"" : L", ") + CString(DignityName(x));
			if (d.Peregrine)
				words += words.IsEmpty() ? L"peregrine" : L", peregrine";
			if (words.IsEmpty())
				words = L"none";
			CString score;
			score.Format(L"%+d", d.Score);
			CString position = Helpers::FormatLongitude(longitudes[d.Body], FormatOptions::ShowDegreeGlyph);
			CString text;
			text.Format(L"%s at %s: %s", Helpers::GetPlanetName(d.Body), (PCWSTR)position, (PCWSTR)words);
			if (d.Score)
				text += L" (" + score + L")";
			if (seven)
				text.AppendFormat(L"; term of %s, face of %s", Helpers::GetPlanetName(d.TermRuler), Helpers::GetPlanetName(d.FaceRuler));
			add(dignities, text, { d.Body });
			CString triplicity;
			if (seven)
				triplicity.Format(L"%s / %s / %s", Helpers::GetPlanetName(d.TriplicityRulers[0]), Helpers::GetPlanetName(d.TriplicityRulers[1]),
					Helpers::GetPlanetName(d.TriplicityRulers[2]));
			rows.push_back({ Helpers::GetPlanetName(d.Body), position, words, triplicity, seven ? CString(Helpers::GetPlanetName(d.TermRuler)) : CString(),
				seven ? CString(Helpers::GetPlanetName(d.FaceRuler)) : CString(), d.Score ? score : CString(L"0") });
		}
		dignities.Table = Printing::TableFromRows({ L"Planet", L"Position", L"Dignity", L"Triplicity (day / night / part.)", L"Term", L"Face", L"Score" }, std::move(rows));
		if (minor) {
			Item note;
			note.Note = true;
			note.Text = r.DayChart ? CString(*r.DayChart ? L"A day chart: the day ruler of each triplicity counts, with the participating one"
				: L"A night chart: the night ruler of each triplicity counts, with the participating one")
				: CString(L"The sect is not known (no houses): the day and night rulers of each triplicity both count");
			dignities.Items.push_back(std::move(note));
			dignities.Items.push_back({ L"Dorothean triplicities, Egyptian terms, Chaldean faces; Lilly's scores (domicile +5, exaltation +4, triplicity +3, "
				L"term +2, face +1, detriment -5, fall -4, peregrine -5)", {}, {}, true });
		}
		sections.push_back(std::move(dignities));

		// mutual receptions
		Section receptions{ L"Mutual Receptions" };
		for (auto const& rec : r.Receptions)
			add(receptions, SignOf(signs, rec.A) + L" and " + SignOf(signs, rec.B) + (rec.Exaltation ? L" (by exaltation)" : L" (by domicile)"), { rec.A, rec.B });
		if (r.Receptions.empty())
			add(receptions, L"None");
		sections.push_back(std::move(receptions));

		// the dispositors, as a tree from where the chains end
		Section dispositors{ L"Dispositors" };
		auto const& nodes = r.Dispositors.Nodes;
		std::set<Planet> placed;
		std::function<void(Item&, Planet, std::set<Planet> const&)> children = [&](Item& parent, Planet ruler, std::set<Planet> const& skip) {
			for (auto const& node : nodes) {
				if (node.Ruler != ruler || !node.RulerInChart || skip.contains(node.Body) || placed.contains(node.Body))
					continue;
				placed.insert(node.Body);
				Item child{ SignOf(signs, node.Body), { node.Body } };
				children(child, node.Body, skip);
				parent.Children.push_back(std::move(child));
			}
		};
		for (auto const& end : r.Dispositors.Ends) {
			Item root;
			std::set<Planet> members(end.begin(), end.end());
			for (auto p : end) {
				root.Text += (root.Text.IsEmpty() ? L"" : L" and ") + SignOf(signs, p);
				placed.insert(p);
			}
			if (end.size() == 1)
				root.Text += r.Dispositors.Final ? L": in its own sign, the final dispositor" : L": in its own sign";
			else
				root.Text += end.size() == 2 ? L": each in the other's sign" : L": a loop, each in the sign of the next";
			root.Planets = end;
			for (auto p : end)
				children(root, p, members);
			dispositors.Items.push_back(std::move(root));
		}
		for (auto const& node : nodes) {
			if (node.RulerInChart || placed.contains(node.Body))
				continue;
			placed.insert(node.Body);
			Item root{ SignOf(signs, node.Body), { node.Body } };
			root.Text.AppendFormat(L": its ruler, %s, is not among the planets", Helpers::GetPlanetName(node.Ruler));
			children(root, node.Body, {});
			dispositors.Items.push_back(std::move(root));
		}
		if (dispositors.Items.empty())
			add(dispositors, L"None");
		sections.push_back(std::move(dispositors));
		return sections;
	}

	Printing::Report ToReport(std::vector<Section> const& sections, CString const& title, std::vector<CString> const& lines, std::vector<BYTE> picture) {
		Printing::Report report;
		report.Title = title;
		report.Lines = lines;
		report.Picture = std::move(picture);
		std::function<void(Item const&, int)> addItem = [&](Item const& item, int level) {
			Printing::ReportBlock line;
			line.Text = item.Text;
			line.Level = level;
			line.Bold = level == 0 && !item.Children.empty();
			report.Blocks.push_back(std::move(line));
			for (auto const& child : item.Children)
				addItem(child, level + 1);
		};
		for (auto const& section : sections) {
			Printing::ReportBlock heading;
			heading.Type = Printing::ReportBlock::Kind::Heading;
			heading.Text = section.Title;
			report.Blocks.push_back(std::move(heading));
			if (section.Table) {
				Printing::ReportBlock table;
				table.Type = Printing::ReportBlock::Kind::Table;
				table.Table = section.Table;
				report.Blocks.push_back(std::move(table));
			}
			for (auto const& item : section.Items)
				if (!section.Table || item.Note)
					addItem(item, 0);
		}
		return report;
	}

	namespace {
		CString Escape(CString const& text) {
			CString result;
			for (int i = 0; i < text.GetLength(); i++) {
				switch (auto c = text[i]) {
					case L'&': result += L"&amp;"; break;
					case L'<': result += L"&lt;"; break;
					case L'>': result += L"&gt;"; break;
					case L'"': result += L"&quot;"; break;
					default: result += c; break;
				}
			}
			return result;
		}

		CString Base64(std::vector<BYTE> const& bytes) {
			DWORD size = 0;
			if (bytes.empty() || !::CryptBinaryToStringW(bytes.data(), static_cast<DWORD>(bytes.size()), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &size))
				return {};
			CString text;
			auto buffer = text.GetBuffer(size);
			BOOL ok = ::CryptBinaryToStringW(bytes.data(), static_cast<DWORD>(bytes.size()), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, buffer, &size);
			text.ReleaseBuffer(ok ? size : 0);
			return text;
		}

		void HtmlItems(CString& html, std::vector<Item const*> const& items) {
			if (items.empty())
				return;
			html += L"<ul>\n";
			for (auto item : items) {
				html += L"<li>" + Escape(item->Text);
				std::vector<Item const*> children;
				for (auto const& child : item->Children)
					children.push_back(&child);
				HtmlItems(html, children);
				html += L"</li>\n";
			}
			html += L"</ul>\n";
		}
	}

	CString ToHtml(std::vector<Section> const& sections, CString const& title, std::vector<CString> const& lines, std::vector<BYTE> const& png) {
		CString html;
		html += L"<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n"
			L"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n<title>" + Escape(title) + L"</title>\n"
			L"<style>\n"
			L":root { --text: #1d2330; --muted: #5a6475; --accent: #1e3c6e; --rule: #c9d1de; --row: #f3f5f9; --bg: #ffffff; }\n"
			L"@media (prefers-color-scheme: dark) { :root { --text: #e3e7ee; --muted: #9aa4b5; --accent: #9cc0ff; --rule: #3a4456; --row: #1d2330; --bg: #12161f; } }\n"
			L"body { font-family: 'Segoe UI', system-ui, sans-serif; font-size: 15px; line-height: 1.5; color: var(--text); background: var(--bg); max-width: 860px; margin: 0 auto; padding: 24px 16px 48px; }\n"
			L"h1 { font-size: 26px; margin: 0 0 4px; }\n"
			L".lines { color: var(--muted); margin: 0 0 16px; }\n"
			L".lines div { margin: 0; }\n"
			L"img.wheel { display: block; width: min(100%, 520px); margin: 8px auto 24px; border-radius: 50%; background: #fff; }\n"
			L"h2 { font-size: 18px; color: var(--accent); border-bottom: 1px solid var(--rule); padding-bottom: 4px; margin: 28px 0 10px; }\n"
			L"ul { margin: 4px 0; padding-left: 22px; }\n"
			L"li { margin: 2px 0; }\n"
			L"table { border-collapse: collapse; width: 100%; margin: 6px 0 10px; font-size: 14px; }\n"
			L"th, td { text-align: left; padding: 5px 10px; border-bottom: 1px solid var(--rule); vertical-align: top; }\n"
			L"th { font-weight: 600; border-bottom: 2px solid var(--muted); }\n"
			L"tbody tr:nth-child(even) { background: var(--row); }\n"
			L".table-wrap { overflow-x: auto; }\n"
			L"@media print { body { max-width: none; } h2 { break-after: avoid; } img.wheel { width: 60%; } }\n"
			L"</style>\n</head>\n<body>\n";
		html += L"<h1>" + Escape(title) + L"</h1>\n<div class=\"lines\">\n";
		for (auto const& line : lines)
			html += L"<div>" + Escape(line) + L"</div>\n";
		html += L"</div>\n";
		if (auto data = Base64(png); !data.IsEmpty())
			html += L"<img class=\"wheel\" alt=\"Chart wheel\" src=\"data:image/png;base64," + data + L"\">\n";

		for (auto const& section : sections) {
			html += L"<h2>" + Escape(section.Title) + L"</h2>\n";
			std::vector<Item const*> items;
			if (auto const& table = section.Table) {
				html += L"<div class=\"table-wrap\"><table>\n<thead><tr>";
				for (auto const& header : table->Headers)
					html += L"<th>" + Escape(header) + L"</th>";
				html += L"</tr></thead>\n<tbody>\n";
				for (int row = 0; row < table->Rows; row++) {
					html += L"<tr>";
					for (int column = 0; column < static_cast<int>(table->Headers.size()); column++)
						html += L"<td>" + Escape(table->Cell(row, column)) + L"</td>";
					html += L"</tr>\n";
				}
				html += L"</tbody>\n</table></div>\n";
				for (auto const& item : section.Items)
					if (item.Note)
						items.push_back(&item);
			}
			else
				for (auto const& item : section.Items)
					items.push_back(&item);
			HtmlItems(html, items);
		}
		html += L"</body>\n</html>\n";
		return html;
	}

	Helpers::TableSource ToTable(std::vector<Section> const& sections) {
		std::vector<std::vector<CString>> rows;
		std::function<void(CString const&, Item const&, int)> add = [&](CString const& section, Item const& item, int level) {
			CString indent;
			for (int i = 0; i < level; i++)
				indent += L"    ";
			rows.push_back({ section, indent + item.Text });
			for (auto const& child : item.Children)
				add(section, child, level + 1);
		};
		for (auto const& section : sections)
			for (auto const& item : section.Items)
				add(section.Title, item, 0);
		return Printing::TableFromRows({ L"Section", L"Item" }, std::move(rows));
	}
}
