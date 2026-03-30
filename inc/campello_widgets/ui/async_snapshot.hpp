#pragma once

#include <optional>
#include <string>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Represents the current state of an async computation.
     *
     * Used by `FutureBuilder` and `StreamBuilder` to communicate the lifecycle
     * of a `std::shared_future<T>` or `Stream<T>` to the builder callback.
     */
    enum class ConnectionState
    {
        none,    ///< Not yet connected to any async source
        waiting, ///< Connected, waiting for first result
        active,  ///< Receiving data (StreamBuilder); or resolving (FutureBuilder)
        done,    ///< Completed (future resolved or stream closed)
    };

    /**
     * @brief Immutable snapshot of an async computation at a point in time.
     *
     * @tparam T  The value type produced by the async source.
     */
    template<typename T>
    struct AsyncSnapshot
    {
        ConnectionState  state = ConnectionState::none;
        std::optional<T> data;
        std::string      error;

        bool hasData()  const noexcept { return data.has_value(); }
        bool hasError() const noexcept { return !error.empty(); }

        static AsyncSnapshot<T> withData(T v)
        {
            return {ConnectionState::done, std::move(v), {}};
        }

        static AsyncSnapshot<T> withError(std::string e)
        {
            return {ConnectionState::done, {}, std::move(e)};
        }

        static AsyncSnapshot<T> waiting()
        {
            return {ConnectionState::waiting, {}, {}};
        }

        static AsyncSnapshot<T> none()
        {
            return {ConnectionState::none, {}, {}};
        }
    };

} // namespace systems::leal::campello_widgets
