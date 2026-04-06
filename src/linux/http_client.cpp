#include <campello_widgets/ui/http_client.hpp>

#include <curl/curl.h>
#include <iostream>
#include <sstream>

namespace systems::leal::campello_widgets
{

    namespace {
        struct CurlGlobalInit {
            CurlGlobalInit() { curl_global_init(CURL_GLOBAL_DEFAULT); }
            ~CurlGlobalInit() { curl_global_cleanup(); }
        };
        
        CurlGlobalInit& getCurlInit() {
            static CurlGlobalInit init;
            return init;
        }

        size_t writeCallback(void* contents, size_t size, size_t nmemb, std::vector<uint8_t>* data)
        {
            size_t totalSize = size * nmemb;
            uint8_t* bytes = static_cast<uint8_t*>(contents);
            data->insert(data->end(), bytes, bytes + totalSize);
            return totalSize;
        }

        size_t headerCallback(char* buffer, size_t size, size_t nitems, std::unordered_map<std::string, std::string>* headers)
        {
            size_t totalSize = size * nitems;
            std::string header(buffer, totalSize);
            
            // Parse header line (format: "Key: Value")
            auto colonPos = header.find(':');
            if (colonPos != std::string::npos) {
                std::string key = header.substr(0, colonPos);
                std::string value = header.substr(colonPos + 1);
                
                // Trim whitespace
                auto start = value.find_first_not_of(" \t\r\n");
                auto end = value.find_last_not_of(" \t\r\n");
                if (start != std::string::npos && end != std::string::npos) {
                    value = value.substr(start, end - start + 1);
                }
                
                (*headers)[key] = value;
            }
            
            return totalSize;
        }
    }

    const char* HttpClient::implementationName()
    {
        return "libcurl (Linux)";
    }

    bool HttpClient::isSupportedUrl(const std::string& url)
    {
        return url.find("http://") == 0 || url.find("https://") == 0;
    }

    HttpResponse HttpClient::request(const HttpRequest& req)
    {
        // Ensure curl is initialized
        getCurlInit();

        HttpResponse response;

        CURL* curl = curl_easy_init();
        if (!curl) {
            response.error_message = "Failed to initialize CURL";
            return response;
        }

        // Set URL
        curl_easy_setopt(curl, CURLOPT_URL, req.url.c_str());

        // Set method
        if (req.method == "GET") {
            curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
        } else if (req.method == "POST") {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
        } else if (req.method == "PUT") {
            curl_easy_setopt(curl, CURLOPT_PUT, 1L);
        } else if (req.method == "DELETE") {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        }

        // Set body if present
        if (!req.body.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.body.data());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)req.body.size());
        }

        // Set timeout
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)req.timeout.count());
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, (long)req.timeout.count());

        // Set redirect handling
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, req.follow_redirects ? 1L : 0L);

        // Set SSL verification (enable for production)
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

        // Set write callback for body
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);

        // Set header callback
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.headers);

        // Set headers
        struct curl_slist* headerList = nullptr;
        for (const auto& [key, value] : req.headers) {
            std::string headerLine = key + ": " + value;
            headerList = curl_slist_append(headerList, headerLine.c_str());
        }
        if (headerList) {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
        }

        // Perform request
        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            response.error_message = curl_easy_strerror(res);
        } else {
            // Get HTTP response code
            long responseCode;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
            response.status_code = static_cast<int>(responseCode);
        }

        // Cleanup
        if (headerList) {
            curl_slist_free_all(headerList);
        }
        curl_easy_cleanup(curl);

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
