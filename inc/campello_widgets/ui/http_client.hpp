#pragma once

#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <optional>
#include <unordered_map>

namespace systems::leal::campello_widgets
{

    /**
     * @brief HTTP response structure.
     */
    struct HttpResponse {
        int status_code = 0;
        std::vector<uint8_t> body;
        std::unordered_map<std::string, std::string> headers;
        std::string error_message;
        bool success() const { return status_code >= 200 && status_code < 300; }
    };

    /**
     * @brief HTTP request structure.
     */
    struct HttpRequest {
        std::string url;
        std::string method = "GET";
        std::unordered_map<std::string, std::string> headers;
        std::vector<uint8_t> body;
        std::chrono::seconds timeout = std::chrono::seconds(30);
        bool follow_redirects = true;
    };

    /**
     * @brief Simple cross-platform HTTP client.
     * 
     * Platform implementations:
     * - macOS/iOS: CFNetwork/CFHTTPStream
     * - Windows: WinHTTP
     * - Android: JNI call to java.net.HttpURLConnection
     * - Linux: libcurl
     * 
     * This is intentionally simple - for advanced use cases, use a dedicated
     * networking library.
     */
    class HttpClient
    {
    public:
        /**
         * @brief Perform synchronous HTTP request.
         * 
         * This blocks the calling thread - use from worker threads only.
         */
        static HttpResponse request(const HttpRequest& req);

        /**
         * @brief Convenience method for simple GET request.
         */
        static HttpResponse get(const std::string& url, 
                                 std::chrono::seconds timeout = std::chrono::seconds(30));

        /**
         * @brief Convenience method for GET with custom headers.
         */
        static HttpResponse get(const std::string& url,
                                 const std::unordered_map<std::string, std::string>& headers,
                                 std::chrono::seconds timeout = std::chrono::seconds(30));

        /**
         * @brief Check if URL is supported by this client.
         * (e.g., https:// is supported, file:// is not)
         */
        static bool isSupportedUrl(const std::string& url);

        /**
         * @brief Get a descriptive name of the platform implementation.
         */
        static const char* implementationName();
    };

    /**
     * @brief Progress callback for long downloads.
     */
    using DownloadProgressCallback = std::function<void(size_t bytes_received, size_t total_bytes)>;

    /**
     * @brief Extended HTTP client with progress reporting.
     */
    class HttpClientWithProgress : public HttpClient
    {
    public:
        static HttpResponse download(const std::string& url,
                                      DownloadProgressCallback progress_cb,
                                      std::chrono::seconds timeout = std::chrono::seconds(30));
    };

} // namespace systems::leal::campello_widgets
