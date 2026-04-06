#include <campello_widgets/ui/http_client.hpp>

#import <Foundation/Foundation.h>

#include <iostream>
#include <sstream>
#include <future>

namespace systems::leal::campello_widgets
{

    const char* HttpClient::implementationName()
    {
        return "NSURLSession (macOS/iOS)";
    }

    bool HttpClient::isSupportedUrl(const std::string& url)
    {
        return url.find("http://") == 0 || url.find("https://") == 0;
    }

    HttpResponse HttpClient::request(const HttpRequest& req)
    {
        HttpResponse response;
        
        // Use a separate autorelease pool for this thread
        @autoreleasepool {
            // Ensure we have a valid URL string
            if (req.url.empty()) {
                response.error_message = "Empty URL";
                return response;
            }
            
            NSString* urlString = [NSString stringWithUTF8String:req.url.c_str()];
            if (!urlString) {
                response.error_message = "Invalid URL encoding: " + req.url;
                return response;
            }
            
            NSURL* url = [NSURL URLWithString:urlString];
            if (!url) {
                response.error_message = "Invalid URL: " + req.url;
                return response;
            }

            // Create request
            NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:url];
            request.HTTPMethod = [NSString stringWithUTF8String:req.method.c_str()];
            request.timeoutInterval = req.timeout.count();
            
            // Add headers
            for (const auto& [key, value] : req.headers) {
                NSString* headerKey = [NSString stringWithUTF8String:key.c_str()];
                NSString* headerValue = [NSString stringWithUTF8String:value.c_str()];
                if (headerKey && headerValue) {
                    [request setValue:headerValue forHTTPHeaderField:headerKey];
                }
            }
            
            // Set body if present
            if (!req.body.empty()) {
                NSData* bodyData = [NSData dataWithBytes:req.body.data() 
                                                  length:req.body.size()];
                request.HTTPBody = bodyData;
            }

            // Create a serial queue for this request's callbacks
            dispatch_queue_t callbackQueue = dispatch_queue_create("com.campello.http", DISPATCH_QUEUE_SERIAL);
            
            // Create semaphore for synchronous execution
            dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
            
            // Use __block to capture values from completion handler
            __block NSData* responseData = nil;
            __block NSHTTPURLResponse* httpResponse = nil;
            __block NSError* responseError = nil;

            // Create session configuration with our callback queue
            NSURLSessionConfiguration* config = [NSURLSessionConfiguration defaultSessionConfiguration];
            config.timeoutIntervalForRequest = req.timeout.count();
            config.timeoutIntervalForResource = req.timeout.count() + 5;
            
            NSURLSession* session = [NSURLSession sessionWithConfiguration:config
                                                                  delegate:nil
                                                             delegateQueue:nil];
            
            NSURLSessionDataTask* task = [session dataTaskWithRequest:request
                completionHandler:^(NSData* data, NSURLResponse* urlResponse, NSError* error) {
                    @autoreleasepool {
                        responseData = data;
                        httpResponse = (NSHTTPURLResponse*)urlResponse;
                        responseError = error;
                        dispatch_semaphore_signal(semaphore);
                    }
                }];

            [task resume];
            
            // Wait for completion with timeout
            dispatch_time_t timeout_time = dispatch_time(DISPATCH_TIME_NOW, 
                (int64_t)(req.timeout.count() + 5) * NSEC_PER_SEC);
            long wait_result = dispatch_semaphore_wait(semaphore, timeout_time);
            
            [session finishTasksAndInvalidate];
            
            if (wait_result != 0) {
                [task cancel];
                response.error_message = "Request timeout";
                return response;
            }

            // Process response - copy data while still in autorelease pool
            if (responseError) {
                NSString* errorDesc = [responseError localizedDescription];
                if (errorDesc) {
                    response.error_message = [errorDesc UTF8String];
                } else {
                    response.error_message = "Unknown network error";
                }
                return response;
            }

            if (httpResponse) {
                response.status_code = (int)[httpResponse statusCode];
                
                // Copy headers
                NSDictionary* headers = [httpResponse allHeaderFields];
                if (headers) {
                    for (NSString* key in headers) {
                        NSString* value = headers[key];
                        if (key && value) {
                            const char* keyStr = [key UTF8String];
                            const char* valueStr = [value UTF8String];
                            if (keyStr && valueStr) {
                                response.headers[keyStr] = valueStr;
                            }
                        }
                    }
                }
            }

            // Copy response body
            if (responseData) {
                NSUInteger length = [responseData length];
                if (length > 0) {
                    const uint8_t* bytes = (const uint8_t*)[responseData bytes];
                    response.body.assign(bytes, bytes + length);
                }
            }
        }

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
        (void)progress_cb;
        return HttpClient::get(url, timeout);
    }

} // namespace systems::leal::campello_widgets
