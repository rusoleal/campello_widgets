#include <campello_widgets/ui/http_client.hpp>

#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

namespace systems::leal::campello_widgets
{

    const char* HttpClient::implementationName()
    {
        return "WinHTTP (Windows)";
    }

    bool HttpClient::isSupportedUrl(const std::string& url)
    {
        return url.find("http://") == 0 || url.find("https://") == 0;
    }

    HttpResponse HttpClient::request(const HttpRequest& req)
    {
        HttpResponse response;

        // Parse URL to extract host, port, and path
        URL_COMPONENTS urlComponents = {0};
        urlComponents.dwStructSize = sizeof(urlComponents);

        wchar_t hostName[256] = {0};
        wchar_t urlPath[2048] = {0};
        wchar_t scheme[16] = {0};

        urlComponents.lpszHostName = hostName;
        urlComponents.dwHostNameLength = _countof(hostName);
        urlComponents.lpszUrlPath = urlPath;
        urlComponents.dwUrlPathLength = _countof(urlPath);
        urlComponents.lpszScheme = scheme;
        urlComponents.dwSchemeLength = _countof(scheme);

        std::wstring wideUrl(req.url.begin(), req.url.end());
        
        if (!WinHttpCrackUrl(wideUrl.c_str(), 0, 0, &urlComponents)) {
            response.error_message = "Failed to parse URL: " + req.url;
            return response;
        }

        // Determine if HTTPS
        bool isHttps = (wcscmp(scheme, L"https") == 0);
        DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;

        // Open session
        HINTERNET hSession = WinHttpOpen(
            L"campello_widgets/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0);

        if (!hSession) {
            response.error_message = "Failed to open HTTP session";
            return response;
        }

        // Set timeout
        int timeoutMs = static_cast<int>(req.timeout.count()) * 1000;
        WinHttpSetTimeouts(hSession, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

        // Connect
        HINTERNET hConnect = WinHttpConnect(
            hSession, hostName, urlComponents.nPort, 0);

        if (!hConnect) {
            response.error_message = "Failed to connect to host";
            WinHttpCloseHandle(hSession);
            return response;
        }

        // Create request
        std::wstring wideMethod(req.method.begin(), req.method.end());
        HINTERNET hRequest = WinHttpOpenRequest(
            hConnect, wideMethod.c_str(), urlPath,
            nullptr, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES, flags);

        if (!hRequest) {
            response.error_message = "Failed to create request";
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return response;
        }

        // Add headers
        for (const auto& [key, value] : req.headers) {
            std::string headerLine = key + ": " + value + "\r\n";
            std::wstring wideHeader(headerLine.begin(), headerLine.end());
            WinHttpAddRequestHeaders(hRequest, wideHeader.c_str(), (ULONG)-1L,
                                     WINHTTP_ADDREQ_FLAG_ADD);
        }

        // Send request
        BOOL result = WinHttpSendRequest(
            hRequest,
            WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            req.body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)req.body.data(),
            (DWORD)req.body.size(),
            (DWORD)req.body.size(), 0);

        if (!result) {
            response.error_message = "Failed to send request";
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return response;
        }

        // Receive response
        result = WinHttpReceiveResponse(hRequest, nullptr);
        if (!result) {
            response.error_message = "Failed to receive response";
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return response;
        }

        // Get status code
        DWORD statusCode = 0;
        DWORD statusCodeSize = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                           WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize,
                           WINHTTP_NO_HEADER_INDEX);
        response.status_code = static_cast<int>(statusCode);

        // Read response body
        std::vector<uint8_t> body;
        DWORD bytesAvailable = 0;
        DWORD bytesRead = 0;
        std::vector<uint8_t> buffer(4096);

        while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0) {
            if (bytesAvailable > buffer.size()) {
                buffer.resize(bytesAvailable);
            }
            
            if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead)) {
                body.insert(body.end(), buffer.begin(), buffer.begin() + bytesRead);
            }
        }

        response.body = std::move(body);

        // Cleanup
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

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
