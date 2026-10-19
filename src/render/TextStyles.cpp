#include "TextStyles.h"

#include "../app/Errors.h"

#include <iterator>
#include <utility>
#include <cwctype>

namespace caecilia {
namespace {

using Microsoft::WRL::ComPtr;
namespace Spec = NotchIsland;

ComPtr<IDWriteTextFormat> CreateFormat(
    IDWriteFactory* factory,
    IDWriteFontCollection* collection,
    const Spec::Typography::Style& style)
{
    const wchar_t* familyName = style.family;
    UINT32 familyIndex = 0;
    BOOL exists = FALSE;
    Check(collection->FindFamilyName(familyName, &familyIndex, &exists),
        "Find preferred font family");
    if (!exists) {
        familyName = Spec::Typography::FallbackFamily;
        Check(collection->FindFamilyName(familyName, &familyIndex, &exists),
            "Find fallback font family");
    }
    Check(exists ? S_OK : DWRITE_E_NOFONT, "Required system font unavailable");

    ComPtr<IDWriteFontFamily> family;
    Check(collection->GetFontFamily(familyIndex, family.GetAddressOf()),
        "Get system font family");
    ComPtr<IDWriteFont> font;
    Check(family->GetFirstMatchingFont(style.weight, DWRITE_FONT_STRETCH_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, font.GetAddressOf()), "Match system font");
    DWRITE_FONT_METRICS metrics{};
    font->GetMetrics(&metrics); // DirectWrite returns void.
    Check(metrics.designUnitsPerEm != 0 ? S_OK : E_UNEXPECTED,
        "Validate font design units");

    // Center the actual ascent/descent box in the specified uniform line box.
    // No guessed baseline and no extra font lineGap: lineHeight is authoritative.
    const float scale = style.size / static_cast<float>(metrics.designUnitsPerEm);
    const float ascent = static_cast<float>(metrics.ascent) * scale;
    const float descent = static_cast<float>(metrics.descent) * scale;
    const float baseline = ascent +
        (style.lineHeight - ascent - descent) * Spec::WindowSpec::Half;

    ComPtr<IDWriteTextFormat> format;
    Check(factory->CreateTextFormat(familyName, collection, style.weight,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, style.size,
        Spec::Typography::Locale, format.GetAddressOf()), "Create text format");
    Check(format->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM,
        style.lineHeight, baseline), "Set exact text line height");
    Check(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP), "Disable text wrapping");
    Check(format->SetReadingDirection(DWRITE_READING_DIRECTION_LEFT_TO_RIGHT),
        "Set text reading direction");
    Check(format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING), "Set text alignment");
    Check(format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR),
        "Set paragraph alignment");

    ComPtr<IDWriteInlineObject> ellipsis;
    Check(factory->CreateEllipsisTrimmingSign(format.Get(), ellipsis.GetAddressOf()),
        "Create ellipsis trimming sign");
    const DWRITE_TRIMMING trimming{ DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0 };
    Check(format->SetTrimming(&trimming, ellipsis.Get()), "Set character trimming");
    return format;
}

} // namespace

void TextStyles::Initialize()
{
    ComPtr<IDWriteFactory> factory;
    Check(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(factory.GetAddressOf())), "Create DirectWrite factory");
    ComPtr<IDWriteFontCollection> collection;
    Check(factory->GetSystemFontCollection(collection.GetAddressOf(), FALSE),
        "Get system font collection");

    decltype(formats_) formats;
    for (std::size_t index = 0; index < formats.size(); ++index) {
        formats[index] = CreateFormat(factory.Get(), collection.Get(),
            Spec::Typography::Styles[index]);
    }

    // Commit only after every role and its fallback have been validated.
    factory_ = std::move(factory);
    formats_ = std::move(formats);
    layouts_.clear();
}

Microsoft::WRL::ComPtr<IDWriteTextLayout> TextStyles::IdleLayout()
{
    return Layout(Spec::DebugHud::IdleText, Spec::DebugHud::TextRole,
        Spec::DebugHud::FpsRect, Spec::DebugHud::Alignment,
        DWRITE_PARAGRAPH_ALIGNMENT_NEAR, Spec::DebugHud::Tabular);
}

ComPtr<IDWriteTextLayout> TextStyles::Layout(std::wstring_view text,
    Spec::Typography::Role textRole, D2D1_RECT_F rect,
    DWRITE_TEXT_ALIGNMENT alignment, DWRITE_PARAGRAPH_ALIGNMENT vertical,
    bool forceTabular)
{
    Check(factory_ ? S_OK : E_UNEXPECTED, "Initialize TextStyles before creating layouts");
    const auto role = static_cast<std::size_t>(textRole);
    Check(role < formats_.size() ? S_OK : E_INVALIDARG, "Validate text role");
    const auto& style = Spec::Typography::Styles[role];
    std::wstring content(text);
    if (style.capitals) {
        for (auto& character : content) character = static_cast<wchar_t>(std::towupper(character));
    }
    const float width = rect.right - rect.left;
    const float height = rect.bottom - rect.top;
    const bool tabularFigures = style.tabular || forceTabular;
    for (const auto& entry : layouts_) {
        if (entry.text == content && entry.role == textRole && entry.width == width &&
            entry.height == height && entry.alignment == alignment &&
            entry.vertical == vertical && entry.tabular == tabularFigures) return entry.layout;
    }
    const UINT32 length = static_cast<UINT32>(content.size());
    const DWRITE_TEXT_RANGE range{ 0, length };
    ComPtr<IDWriteTextLayout> layout;
    Check(factory_->CreateTextLayout(content.c_str(), length,
        formats_[role].Get(), width, height, layout.GetAddressOf()), "Create text layout");
    Check(layout->SetTextAlignment(alignment), "Align text");
    Check(layout->SetParagraphAlignment(vertical), "Align paragraph");

    ComPtr<IDWriteTextLayout1> layout1;
    Check(layout.As(&layout1), "Query text character-spacing interface");
    // Split tracking evenly between both sides of each glyph.
    const float spacing = style.tracking * Spec::WindowSpec::Half;
    Check(layout1->SetCharacterSpacing(spacing, spacing, Spec::PaintSpec::Zero, range),
        "Set debug text tracking");

    if (tabularFigures) {
        ComPtr<IDWriteTypography> typography;
        Check(factory_->CreateTypography(typography.GetAddressOf()), "Create debug typography");
        const DWRITE_FONT_FEATURE tabular{ DWRITE_FONT_FEATURE_TAG_TABULAR_FIGURES, 1 };
        Check(typography->AddFontFeature(tabular), "Enable tabular figures");
        Check(layout->SetTypography(typography.Get(), range), "Apply debug typography");
    }

    if (layouts_.size() == CacheCapacity) layouts_.pop_front();
    layouts_.push_back({std::move(content), textRole, width, height, alignment,
        vertical, tabularFigures, layout});
    return layout;
}

} // namespace caecilia
