#include <campello_widgets/widgets/image_widget.hpp>

#include <campello_widgets/widgets/raw_image.hpp>
#include <campello_widgets/widgets/center.hpp>
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/widgets/circular_progress_indicator.hpp>
#include <campello_widgets/widgets/text.hpp>
#include <campello_widgets/ui/image_provider.hpp>
#include <campello_widgets/ui/image_loader.hpp>
#include <campello_widgets/ui/renderer.hpp>

#include <iostream>

namespace systems::leal::campello_widgets
{

    // ============================================================================
    // ImageWidgetState
    // ============================================================================

    void ImageWidgetState::initState()
    {
        State::initState();
        startLoading();
    }

    void ImageWidgetState::dispose()
    {
        stopPolling();
        cancelLoading();
        State::dispose();
    }

    void ImageWidgetState::didUpdateWidget(const Widget& old_base)
    {
        const auto& old_widget = static_cast<const ImageWidget&>(old_base);
        const auto& new_widget = widget();

        // Check if provider changed
        if (!new_widget.provider || 
            (old_widget.provider && new_widget.provider && 
             !(*old_widget.provider == *new_widget.provider)) ||
            (!old_widget.provider && new_widget.provider) ||
            (old_widget.provider && !new_widget.provider)) {
            
            cancelLoading();
            current_result_ = ImageLoadResult{};
            startLoading();
        }
    }

    void ImageWidgetState::startLoading()
    {
        const auto& w = widget();
        if (!w.provider) {
            std::cerr << "[ImageWidget] No provider set\n";
            return;
        }

        // Check cache first
        auto cache_key = w.provider->cacheKey();
        if (auto cached = ImageCache::instance().get(cache_key)) {
            std::cerr << "[ImageWidget] Cache hit for " << cache_key << "\n";
            current_result_.status = ImageLoadStatus::completed;
            current_result_.image = cached;
            last_cache_key_ = cache_key;
            return;  // Already loaded, no need for async
        }

        std::cerr << "[ImageWidget] Starting async load for " << cache_key << "\n";
        is_loading_ = true;
        done_ = false;
        last_cache_key_ = cache_key;

        // Get device from renderer context
        ImageConfiguration config;
        // The device is not passed here because texture creation happens on the main thread
        // The image loading (network/file decode) happens async without GPU context

        // Start async load
        load_future_ = ImageLoader::instance().loadAsync(w.provider, config);
        
        // Start polling via ticker
        startPolling();
    }

    void ImageWidgetState::startPolling()
    {
        auto* ts = TickerScheduler::active();
        if (!ts) {
            std::cerr << "[ImageWidget] No TickerScheduler available\n";
            return;
        }
        ticker_id_ = ts->subscribe([this](uint64_t) {
            checkFuture();
        });
        std::cerr << "[ImageWidget] Started polling, ticker_id=" << ticker_id_ << "\n";
    }

    void ImageWidgetState::stopPolling()
    {
        if (ticker_id_ != 0) {
            if (auto* ts = TickerScheduler::active()) {
                ts->unsubscribe(ticker_id_);
            }
            ticker_id_ = 0;
        }
    }

    void ImageWidgetState::checkFuture()
    {
        if (done_ || !is_loading_) return;
        if (!load_future_.valid()) return;

        auto status = load_future_.wait_for(std::chrono::seconds(0));
        if (status == std::future_status::ready) {
            done_ = true;
            stopPolling();
            current_result_ = load_future_.get();
            is_loading_ = false;
            std::cerr << "[ImageWidget] Load complete via ticker, status=" << (int)current_result_.status
                      << " has_image=" << (current_result_.image ? "yes" : "no") << "\n";

            // Create texture here on the main thread (ticker fires on main thread),
            // so build() can directly return RawImage without a second setState call.
            if (current_result_.status == ImageLoadStatus::completed &&
                current_result_.image && !current_result_.image->texture) {
                if (auto* renderer = detail::currentRenderer()) {
                    if (current_result_.image->createTexture(&renderer->device())) {
                        std::cerr << "[ImageWidget] Texture created in checkFuture\n";
                    }
                }
            }

            this->setState([](){});
        }
    }

    void ImageWidgetState::cancelLoading()
    {
        if (is_loading_ && widget().provider) {
            ImageLoader::instance().cancel(widget().provider);
        }
        is_loading_ = false;
    }

