#include "pch.h"
#include "ToolbarStrip.h"
#include "Helpers.h"
#include <IconHelper.h>

HWND ToolbarStrip::Create(HWND parent, ToolBarButtonInfo const* buttons, int count, int size) {
	m_Buttons.assign(buttons, buttons + count);
	m_Size = size;
	m_Toolbar = ToolbarHelper::CreateAndInitToolBar(parent, buttons, count, size);
	m_Dpi = DpiHelper::GetDpi(m_Toolbar);
	Helpers::GuiFont(m_Font, m_Dpi);
	return m_Toolbar;
}

int ToolbarStrip::Scale(int value) const noexcept {
	return DpiHelper::Scale(value, m_Dpi);
}

int ToolbarStrip::IconSize() const noexcept {
	// the size ToolbarHelper makes the image list in
	return Scale(m_Size);
}

int ToolbarStrip::AddButton(UINT id, UINT icon, BYTE style, PCWSTR text) {
	CImageList images = m_Toolbar.GetImageList();
	int image = images.AddIcon(CIcon(IconHelper::Load((int)icon, IconSize())));
	m_Extras.push_back({ icon });
	m_Toolbar.AddButton(id, style, TBSTATE_ENABLED, image, text, 0);
	return m_Toolbar.GetButtonCount() - 1;
}

int ToolbarStrip::AddSeparator(int width) {
	m_Toolbar.AddSeparator(Scale(width));
	int index = m_Toolbar.GetButtonCount() - 1;
	m_Separators.push_back({ index, width });
	return index;
}

void ToolbarStrip::Place(HWND child, int index, int height, int drop, int inset) {
	m_Children.push_back({ child, index, height, drop, inset });
	::SendMessage(child, WM_SETFONT, (WPARAM)m_Font.m_hFont, FALSE);
	PlaceChild(m_Children.back());
}

void ToolbarStrip::PlaceChild(Child const& child) const {
	CRect item;
	m_Toolbar.GetItemRect(child.Index, &item);
	int height = child.Height ? Scale(child.Height) : item.Height();
	int top = item.top + (item.Height() - height) / 2;
	::SetWindowPos(child.Window, nullptr, item.left, top, item.Width() - Scale(child.Inset), height + Scale(child.Drop), SWP_NOZORDER | SWP_NOACTIVATE);
}

bool ToolbarStrip::Update() {
	UINT dpi = DpiHelper::GetDpi(m_Toolbar);
	if (dpi == m_Dpi)
		return false;
	m_Dpi = dpi;

	// the images of the table's buttons, then those added after them, in the same order (so that the indexes stay)
	ToolbarHelper::UpdateToolBarImages(m_Toolbar, m_Buttons.data(), (int)m_Buttons.size(), m_Size);
	CImageList images = m_Toolbar.GetImageList();
	for (auto const& extra : m_Extras)
		images.AddIcon(CIcon(IconHelper::Load((int)extra.Icon, IconSize())));

	// the buttons are measured again from the new images and the font of the new DPI
	m_Toolbar.SetButtonSize(0, 0);
	// the text of the buttons is measured again (a tool bar keeps the widths of its texts in the font of the DPI they were set at,
	// so that a tool bar made at 96 DPI and moved to 120 cut its texts short: "N...")
	CFont old(m_TextFont.Detach());		// (deleted when the tool bar has the new one)
	m_TextFont.Attach(DpiHelper::CreateControlFont(m_Dpi));		// (the font a tool bar has of itself: the icon title font)
	m_Toolbar.SetFont(m_TextFont);
	// (a new font isn't enough: a button's text is measured when the text is set)
	for (int i = 0; i < m_Toolbar.GetButtonCount(); i++) {
		WCHAR text[128]{};
		TBBUTTONINFO info{ sizeof(info), TBIF_BYINDEX | TBIF_TEXT | TBIF_STYLE };
		info.pszText = text;
		info.cchText = _countof(text);
		if (m_Toolbar.GetButtonInfo(i, &info) < 0 || (info.fsStyle & BTNS_SEP) || text[0] == 0)
			continue;
		info.dwMask = TBIF_BYINDEX | TBIF_TEXT;
		m_Toolbar.SetButtonInfo(i, &info);
	}
	for (auto const& separator : m_Separators) {
		TBBUTTONINFO info{ sizeof(info), TBIF_BYINDEX | TBIF_SIZE };
		info.cx = static_cast<WORD>(Scale(separator.Width));
		m_Toolbar.SetButtonInfo(separator.Index, &info);
	}
	m_Toolbar.AutoSize();

	Helpers::GuiFont(m_Font, m_Dpi);
	for (auto const& child : m_Children) {
		::SendMessage(child.Window, WM_SETFONT, (WPARAM)m_Font.m_hFont, TRUE);
		PlaceChild(child);
	}
	FitBand();
	return true;
}

void ToolbarStrip::FitBand() const {
	HWND parent = m_Toolbar.GetParent();
	WCHAR name[32]{};
	if (::GetClassName(parent, name, _countof(name)) && _wcsicmp(name, REBARCLASSNAME) == 0)
		Helpers::FitReBarBand(parent, m_Toolbar);
}
