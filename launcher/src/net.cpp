#include "net.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>

#include <ctype.h>
#include <stdio.h>

static std::wstring Widen(const std::string& s)
{
    if (s.empty())
        return std::wstring();
    const int n = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

std::string UrlEncode(const std::string& s)
{
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s)
    {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            out += (char)c;
        else
        {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0x0F];
        }
    }
    return out;
}

bool HttpGet(const std::string& url, std::string* body, std::string* error, const std::atomic<bool>* cancel, unsigned long* status_out)
{
    if (status_out)
        *status_out = 0;
    const std::wstring wurl = Widen(url);
    URL_COMPONENTS uc = {};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256] = {}, path[2048] = {};
    uc.lpszHostName = host;
    uc.dwHostNameLength = (DWORD)(sizeof(host) / sizeof(host[0]));
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = (DWORD)(sizeof(path) / sizeof(path[0]));
    if (!::WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc))
    {
        *error = "Bad URL.";
        return false;
    }

    HINTERNET session = ::WinHttpOpen(L"NorthrendLauncher/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session)
    {
        *error = "Could not start a network session.";
        return false;
    }
    ::WinHttpSetTimeouts(session, 8000, 8000, 10000, 15000);

    bool ok = false;
    *error = "Could not reach the server.";
    if (HINTERNET connect = ::WinHttpConnect(session, host, uc.nPort, 0))
    {
        const DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
        if (HINTERNET request = ::WinHttpOpenRequest(connect, L"GET", path, nullptr, WINHTTP_NO_REFERER,
                                                     WINHTTP_DEFAULT_ACCEPT_TYPES, flags))
        {
            if (::WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                ::WinHttpReceiveResponse(request, nullptr))
            {
                DWORD status = 0, len = sizeof(status);
                ::WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                      WINHTTP_HEADER_NAME_BY_INDEX, &status, &len, WINHTTP_NO_HEADER_INDEX);
                if (status_out)
                    *status_out = status;
                if (status == 200)
                {
                    body->clear();
                    for (;;)
                    {
                        if (cancel && cancel->load())
                            break;
                        DWORD avail = 0;
                        if (!::WinHttpQueryDataAvailable(request, &avail) || avail == 0)
                            break;
                        const size_t old = body->size();
                        body->resize(old + avail);
                        DWORD read = 0;
                        if (!::WinHttpReadData(request, &(*body)[old], avail, &read))
                            break;
                        body->resize(old + read);
                        if (body->size() > 4u * 1024u * 1024u) // nothing we fetch is this big
                            break;
                    }
                    ok = !(cancel && cancel->load());
                    *error = ok ? std::string() : std::string("Cancelled.");
                }
                else
                {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "The server returned HTTP %lu.", (unsigned long)status);
                    *error = buf;
                }
            }
            ::WinHttpCloseHandle(request);
        }
        ::WinHttpCloseHandle(connect);
    }
    ::WinHttpCloseHandle(session);
    return ok;
}
