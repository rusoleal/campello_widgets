#pragma once

#include <optional>
#include <campello_widgets/diagnostics/diagnostic_property.hpp>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A box with a specified size.
     *
     * If `width` or `height` is omitted the corresponding axis sizes to the
     * maximum available from the parent constraints.
     *
     * Factory methods (preferred for clarity):
     *   - SizedBox::from_width(w, child)  - fixed width, height expands
     *   - SizedBox::from_height(h, child) - fixed height, width expands
     *   - SizedBox::square(size, child)   - fixed width and height
     *   - SizedBox::expand(child)         - expands in both directions
     *   - SizedBox::shrink(child)         - shrinks to child size (no constraints)
     */
    class SizedBox : public SingleChildRenderObjectWidget
    {
    public:
        std::optional<float> width;
        std::optional<float> height;

        SizedBox() = default;

        /// Direct constructor - prefer factory methods for clarity.
        /// Note: Both w and h have defaults to support mw<SizedBox>(width) pattern.
        explicit SizedBox(std::optional<float> w,
                          std::optional<float> h = std::nullopt,
                          WidgetRef            c = nullptr)
        {
            width  = w;
            height = h;
            child  = std::move(c);
        }

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;

        // ------------------------------------------------------------------
        // Factory methods (preferred for clarity)
        // ------------------------------------------------------------------

        /// Creates a box with fixed width, height expands to fill constraints.
        static std::shared_ptr<SizedBox> from_width(float w, WidgetRef c = nullptr) {
            auto s = std::make_shared<SizedBox>();
            s->width = w;
            s->height = std::nullopt;
            s->child = std::move(c);
            return s;
        }

        /// Creates a box with fixed height, width expands to fill constraints.
        static std::shared_ptr<SizedBox> from_height(float h, WidgetRef c = nullptr) {
            auto s = std::make_shared<SizedBox>();
            s->width = std::nullopt;
            s->height = h;
            s->child = std::move(c);
            return s;
        }

        /// Creates a square box with identical width and height.
        static std::shared_ptr<SizedBox> square(float size, WidgetRef c = nullptr) {
            auto s = std::make_shared<SizedBox>();
            s->width = size;
            s->height = size;
            s->child = std::move(c);
            return s;
        }

        /// Creates a box that expands to fill all available space in both dimensions.
        static std::shared_ptr<SizedBox> expand(WidgetRef c = nullptr) {
            auto s = std::make_shared<SizedBox>();
            s->width = std::nullopt;
            s->height = std::nullopt;
            s->child = std::move(c);
            return s;
        }

        /// Creates a box that passes through constraints (no size enforcement).
        static std::shared_ptr<SizedBox> shrink(WidgetRef c = nullptr) {
            auto s = std::make_shared<SizedBox>();
            s->width = std::nullopt;
            s->height = std::nullopt;
            s->child = std::move(c);
            return s;
        }

        // ------------------------------------------------------------------
        // Deprecated: Old factory method for backward compatibility
        // ------------------------------------------------------------------

        [[deprecated("Use square() or from_width()/from_height() instead")]]
        static std::shared_ptr<SizedBox> create(float w, float h, WidgetRef child = nullptr) {
            auto s = std::make_shared<SizedBox>();
            s->width = w;
            s->height = h;
            s->child = std::move(child);
            return s;
        }
        void debugFillProperties(DiagnosticsPropertyBuilder& properties) const override;

    };

} // namespace systems::leal::campello_widgets
