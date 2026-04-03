#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/async_snapshot.hpp>
#include <campello_widgets/ui/stream.hpp>

#include <functional>
#include <memory>
#include <optional>

namespace systems::leal::campello_widgets
{

    // Forward declaration
    template<typename T> class StreamBuilder;

    /**
     * @brief State for StreamBuilder<T>.
     *
     * Subscribes to the stream's data/error/done events and calls setState
     * on each event to trigger a rebuild.
     */
    template<typename T>
    class StreamBuilderState : public State<StreamBuilder<T>>
    {
    public:
        void initState() override
        {
            const auto& w = this->widget();
            if (w.initialData.has_value())
                snapshot_ = {ConnectionState::waiting, w.initialData, {}};
            else
                snapshot_ = AsyncSnapshot<T>::waiting();

            if (w.stream)
                subscribe(w.stream);
        }

        void dispose() override
        {
            unsubscribe();
        }

        void didUpdateWidget(const Widget& old_base) override
        {
            const auto& old_w = static_cast<const StreamBuilder<T>&>(old_base);
            const auto& new_w = this->widget();

            if (old_w.stream.get() != new_w.stream.get())
            {
                if (old_w.stream) unsubscribeFrom(old_w.stream);

                snapshot_ = new_w.initialData.has_value()
                    ? AsyncSnapshot<T>{ConnectionState::waiting, new_w.initialData, {}}
                    : AsyncSnapshot<T>::waiting();

                if (new_w.stream)
                    subscribe(new_w.stream);
            }
        }

        WidgetRef build(BuildContext& ctx) override
        {
            return this->widget().builder(ctx, snapshot_);
        }

    private:
        AsyncSnapshot<T> snapshot_;
        uint64_t         data_id_ = 0, error_id_ = 0, done_id_ = 0;

        void subscribe(std::shared_ptr<Stream<T>> s)
        {
            data_id_ = s->addListener([this](const T& v) {
                snapshot_ = {ConnectionState::active, v, {}};
                this->setState([](){});
            });
            error_id_ = s->addErrorListener([this](std::string e) {
                snapshot_.state = ConnectionState::active;
                snapshot_.error = std::move(e);
                this->setState([](){});
            });
            done_id_ = s->addDoneListener([this]() {
                snapshot_.state = ConnectionState::done;
                this->setState([](){});
            });
        }

        void unsubscribe()
        {
            const auto& w = this->widget();
            if (w.stream) unsubscribeFrom(w.stream);
        }

        void unsubscribeFrom(std::shared_ptr<Stream<T>> s)
        {
            if (data_id_)  { s->removeListener(data_id_);       data_id_  = 0; }
            if (error_id_) { s->removeErrorListener(error_id_); error_id_ = 0; }
            if (done_id_)  { s->removeDoneListener(done_id_);   done_id_  = 0; }
        }
    };

    /**
     * @brief Builds UI based on the latest event from a `Stream<T>`.
     *
     * Connect a `StreamController<T>` to produce events. The builder is called
     * each time a new value, error, or done event arrives.
     *
     * @code
     * auto ctrl = std::make_shared<StreamController<std::string>>();
     *
     * auto w = std::make_shared<StreamBuilder<std::string>>();
     * w->stream = ctrl->stream();
     * w->builder = [](BuildContext&, const AsyncSnapshot<std::string>& snap) {
     *     if (!snap.hasData())
     *         return std::make_shared<Text>("Waiting...");
     *     return std::make_shared<Text>(*snap.data);
     * };
     *
     * // From elsewhere:
     * ctrl->add("Hello");
     * @endcode
     */
    template<typename T>
    class StreamBuilder : public StatefulWidget
    {
    public:
        std::shared_ptr<Stream<T>>                                     stream;
        std::function<WidgetRef(BuildContext&, AsyncSnapshot<T>)>      builder;
        std::optional<T>                                               initialData;

        StreamBuilder() = default;
        explicit StreamBuilder(
            std::shared_ptr<Stream<T>> s,
            std::function<WidgetRef(BuildContext&, AsyncSnapshot<T>)> b)
            : stream(std::move(s)), builder(std::move(b))
        {}
        explicit StreamBuilder(
            std::shared_ptr<Stream<T>> s,
            std::function<WidgetRef(BuildContext&, AsyncSnapshot<T>)> b,
            T init_data)
            : stream(std::move(s)), builder(std::move(b)), initialData(std::move(init_data))
        {}

        std::unique_ptr<StateBase> createState() const override
        {
            return std::make_unique<StreamBuilderState<T>>();
        }
    };

} // namespace systems::leal::campello_widgets
