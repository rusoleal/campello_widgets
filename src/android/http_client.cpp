#include <campello_widgets/ui/http_client.hpp>

#include <jni.h>
#include <android/log.h>

#include <sstream>

namespace systems::leal::campello_widgets
{

    // Android JNI implementation
    // This requires the JNI environment to be available
    // For simplicity, this is a stub that logs an error
    // Full implementation would use JNI to call java.net.HttpURLConnection

    const char* HttpClient::implementationName()
    {
        return "Java HttpURLConnection (Android)";
    }

    bool HttpClient::isSupportedUrl(const std::string& url)
    {
        return url.find("http://") == 0 || url.find("https://") == 0;
    }

    HttpResponse HttpClient::request(const HttpRequest& req)
    {
        HttpResponse response;
        
        // Note: Full Android implementation requires JNI integration
        // This stub returns an error suggesting to use native curl on Android
        // or implement proper JNI calls
        
        __android_log_print(ANDROID_LOG_WARN, "campello_widgets",
            "HttpClient::request not fully implemented on Android. "
            "Consider linking with libcurl or implementing JNI integration.");

        response.error_message = "HTTP not implemented on Android. Use libcurl or JNI.";
        
        // If libcurl is available as fallback, we could try that:
        #ifdef HAS_CURL_FALLBACK
        // Delegate to curl implementation
        #endif
        
        return response;
    }

    HttpResponse HttpClient::get(const std::string& url, std::chrono::seconds timeout)
    {
        HttpRequest req;
        req.url = url;
        req.method = "GET";
        req.timeout = timeout;
        return request(req);
    }

    HttpResponse HttpClient::get(const std::string& url,
                                  const std::unordered_map<std::string, std::string>& headers,
                                  std::chrono::seconds timeout)
    {
        HttpRequest req;
        req.url = url;
        req.method = "GET";
        req.headers = headers;
        req.timeout = timeout;
        return request(req);
    }

    HttpResponse HttpClientWithProgress::download(const std::string& url,
                                                   DownloadProgressCallback progress_cb,
                                                   std::chrono::seconds timeout)
    {
        return HttpClient::get(url, timeout);
    }

} // namespace systems::leal::campello_widgets
