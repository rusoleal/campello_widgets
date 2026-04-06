#pragma once

#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/ui/image_provider.hpp>
#include <campello_widgets/ui/image_loader.hpp>
#include <campello_widgets/ui/async_snapshot.hpp>
#include <campello_widgets/ui/ticker.hpp>
#include <campello_widgets/ui/size.hpp>
#include <campello_widgets/ui/box_fit.hpp>
#include <campello_widgets/ui/alignment.hpp>

#include <memory>
#include <functional>
#include <variant>

namespace systems::leal::campello_widgets
{

    // Forward declarations
    class ImageWidget;

    /**
     * @brief State for ImageWidget.
     * 
     * Manages the async loading lifecycle using FutureBuilder pattern.
     */
    class ImageWidgetState : public State<ImageWidget>
    {
    public:
        void initState() override;
        void dispose() override;
        void didUpdateWidget(const Widget& old_base) override;
        WidgetRef build(BuildContext& context) override;

    private:
        void startLoading();
        void cancelLoading();
        void startPolling();
        void stopPolling();
        void checkFuture();

        std::future<ImageLoadResult> load_future_;
        ImageLoadResult current_result_;
        bool is_loading_ = false;
        bool done_ = false;
        std::string last_cache_key_;
        uint64_t ticker_id_ = 0;
    };

    /**
     * @brief A widget that displays an image.
     * 
     * Flutter-compatible API supporting multiple image sources:
     * - Image::file() - Load from local file system
     * - Image::network() - Load from URL
     * - Image::asset() - Load from app bundle
     * - Image::memory() - Load from bytes in memory
     * 
     * Example usage:
     * @code
     * // From file
     * auto image = Image::file("/path/to/photo.png");
     * 
     * // From network
     * auto image = Image::network("https://example.com/photo.png");
     * 
     * // From assets
     * auto image = Image::asset("images/logo.png");
     * 
     * // With options
     * auto image = Image::network("https://example.com/photo.png", {
     *     .fit = BoxFit::cover,
     *     .width = 200,
     *     .height = 200,
     *     .loading_builder = []() { return std::make_shared<CircularProgressIndicator>(); }
     * });
     * @endcode
     */
    class ImageWidget : public StatefulWidget
    {
    public:
        /**
         * @brief Builder function for loading state.
         */
        using LoadingBuilder = std::function<WidgetRef()>;

        /**
         * @brief Builder function for error state.
         */
        using ErrorBuilder = std::function<WidgetRef(const std::string& error)>;

        /**
         * @brief Builder function for frame rendering (for animated images).
         */
        using FrameBuilder = std::function<WidgetRef(BuildContext&, WidgetRef child, bool is_sync_loaded)>;

        // Factory methods (Flutter-compatible)
        
        /**
         * @brief Load an image from a file.
         */
        static std::shared_ptr<ImageWidget> file(
            const std::string& path,
            BoxFit fit = BoxFit::contain,
            std::optional<float> width = std::nullopt,
            std::optional<float> height = std::nullopt,
            Alignment alignment = Alignment::center(),
            float opacity = 1.0f,
            LoadingBuilder loading_builder = nullptr,
            ErrorBuilder error_builder = nullptr);

        /**
         * @brief Load an image from a network URL.
         */
        static std::shared_ptr<ImageWidget> network(
            const std::string& url,
            BoxFit fit = BoxFit::contain,
            std::optional<float> width = std::nullopt,
            std::optional<float> height = std::nullopt,
            Alignment alignment = Alignment::center(),
            float opacity = 1.0f,
            std::chrono::seconds timeout = std::chrono::seconds(30),
            LoadingBuilder loading_builder = nullptr,
            ErrorBuilder error_builder = nullptr);

        /**
         * @brief Load an image from the asset bundle.
         */
        static std::shared_ptr<ImageWidget> asset(
            const std::string& name,
            BoxFit fit = BoxFit::contain,
            std::optional<float> width = std::nullopt,
            std::optional<float> height = std::nullopt,
            Alignment alignment = Alignment::center(),
            float opacity = 1.0f,
            LoadingBuilder loading_builder = nullptr,
            ErrorBuilder error_builder = nullptr);

        /**
         * @brief Load an image from memory bytes.
         */
        static std::shared_ptr<ImageWidget> memory(
            std::vector<uint8_t> bytes,
            BoxFit fit = BoxFit::contain,
            std::optional<float> width = std::nullopt,
            std::optional<float> height = std::nullopt,
            Alignment alignment = Alignment::center(),
            float opacity = 1.0f,
            LoadingBuilder loading_builder = nullptr,
            ErrorBuilder error_builder = nullptr);

        /**
         * @brief Create an Image from a custom provider.
         */
        static std::shared_ptr<ImageWidget> fromProvider(
            ImageProviderRef provider,
            BoxFit fit = BoxFit::contain,
            std::optional<float> width = std::nullopt,
            std::optional<float> height = std::nullopt,
            Alignment alignment = Alignment::center(),
            float opacity = 1.0f,
            LoadingBuilder loading_builder = nullptr,
            ErrorBuilder error_builder = nullptr);

        // Direct construction for advanced use
        ImageWidget() = default;

        ImageProviderRef provider;
        BoxFit fit = BoxFit::contain;
        std::optional<float> width;
        std::optional<float> height;
        Alignment alignment = Alignment::center();
        float opacity = 1.0f;
        bool exclude_from_semantics = false;
        bool gapless_playback = false;
        bool is_antialias = true;
        
        LoadingBuilder loading_builder;
        ErrorBuilder error_builder;
        FrameBuilder frame_builder;

        std::unique_ptr<StateBase> createState() const override
        {
            return std::make_unique<ImageWidgetState>();
        }

    private:
        static std::shared_ptr<ImageWidget> createWithProvider(
            ImageProviderRef provider,
            BoxFit fit,
            std::optional<float> width,
            std::optional<float> height,
            Alignment alignment,
            float opacity,
            LoadingBuilder loading_builder,
            ErrorBuilder error_builder);
    };

    // Note: We don't provide an 'Image' alias here because it conflicts with
    // the existing Image widget (raw texture display). Use ImageWidget explicitly.

} // namespace systems::leal::campello_widgets
