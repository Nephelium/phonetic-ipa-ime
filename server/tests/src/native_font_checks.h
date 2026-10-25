#pragma once
// Exercise actual DirectWrite mappings using the bundled collections.
#include <dwrite_2.h>
#include <cwchar>
#include <wrl/client.h>
#define FONT_REQUIRE(expression) Require((expression), #expression)
namespace
{
using Microsoft::WRL::ComPtr;
class TextSource : public IDWriteTextAnalysisSource
{
  public:
    explicit TextSource(const wchar_t *text) : text_(text)
    {
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **object) override
    {
        *object = nullptr;
        if (iid != __uuidof(IUnknown) && iid != __uuidof(IDWriteTextAnalysisSource))
            return E_NOINTERFACE;
        *object = this;
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return 1;
    }
    ULONG STDMETHODCALLTYPE Release() override
    {
        return 1;
    }
    HRESULT STDMETHODCALLTYPE GetTextAtPosition(UINT32 position, const WCHAR **text, UINT32 *length) override
    {
        *length = position < text_.size() ? static_cast<UINT32>(text_.size()) - position : 0;
        *text = *length ? text_.c_str() + position : nullptr;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetTextBeforePosition(UINT32 position, const WCHAR **text, UINT32 *length) override
    {
        *length = position <= text_.size() ? position : 0;
        *text = *length ? text_.c_str() : nullptr;
        return S_OK;
    }
    DWRITE_READING_DIRECTION STDMETHODCALLTYPE GetParagraphReadingDirection() override
    {
        return DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
    }
    HRESULT STDMETHODCALLTYPE GetLocaleName(UINT32 position, UINT32 *length, const WCHAR **name) override
    {
        *length = static_cast<UINT32>(text_.size()) - position;
        *name = L"en-us";
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetNumberSubstitution(UINT32 position, UINT32 *length,
                                                    IDWriteNumberSubstitution **substitution) override
    {
        *length = static_cast<UINT32>(text_.size()) - position;
        *substitution = nullptr;
        return S_OK;
    }

  private:
    std::wstring text_;
};

std::wstring MappedIpaFamily(const wchar_t *text, const std::vector<std::wstring> &families)
{
    ComPtr<IDWriteFactory> factory;
    FONT_REQUIRE(SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                               reinterpret_cast<IUnknown **>(factory.GetAddressOf()))));
    ComPtr<IDWriteTextFormat> format;
    FONT_REQUIRE(SUCCEEDED(factory->CreateTextFormat(L"Doulos SIL", msimeui::PrivateIpaFontCollection(L"Doulos SIL"),
                                                     DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                                                     DWRITE_FONT_STRETCH_NORMAL, 16, L"en-us", &format)));
    if (families.empty())
        msimeui::ApplyPrivateFontFallback(factory.Get(), format.Get());
    else
        msimeui::ApplyFontFallback(factory.Get(), format.Get(), families);
    ComPtr<IDWriteTextFormat1> format1;
    FONT_REQUIRE(SUCCEEDED(format.As(&format1)));
    ComPtr<IDWriteFontFallback> fallback;
    FONT_REQUIRE(SUCCEEDED(format1->GetFontFallback(&fallback)));
    FONT_REQUIRE(fallback != nullptr);
    TextSource source(text);
    UINT32 mapped = 0;
    FLOAT scale = 0;
    ComPtr<IDWriteFont> font;
    FONT_REQUIRE(SUCCEEDED(fallback->MapCharacters(
        &source, 0, static_cast<UINT32>(wcslen(text)), msimeui::PrivateIpaFontCollection(L"Doulos SIL"), L"Doulos SIL",
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, &mapped, &font, &scale)));
    FONT_REQUIRE(mapped > 0);
    FONT_REQUIRE(font != nullptr);
    ComPtr<IDWriteFontFamily> family;
    FONT_REQUIRE(SUCCEEDED(font->GetFontFamily(&family)));
    ComPtr<IDWriteLocalizedStrings> names;
    FONT_REQUIRE(SUCCEEDED(family->GetFamilyNames(&names)));
    UINT32 index = 0;
    BOOL exists = FALSE;
    names->FindLocaleName(L"en-us", &index, &exists);
    if (!exists)
        index = 0;
    WCHAR name[256]{};
    FONT_REQUIRE(SUCCEEDED(names->GetString(index, name, 256)));
    return name;
}
class GlyphCounter : public IDWriteTextRenderer
{
  public:
    UINT32 count = 0;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **p) override
    {
        *p = nullptr;
        if (iid != __uuidof(IUnknown) && iid != __uuidof(IDWritePixelSnapping) && iid != __uuidof(IDWriteTextRenderer))
            return E_NOINTERFACE;
        *p = this;
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return 1;
    }
    ULONG STDMETHODCALLTYPE Release() override
    {
        return 1;
    }
    HRESULT STDMETHODCALLTYPE IsPixelSnappingDisabled(void *, BOOL *disabled) override
    {
        *disabled = TRUE;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetCurrentTransform(void *, DWRITE_MATRIX *m) override
    {
        *m = {1, 0, 0, 1, 0, 0};
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetPixelsPerDip(void *, FLOAT *v) override
    {
        *v = 1;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawGlyphRun(void *, FLOAT, FLOAT, DWRITE_MEASURING_MODE, const DWRITE_GLYPH_RUN *run,
                                           const DWRITE_GLYPH_RUN_DESCRIPTION *, IUnknown *) override
    {
        count += run->glyphCount;
        for (UINT32 i = 0; i < run->glyphCount; ++i)
            if (!run->glyphIndices[i])
                return E_FAIL;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawUnderline(void *, FLOAT, FLOAT, const DWRITE_UNDERLINE *, IUnknown *) override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawStrikethrough(void *, FLOAT, FLOAT, const DWRITE_STRIKETHROUGH *, IUnknown *) override
    {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawInlineObject(void *, FLOAT, FLOAT, IDWriteInlineObject *, BOOL, BOOL,
                                               IUnknown *) override
    {
        return E_FAIL;
    }
};
void CheckEnclosedGlyphs()
{
    ComPtr<IDWriteFactory> factory;
    FONT_REQUIRE(SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                               reinterpret_cast<IUnknown **>(factory.GetAddressOf()))));
    ComPtr<IDWriteTextFormat> format;
    FONT_REQUIRE(SUCCEEDED(factory->CreateTextFormat(
        L"PTB IPA Plus", msimeui::PrivateIpaFontCollection(L"PTB IPA Plus"), DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 32, L"en-us", &format)));
    for (const wchar_t *text : {L"C⃝", L"Ȼ⃝", L"F⃝", L"L⃝", L"G⃝", L"N⃝", L"P⃝", L"R⃝", L"S⃝", L"T⃝", L"Ṽ⃝", L"Ʞ⃝", L"σ⃝"})
    {
        ComPtr<IDWriteTextLayout> layout;
        FONT_REQUIRE(SUCCEEDED(
            factory->CreateTextLayout(text, static_cast<UINT32>(wcslen(text)), format.Get(), 200, 80, &layout)));
        GlyphCounter renderer;
        FONT_REQUIRE(SUCCEEDED(layout->Draw(nullptr, &renderer, 0, 0)));
        FONT_REQUIRE(renderer.count == 1);
    }
}

} // namespace

#undef FONT_REQUIRE
