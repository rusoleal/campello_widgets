#include <gtest/gtest.h>
#include <campello_widgets/ui/inline_span.hpp>
#include <campello_widgets/ui/render_paragraph.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/text_style.hpp>
#include <campello_widgets/ui/color.hpp>

namespace cw = systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// InlineSpan tests
// ---------------------------------------------------------------------------

TEST(InlineSpan, TextSpanIsLeaf)
{
    auto span = std::make_shared<cw::InlineTextSpan>("Hello", cw::TextStyle{});
    EXPECT_TRUE(span->isLeaf());
    EXPECT_EQ(span->text(), "Hello");
}

TEST(InlineSpan, TextSpanCollectsSingleRun)
{
    auto span = std::make_shared<cw::InlineTextSpan>("Hello", cw::TextStyle{});
    std::vector<cw::InlineSpan::TextRun> runs;
    span->collectTextRuns(runs);
    
    EXPECT_EQ(runs.size(), 1u);
    EXPECT_EQ(runs[0].text, "Hello");
}

TEST(InlineSpan, EmptyTextSpanCollectsNoRuns)
{
    auto span = std::make_shared<cw::InlineTextSpan>("", cw::TextStyle{});
    std::vector<cw::InlineSpan::TextRun> runs;
    span->collectTextRuns(runs);
    
    EXPECT_EQ(runs.size(), 0u);
}

TEST(InlineSpan, CompositeSpanCollectsChildRuns)
{
    auto composite = std::make_shared<cw::CompositeInlineSpan>();
    composite->child_spans.push_back(
        std::make_shared<cw::InlineTextSpan>("Hello ", cw::TextStyle{}));
    composite->child_spans.push_back(
        std::make_shared<cw::InlineTextSpan>("World", cw::TextStyle{}));
    
    std::vector<cw::InlineSpan::TextRun> runs;
    composite->collectTextRuns(runs);
    
    EXPECT_EQ(runs.size(), 2u);
    EXPECT_EQ(runs[0].text, "Hello ");
    EXPECT_EQ(runs[1].text, "World");
}

TEST(InlineSpan, StyleMergePreservesChildValues)
{
    cw::TextStyle parent;
    parent.font_size = 14.0f;
    parent.color = cw::Color::black();
    
    cw::TextStyle child;
    child.font_size = 20.0f;  // Override
    // color not set, should inherit
    
    auto merged = parent.merge(child);
    EXPECT_FLOAT_EQ(merged.font_size, 20.0f);  // Child value
    EXPECT_EQ(merged.color, cw::Color::black());  // Parent value
}

TEST(InlineSpan, StyleBuilderMethods)
{
    auto style = cw::TextStyle{}
        .withColor(cw::Color::red())
        .withFontSize(18.0f)
        .bold();
    
    EXPECT_EQ(style.color, cw::Color::red());
    EXPECT_FLOAT_EQ(style.font_size, 18.0f);
    EXPECT_EQ(style.font_weight, cw::FontWeight::bold);
}

// ---------------------------------------------------------------------------
// RenderParagraph layout tests
// ---------------------------------------------------------------------------

TEST(RenderParagraph, EmptyTextHasZeroSize)
{
    auto paragraph = std::make_shared<cw::RenderParagraph>();
    paragraph->setText(std::make_shared<cw::InlineTextSpan>(""));
    
    paragraph->layout(cw::BoxConstraints::loose(400.0f, 600.0f));
    
    EXPECT_FLOAT_EQ(paragraph->size().width, 0.0f);
    EXPECT_FLOAT_EQ(paragraph->size().height, 0.0f);
}

TEST(RenderParagraph, SingleLineLayout)
{
    auto paragraph = std::make_shared<cw::RenderParagraph>();
    auto span = std::make_shared<cw::InlineTextSpan>(
        "Hello", cw::TextStyle{}.withFontSize(20.0f));
    paragraph->setText(span);
    
    // With no width constraint, should be single line
    paragraph->layout(cw::BoxConstraints::loose(
        std::numeric_limits<float>::infinity(), 600.0f));
    
    // Height should be line height (font_size * 1.2)
    EXPECT_FLOAT_EQ(paragraph->size().height, 20.0f * 1.2f);
    // Width should be estimated (char_width * count)
    EXPECT_GT(paragraph->size().width, 0.0f);
}

