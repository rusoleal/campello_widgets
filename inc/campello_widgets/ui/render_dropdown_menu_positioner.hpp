#pragma once

#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/offset.hpp>
#include <campello_widgets/ui/size.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Positions a dropdown menu relative to its anchor button,
     * flipping to open upward when the menu doesn't fit below.
     *
     * Expects to fill its incoming constraints (i.e. be the child of a
     * `Positioned::fill` inside a `Stack`, so those constraints equal the
     * live overlay/viewport size — always fresh, never a value cached
     * ahead of time). Lays out its child (the menu content) with loose,
     * unbounded-height constraints first to find its true natural size,
     * then decides where to place it — mirrors, at a much smaller scale,
     * how Flutter's real DropdownButton menu route measures the menu
     * before choosing where it goes, rather than guessing.
     */
    class RenderDropdownMenuPositioner : public RenderBox
    {
    public:
        /** @brief The anchor button's on-screen position (logical pixels). */
        Offset anchor_pos;

        /** @brief The anchor button's size (logical pixels). */
        Size anchor_size;

        void performLayout() override;
    };

} // namespace systems::leal::campello_widgets
