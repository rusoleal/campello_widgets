#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/async_snapshot.hpp>
#include <campello_widgets/ui/ticker.hpp>

#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <optional>

namespace systems::leal::campello_widgets
{

    // Forward declaration
    template<typename T> class FutureBuilder;

    /**
     * @brief State for FutureBuilder<T>.
     *
     * Polls the shared_future each vsync tick using TickerScheduler.
     * When the future resolves (or throws), fires setState with the result.
     */
    template<typename T>
    class FutureBuilderState : public State<FutureBuilder<T>>
    {
    public:
        void initState() override
        {
            const auto& w = this->widget();
            if (w.initialData.has_value())
                snapshot_ = AsyncSnapshot<T>::withData(*w.initialData);

            if (w.future.valid())
            {
                snapshot_.state = ConnectionState::waiting;
                startPolling();
            }
        }

        void dispose() override
        {
            stopPolling();
        }

        void didUpdateWidget(const Widget& old_base) override
        {
            const auto& old_w = static_cast<const FutureBuilder<T>&>(old_base);
            const auto& new_w = this->widget();

            // Different future — restart
            if (old_w.future.get() != new_w.future.get())
            {
                stopPolling();
                done_ = false;
                snapshot_ = new_w.initialData.has_value()
                    ? AsyncSnapshot<T>::withData(*new_w.initialData)
                    : AsyncSnapshot<T>::none();

                if (new_w.future.valid())
                {
                    snapshot_.state = ConnectionState::waiting;
                    startPolling();
                }
            }
        }

        WidgetRef build(BuildContext& ctx) override
        {
            return this->widget().builder(ctx, snapshot_);
        }

    private:
        AsyncSnapshot<T> snapshot_;
        uint64_t         ticker_id_ = 0;
        bool             done_      = false;

        void startPolling()
        {
            auto* ts = TickerScheduler::active();
            if (!ts) return;
            ticker_id_ = ts->subscribe([this](uint64_t) {
                if (done_) return;
                const auto& w = this->widget();
                if (!w.future.valid()) return;

                auto status = w.future.wait_for(std::chrono::seconds(0));
                if (status == std::future_status::ready)
                {
                    done_ = true;
                    stopPolling();
                    try
                    {
                        snapshot_ = AsyncSnapshot<T>::withData(w.future.get());
                    }
                    catch (const std::exception& e)
                    {
                        snapshot_ = AsyncSnapshot<T>::withError(e.what());
                    }
                    catch (...)
                    {
                        snapshot_ = AsyncSnapshot<T>::withError("unknown error");
                    }
                    this->setState([](){});
                }
            });
        }

        void stopPolling()
        {
            if (ticker_id_ != 0)
            {
                if (auto* ts = TickerScheduler::active())
                    ts->unsubscribe(ticker_id_);
                ticker_id_ = 0;
            }
        }
    };

    /**
     * @brief Builds UI based on the latest snapshot of a `std::shared_future<T>`.
     *
     * Polls the future each vsync frame. While waiting, `snapshot.state` is
     * `ConnectionState::waiting`. Once resolved, `snapshot.state` is `done` and
     * `snapshot.data` or `snapshot.error` is populated.
     *
     * @code
     * auto fut = std::async(std::launch::async, []() -> int {
     *     std::this_thread::sleep_for(std::chrono::seconds(1));
     *     return 42;
     * }).share();
     *
     * auto w = std::make_shared<FutureBuilder<int>>();
     * w->future = fut;
     * w->builder = [](BuildContext&, const AsyncSnapshot<int>& snap) {
     *     if (snap.state == ConnectionState::waiting)
     *         return std::make_shared<CircularProgressIndicator>();
     *     if (snap.hasError())
     *         return std::make_shared<Text>("Error: " + snap.error);
     *     return std::make_shared<Text>("Result: " + std::to_string(*snap.data));
     * };
     * @endcode
     */
    template<typename T>
    class FutureBuilder : public StatefulWidget
    {
    public:
        std::shared_future<T>                                      future;
        std::function<WidgetRef(BuildContext&, AsyncSnapshot<T>)>  builder;
        std::optional<T>                                           initialData;

        std::unique_ptr<StateBase> createState() const override
        {
            return std::make_unique<FutureBuilderState<T>>();
        }
    };

} // namespace systems::leal::campello_widgets
