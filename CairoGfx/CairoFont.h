#pragma once

typedef struct _cairo_font_face cairo_font_face_t;

class CairoFont final {
public:
	explicit CairoFont(LOGFONT const& lf);
	explicit CairoFont(HFONT hFont);
	explicit CairoFont(wchar_t const* family, bool bold = false, bool italics = false, bool underline = false);

	CairoFont(CairoFont const&) = delete;
	CairoFont& operator=(CairoFont const&) = delete;
	CairoFont(CairoFont&& other);
	CairoFont& operator=(CairoFont&& other);

	void Destroy();
	~CairoFont();

	operator cairo_font_face_t* () const;

private:
	cairo_font_face_t* m_font;
};

