#pragma once

#include <campello_widgets/ui/size.hpp>
#include <campello_widgets/ui/box_fit.hpp>
#include <campello_widgets/ui/alignment.hpp>

#include <memory>
#include <string>
#include <functional>
#include <future>
#include <optional>
#include <chrono>
#include <cstdint>

namespace systems::leal::campello_gpu { class Texture; class Device; }

namespace systems::leal::campello_widgets
{

    // Forward declarations
    class ImageCache;

    /**
     * @brief Configuration for image loading and display.
     */
    struct ImageConfiguration {
        std::shared_ptr<campello_gpu::Device> device;
        Size target_size = Size::zero();
        float device_pixel_ratio = 1.0f;
    };

    /**
     * @brief Represents decoded image data in CPU memory (before GPU upload).
     */
    struct DecodedImage {
        std::vector<uint8_t> pixels;  // RGBA pixel data
        int width = 0;
        int height = 0;
        int channels = 4;
    };

    /**
     * @brief Represents a loaded image with its metadata.
     */
    struct LoadedImage {
        std::shared_ptr<campello_gpu::Texture> texture;
        std::shared_ptr<DecodedImage> decoded;  // Raw pixel data (for lazy texture creation)
        int width = 0;
        int height = 0;
        int channels = 0;
        std::chrono::steady_clock::time_point load_time;

        /**
         * @brief Create GPU texture from decoded pixels.
         * Call this on the main thread with a valid device.
         */
        bool createTexture(campello_gpu::Device* device);
    };

    /**
     * @brief Abstract base class for image providers.
     * 
     * Similar to Flutter's ImageProvider, this defines the interface for
     * resolving images from various sources (file, network, assets).
     */
    class ImageProvider : public std::enable_shared_from_this<ImageProvider>
    {
    public:
        virtual ~ImageProvider() = default;

        /**
         * @brief Asynchronously loads the image and returns a texture.
         * 
         * This method should be called from a background thread. It handles
         * the complete loading pipeline: fetch (if network), decode, and upload.
         */
        virtual std::shared_ptr<LoadedImage> load(const ImageConfiguration& config) const = 0;

        /**
         * @brief Returns a unique key for cache identification.
         */
        virtual std::string cacheKey() const = 0;

        /**
         * @brief Compare two providers for equality.
         */
        virtual bool operator==(const ImageProvider& other) const = 0;

        bool operator!=(const ImageProvider& other) const { return !(*this == other); }

        /**
         * @brief Get the natural size of the image (if known before loading).
         * @return std::nullopt if size is not known ahead of time.
         */
        virtual std::optional<Size> getSize() const { return std::nullopt; }

    protected:
        ImageProvider() = default;
    };

    using ImageProviderRef = std::shared_ptr<ImageProvider>;

    /**
     * @brief Loads an image from the local file system.
     * 
     * Supports PNG, JPEG, WebP, BMP, TGA, and GIF formats via campello_image.
     * 
     * Example:
     * @code
     * auto image = Image::file("/path/to/photo.png");
     * @endcode
     */
    class FileImage : public ImageProvider
    {
    public:
        explicit FileImage(std::string file_path);

        std::shared_ptr<LoadedImage> load(const ImageConfiguration& config) const override;
        std::string cacheKey() const override;
        bool operator==(const ImageProvider& other) const override;
        std::optional<Size> getSize() const override;

        const std::string& filePath() const { return file_path_; }
        float scale() const { return scale_; }

    private:
        std::string file_path_;
        float scale_ = 1.0f;
        mutable std::optional<Size> cached_size_;
    };

    /**
     * @brief Loads an image from a network URL.
     * 
     * Images are fetched via platform-native HTTP APIs:
     * - macOS/iOS: CFNetwork/NSURLSession
     * - Windows: WinHTTP
     * - Android: Java URLConnection via JNI
     * - Linux: libcurl
     * 
     * Example:
     * @code
     * auto image = Image::network("https://example.com/photo.png");
     * @endcode
     */
    class NetworkImage : public ImageProvider
    {
    public:
        explicit NetworkImage(std::string url);
        
        NetworkImage(std::string url, 
                     std::optional<std::string> headers,
                     std::chrono::seconds timeout);

        std::shared_ptr<LoadedImage> load(const ImageConfiguration& config) const override;
        std::string cacheKey() const override;
        bool operator==(const ImageProvider& other) const override;

        const std::string& url() const { return url_; }
        std::chrono::seconds timeout() const { return timeout_; }

    private:
        std::string url_;
        std::optional<std::string> headers_;
        std::chrono::seconds timeout_ = std::chrono::seconds(30);
    };

    /**
     * @brief Loads an image from the application's asset bundle.
     * 
     * Platform-specific resolution:
     * - macOS/iOS: NSBundle mainBundle pathForResource
     * - Android: assets/ directory via AAssetManager
     * - Windows: Embedded resources or relative to executable
     * - Linux: Relative to executable or /usr/share
     * 
     * Supports automatic scale selection (@2x, @3x suffixes).
     * 
     * Example:
     * @code
     * auto image = Image::asset("images/logo.png");
     * @endcode
     */
    class AssetImage : public ImageProvider
    {
    public:
        explicit AssetImage(std::string asset_name);
        
        AssetImage(std::string asset_name, std::string package);

        std::shared_ptr<LoadedImage> load(const ImageConfiguration& config) const override;
        std::string cacheKey() const override;
        bool operator==(const ImageProvider& other) const override;

        const std::string& assetName() const { return asset_name_; }
        const std::string& package() const { return package_; }

    private:
        std::string asset_name_;
        std::string package_;
        
        // Platform-specific: resolve asset path
        std::string resolvePath(float device_pixel_ratio) const;
    };

    /**
     * @brief Loads an image from raw memory bytes.
     * 
     * Useful for images received from databases, IPC, or generated in memory.
     */
    class MemoryImage : public ImageProvider
    {
    public:
        MemoryImage(std::vector<uint8_t> bytes, std::string debug_name = "memory");

        std::shared_ptr<LoadedImage> load(const ImageConfiguration& config) const override;
        std::string cacheKey() const override;
        bool operator==(const ImageProvider& other) const override;

    private:
        std::vector<uint8_t> bytes_;
        std::string debug_name_;
    };

} // namespace systems::leal::campello_widgets
