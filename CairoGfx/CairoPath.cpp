#include "pch.h"
#include "CairoPath.h"
#include <assert.h>

CairoPath::CairoPath(CairoPath&& other) : m_path(other.m_path) {
	other.m_path = nullptr;
}

CairoPath& CairoPath::operator=(CairoPath&& other) {
	if (&other != this) {
		Destroy();
		m_path = other.m_path;
		other.m_path = nullptr;
	}
	return *this;
}

void CairoPath::Destroy() {
	if (m_path) {
		cairo_path_destroy(m_path);
		m_path = nullptr;
	}
}

CairoPath::~CairoPath() {
	Destroy();
}

CairoPath::operator const cairo_path* () const {
	return m_path;
}

CairoPath::CairoPath(cairo_path* path) : m_path(path) {
	assert(m_path);
}
