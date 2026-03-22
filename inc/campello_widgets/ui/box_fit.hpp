#pragma once

namespace systems::leal::campello_widgets
{

    /**
     * @brief How an image should be inscribed into the space allocated by its parent.
     *
     * Matches Flutter's `BoxFit` enum.
     */
    enum class BoxFit
    {
        /**
         * Fill the box by distorting the image (no letterboxing / cropping).
         * Equivalent to Flutter's BoxFit.fill.
         */
        fill,

        /**
         * Scale the image uniformly to fit within the box; letterbox/pillarbox
         * any remaining space. Equivalent to Flutter's BoxFit.contain.
         */
        contain,

        /**
         * Scale the image uniformly to cover the box entirely; crop any overflow.
         * Equivalent to Flutter's BoxFit.cover.
         */
        cover,

        /**
         * Scale the image to match the box width; height may overflow.
         * Equivalent to Flutter's BoxFit.fitWidth.
         */
        fitWidth,

        /**
         * Scale the image to match the box height; width may overflow.
         * Equivalent to Flutter's BoxFit.fitHeight.
         */
        fitHeight,

        /**
         * No scaling — draw the image at its natural size; clip any overflow.
         * Equivalent to Flutter's BoxFit.none.
         */
        none,

        /**
         * Like `contain` but only scales down, never up.
         * Equivalent to Flutter's BoxFit.scaleDown.
         */
        scaleDown,
    };

} // namespace systems::leal::campello_widgets
