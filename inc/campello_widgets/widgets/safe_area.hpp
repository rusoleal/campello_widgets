#pragma once

#include <campello_widgets/diagnostics/diagnostic_property.hpp>
#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/ui/edge_insets.hpp>
#include <optional>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A widget that insets its child by sufficient padding to avoid
     * intrusions by the operating system.
     *
     * SafeArea is useful for ensuring your app’s content isn't obscured by
     * system UI like the status bar, notch, home indicator, or keyboard.
     *
     * For example, use SafeArea to prevent text from being hidden by the
     * iPhone notch or the Android status bar.
     *
     * By default, SafeArea applies padding for all sides. You can selectively
     * disable padding for specific edges using the `left`, `top`, `right`,
     * and `bottom` boolean flags.
     *
     * Example usage:
     * @code
     * SafeArea::create(
     *     Column::create({
     *         Text::create("Header"),
     *         Expanded::create(Content::create()),
     *         Text::create("Footer"),
     *     })
     * )
     * @endcode
     *
     * SafeArea automatically responds to changes in view insets (e.g., when
     * the keyboard appears or the device rotates). The child is rebuilt with
     * updated padding when this happens.
     *
     * @note For full-screen immersive experiences (e.g., games, video players),
     *       you may want to disable SafeArea or wrap only specific UI elements.
     *
     * @see EdgeInsets for the underlying padding mechanism.
     * @see Renderer::setViewInsets() for how platform code reports safe areas.
     */
    class SafeArea : public StatelessWidget
    {
    public:
        /** @brief The widget to pad. */
        WidgetRef child;

        /** @brief Whether to avoid system intrusions on the left. Default: true. */
        bool left = true;

        /** @brief Whether to avoid system intrusions on the top. Default: true. */
        bool top = true;

        /** @brief Whether to avoid system intrusions on the right. Default: true. */
        bool right = true;

        /** @brief Whether to avoid system intrusions on the bottom. Default: true. */
        bool bottom = true;

        /** @brief Additional padding to apply beyond the safe area. Default: zero. */
        EdgeInsets minimum = EdgeInsets::zero();

        SafeArea() = default;

        explicit SafeArea(WidgetRef c)
            : child(std::move(c))
        {}

        SafeArea(WidgetRef c, bool l, bool t, bool r, bool b)
            : child(std::move(c)), left(l), top(t), right(r), bottom(b)
        {}

        /** @brief Creates a SafeArea with all sides enabled (default). */
        static std::shared_ptr<SafeArea> create(WidgetRef child)
        {
            return std::make_shared<SafeArea>(std::move(child));
        }

        /** @brief Creates a SafeArea with custom edge configuration. */
        static std::shared_ptr<SafeArea> create(
            WidgetRef child,
            bool left,
            bool top,
            bool right,
            bool bottom)
        {
            return std::make_shared<SafeArea>(
                std::move(child), left, top, right, bottom);
        }

        /** @brief Creates a SafeArea that only avoids the top inset (status bar). */
        static std::shared_ptr<SafeArea> onlyTop(WidgetRef child)
        {
            auto sa = std::make_shared<SafeArea>();
            sa->child = std::move(child);
            sa->left = false;
            sa->top = true;
            sa->right = false;
            sa->bottom = false;
            return sa;
        }

        /** @brief Creates a SafeArea that only avoids the bottom inset (home indicator). */
        static std::shared_ptr<SafeArea> onlyBottom(WidgetRef child)
        {
            auto sa = std::make_shared<SafeArea>();
            sa->child = std::move(child);
            sa->left = false;
            sa->top = false;
            sa->right = false;
            sa->bottom = true;
            return sa;
        }

        WidgetRef build(BuildContext& context) const override;
        void debugFillProperties(DiagnosticsPropertyBuilder& properties) const override;

    };

} // namespace systems::leal::campello_widgets
