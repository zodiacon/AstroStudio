#pragma once

enum class CairoFormat {
    Invalid = -1,
    ARGB32 = 0,
    RGB24 = 1,
    A8 = 2,
    A1 = 3,
    RGB16_565 = 4,
    RGB30 = 5
};

enum class CairoContent {
	Color = 0x1000,
	Alpha = 0x2000,
	ColorAlpha = 0x3000
};

enum class CairoStatus {
    Success = 0,

    No_memory,
    Invalid_restore,
    Invalid_pop_group,
    No_current_point,
    Invalid_matrix,
    Invalid_status,
    Null_pointer,
    Invalid_string,
    Invalid_path_data,
    Read_error,
    Write_error,
    Surface_finished,
    Surface_type_mismatch,
    Pattern_type_mismatch,
    Invalid_content,
    Invalid_format,
    Invalid_visual,
    File_not_found,
    Invalid_dash,
    Invalid_dsc_comment,
    Invalid_index,
    Clip_not_representable,
    Temp_file_error,
    Invalid_stride,
    Font_type_mismatch,
    User_font_immutable,
    User_font_error,
    Negative_count,
    Invalid_clusters,
    Invalid_slant,
    Invalid_weight,
    Invalid_size,
    User_font_not_implemented,
    Device_type_mismatch,
    Device_error,
    Invalid_mesh_construction,
    Device_finished,
    Jbig2_global_missing,
    Png_error,
    Freetype_error,
    Win32_gdi_error,
    Tag_error,

    Last_status
};

struct CairoTextExtents {
    double x_bearing;
    double y_bearing;
    double width;
    double height;
    double x_advance;
    double y_advance;
};

template<typename T>
struct CairoPointT final {
    CairoPointT() = default;
    CairoPointT(T x, T y) : X(x), Y(y) {}

    CairoPointT& Offset(T dx, T dy) {
        X += dx; Y += dy;
        return *this;
    }

    T X, Y;
};

using CairoPoint = CairoPointT<double>;
using CairoPointI = CairoPointT<int>;

template<typename T>
struct CairoRectT final {
    CairoRectT(T x, T y, T width, T height) : X(x), Y(y), Width(width), Height(height) {}
	CairoRectT() = default;

	T Right() const {
		return X + Width;
	}

	T Bottom() const {
		return Y + Height;
	}

    CairoPointT<T> CenterPoint() const {
        return CairoPointT(X + Width / 2, Y + Height / 2);
    }

    CairoRectT& Shrink(T dx, T dy) {
        X += dx; Y += dy;
        Width -= 2 * dx;
        Height -= 2 * dy;
        return *this;
    }

	T X, Y, Width, Height;
};

using CairoRect = CairoRectT<double>;
using CairoRectI = CairoRectT<int>;


constexpr double pi = 3.14159265358979323846264338795;

constexpr double Rad(double angle) {
	return angle * pi / 180;
}

constexpr double Deg(double angle) {
	return angle * 180 / pi;
}

char const* StatusToString(CairoStatus status);
