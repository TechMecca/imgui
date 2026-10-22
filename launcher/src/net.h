#pragma once

// Minimal blocking HTTP(S) GET over WinHTTP. Call it from a worker thread.

#include <atomic>
#include <string>

// Returns true for HTTP 200 with the body in *body. Otherwise *error describes the failure;
// *status (optional) receives the HTTP status code, or 0 if no response arrived.
bool HttpGet(const std::string& url, std::string* body, std::string* error,
             const std::atomic<bool>* cancel = nullptr, unsigned long* status = nullptr);

std::string UrlEncode(const std::string& s);