TEST(RenderParagraph, MultiSpanSingleLine)
{
    auto paragraph = std::make_shared<cw::RenderParagraph>();
    auto composite = std::make_shared<cw::CompositeInlineSpan>();
    composite->child_spans.push_back(
        std::make_shared<cw::InlineTextSpan>("Hello ", cw::TextStyle{}.withFontSize(20.0f)));
    composite->child_spans.push_back(
        std::make_shared<cw::InlineTextSpan>("World", cw::TextStyle{}.withFontSize(20.0f)));
    
    paragraph->setText(composite);
    paragraph->layout(cw::BoxConstraints::loose(
        std::numeric_limits<float>::infinity(), 600.0f));
    
    // Should be single line
    EXPECT_FLOAT_EQ(paragraph->size().height, 20.0f * 1.2f);
    EXPECT_GT(paragraph->size().width, 0.0f);
}

TEST(RenderParagraph, WordWrapping)
{
    auto paragraph = std::make_shared<cw::RenderParagraph>();
    // Long text that should wrap
    auto span = std::make_shared<cw::InlineTextSpan>(
        "This is a very long text that should wrap to multiple lines",
        cw::TextStyle{}.withFontSize(20.0f));
    
    paragraph->setText(span);
    paragraph->setSoftWrap(true);
    // Constrain width to force wrapping
    paragraph->layout(cw::BoxConstraints::loose(150.0f, 600.0f));
    
    // Should be taller than single line
    float line_height = 20.0f * 1.2f;
    EXPECT_GT(paragraph->size().height, line_height * 1.5f);
    // Width should respect constraint
    EXPECT_LE(paragraph->size().width, 150.0f);
}

TEST(RenderParagraph, NoWrappingWhenDisabled)
{
    auto paragraph = std::make_shared<cw::RenderParagraph>();
    auto span = std::make_shared<cw::InlineTextSpan>(
        "This is a very long text",
        cw::TextStyle{}.withFontSize(20.0f));
    
    paragraph->setText(span);
    paragraph->setSoftWrap(false);
    paragraph->layout(cw::BoxConstraints::loose(100.0f, 600.0f));
    
    // Should be single line
    EXPECT_FLOAT_EQ(paragraph->size().height, 20.0f * 1.2f);
    // Without wrapping, the intrinsic width is used (may be clamped by constraint)
    EXPECT_GE(paragraph->size().width, 0.0f);
}

TEST(RenderParagraph, MaxLinesTruncation)
{
    auto paragraph = std::make_shared<cw::RenderParagraph>();
    auto span = std::make_shared<cw::InlineTextSpan>(
        "Line one line two line three line four line five",
        cw::TextStyle{}.withFontSize(20.0f));
    
    paragraph->setText(span);
    paragraph->setSoftWrap(true);
    paragraph->setMaxLines(2);
    // Constrain to force multiple lines
    paragraph->layout(cw::BoxConstraints::loose(100.0f, 600.0f));
    
    float line_height = 20.0f * 1.2f;
    // Height should be approximately 2 lines
    EXPECT_LE(paragraph->size().height, line_height * 2.5f);
}

TEST(RenderParagraph, TextAlignmentCenter)
{
    auto paragraph = std::make_shared<cw::RenderParagraph>();
    auto span = std::make_shared<cw::InlineTextSpan>(
        "Short", cw::TextStyle{}.withFontSize(20.0f));
    
    paragraph->setText(span);
    paragraph->setTextAlign(cw::TextAlign::center);
    paragraph->layout(cw::BoxConstraints::tight(400.0f, 600.0f));
    
    // Width should fill tight constraint
    EXPECT_FLOAT_EQ(paragraph->size().width, 400.0f);
}

TEST(RenderParagraph, TextAlignmentRight)
{
    auto paragraph = std::make_shared<cw::RenderParagraph>();
    auto span = std::make_shared<cw::InlineTextSpan>(
        "Short", cw::TextStyle{}.withFontSize(20.0f));
    
    paragraph->setText(span);
    paragraph->setTextAlign(cw::TextAlign::right);
    paragraph->layout(cw::BoxConstraints::tight(400.0f, 600.0f));
    
    // Width should fill tight constraint
    EXPECT_FLOAT_EQ(paragraph->size().width, 400.0f);
}

