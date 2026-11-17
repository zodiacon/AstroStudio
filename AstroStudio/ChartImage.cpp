#include "pch.h"
#include "ChartImage.h"

#pragma comment(lib, "windowscodecs.lib")

namespace {
	std::wstring Describe(HRESULT hr) {
		wchar_t text[256]{};
		if (!::FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, hr, 0, text, _countof(text), nullptr))
			swprintf_s(text, L"error 0x%08X", (unsigned)hr);
		std::wstring result(text);
		while (!result.empty() && (result.back() == L'\r' || result.back() == L'\n' || result.back() == L' '))
			result.pop_back();
		return result;
	}
}

CComPtr<IWICBitmap> ChartImage::Render(D2DChartDrawing& drawing, int size) {
	auto& resources = D2DResources::Get();
	if (FAILED(resources.Ensure()))
		return nullptr;

	CComPtr<IWICImagingFactory> wic;
	if (FAILED(wic.CoCreateInstance(CLSID_WICImagingFactory)))
		return nullptr;

	CComPtr<IWICBitmap> bitmap;
	if (FAILED(wic->CreateBitmap(size, size, GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &bitmap)))
		return nullptr;

	// 96 DPI, like the window render targets: a DIP is a pixel
	auto props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
		D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED), 96, 96);
	CComPtr<ID2D1RenderTarget> rt;
	if (FAILED(resources.Factory()->CreateWicBitmapRenderTarget(bitmap, props, &rt)))
		return nullptr;

	rt->BeginDraw();
	rt->Clear(D2D1::ColorF(D2D1::ColorF::White));
	drawing.Draw(rt, (float)size);
	if (FAILED(rt->EndDraw()))
		return nullptr;
	return bitmap;
}

namespace {
	// the bitmap as PNG into a stream
	HRESULT WritePng(IWICImagingFactory* wic, IStream* stream, IWICBitmap* bitmap) {
		CComPtr<IWICBitmapEncoder> encoder;
		if (HRESULT hr = wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder); FAILED(hr))
			return hr;
		if (HRESULT hr = encoder->Initialize(stream, WICBitmapEncoderNoCache); FAILED(hr))
			return hr;

		CComPtr<IWICBitmapFrameEncode> frame;
		CComPtr<IPropertyBag2> options;
		if (HRESULT hr = encoder->CreateNewFrame(&frame, &options); FAILED(hr))
			return hr;
		if (HRESULT hr = frame->Initialize(options); FAILED(hr))
			return hr;

		UINT width, height;
		bitmap->GetSize(&width, &height);
		if (HRESULT hr = frame->SetSize(width, height); FAILED(hr))
			return hr;
		WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
		if (HRESULT hr = frame->SetPixelFormat(&format); FAILED(hr))
			return hr;
		// the picture is premultiplied BGRA; WriteSource converts it to the format the encoder settled on
		if (HRESULT hr = frame->WriteSource(bitmap, nullptr); FAILED(hr))
			return hr;
		if (HRESULT hr = frame->Commit(); FAILED(hr))
			return hr;
		return encoder->Commit();
	}
}

bool ChartImage::SavePng(IWICBitmap* bitmap, PCWSTR path, std::wstring& error) {
	auto fail = [&](HRESULT hr) {
		error = Describe(hr);
		return false;
	};

	CComPtr<IWICImagingFactory> wic;
	if (HRESULT hr = wic.CoCreateInstance(CLSID_WICImagingFactory); FAILED(hr))
		return fail(hr);

	CComPtr<IWICStream> stream;
	if (HRESULT hr = wic->CreateStream(&stream); FAILED(hr))
		return fail(hr);
	if (HRESULT hr = stream->InitializeFromFilename(path, GENERIC_WRITE); FAILED(hr))
		return fail(hr);
	if (HRESULT hr = WritePng(wic, stream, bitmap); FAILED(hr))
		return fail(hr);
	return true;
}