    WidgetRef ImageWidgetState::build(BuildContext& context)
    {
        const auto& w = widget();

        // Build based on current state
        switch (current_result_.status) {
            case ImageLoadStatus::pending:
            case ImageLoadStatus::loading:
                if (w.loading_builder) {
                    return w.loading_builder();
                }
                // Default loading indicator
                return Center::create(
                    SizedBox::square(32, std::make_shared<CircularProgressIndicator>())
                );

            case ImageLoadStatus::failed:
                if (w.error_builder) {
                    return w.error_builder(current_result_.error_message);
                }
                // Default error display
                return Center::create(
                    std::make_shared<Text>("Error: " + current_result_.error_message)
                );

            case ImageLoadStatus::completed:
                if (!current_result_.image || !current_result_.image->texture) {
                    // Texture not ready (rare: renderer unavailable during checkFuture)
                    if (w.loading_builder) {
                        return w.loading_builder();
                    }
                    return Center::create(
                        SizedBox::square(32, std::make_shared<CircularProgressIndicator>())
                    );
                }

                std::cerr << "[ImageWidget] Rendering image " << current_result_.image->width
                          << "x" << current_result_.image->height
                          << " texture_ptr=" << current_result_.image->texture.get() << "\n";

                // Build the actual image display
                Size display_size = w.width.has_value() && w.height.has_value()
                    ? Size{*w.width, *w.height}
                    : Size::zero();

                auto raw_image = RawImage::create(
                    current_result_.image->texture,
                    display_size,
                    w.fit,
                    w.alignment,
                    w.opacity
                );
                
                std::cerr << "[ImageWidget] RawImage created with texture=" << raw_image->texture.get() << "\n";

                // Apply frame builder if provided
                if (w.frame_builder) {
                    return w.frame_builder(context, raw_image, false);
                }

                return raw_image;
        }

        // Should not reach here
        return Center::create(
            std::make_shared<Text>("Unknown state")
        );
    }

    // ============================================================================
    // ImageWidget Factory Methods
    // ============================================================================

    std::shared_ptr<ImageWidget> ImageWidget::file(
        const std::string& path,
        BoxFit fit,
        std::optional<float> width,
        std::optional<float> height,
        Alignment alignment,
        float opacity,
        LoadingBuilder loading_builder,
        ErrorBuilder error_builder)
    {
        auto provider = std::make_shared<FileImage>(path);
        return createWithProvider(provider, fit, width, height, alignment, opacity,
                                  loading_builder, error_builder);
    }

    std::shared_ptr<ImageWidget> ImageWidget::network(
        const std::string& url,
        BoxFit fit,
        std::optional<float> width,
        std::optional<float> height,
        Alignment alignment,
        float opacity,
        std::chrono::seconds timeout,
        LoadingBuilder loading_builder,
        ErrorBuilder error_builder)
    {
        auto provider = std::make_shared<NetworkImage>(url);
        (void)timeout;  // TODO: Pass timeout to provider
        return createWithProvider(provider, fit, width, height, alignment, opacity,
                                  loading_builder, error_builder);
    }

    std::shared_ptr<ImageWidget> ImageWidget::asset(
        const std::string& name,
        BoxFit fit,
        std::optional<float> width,
        std::optional<float> height,
        Alignment alignment,
        float opacity,
        LoadingBuilder loading_builder,
        ErrorBuilder error_builder)
    {
        auto provider = std::make_shared<AssetImage>(name);
        return createWithProvider(provider, fit, width, height, alignment, opacity,
                                  loading_builder, error_builder);
    }

    std::shared_ptr<ImageWidget> ImageWidget::memory(
        std::vector<uint8_t> bytes,
        BoxFit fit,
        std::optional<float> width,
        std::optional<float> height,
        Alignment alignment,
        float opacity,
        LoadingBuilder loading_builder,
        ErrorBuilder error_builder)
    {
        auto provider = std::make_shared<MemoryImage>(std::move(bytes));
        return createWithProvider(provider, fit, width, height, alignment, opacity,
                                  loading_builder, error_builder);
    }

    std::shared_ptr<ImageWidget> ImageWidget::fromProvider(
        ImageProviderRef provider,
        BoxFit fit,
        std::optional<float> width,
        std::optional<float> height,
        Alignment alignment,
        float opacity,
        LoadingBuilder loading_builder,
        ErrorBuilder error_builder)
    {
        return createWithProvider(provider, fit, width, height, alignment, opacity,
                                  loading_builder, error_builder);
    }

    std::shared_ptr<ImageWidget> ImageWidget::createWithProvider(
        ImageProviderRef provider,
        BoxFit fit,
        std::optional<float> width,
        std::optional<float> height,
        Alignment alignment,
        float opacity,
        LoadingBuilder loading_builder,
        ErrorBuilder error_builder)
    {
        auto widget = std::make_shared<ImageWidget>();
        widget->provider = provider;
        widget->fit = fit;
        widget->width = width;
        widget->height = height;
        widget->alignment = alignment;
        widget->opacity = opacity;
        widget->loading_builder = loading_builder;
        widget->error_builder = error_builder;
        return widget;
    }

} // namespace systems::leal::campello_widgets