TEST(RenderParagraph, DifferentFontSizesInSpans)
{
    auto paragraph = std::make_shared<cw::RenderParagraph>();
    auto composite = std::make_shared<cw::CompositeInlineSpan>();
    composite->child_spans.push_back(
        std::make_shared<cw::InlineTextSpan>("Small ", cw::TextStyle{}.withFontSize(10.0f)));
    composite->child_spans.push_back(
        std::make_shared<cw::InlineTextSpan>("BIG", cw::TextStyle{}.withFontSize(30.0f)));
    
    paragraph->setText(composite);
    paragraph->layout(cw::BoxConstraints::loose(
        std::numeric_limits<float>::infinity(), 600.0f));
    
    // Line height should be based on largest font (30 * 1.2)
    EXPECT_FLOAT_EQ(paragraph->size().height, 30.0f * 1.2f);
}

TEST(RenderParagraph, SetTextTriggersRelayout)
{
    auto paragraph = std::make_shared<cw::RenderParagraph>();
    auto span1 = std::make_shared<cw::InlineTextSpan>(
        "Short", cw::TextStyle{}.withFontSize(20.0f));
    
    paragraph->setText(span1);
    paragraph->layout(cw::BoxConstraints::loose(400.0f, 600.0f));
    
    float width1 = paragraph->size().width;
    
    // Change text to longer string
    auto span2 = std::make_shared<cw::InlineTextSpan>(
        "Much longer text string", cw::TextStyle{}.withFontSize(20.0f));
    paragraph->setText(span2);
    
    // Should need layout again
    EXPECT_TRUE(paragraph->needsLayout());
    
    paragraph->layout(cw::BoxConstraints::loose(400.0f, 600.0f));
    
    float width2 = paragraph->size().width;
    EXPECT_GT(width2, width1);
}

TEST(RenderParagraph, NullTextHandled)
{
    auto paragraph = std::make_shared<cw::RenderParagraph>();
    paragraph->setText(nullptr);
    
    paragraph->layout(cw::BoxConstraints::loose(400.0f, 600.0f));
    
    EXPECT_FLOAT_EQ(paragraph->size().width, 0.0f);
    EXPECT_FLOAT_EQ(paragraph->size().height, 0.0f);
}

TEST(RenderParagraph, VeryLongWordWrapping)
{
    auto paragraph = std::make_shared<cw::RenderParagraph>();
    // One very long word
    auto span = std::make_shared<cw::InlineTextSpan>(
        "Supercalifragilisticexpialidocious",
        cw::TextStyle{}.withFontSize(20.0f));
    
    paragraph->setText(span);
    paragraph->setSoftWrap(true);
    paragraph->layout(cw::BoxConstraints::loose(100.0f, 600.0f));
    
    // Very long words may overflow single line; height should be at least one line
    EXPECT_GE(paragraph->size().height, 20.0f * 1.2f);
    // Width should respect constraint
    EXPECT_LE(paragraph->size().width, 100.0f);
}

// ---------------------------------------------------------------------------
// TextStyle tests
// ---------------------------------------------------------------------------

TEST(TextStyle, DefaultValues)
{
    cw::TextStyle style;
    EXPECT_EQ(style.color, cw::Color::black());
    EXPECT_FLOAT_EQ(style.font_size, 14.0f);
    EXPECT_TRUE(style.font_family.empty());
    EXPECT_EQ(style.font_weight, cw::FontWeight::normal);
    EXPECT_FALSE(style.italic);
}

TEST(TextStyle, ChainedBuilders)
{
    auto style = cw::TextStyle{}
        .withColor(cw::Color::blue())
        .withFontSize(24.0f)
        .bold()
        .withItalic(true);
    
    EXPECT_EQ(style.color, cw::Color::blue());
    EXPECT_FLOAT_EQ(style.font_size, 24.0f);
    EXPECT_EQ(style.font_weight, cw::FontWeight::bold);
    EXPECT_TRUE(style.italic);
}

TEST(TextStyle, MergeWithDefaults)
{
    cw::TextStyle parent;
    parent.font_size = 16.0f;
    parent.color = cw::Color::green();
    
    cw::TextStyle child;  // All defaults
    
    auto merged = parent.merge(child);
    // Child has all defaults, should inherit from parent
    EXPECT_FLOAT_EQ(merged.font_size, 16.0f);
    EXPECT_EQ(merged.color, cw::Color::green());
}

TEST(TextStyle, MergeOverride)
{
    cw::TextStyle parent;
    parent.font_size = 16.0f;
    parent.color = cw::Color::green();
    
    cw::TextStyle child;
    child.color = cw::Color::red();  // Override
    
    auto merged = parent.merge(child);
    EXPECT_FLOAT_EQ(merged.font_size, 16.0f);  // Inherited
    EXPECT_EQ(merged.color, cw::Color::red());  // Overridden
}
