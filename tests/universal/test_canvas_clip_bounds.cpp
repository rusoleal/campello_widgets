#include <gtest/gtest.h>
#include <campello_widgets/ui/canvas.hpp>

namespace cw = systems::leal::campello_widgets;

TEST(CanvasClipBounds, DefaultsToFullViewportInBothSpaces)
{
    cw::Canvas canvas(400.0f, 300.0f);

    const cw::Rect dst = canvas.getDestinationClipBounds();
    EXPECT_FLOAT_EQ(dst.left(), 0.0f);
    EXPECT_FLOAT_EQ(dst.top(), 0.0f);
    EXPECT_FLOAT_EQ(dst.right(), 400.0f);
    EXPECT_FLOAT_EQ(dst.bottom(), 300.0f);

    const cw::Rect local = canvas.getLocalClipBounds();
    EXPECT_FLOAT_EQ(local.left(), 0.0f);
    EXPECT_FLOAT_EQ(local.top(), 0.0f);
    EXPECT_FLOAT_EQ(local.right(), 400.0f);
    EXPECT_FLOAT_EQ(local.bottom(), 300.0f);
}

TEST(CanvasClipBounds, DestinationBoundsReflectClipRectAsIs)
{
    cw::Canvas canvas(400.0f, 300.0f);

    canvas.clipRect(cw::Rect::fromLTWH(10.0f, 20.0f, 100.0f, 50.0f));

    const cw::Rect dst = canvas.getDestinationClipBounds();
    EXPECT_FLOAT_EQ(dst.left(), 10.0f);
    EXPECT_FLOAT_EQ(dst.top(), 20.0f);
    EXPECT_FLOAT_EQ(dst.right(), 110.0f);
    EXPECT_FLOAT_EQ(dst.bottom(), 70.0f);
}

TEST(CanvasClipBounds, LocalBoundsUndoAmbientTranslate)
{
    cw::Canvas canvas(400.0f, 300.0f);

    // Simulate a scrolled ancestor: translate first, then clip in the
    // now-shifted local space.
    canvas.translate(50.0f, 0.0f);
    canvas.clipRect(cw::Rect::fromLTWH(10.0f, 20.0f, 100.0f, 50.0f));

    // Destination space: local rect shifted by the translate.
    const cw::Rect dst = canvas.getDestinationClipBounds();
    EXPECT_FLOAT_EQ(dst.left(), 60.0f);
    EXPECT_FLOAT_EQ(dst.top(), 20.0f);
    EXPECT_FLOAT_EQ(dst.right(), 160.0f);
    EXPECT_FLOAT_EQ(dst.bottom(), 70.0f);

    // Local space: the inverse translate should recover the original
    // pre-transform clip rect exactly.
    const cw::Rect local = canvas.getLocalClipBounds();
    EXPECT_FLOAT_EQ(local.left(), 10.0f);
    EXPECT_FLOAT_EQ(local.top(), 20.0f);
    EXPECT_FLOAT_EQ(local.right(), 110.0f);
    EXPECT_FLOAT_EQ(local.bottom(), 70.0f);
}

TEST(CanvasClipBounds, RestoreRevertsBothAccessors)
{
    cw::Canvas canvas(400.0f, 300.0f);

    canvas.save();
    canvas.translate(50.0f, 0.0f);
    canvas.clipRect(cw::Rect::fromLTWH(10.0f, 20.0f, 100.0f, 50.0f));
    canvas.restore();

    const cw::Rect dst = canvas.getDestinationClipBounds();
    EXPECT_FLOAT_EQ(dst.left(), 0.0f);
    EXPECT_FLOAT_EQ(dst.right(), 400.0f);

    const cw::Rect local = canvas.getLocalClipBounds();
    EXPECT_FLOAT_EQ(local.left(), 0.0f);
    EXPECT_FLOAT_EQ(local.right(), 400.0f);
}
