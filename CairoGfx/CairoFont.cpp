#include "pch.h"
#include "CairoFont.h"

CairoFont::CairoFont(LOGFONT const& lf) {
	m_font = cairo_win32_font_face_create_for_logfontw((LOGFONT*)&lf);
}

CairoFont::CairoFont(HFONT hFont) {
	m_font = cairo_win32_font_face_create_for_hfont(hFont);
}

CairoFont::CairoFont(wchar_t const* family, bool bold, bool italics, bool underline) {
	LOGFONT logFont {};
	logFont.lfCharSet = DEFAULT_CHARSET;
	logFont.lfHeight = 20;
	wcscpy_s(logFont.lfFaceName, family);

	if (bold)
		logFont.lfWeight = FW_BOLD;
	if (italics)
		logFont.lfItalic = TRUE;
	m_font = cairo_win32_font_face_create_for_logfontw(&logFont);
}

CairoFont::CairoFont(CairoFont&& other) : m_font(other.m_font) {
	other.m_font = nullptr;
}

CairoFont& CairoFont::operator=(CairoFont&& other) {
	if (&other != this) {
		Destroy();
		m_font = other.m_font;
		other.m_font = nullptr;
	}
	return *this;
}

void CairoFont::Destroy() {
	if (m_font) {
		cairo_font_face_destroy(m_font);
		m_font = nullptr;
	}
}

CairoFont::~CairoFont() {
	Destroy();
}

CairoFont::operator cairo_font_face_t* () const {
	return m_font;
}
