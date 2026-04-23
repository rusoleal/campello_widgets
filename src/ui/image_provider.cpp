#include <campello_widgets/ui/image_provider.hpp>
#include <campello_widgets/ui/http_client.hpp>
#include <campello_widgets/ui/image_loader.hpp>

#include <campello_gpu/device.hpp>
#include <campello_gpu/texture.hpp>

#include <campello_image/image.hpp>

#include <filesystem>
#include <fstream>
#include <cstring>
#include <iostream>

namespace systems::leal::campello_widgets
{

    // ============================================================================
    // FileImage
    // ============================================================================

    FileImage::FileImage(std::string file_path)
        : file_path_(std::move(file_path))
        , scale_(1.0f)
    {
    }

    // Helper function to decode image data using campello_image
    static std::shared_ptr<DecodedImage> decodeImageData(const uint8_t* data, size_t size, 
                                                          const std::string& debug_name)
    {
        auto decoded = std::make_shared<DecodedImage>();
        
        // Use campello_image to decode the image
        auto img = campello_image::Image::fromMemory(data, size);
        if (!img) {
            throw std::runtime_error("Failed to decode image: " + debug_name);
        }
        
        decoded->width = static_cast<int>(img->getWidth());
        decoded->height = static_cast<int>(img->getHeight());
        decoded->channels = 4;  // campello_image v0.4.0+ returns RGBA8 for LDR, RGBA32F for HDR
        
        // For now, only RGBA8 is supported by the texture pipeline
        if (img->getFormat() != campello_image::ImageFormat::rgba8) {
            throw std::runtime_error("HDR/FP image formats not yet supported: " + debug_name);
        }
        
        // Copy pixel data to vector (getData() returns const void* since v0.4.0)
        size_t data_size = img->getDataSize();
        decoded->pixels.resize(data_size);
        std::memcpy(decoded->pixels.data(), img->getData(), data_size);
        
        // Debug: print first pixel
        if (!decoded->pixels.empty()) {
            std::cerr << "[ImageProvider] First pixel: R=" << (int)decoded->pixels[0] 
                      << " G=" << (int)decoded->pixels[1] 
                      << " B=" << (int)decoded->pixels[2] 
                      << " A=" << (int)decoded->pixels[3] << "\n";
        }
        
        return decoded;
    }

    bool LoadedImage::createTexture(campello_gpu::Device* device)
    {
        if (texture || !decoded || !device) {
            std::cerr << "[ImageProvider] createTexture early exit: has_texture=" << (texture ? "yes" : "no")
                      << " has_decoded=" << (decoded ? "yes" : "no") << " has_device=" << (device ? "yes" : "no") << "\n";
            return false;
        }

        std::cerr << "[ImageProvider] Creating texture " << decoded->width << "x" << decoded->height 
                  << " pixels_size=" << decoded->pixels.size() << "\n";

        auto new_texture = device->createTexture(
            campello_gpu::TextureType::tt2d,
            campello_gpu::PixelFormat::rgba8unorm,
            decoded->width, decoded->height, 1, 1, 1,
            static_cast<campello_gpu::TextureUsage>(
                static_cast<int>(campello_gpu::TextureUsage::textureBinding) |
                static_cast<int>(campello_gpu::TextureUsage::copyDst))
        );

        if (!new_texture) {
            std::cerr << "[ImageProvider] createTexture failed\n";
            return false;
        }

        size_t data_size = decoded->pixels.size();
        std::cerr << "[ImageProvider] Uploading " << data_size << " bytes...\n";
        if (!new_texture->upload(0, data_size, decoded->pixels.data())) {
            std::cerr << "[ImageProvider] upload failed\n";
            return false;
        }
        std::cerr << "[ImageProvider] Upload successful, tex_size=" << new_texture->getWidth() << "x" << new_texture->getHeight() << "\n";

        texture = std::move(new_texture);
        width = decoded->width;
        height = decoded->height;
        channels = decoded->channels;
        
        // Free decoded pixels to save memory
        decoded.reset();
        
        return true;
    }

    std::shared_ptr<LoadedImage> FileImage::load(const ImageConfiguration& config) const
    {
        (void)config;  // Device not needed during decode - we'll create texture later

        if (!std::filesystem::exists(file_path_)) {
            throw std::runtime_error("File not found: " + file_path_);
        }

        // Read file into memory
        std::ifstream file(file_path_, std::ios::binary | std::ios::ate);
        if (!file) {
            throw std::runtime_error("Cannot open file: " + file_path_);
        }

        auto size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> buffer(size);
        if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
            throw std::runtime_error("Failed to read file: " + file_path_);
        }