std::vector<BYTE> ChartImage::EncodePng(IWICBitmap* bitmap) {
	std::vector<BYTE> result;
	CComPtr<IWICImagingFactory> wic;
	CComPtr<IStream> stream;
	if (!bitmap || FAILED(wic.CoCreateInstance(CLSID_WICImagingFactory)) || FAILED(::CreateStreamOnHGlobal(nullptr, TRUE, &stream)))
		return result;
	if (FAILED(WritePng(wic, stream, bitmap)))
		return result;
	HGLOBAL memory{};
	if (FAILED(::GetHGlobalFromStream(stream, &memory)))
		return result;
	STATSTG stat{};
	if (FAILED(stream->Stat(&stat, STATFLAG_NONAME)))
		return result;
	if (auto data = static_cast<BYTE const*>(::GlobalLock(memory))) {
		result.assign(data, data + static_cast<size_t>(stat.cbSize.QuadPart));
		::GlobalUnlock(memory);
	}
	return result;
}

bool ChartImage::CopyToClipboard(HWND owner, IWICBitmap* bitmap) {
	UINT width, height;
	bitmap->GetSize(&width, &height);

	CComPtr<IWICBitmapLock> lock;
	WICRect all{ 0, 0, (INT)width, (INT)height };
	if (FAILED(bitmap->Lock(&all, WICBitmapLockRead, &lock)))
		return false;
	UINT stride = 0, bytes = 0;
	BYTE* pixels = nullptr;
	if (FAILED(lock->GetStride(&stride)) || FAILED(lock->GetDataPointer(&bytes, &pixels)))
		return false;

	const size_t rowBytes = (size_t)width * 4;
	HGLOBAL mem = ::GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + rowBytes * height);
	if (!mem)
		return false;
	auto block = static_cast<BYTE*>(::GlobalLock(mem));
	auto header = reinterpret_cast<BITMAPINFOHEADER*>(block);
	*header = {};
	header->biSize = sizeof(BITMAPINFOHEADER);
	header->biWidth = (LONG)width;
	header->biHeight = (LONG)height;		// positive: the rows are stored bottom-up
	header->biPlanes = 1;
	header->biBitCount = 32;
	header->biCompression = BI_RGB;
	header->biSizeImage = (DWORD)(rowBytes * height);
	auto dest = block + sizeof(BITMAPINFOHEADER);
	for (UINT y = 0; y < height; y++)
		memcpy(dest + rowBytes * (height - 1 - y), pixels + (size_t)stride * y, rowBytes);
	::GlobalUnlock(mem);

	if (!::OpenClipboard(owner)) {
		::GlobalFree(mem);
		return false;
	}
	::EmptyClipboard();
	bool ok = ::SetClipboardData(CF_DIB, mem) != nullptr;
	::CloseClipboard();
	if (!ok)
		::GlobalFree(mem);		// the clipboard owns it only if it took it
	return ok;
}

std::vector<BYTE> ChartImage::ToDib(IWICBitmap* bitmap) {
	UINT width, height;
	bitmap->GetSize(&width, &height);
	CComPtr<IWICBitmapLock> lock;
	WICRect all{ 0, 0, (INT)width, (INT)height };
	if (FAILED(bitmap->Lock(&all, WICBitmapLockRead, &lock)))
		return {};
	UINT stride = 0, bytes = 0;
	BYTE* pixels = nullptr;
	if (FAILED(lock->GetStride(&stride)) || FAILED(lock->GetDataPointer(&bytes, &pixels)))
		return {};

	const size_t rowBytes = (size_t)width * 4;
	std::vector<BYTE> dib(sizeof(BITMAPINFOHEADER) + rowBytes * height);
	auto header = reinterpret_cast<BITMAPINFOHEADER*>(dib.data());
	*header = {};
	header->biSize = sizeof(BITMAPINFOHEADER);
	header->biWidth = (LONG)width;
	header->biHeight = (LONG)height;		// the rows are stored bottom-up
	header->biPlanes = 1;
	header->biBitCount = 32;
	header->biCompression = BI_RGB;
	header->biSizeImage = (DWORD)(rowBytes * height);
	auto dest = dib.data() + sizeof(BITMAPINFOHEADER);
	for (UINT y = 0; y < height; y++)
		memcpy(dest + rowBytes * (height - 1 - y), pixels + (size_t)stride * y, rowBytes);
	return dib;
}
