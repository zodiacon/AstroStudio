#pragma once

#include "D2DChartDrawing.h"
#include <wincodec.h>

// The chart wheel as a picture: drawn with the same D2DChartDrawing as on screen, but into a WIC bitmap, so it can be
// written to a file or put on the clipboard at any size. All of these are for the UI thread (COM and the D2D factory).
namespace ChartImage {
	// Side of the square pictures the views make, in pixels.
	constexpr int DefaultSize = 1600;

	// Draws the chart into a new size x size bitmap. Null if Direct2D or WIC couldn't be set up.
	CComPtr<IWICBitmap> Render(D2DChartDrawing& drawing, int size);

	// Writes the bitmap to a PNG file. On failure, error says why.
	bool SavePng(IWICBitmap* bitmap, PCWSTR path, std::wstring& error);

	// The bitmap as a packed device independent bitmap (a header and the pixels, bottom-up, 32 bits): what Printing draws with
	// StretchDIBits. Empty if it can't be read.
	std::vector<BYTE> ToDib(IWICBitmap* bitmap);

	// Puts the bitmap on the clipboard as a device independent bitmap (which every program that pastes pictures takes).
	bool CopyToClipboard(HWND owner, IWICBitmap* bitmap);
}
