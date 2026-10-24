#pragma once

#include "ipa/ipa_profile.h"
#include "ipa/ipa_runtime.h"
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#pragma comment(lib, "windowscodecs.lib")

namespace PhoneticIpa::AppIcon
{
inline std::filesystem::path Current(const std::filesystem::path &root = ResourceRoot())
{
    std::ifstream input(root / "custom-icon-name.txt", std::ios::binary);
    char buffer[128]{};
    input.read(buffer, sizeof(buffer));
    if (input.gcount() == sizeof(buffer))
        return {};
    std::string name(buffer, static_cast<size_t>(input.gcount()));
    if (name.rfind("custom-icon-", 0) != 0 || name.size() < 4 || name.substr(name.size() - 4) != ".ico" ||
        name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-.") != std::string::npos)
        return {};
    const auto path = root / name;
    std::error_code error;
    return std::filesystem::is_regular_file(path, error) ? path : std::filesystem::path{};
}

inline std::wstring AssetOr(const std::wstring &fallback)
{
    const auto path = Current();
    return path.empty() ? fallback : path.wstring();
}

inline void Reset(const std::filesystem::path &root = ResourceRoot())
{
    // Keep imported files so restoring the default never deletes a user image.
    WriteProfileFile(root / "custom-icon-name.txt", "");
}

inline void Check(HRESULT value)
{
    if (FAILED(value))
        throw std::runtime_error("无法读取这张图片，请选择 PNG、JPEG 或 BMP 图片。");
}

inline void Import(const std::filesystem::path &source, const std::filesystem::path &root = ResourceRoot())
{
    if (std::filesystem::file_size(source) > 10 * 1024 * 1024)
        throw std::runtime_error("图片最大支持 10 MB。");
    using Microsoft::WRL::ComPtr;
    ComPtr<IWICImagingFactory> factory;
    Check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)));
    ComPtr<IWICBitmapDecoder> decoder;
    Check(factory->CreateDecoderFromFilename(source.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad,
                                             &decoder));
    GUID format{};
    Check(decoder->GetContainerFormat(&format));
    if (format != GUID_ContainerFormatPng && format != GUID_ContainerFormatJpeg && format != GUID_ContainerFormatBmp)
        throw std::runtime_error("请选择 PNG、JPEG 或 BMP 图片。");
    ComPtr<IWICBitmapFrameDecode> frame;
    Check(decoder->GetFrame(0, &frame));
    UINT width = 0, height = 0;
    Check(frame->GetSize(&width, &height));
    if (!width || !height || width > 8192 || height > 8192 || uint64_t(width) * height > 32000000)
        throw std::runtime_error("图片尺寸过大，请使用不超过 3200 万像素的图片。");
    const UINT edge = (std::min)(width, height);
    WICRect crop{static_cast<INT>((width - edge) / 2), static_cast<INT>((height - edge) / 2), static_cast<INT>(edge),
                 static_cast<INT>(edge)};
    ComPtr<IWICBitmapClipper> clipper;
    Check(factory->CreateBitmapClipper(&clipper));
    Check(clipper->Initialize(frame.Get(), &crop));
    ComPtr<IWICBitmapScaler> scaler;
    Check(factory->CreateBitmapScaler(&scaler));
    Check(scaler->Initialize(clipper.Get(), 256, 256, WICBitmapInterpolationModeFant));
    ComPtr<IWICFormatConverter> converter;
    Check(factory->CreateFormatConverter(&converter));
    Check(converter->Initialize(scaler.Get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0,
                                WICBitmapPaletteTypeCustom));
    std::vector<BYTE> pixels(256 * 256 * 4);
    Check(converter->CopyPixels(nullptr, 256 * 4, static_cast<UINT>(pixels.size()), pixels.data()));
    std::string ico;
    const auto append = [&ico](uint32_t value, int bytes) {
        for (int i = 0; i < bytes; ++i)
            ico.push_back(static_cast<char>((value >> (i * 8)) & 255));
    };
    append(0, 2);
    append(1, 2);
    append(1, 2);
    append(0, 4);
    append(1, 2);
    append(32, 2);
    append(40 + static_cast<uint32_t>(pixels.size()) + 256 * 32, 4);
    append(22, 4);
    append(40, 4);
    append(256, 4);
    append(512, 4);
    append(1, 2);
    append(32, 2);
    append(0, 4);
    append(static_cast<uint32_t>(pixels.size()), 4);
    for (int i = 0; i < 4; ++i)
        append(0, 4);
    for (int row = 255; row >= 0; --row)
        ico.append(reinterpret_cast<const char *>(pixels.data() + row * 256 * 4), 256 * 4);
    ico.append(256 * 32, '\0');
    FILETIME time{};
    GetSystemTimeAsFileTime(&time);
    const uint64_t stamp = (uint64_t(time.dwHighDateTime) << 32) | time.dwLowDateTime;
    const auto name = "custom-icon-" + std::to_string(stamp) + "-" + std::to_string(GetCurrentProcessId()) + ".ico";
    const auto destination = root / name;
    WriteProfileFile(destination, ico);
    HICON decoded = static_cast<HICON>(LoadImageW(nullptr, destination.c_str(), IMAGE_ICON, 32, 32, LR_LOADFROMFILE));
    if (!decoded)
        throw std::runtime_error("图片转换失败，请选择另一张图片。");
    DestroyIcon(decoded);
    WriteProfileFile(root / "custom-icon-name.txt", name);
}

inline void Refresh(HWND hwnd, int defaultResource)
{
    static std::wstring previous;
    static HICON iconBig = nullptr, iconSmall = nullptr;
    static HWND owner = nullptr;
    const auto path = Current();
    const UINT dpi = GetDpiForWindow(hwnd);
    const auto signature = path.wstring() + std::to_wstring(dpi);
    if (owner == hwnd && previous == signature)
        return;
    const auto load = [&](int size) {
        HICON icon = static_cast<HICON>(
            path.empty() ? nullptr : LoadImageW(nullptr, path.c_str(), IMAGE_ICON, size, size, LR_LOADFROMFILE));
        return icon ? icon : CopyIcon(LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(defaultResource)));
    };
    HICON nextBig = load(GetSystemMetricsForDpi(SM_CXICON, dpi));
    HICON nextSmall = load(GetSystemMetricsForDpi(SM_CXSMICON, dpi));
    SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(nextBig));
    SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(nextSmall));
    if (iconBig)
        DestroyIcon(iconBig);
    if (iconSmall)
        DestroyIcon(iconSmall);
    iconBig = nextBig;
    iconSmall = nextSmall;
    owner = hwnd;
    previous = signature;
}
} // namespace PhoneticIpa::AppIcon
