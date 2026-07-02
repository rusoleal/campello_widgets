#include <gtest/gtest.h>
#include <campello_widgets/ui/picture_layer.hpp>
#include <campello_widgets/ui/render_clip_rect.hpp>
#include <campello_widgets/ui/render_backdrop_filter.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/paint.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/rect.hpp>
#include <campello_widgets/ui/image_filter.hpp>

namespace cw = systems::leal::campello_widgets;

namespace
{
    class PictureLayerLeafBox : public cw::RenderBox
    {
    public:
        void performLayout() override
        {
            size_ = constraints_.constrain(cw::Size{40.0f, 30.0f});
        }

        void performPaint(cw::PaintContext& context, const cw::Offset& offset) override
        {
            context.canvas().drawRect(
                cw::Rect::fromLTWH(offset.x, offset.y, size_.width, size_.height),
                cw::Paint::filled(cw::Color::black()));
        }
    };
} // namespace

TEST(PictureLayer, RecordsPlainContentAsSafe)
{
    auto child = std::make_shared<PictureLayerLeafBox>();
    child->layout(cw::BoxConstraints::loose(200.0f, 200.0f));

    cw::PictureLayer layer;
    cw::PaintContext ctx(200.0f, 200.0f);
    layer.record(ctx, [&] { child->paint(ctx, cw::Offset{10.0f, 10.0f}); });

    EXPECT_TRUE(layer.hasContent());
    EXPECT_FALSE(layer.hasUnsafeGeometry());
    EXPECT_FALSE(layer.commands().empty());
}

TEST(PictureLayer, ClipRectContentIsUnsafe)
{
    auto leaf = std::make_shared<PictureLayerLeafBox>();
    auto clip = std::make_shared<cw::RenderClipRect>();
    clip->setChild(leaf);
    clip->layout(cw::BoxConstraints::loose(200.0f, 200.0f));

    cw::PictureLayer layer;
    cw::PaintContext ctx(200.0f, 200.0f);
    layer.record(ctx, [&] { clip->paint(ctx, cw::Offset{0.0f, 0.0f}); });

    EXPECT_TRUE(layer.hasContent());
    EXPECT_TRUE(layer.hasUnsafeGeometry())
        << "PushClipRectCmd bakes an absolute rect and must be flagged unsafe";
}

TEST(PictureLayer, BackdropFilterContentIsUnsafe)
{
    auto leaf = std::make_shared<PictureLayerLeafBox>();
    auto bf   = std::make_shared<cw::RenderBackdropFilter>(cw::ImageFilter::blur(5.0f));
    bf->setChild(leaf);
    bf->layout(cw::BoxConstraints::loose(200.0f, 200.0f));

    cw::PictureLayer layer;
    cw::PaintContext ctx(200.0f, 200.0f);
    layer.record(ctx, [&] { bf->paint(ctx, cw::Offset{0.0f, 0.0f}); });

    EXPECT_TRUE(layer.hasUnsafeGeometry())
        << "DrawBackdropFilterBeginCmd bakes absolute bounds and must be flagged unsafe";
    EXPECT_TRUE(layer.hasBackdropFilter())
        << "noteBackdropFilter()'s side effect must be re-triggered every frame — "
           "OffsetLayer relies on this flag to never replay this content";
}

TEST(PictureLayer, PlainClipContentHasNoBackdropFilter)
{
    auto leaf = std::make_shared<PictureLayerLeafBox>();
    auto clip = std::make_shared<cw::RenderClipRect>();
    clip->setChild(leaf);
    clip->layout(cw::BoxConstraints::loose(200.0f, 200.0f));

    cw::PictureLayer layer;
    cw::PaintContext ctx(200.0f, 200.0f);
    layer.record(ctx, [&] { clip->paint(ctx, cw::Offset{0.0f, 0.0f}); });

    EXPECT_TRUE(layer.hasUnsafeGeometry());
    EXPECT_FALSE(layer.hasBackdropFilter())
        << "a plain clip is repositionable-unsafe but must not be flagged as a backdrop filter";
}

TEST(PictureLayer, EmptyContentHasNoCommands)
{
    cw::PictureLayer layer;
    cw::PaintContext ctx(200.0f, 200.0f);
    layer.record(ctx, [&] { /* paints nothing */ });

    EXPECT_TRUE(layer.hasContent());
    EXPECT_FALSE(layer.hasUnsafeGeometry());
    EXPECT_TRUE(layer.commands().empty());
}
