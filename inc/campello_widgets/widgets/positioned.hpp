#pragma once

#include <optional>
#include <campello_widgets/widgets/stateless_widget.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A widget that controls where a child of a Stack is positioned.
     *
     * Supply any combination of `left`, `top`, `right`, `bottom`, `width`,
     * and `height`. Unset edges/dimensions are unconstrained. At least one
     * horizontal and one vertical constraint must be provided for the child
     * to be positioned meaningfully.
     *
     * `Positioned::build()` is transparent — it returns its child unchanged.
     */
    class Positioned : public StatelessWidget
    {
    public:
        std::optional<float> left;
        std::optional<float> top;
        std::optional<float> right;
        std::optional<float> bottom;
        std::optional<float> width;
        std::optional<float> height;
        WidgetRef             child;

        Positioned() = default;

        /** @brief Full constructor for mw<>() convenience. */
        explicit Positioned(std::optional<float> l,
                            std::optional<float> t,
                            std::optional<float> r,
                            std::optional<float> b,
                            std::optional<float> w,
                            std::optional<float> h,
                            WidgetRef            c)
            : left(l)
            , top(t)
            , right(r)
            , bottom(b)
            , width(w)
            , height(h)
            , child(std::move(c))
        {}

        WidgetRef build(BuildContext&) const override { return child; }

        /**
         * @brief Creates a Positioned that fills the Stack (all edges = 0).
         */
        static std::shared_ptr<Positioned> fill(const WidgetRef& child) {
            auto p = std::make_shared<Positioned>();
            p->left = 0;
            p->top = 0;
            p->right = 0;
            p->bottom = 0;
            p->child = child;
            return p;
        }
    };

} // namespace systems::leal::campello_widgets
