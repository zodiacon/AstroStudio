#pragma once

struct cairo_path;

class CairoPath final {
public:
	CairoPath(CairoPath const&) = delete;
	CairoPath& operator=(CairoPath const&) = delete;
	CairoPath(CairoPath&& other);
	CairoPath& operator=(CairoPath&& other);

	void Destroy();
	~CairoPath();

	operator const cairo_path* () const;

private:
	explicit CairoPath(cairo_path* path);

	cairo_path* m_path;
	friend class CairoCtx;
};

