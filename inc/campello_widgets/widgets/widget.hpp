#pragma once

#include <memory>
#include <typeinfo>
#include <source_location>
#include <campello_widgets/ui/key.hpp>
#include <campello_widgets/diagnostics/diagnosticable.hpp>

namespace systems::leal::campello_widgets
{

    class Element;

    /**
     * @brief Immutable description of a piece of UI.
     *
     * A Widget is a lightweight, immutable configuration object. It describes
     * *what* should be on screen but does not hold mutable state or render anything
     * itself. Widgets are cheap to create and discard — the framework reconciles
     * the new widget tree against the existing Element tree each frame.
     *
     * Every concrete widget must implement `createElement()` to produce the
     * appropriate Element subclass. Prefer subclassing `StatelessWidget` or
     * `StatefulWidget` over inheriting Widget directly.
     */
    /**
     * @brief Source location where a widget was instantiated.
     */
    struct WidgetLocation
    {
        std::string file;
        int line = 0;
        int column = 0;
        std::string function;

        bool isEmpty() const noexcept { return file.empty(); }

        std::string toString() const
        {
            return file + ":" + std::to_string(line);
        }
    };

    class Widget : public std::enable_shared_from_this<Widget>, public Diagnosticable
    {
    public:
        virtual ~Widget() = default;

        /**
         * @brief Optional identity key.
         *
         * When set, the reconciler uses key equality (instead of position) to
         * match this widget to an existing element. See `Key`, `ValueKey<T>`,
         * `UniqueKey`, and `GlobalKey`.
         */
        std::shared_ptr<Key> key;

        /**
         * @brief Creates the Element that manages this widget's lifecycle in the tree.
         *
         * Called by the framework the first time this widget type appears at a given
         * position in the tree. Subsequent renders reuse the existing Element and
         * call `Element::update()` instead.
         */
        virtual std::shared_ptr<Element> createElement() const = 0;

        /**
         * @brief Returns the dynamic type identity of this widget.
         *
         * Used during reconciliation to decide whether an existing Element can be
         * updated in-place (same type) or must be replaced (different type).
         */
        const std::type_info& widgetType() const noexcept { return typeid(*this); }

        std::string toStringShort() const override { return typeName(); }

        void debugFillProperties(DiagnosticsPropertyBuilder& properties) const override
        {
            if (!location_.isEmpty())
            {
                // Strip common prefix for brevity
                std::string file = location_.file;
                const std::string prefix = "/Users/rubenleal/Projects/campello_widgets/";
                if (file.find(prefix) == 0)
                    file = file.substr(prefix.size());
                properties.add(std::make_unique<StringProperty>("creation", file + ":" + std::to_string(location_.line)));
            }
        }

        // ------------------------------------------------------------------
        // Source location tracking
        // ------------------------------------------------------------------

        /**
         * @brief Records the source file and line where this widget was created.
         *
         * Called automatically by the `mw<>()` factory. You only need to call
         * this manually if you construct widgets with `std::make_shared`.
         */
        void captureLocation(const std::source_location& loc = std::source_location::current())
        {
            location_.file = loc.file_name();
            location_.line = static_cast<int>(loc.line());
            location_.column = static_cast<int>(loc.column());
            location_.function = loc.function_name();
        }

        /** @brief The source location where this widget was created, if any. */
        const WidgetLocation& creationLocation() const noexcept { return location_; }

    protected:
        WidgetLocation location_;
    };

    /** @brief Shared ownership handle to an immutable Widget. */
    using WidgetRef = std::shared_ptr<const Widget>;

    /** @brief Convenience alias for a brace-initialised child list.
     *
     *  Allows passing children inline without an explicit std::vector:
     *  @code
     *    make<Column>(WidgetList{a, b, c}, MainAxisAlignment::start)
     *  @endcode
     */
    using WidgetList = std::initializer_list<WidgetRef>;

} // namespace systems::leal::campello_widgets
