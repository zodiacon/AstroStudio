#include "pch.h"
#include "CairoTypes.h"

char const* StatusToString(CairoStatus status) {
	return cairo_status_to_string(static_cast<cairo_status_t>(status));
}
