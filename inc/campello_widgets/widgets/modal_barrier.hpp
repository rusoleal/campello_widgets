#pragma once

#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/ui/color.hpp>
#include <functional>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A widget that blocks interaction with widgets behind it.
     *
     * ModalBarrier is a full-screen barrier that:
     * - Prevents pointer events from reaching widgets below
     * - Optionally draws a semi-transparent color behind modal content
     * - Can dismiss the modal when tapped (if dismissible)
     *
     * It's typically used behind dialogs, drawers, and bottom sheets.
     *
     * Example:
     * @code
     * Stack::create({
     *     ModalBarrier::create(Color::fromRGBA(0, 0, 0, 0.5f), true, onDismiss),
     *     Center::create(Dialog::create(...)),
     * })
     * @endcode
     */
    class ModalBarrier : public StatelessWidget
    {
    public:
        /** @brief Color to fill the barrier (typically semi-transparent black). */
        Color color = Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.5f);

        /** @brief Whether tapping the barrier dismisses the modal. */
        bool dismissible = true;

        /** @brief Callback when barrier is tapped (if dismissible). */
        std::function<void()> on_dismiss;

        ModalBarrier() = default;
        explicit ModalBarrier(Color c, bool d = true, std::function<void()> on_dismiss = nullptr)
            : color(c)
            , dismissible(d)
            , on_dismiss(std::move(on_dismiss))
        {}

        WidgetRef build(BuildContext& context) const override;

        // Factory
        static std::shared_ptr<ModalBarrier> create(
            Color color = Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.5f),
            bool dismissible = true,
            std::function<void()> on_dismiss = nullptr)
        {
            return std::make_shared<ModalBarrier>(color, dismissible, std::move(on_dismiss));
        }

        /**
         * @brief Creates a standard black 50% opacity barrier.
         */
        static std::shared_ptr<ModalBarrier> standard(std::function<void()> on_dismiss = nullptr)
        {
            return create(Color::fromRGBA(0.0f, 0.0f, 0.0f, 0.5f), true, std::move(on_dismiss));
        }
    };

} // namespace systems::leal::campello_widgets
