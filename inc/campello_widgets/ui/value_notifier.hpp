#pragma once

#include <cstdint>
#include <functional>
#include <unordered_map>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Holds a value and notifies listeners when it changes.
     *
     * Works like Dart/Flutter's `ValueNotifier<T>`. Listeners receive no
     * arguments — they should read the new value via `value()`.
     *
     * @code
     * auto counter = std::make_shared<ValueNotifier<int>>(0);
     * counter->addListener([&]() { std::cout << counter->value(); });
     * counter->setValue(42); // triggers all listeners
     * @endcode
     */
    template<typename T>
    class ValueNotifier
    {
    public:
        explicit ValueNotifier(T v = {}) : value_(std::move(v)) {}

        /** @brief The current value. */
        const T& value() const noexcept { return value_; }

        /** @brief Updates the value and notifies all listeners. */
        void setValue(T v)
        {
            value_ = std::move(v);
            // Iterate a snapshot in case a listener removes itself
            auto copy = listeners_;
            for (auto& [id, fn] : copy)
                fn();
        }

        /**
         * @brief Registers a change listener.
         * @return A subscription ID for use with `removeListener`.
         */
        uint64_t addListener(std::function<void()> cb)
        {
            uint64_t id = next_id_++;
            listeners_[id] = std::move(cb);
            return id;
        }

        /** @brief Unregisters a previously added listener. */
        void removeListener(uint64_t id)
        {
            listeners_.erase(id);
        }

    private:
        T value_;
        uint64_t next_id_ = 1;
        std::unordered_map<uint64_t, std::function<void()>> listeners_;
    };

} // namespace systems::leal::campello_widgets
