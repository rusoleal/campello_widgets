#pragma once

// On Windows, campello_widgets builds as a DLL with WINDOWS_EXPORT_ALL_SYMBOLS
// ON (windows.cmake) — but that CMake mechanism only auto-exports *function*
// symbols, not *data* symbols. Without an explicit dllexport/dllimport
// annotation, the DLL and any executable linking against it (e.g. the test
// binary) each get their own private copy of every `inline static` member
// below, so toggling a flag from outside the DLL is invisible to framework
// code running inside it. CAMPELLO_WIDGETS_BUILDING_DLL is set as a PRIVATE
// compile definition on the campello_widgets target itself (windows.cmake),
// so it's defined while compiling the library but not for consumers.
#if defined(_WIN32) && defined(CAMPELLO_WIDGETS_BUILDING_DLL)
    #define CW_DEBUG_FLAGS_API __declspec(dllexport)
#elif defined(_WIN32)
    #define CW_DEBUG_FLAGS_API __declspec(dllimport)
#else
    #define CW_DEBUG_FLAGS_API
#endif

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
    struct CW_DEBUG_FLAGS_API DebugFlags
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
         * @brief Draw a unified frame-timing overlay in the bottom-left corner.
         *
         * Mirrors Flutter DevTools' "Flutter frames chart": each frame is
         * a group of two adjacent bars (UI build/layout/paint, then raster
         * encode/submit) followed by a blank gap, sharing a single chart
         * area and a single budget reference line, over the most recent
         * ~20 frames (the underlying samplers retain more history, used
         * for the averages shown in the label). Bar heights are scaled so
         * that 2× the target frame budget (33.3 ms at 60 fps) fills the
         * chart area — the reference line therefore sits at the panel's
         * vertical midpoint, and a bar filling the whole panel height is
         * 30fps-equivalent cost. Each bar is colored by phase (UI vs
         * raster) when within the 16.6 ms target, or red when it exceeded
         * the target.
         */
        inline static bool showPerformanceOverlay = false;

        /**
         * @brief Skip the "UI: x ms   RASTER: y ms" text label drawn atop
         * the performance overlay's frame chart, leaving just the bars.
         *
         * Diagnostic-only: lets you isolate whether text rendering itself
         * (glyph rasterization/upload or per-glyph draw cost) is
         * contributing to a measured raster-time regression, independent
         * of the rest of the overlay. Has no effect unless
         * showPerformanceOverlay is also true.
         */
        inline static bool performanceOverlayTextEnabled = true;

        /**
         * @brief Log widget rebuilds to stdout.
         *
         * Every time an Element's rebuild() method runs, prints the widget
         * type name to standard output. Useful for detecting unnecessary
         * rebuilds or rebuild storms.
         *
         * Equivalent to Flutter's `debugPrintRebuildDirtyWidgets`.
         */
        inline static bool printRebuildsEnabled = false;

        /**
         * @brief Draw green lines at text baselines.
         *
         * Useful for debugging text alignment and positioning issues.
         * Each line of text gets a green horizontal line at its alphabetic
         * baseline.
         *
         * Equivalent to Flutter's `debugPaintBaselinesEnabled`.
         */
        inline static bool paintBaselinesEnabled = false;

        /**
         * @brief Draw colored dots at recent pointer/touch positions.
         *
         * Useful for verifying hit-test areas and gesture responsiveness.
         *
         * Equivalent to Flutter's `debugPaintPointersEnabled`.
         */
        inline static bool paintPointersEnabled = false;

        /**
         * @brief Print a breakdown of `Renderer::renderFrame()`'s raster
         * phase (encoder creation, render-pass begin/end, draw-list flush,
         * finish + submit) to stderr every frame.
         *
         * Diagnostic-only — the raster lane on the performance overlay
         * (`showPerformanceOverlay`) reports a single total; this breaks
         * that total down to find where the cost actually goes.
         */
        inline static bool printRasterSubPhaseTimings = false;

        /**
         * @brief Print a trace of dirty-region reporting and the resulting
         * BackdropFilter capture-skip decision to stderr every frame.
         *
         * Diagnostic-only, for verifying/debugging Renderer's dirty-region
         * tracking (see Renderer::noteDirtyRegion()'s doc): which nodes
         * reported bounds as dirty this frame, what BackdropFilter regions
         * exist, and whether the capture pass ran or was skipped.
         */
        inline static bool printDirtyRegionTrace = false;

        /**
         * @brief Print every scroll-offset update from virtualized
         * scrollables (RenderListView, RenderGridView) to stderr, flagging
         * any update whose applied movement doesn't plausibly follow from
         * its input delta as a JUMP.
         *
         * Each line reports the call site ("drag", "wheel", "spring",
         * "momentum"), the offset before/after, the requested delta, and
         * the actually-applied movement (post boundary-condition
         * resistance). A JUMP is flagged when the applied movement's
         * magnitude or sign doesn't plausibly follow from the requested
         * delta — e.g. boundary resistance should only ever shrink the
         * applied movement relative to the request, never grow or invert
         * it. Diagnostic-only, for tracking down overscroll/spring-back
         * discontinuities.
         */
        inline static bool printScrollTrace = false;
    };

} // namespace systems::leal::campello_widgets