        // Decode image (but don't create texture yet)
        auto decoded = decodeImageData(buffer.data(), buffer.size(), file_path_);

        auto result = std::make_shared<LoadedImage>();
        result->decoded = decoded;
        result->width = decoded->width;
        result->height = decoded->height;
        result->channels = decoded->channels;
        result->load_time = std::chrono::steady_clock::now();

        return result;
    }

    std::string FileImage::cacheKey() const
    {
        // Use absolute path + modification time for cache key
        try {
            auto abs_path = std::filesystem::canonical(file_path_).string();
            auto mtime = std::filesystem::last_write_time(file_path_);
            auto mtime_count = static_cast<long long>(mtime.time_since_epoch().count());
            return "file:" + abs_path + "@" + std::to_string(mtime_count);
        } catch (...) {
            return "file:" + file_path_;
        }
    }

    bool FileImage::operator==(const ImageProvider& other) const
    {
        if (auto* other_file = dynamic_cast<const FileImage*>(&other)) {
            return file_path_ == other_file->file_path_ && 
                   scale_ == other_file->scale_;
        }
        return false;
    }

    std::optional<Size> FileImage::getSize() const
    {
        if (cached_size_.has_value()) {
            return cached_size_;
        }

        // Try to get size without fully loading using campello_image
        auto img = campello_image::Image::fromFile(file_path_.c_str());
        if (img) {
            cached_size_ = Size{static_cast<float>(img->getWidth()), 
                                static_cast<float>(img->getHeight())};
            return cached_size_;
        }

        return std::nullopt;
    }

    // ============================================================================
    // NetworkImage
    // ============================================================================

    NetworkImage::NetworkImage(std::string url)
        : url_(std::move(url))
    {
    }

    NetworkImage::NetworkImage(std::string url,
                               std::optional<std::string> headers,
                               std::chrono::seconds timeout)
        : url_(std::move(url))
        , headers_(std::move(headers))
        , timeout_(timeout)
    {
    }

    std::shared_ptr<LoadedImage> NetworkImage::load(const ImageConfiguration& config) const
    {
        (void)config;  // Device not needed during decode

        // Fetch from network
        auto response = HttpClient::get(url_, timeout_);
        
        if (!response.success()) {
            throw std::runtime_error("HTTP error " + std::to_string(response.status_code) + 
                                     ": " + response.error_message);
        }

        if (response.body.empty()) {
            throw std::runtime_error("Empty response from: " + url_);
        }

        // Decode image (but don't create texture yet)
        auto decoded = decodeImageData(response.body.data(), response.body.size(), url_);

        auto result = std::make_shared<LoadedImage>();
        result->decoded = decoded;
        result->width = decoded->width;
        result->height = decoded->height;
        result->channels = decoded->channels;
        result->load_time = std::chrono::steady_clock::now();

        return result;
    }

    std::string NetworkImage::cacheKey() const
    {
        return "network:" + url_;
    }

    bool NetworkImage::operator==(const ImageProvider& other) const
    {
        if (auto* other_net = dynamic_cast<const NetworkImage*>(&other)) {
            return url_ == other_net->url_;
        }
        return false;
    }

    // ============================================================================
    // AssetImage
    // ============================================================================

    AssetImage::AssetImage(std::string asset_name)
        : asset_name_(std::move(asset_name))
    {
    }

    AssetImage::AssetImage(std::string asset_name, std::string package)
        : asset_name_(std::move(asset_name))
        , package_(std::move(package))
    {
    }

    std::string AssetImage::resolvePath(float device_pixel_ratio) const
    {
        // Platform-specific implementation will be in platform files
        // This is a stub that looks in common locations
        
        std::string base_name = asset_name_;
        std::string ext;
        
        // Extract extension
        auto dot_pos = base_name.rfind('.');
        if (dot_pos != std::string::npos) {
            ext = base_name.substr(dot_pos);
            base_name = base_name.substr(0, dot_pos);
        }

        // Try different scale suffixes
        std::vector<std::string> candidates;
        
        if (device_pixel_ratio >= 3.0f) {
            candidates.push_back(base_name + "@3x" + ext);
        }
        if (device_pixel_ratio >= 2.0f) {
            candidates.push_back(base_name + "@2x" + ext);
        }
        candidates.push_back(base_name + ext);

        // Search in asset paths
        std::vector<std::filesystem::path> search_paths = {
            "assets/",
            "../assets/",
            "../../assets/",
            "Resources/",  // macOS bundle
            "../Resources/",
        };

        for (const auto& search_path : search_paths) {
            for (const auto& candidate : candidates) {
                auto full_path = search_path / candidate;
                if (std::filesystem::exists(full_path)) {
                    return full_path.string();
                }
            }
        }

        // Fallback to original name
        return asset_name_;
    }

    std::shared_ptr<LoadedImage> AssetImage::load(const ImageConfiguration& config) const
    {
        std::string resolved_path = resolvePath(config.device_pixel_ratio);
        
        // Delegate to FileImage
        FileImage file_provider(resolved_path);
        return file_provider.load(config);
    }

    std::string AssetImage::cacheKey() const
    {
        std::string key = "asset:" + asset_name_;
        if (!package_.empty()) {
            key = "package:" + package_ + "/" + key;
        }
        return key;
    }

    bool AssetImage::operator==(const ImageProvider& other) const
    {
        if (auto* other_asset = dynamic_cast<const AssetImage*>(&other)) {
            return asset_name_ == other_asset->asset_name_ &&
                   package_ == other_asset->package_;
        }
        return false;
    }

    // ============================================================================
    // MemoryImage
    // ============================================================================

    MemoryImage::MemoryImage(std::vector<uint8_t> bytes, std::string debug_name)
        : bytes_(std::move(bytes))
        , debug_name_(std::move(debug_name))
    {
    }

    std::shared_ptr<LoadedImage> MemoryImage::load(const ImageConfiguration& config) const
    {
        (void)config;  // Device not needed during decode

        if (bytes_.empty()) {
            throw std::runtime_error("Empty image data: " + debug_name_);
        }

        // Decode image (but don't create texture yet)
        auto decoded = decodeImageData(bytes_.data(), bytes_.size(), debug_name_);

        auto result = std::make_shared<LoadedImage>();
        result->decoded = decoded;
        result->width = decoded->width;
        result->height = decoded->height;
        result->channels = decoded->channels;
        result->load_time = std::chrono::steady_clock::now();

        return result;
    }

    std::string MemoryImage::cacheKey() const
    {
        // Hash the first 1KB for a cache key
        size_t hash_len = std::min(bytes_.size(), static_cast<size_t>(1024));
        size_t hash = 0;
        for (size_t i = 0; i < hash_len; ++i) {
            hash = hash * 31 + bytes_[i];
        }
        return "memory:" + debug_name_ + "@" + std::to_string(hash);
    }

    bool MemoryImage::operator==(const ImageProvider& other) const
    {
        if (auto* other_mem = dynamic_cast<const MemoryImage*>(&other)) {
            return bytes_ == other_mem->bytes_;
        }
        return false;
    }

    // ============================================================================
    // Format Detection
    // ============================================================================

    ImageFormat detectImageFormatFromBytes(const uint8_t* data, size_t size)
    {
        if (size < 4) return ImageFormat::unknown;

        // PNG: 89 50 4E 47
        if (data[0] == 0x89 && data[1] == 0x50 && 
            data[2] == 0x4E && data[3] == 0x47) {
            return ImageFormat::png;
        }

        // JPEG: FF D8 FF
        if (data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) {
            return ImageFormat::jpeg;
        }

        // WebP: RIFF....WEBP
        if (size >= 12 && 
            data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F' &&
            data[8] == 'W' && data[9] == 'E' && data[10] == 'B' && data[11] == 'P') {
            return ImageFormat::webp;
        }

        // GIF: GIF87a or GIF89a
        if (size >= 6 && data[0] == 'G' && data[1] == 'I' && data[2] == 'F' &&
            data[3] == '8' && (data[4] == '7' || data[4] == '9') && data[5] == 'a') {
            return ImageFormat::gif;
        }

        // BMP: BM
        if (data[0] == 'B' && data[1] == 'M') {
            return ImageFormat::bmp;
        }

        // TGA: No magic, would need to check footer (not implemented)

        return ImageFormat::unknown;
    }

    ImageFormat detectImageFormat(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) return ImageFormat::unknown;

        uint8_t header[16];
        file.read(reinterpret_cast<char*>(header), sizeof(header));
        
        return detectImageFormatFromBytes(header, file.gcount());
    }

} // namespace systems::leal::campello_widgets
