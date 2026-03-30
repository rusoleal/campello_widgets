#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace systems::leal::campello_widgets
{

    class Element; // forward declaration — avoids circular include with widget.hpp

    // -----------------------------------------------------------------------
    // Key — abstract base
    // -----------------------------------------------------------------------

    /**
     * @brief Identity tag for a widget.
     *
     * When a widget carries a Key the reconciler uses key-equality rather than
     * position to match old and new widgets. This allows elements (and their
     * State) to survive reordering in a list.
     *
     * Subclass `ValueKey<T>` for the common case of a stable identifier,
     * `UniqueKey` to always force element recreation, or `GlobalKey` to locate
     * an element from anywhere in the tree.
     */
    class Key
    {
    public:
        virtual ~Key() = default;

        /** @brief Returns true when `other` has the same identity as this key. */
        virtual bool equals(const Key& other) const noexcept = 0;

        /** @brief Hash consistent with `equals`: equal keys must have equal hashes. */
        virtual std::size_t hash() const noexcept = 0;
    };

    // -----------------------------------------------------------------------
    // Hash / equality wrappers — for use as unordered_map<Key*, …> template args
    // -----------------------------------------------------------------------

    struct KeyPtrHash
    {
        std::size_t operator()(Key* k) const noexcept { return k->hash(); }
    };

    struct KeyPtrEqual
    {
        bool operator()(Key* a, Key* b) const noexcept { return a->equals(*b); }
    };

    // -----------------------------------------------------------------------
    // ValueKey<T>
    // -----------------------------------------------------------------------

    /**
     * @brief A key backed by a value of type T.
     *
     * Two `ValueKey<T>` instances are equal when their values compare equal
     * via `operator==`. T must be hashable via `std::hash<T>`.
     *
     * @code
     * auto w = std::make_shared<MyWidget>();
     * w->key = std::make_shared<ValueKey<int>>(item.id);
     * @endcode
     */
    template<typename T>
    class ValueKey : public Key
    {
    public:
        explicit ValueKey(T v) : value_(std::move(v)) {}

        const T& value() const noexcept { return value_; }

        bool equals(const Key& other) const noexcept override
        {
            const auto* o = dynamic_cast<const ValueKey<T>*>(&other);
            return o && o->value_ == value_;
        }

        std::size_t hash() const noexcept override
        {
            // Mix in the type to avoid collisions between ValueKey<int> and ValueKey<string>
            const std::size_t type_hash = typeid(T).hash_code();
            const std::size_t val_hash  = std::hash<T>{}(value_);
            return type_hash ^ (val_hash * 2654435761u);
        }

    private:
        T value_;
    };

    // -----------------------------------------------------------------------
    // ObjectKey
    // -----------------------------------------------------------------------

    /**
     * @brief A key backed by the identity (pointer address) of an object.
     *
     * Two `ObjectKey` instances are equal when they point to the same object.
     *
     * @code
     * auto w = std::make_shared<MyWidget>();
     * w->key = std::make_shared<ObjectKey>(someSharedPtr.get());
     * @endcode
     */
    class ObjectKey : public Key
    {
    public:
        explicit ObjectKey(const void* ptr) noexcept : ptr_(ptr) {}

        bool equals(const Key& other) const noexcept override
        {
            const auto* o = dynamic_cast<const ObjectKey*>(&other);
            return o && o->ptr_ == ptr_;
        }

        std::size_t hash() const noexcept override
        {
            return std::hash<const void*>{}(ptr_);
        }

    private:
        const void* ptr_;
    };

    // -----------------------------------------------------------------------
    // UniqueKey
    // -----------------------------------------------------------------------

    /**
     * @brief A key whose identity is itself — no two instances are equal.
     *
     * Use `UniqueKey` to prevent element reuse between two widget instances of
     * the same type. A common pattern for forcing a StatefulWidget to recreate
     * its State is to assign a freshly-constructed `UniqueKey` each build.
     *
     * @code
     * // Force re-creation every build:
     * w->key = std::make_shared<UniqueKey>();
     *
     * // Stable across builds (same instance reused):
     * static auto stableKey = std::make_shared<UniqueKey>();
     * w->key = stableKey;
     * @endcode
     */
    class UniqueKey : public Key
    {
    public:
        bool equals(const Key& other) const noexcept override { return this == &other; }

        std::size_t hash() const noexcept override
        {
            return std::hash<const void*>{}(this);
        }
    };

    // -----------------------------------------------------------------------
    // GlobalKey
    // -----------------------------------------------------------------------

    /**
     * @brief A key that provides access to the element it is attached to from
     *        anywhere in the application.
     *
     * GlobalKey uses object identity (like UniqueKey), so the *same* GlobalKey
     * instance must be kept alive and reused across builds to maintain the
     * association.
     *
     * @code
     * // Keep the key alive (e.g. in a State member):
     * auto form_key_ = std::make_shared<GlobalKey>();
     *
     * // Attach to a widget:
     * auto form = std::make_shared<MyForm>();
     * form->key = form_key_;
     *
     * // Access the element from elsewhere:
     * Element* el = form_key_->currentElement();
     * @endcode
     */
    class GlobalKey : public Key
    {
    public:
        GlobalKey()  = default;
        ~GlobalKey() override;

        /** @brief Returns the Element currently associated with this key, or nullptr. */
        Element* currentElement() const noexcept;

        bool equals(const Key& other) const noexcept override { return this == &other; }

        std::size_t hash() const noexcept override
        {
            return std::hash<const void*>{}(this);
        }

        // Internal — called by Element::mount / Element::unmount
        static void _register(GlobalKey* key, Element* element) noexcept;
        static void _unregister(GlobalKey* key) noexcept;

    private:
        static std::unordered_map<GlobalKey*, Element*> s_registry_;
    };

} // namespace systems::leal::campello_widgets
