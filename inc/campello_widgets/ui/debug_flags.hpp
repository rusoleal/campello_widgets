#pragma once

namespace systems::leal::campello_widgets
{

    /**
     * @brief Global flags for enabling debug overlays at runtime.
     *
     * Inspired by Flutter's debugPaint* flags. All flags default to false
     * and can be toggled at any point — changes take effect on the next frame.
     * All overhead is guarded by the flags; there is zero cost when disabled.
     *
     * @code
     * // In your app's main / initialisation:
     * DebugFlags::paintSizeEnabled     = true;  // show layout bounds
     * DebugFlags::repaintRainbowEnabled = true;  // visualise repaints
     * DebugFlags::showDebugBanner       = true;  // top-right DEBUG ribbon
     * @endcode
     */
    struct DebugFlags
    {
        /**
         * @brief Outline every RenderObject with a coloured 1-pixel border.
         *
         * Equivalent to Flutter's `debugPaintSizeEnabled`. Useful for
         * understanding how layout constraints are resolved and which widget
         * owns which screen region.
         */
        inline static bool paintSizeEnabled = false;

        /**
         * @brief Fill repainting render objects with a cycling translucent colour.
         *
         * Each time a RenderObject's paint pass actually runs (i.e. it was
         * marked dirty), the bounds are overlaid with a semi-transparent colour
         * chosen from a rotating 6-colour palette. Regions that keep the same
         * colour between frames are not repainting — helping you spot
         * unnecessary work.
         *
         * Equivalent to Flutter's `debugRepaintRainbowEnabled`.
         */
        inline static bool repaintRainbowEnabled = false;

        /**
         * @brief Draw a small "DEBUG" ribbon in the top-right corner every frame.
         *
         * Drawn by the Renderer after the normal paint pass so it always
         * appears on top of all widget content.
         *
         * Equivalent to Flutter's `CheckedModeBanner`.
         */
        inline static bool showDebugBanner = false;

        /**
         * @brief Draw an FPS performance overlay in the bottom-left corner.
         *
         * Displays a scrolling bar chart of the last 64 frame times and a
         * live FPS / ms-per-frame label, mirroring Flutter's
         * `showPerformanceOverlay`. Bar heights are scaled so that 3× the
         * target frame budget (48 ms at 60 fps) fills the chart area.
         * Bars are green when the frame was on budget, red when it exceeded
         * the 16 ms target. Horizontal guide lines mark 16 ms (green) and
         * 32 ms (red).
         */
        inline static bool showPerformanceOverlay = false;
    };

} // namespace systems::leal::campello_widgets
