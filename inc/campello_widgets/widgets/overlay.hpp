#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/ui/color.hpp>
#include <functional>
#include <vector>

namespace systems::leal::campello_widgets
{

    // Forward declarations
    class OverlayEntry;
    class Overlay;
    class OverlayState;
    class OverlayEntryState;

    /**
     * @brief State for an OverlayEntry.
     */
    class OverlayEntryState : public State<OverlayEntry>
    {
    public:
        void initState() override;
        void dispose() override;

        /** @brief Removes this entry from the overlay. */
        void remove();

        /** @brief Marks this entry for rebuild. */
        void markNeedsBuild();

        WidgetRef build(BuildContext& context) override;

    private:
        OverlayState* overlay_ = nullptr;
        friend class OverlayState;
    };

    /**
     * @brief A single entry in an Overlay.
     *
     * OverlayEntry is a widget that can be inserted into an Overlay. It
     * maintains independent state and can be removed or rebuilt without
     * affecting other entries in the overlay.
     *
     * Each entry can be positioned independently using Positioned or Align
     * widgets as its child.
     */
    class OverlayEntry : public StatefulWidget
    {
    public:
        /** @brief The widget to display in this overlay entry. */
        WidgetRef child;

        /** @brief Whether this entry blocks interaction with entries below it. */
        bool opaque = false;

        /** @brief Whether this entry should maintain state when moved. */
        bool maintain_state = false;

        /** @brief Callback when this entry is removed from the overlay. */
        std::function<void()> on_remove;

        OverlayEntry() = default;
        explicit OverlayEntry(WidgetRef c) : child(std::move(c)) {}

        std::unique_ptr<StateBase> createState() const override
        {
            return std::make_unique<OverlayEntryState>();
        }

        // Internal state access
        OverlayEntryState* entryState() const noexcept { return state_; }
        void setEntryState(OverlayEntryState* s) const noexcept { state_ = s; }

        // Factory
        static std::shared_ptr<OverlayEntry> create(WidgetRef child);

    private:
        mutable OverlayEntryState* state_ = nullptr;
    };

    /**
     * @brief State for Overlay that manages the entry list.
     */
    class OverlayState : public State<Overlay>
    {
    public:
        std::vector<std::shared_ptr<OverlayEntry>> entries;

        void initState() override;
        void dispose() override;

        void insert(std::shared_ptr<OverlayEntry> entry);
        void insertAt(int index, std::shared_ptr<OverlayEntry> entry);
        void remove(std::shared_ptr<OverlayEntry> entry);

        WidgetRef build(BuildContext& context) override;

        void didUpdateWidget(const Widget& old_widget) override;

    private:
        void _markDirty();
    };

    /**
     * @brief A stack of entries that can be managed independently.
     *
     * Overlay is a Stack that allows its children to be added and removed
     * dynamically. It's used for displaying floating UI like dialogs, menus,
     * tooltips, and snackbars.
     *
     * An Overlay is typically inserted at the top of the widget tree and
     * accessed via the static Overlay.of() method or showDialog() helper.
     *
     * Example:
     * @code
     * Overlay::create(
     *     // Initial entries
     *     OverlayEntry::create(
     *         Positioned::create(...)
     *     )
     * )
     * @endcode
     */
    class Overlay : public StatefulWidget
    {
    public:
        /** @brief Initial entries in the overlay. */
        std::vector<std::shared_ptr<OverlayEntry>> initial_entries;

        Overlay() = default;
        explicit Overlay(std::vector<std::shared_ptr<OverlayEntry>> entries)
            : initial_entries(std::move(entries))
        {}

        std::unique_ptr<StateBase> createState() const override
        {
            return std::make_unique<OverlayState>();
        }

        /**
         * @brief Inserts an entry into the overlay.
         *
         * The entry is inserted at the end of the list (on top).
         */
        static void insert(std::shared_ptr<OverlayEntry> entry);

        /**
         * @brief Inserts an entry at a specific index.
         */
        static void insertAt(int index, std::shared_ptr<OverlayEntry> entry);

        /**
         * @brief Removes an entry from the overlay.
         */
        static void remove(std::shared_ptr<OverlayEntry> entry);

        // Internal state management
        static void setGlobalState(OverlayState* state) noexcept;
        static OverlayState* globalState() noexcept;

    private:
        static OverlayState* global_state_;
    };

} // namespace systems::leal::campello_widgets
