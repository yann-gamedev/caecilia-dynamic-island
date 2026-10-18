#include "TextStyles.h"

#include "../app/Errors.h"

#include <iterator>
#include <utility>

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
    idleLayout_.Reset();
}

Microsoft::WRL::ComPtr<IDWriteTextLayout> TextStyles::IdleLayout()
{
    Check(factory_ ? S_OK : E_UNEXPECTED, "Initialize TextStyles before creating layouts");
    if (idleLayout_) {
        return idleLayout_;
    }

    constexpr auto role = static_cast<std::size_t>(Spec::DebugHud::TextRole);
    constexpr auto style = Spec::Typography::Styles[role];
    constexpr auto rect = Spec::DebugHud::FpsRect;
    constexpr UINT32 length = static_cast<UINT32>(std::size(Spec::DebugHud::IdleText) - 1);
    const DWRITE_TEXT_RANGE range{ 0, length };
    ComPtr<IDWriteTextLayout> layout;
    Check(factory_->CreateTextLayout(Spec::DebugHud::IdleText, length,
        formats_[role].Get(), rect.right - rect.left, rect.bottom - rect.top,
        layout.GetAddressOf()), "Create IDLE layout");
    Check(layout->SetTextAlignment(Spec::DebugHud::Alignment), "Align debug text left");

    ComPtr<IDWriteTextLayout1> layout1;
    Check(layout.As(&layout1), "Query text character-spacing interface");
    // Split tracking evenly between both sides of each glyph.
    const float spacing = style.tracking * Spec::WindowSpec::Half;
    Check(layout1->SetCharacterSpacing(spacing, spacing, Spec::PaintSpec::Zero, range),
        "Set debug text tracking");

    if constexpr (Spec::DebugHud::Tabular) {
        ComPtr<IDWriteTypography> typography;
        Check(factory_->CreateTypography(typography.GetAddressOf()), "Create debug typography");
        const DWRITE_FONT_FEATURE tabular{ DWRITE_FONT_FEATURE_TAG_TABULAR_FIGURES, 1 };
        Check(typography->AddFontFeature(tabular), "Enable tabular figures");
        Check(layout->SetTypography(typography.Get(), range), "Apply debug typography");
    }

    // The painter supplies FpsRect's origin, IdleColor, and grayscale antialiasing.
    idleLayout_ = std::move(layout);
    return idleLayout_;
}

} // namespace caecilia
