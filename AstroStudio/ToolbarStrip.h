#pragma once

#include <ToolbarHelper.h>

//
// A view's tool bar (made by ToolbarHelper) and what the view put on it afterwards: buttons with icons of their own, separators
// of a given width and the child windows sitting on them (labels, combo boxes, a text box). Every size is at 96 DPI, scaled to
// the tool bar's DPI; when that changes (the window moved to a monitor of another DPI), Update makes it all again for the new
// DPI: the button images, the separators' widths, the children's places and their font, and the rebar band's size.
//
class ToolbarStrip {
public:
	// the tool bar, a child of parent (a view's rebar, or the view itself); size: of the button images at 96 DPI
	HWND Create(HWND parent, ToolBarButtonInfo const* buttons, int count, int size = 24);

	HWND Toolbar() const noexcept {
		return m_Toolbar;
	}
	operator HWND() const noexcept {
		return m_Toolbar;
	}
	UINT Dpi() const noexcept {
		return m_Dpi;
	}
	// a size at 96 DPI for the tool bar's DPI
	int Scale(int value) const noexcept;

	// a button with an icon of the app's, after those of the table; returns the button's index
	int AddButton(UINT id, UINT icon, BYTE style, PCWSTR text);
	// a separator width wide (at 96 DPI); returns its index, for Place
	int AddSeparator(int width);
	// a child window on the separator at index: height high (at 96 DPI, centred in the tool bar's height; 0: as tall as the
	// separator), with drop more below it (a combo box's list), and inset short of the separator's right end
	void Place(HWND child, int index, int height = 22, int drop = 0, int inset = 4);
	// the font of the children placed (DEFAULT_GUI_FONT at the tool bar's DPI); Place sets it
	HFONT Font() const noexcept {
		return m_Font;
	}

	// after a change of DPI (WM_DPICHANGED_AFTERPARENT of the view): everything again for the tool bar's DPI now.
	// Returns false if the DPI didn't change
	bool Update();
	// the rebar band holding the tool bar sized again to it (Update does it)
	void FitBand() const;

private:
	struct Extra {
		UINT Icon;
	};
	struct Separator {
		int Index, Width;
	};
	struct Child {
		HWND Window;
		int Index, Height, Drop, Inset;
	};
	void PlaceChild(Child const& child) const;
	int IconSize() const noexcept;

	CToolBarCtrl m_Toolbar;
	std::vector<ToolBarButtonInfo> m_Buttons;
	std::vector<Extra> m_Extras;
	std::vector<Separator> m_Separators;
	std::vector<Child> m_Children;
	CFont m_Font;
	CFont m_TextFont;	// the buttons' text, after a change of DPI
	int m_Size{ 24 };
	UINT m_Dpi{ 96 };
};
