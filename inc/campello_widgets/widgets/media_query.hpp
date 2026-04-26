#pragma once

#include <campello_widgets/widgets/inherited_widget.hpp>
#include <campello_widgets/ui/brightness.hpp>
#include <campello_widgets/ui/size.hpp>
#include <campello_widgets/ui/edge_insets.hpp>

namespace systems::leal::campello_widgets
{

    class BuildContext;

    /**
     * @brief Data about the current media (screen/window) environment.
     *
     * MediaQueryData contains information about the viewport size, device pixel
     * ratio, and safe area insets. This data is exposed to widgets via the
     * MediaQuery inherited widget.
     */
    struct MediaQueryData
    {
        /** @brief The size of the viewport in logical pixels. */
        Size logical_size;

        /** @brief The device pixel ratio (physical pixels per logical pixel). */
        float device_pixel_ratio = 1.0f;

        /** @brief Safe area insets from display cutouts/notches (in logical pixels). */
        EdgeInsets padding;

        /** @brief Insets from system UI like keyboard (in logical pixels). */
        EdgeInsets view_insets;

        /** @brief The platform's brightness preference (light or dark mode). */
        Brightness platform_brightness = Brightness::light;

        /**
         * @brief Returns the size in physical pixels.
         */
        Size physicalSize() const noexcept
        {
            return Size{
                logical_size.width * device_pixel_ratio,
                logical_size.height * device_pixel_ratio};
        }

        bool operator==(const MediaQueryData& other) const noexcept
        {
            return logical_size == other.logical_size &&
                   device_pixel_ratio == other.device_pixel_ratio &&
                   padding == other.padding &&
                   view_insets == other.view_insets &&
                   platform_brightness == other.platform_brightness;
        }

        bool operator!=(const MediaQueryData& other) const noexcept
        {
            return !(*this == other);
        }
    };

    /**
     * @brief Provides media query data to descendant widgets.
     *
     * MediaQuery is an InheritedWidget that makes MediaQueryData available to
     * all descendants in the widget tree. Use MediaQuery::of(context) to access
     * the current media data from within a build() method.
     *
     * @code
     * class MyWidget : public StatelessWidget {
     *     WidgetRef build(BuildContext& context) const override {
     *         const auto* media = MediaQuery::of(context);
     *         if (!media) return make<Text>("No media data");
     *
     *         float width = media->logical_size.width;
     *         float dpr = media->device_pixel_ratio;
     *         // ... build based on media data
     *     }
     * };
     * @endcode
     */
    class MediaQuery : public InheritedWidget
    {
    public:
        /** @brief The media query data exposed to descendants. */
        MediaQueryData data;

        MediaQuery(MediaQueryData data, WidgetRef child)
            : data(std::move(data))
        {
            this->child = std::move(child);
        }

        /**
         * @brief Looks up the nearest ancestor MediaQuery and registers the
         *        calling element as a dependent.
         *
         * Returns nullptr if no MediaQuery is found in the ancestor chain.
         * The calling widget will be rebuilt when the MediaQueryData changes.
         *
         * @code
         * const MediaQueryData* media = MediaQuery::of(context);
         * @endcode
         */
        static const MediaQueryData* of(BuildContext& context);

        /**
         * @brief Returns true if the media data has changed.
         *
         * Called by the framework when the parent rebuilds and produces a new
         * MediaQuery widget. If true, all registered dependents are marked dirty
         * and will rebuild.
         */
        bool updateShouldNotify(const InheritedWidget& old_widget) const override;
    };

} // namespace systems::leal::campello_widgets
