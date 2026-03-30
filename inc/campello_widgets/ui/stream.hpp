#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A push-based sequence of values of type T.
     *
     * Consumers subscribe via `addListener`, `addErrorListener`, and
     * `addDoneListener`. Values are delivered by a `StreamController<T>`.
     *
     * @code
     * auto ctrl = std::make_shared<StreamController<int>>();
     * auto s    = ctrl->stream();
     * s->addListener([](const int& v) { ... });
     * ctrl->add(42);
     * ctrl->close();
     * @endcode
     */
    template<typename T>
    class Stream
    {
    public:
        /** @brief Subscribe to data events. Returns a subscription ID. */
        uint64_t addListener(std::function<void(const T&)> cb)
        {
            uint64_t id = next_data_id_++;
            data_listeners_[id] = std::move(cb);
            return id;
        }

        void removeListener(uint64_t id) { data_listeners_.erase(id); }

        /** @brief Subscribe to error events. Returns a subscription ID. */
        uint64_t addErrorListener(std::function<void(std::string)> cb)
        {
            uint64_t id = next_err_id_++;
            error_listeners_[id] = std::move(cb);
            return id;
        }

        void removeErrorListener(uint64_t id) { error_listeners_.erase(id); }

        /** @brief Subscribe to the done (stream closed) event. Returns a subscription ID. */
        uint64_t addDoneListener(std::function<void()> cb)
        {
            uint64_t id = next_done_id_++;
            done_listeners_[id] = std::move(cb);
            return id;
        }

        void removeDoneListener(uint64_t id) { done_listeners_.erase(id); }

        // -- Internal: called by StreamController ----------------------------

        void _emit(const T& v)
        {
            auto copy = data_listeners_;
            for (auto& [id, fn] : copy) fn(v);
        }

        void _emitError(std::string e)
        {
            auto copy = error_listeners_;
            for (auto& [id, fn] : copy) fn(e);
        }

        void _emitDone()
        {
            auto copy = done_listeners_;
            for (auto& [id, fn] : copy) fn();
        }

    private:
        uint64_t next_data_id_ = 1, next_err_id_ = 1, next_done_id_ = 1;
        std::unordered_map<uint64_t, std::function<void(const T&)>> data_listeners_;
        std::unordered_map<uint64_t, std::function<void(std::string)>>  error_listeners_;
        std::unordered_map<uint64_t, std::function<void()>>             done_listeners_;
    };

    /**
     * @brief Produces events for a `Stream<T>`.
     *
     * Create one `StreamController` per logical data source. Hand the
     * `stream()` to widgets and call `add()` / `addError()` / `close()` to
     * push events.
     */
    template<typename T>
    class StreamController
    {
    public:
        StreamController() : stream_(std::make_shared<Stream<T>>()) {}

        /** @brief The stream that consumers subscribe to. */
        std::shared_ptr<Stream<T>> stream() const { return stream_; }

        /** @brief Pushes a value to all current listeners. */
        void add(T value) { stream_->_emit(value); }

        /** @brief Pushes an error string to all current error listeners. */
        void addError(std::string e) { stream_->_emitError(std::move(e)); }

        /** @brief Signals that the stream has ended. */
        void close() { stream_->_emitDone(); }

    private:
        std::shared_ptr<Stream<T>> stream_;
    };

} // namespace systems::leal::campello_widgets
