#include "logon_status.h"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#include <chrono>

static const int kTimeoutMs = 5000;

LogonStatus::~LogonStatus()
{
    Cancel();
}

void LogonStatus::Check(const std::string& realmlist)
{
    if (m_Busy)
        return;
    if (m_Worker.joinable())
        m_Worker.join();

    // "host" or "host:port"
    std::string host = realmlist, port = "3724";
    const size_t colon = realmlist.rfind(':');
    if (colon != std::string::npos)
    {
        host = realmlist.substr(0, colon);
        port = realmlist.substr(colon + 1);
    }
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Host = host;
        m_Error.clear();
    }
    m_CancelRequested = false;
    m_Busy = true;
    m_State = State::Resolving;
    m_Worker = std::thread(&LogonStatus::Run, this, host, port);
}

void LogonStatus::Cancel()
{
    m_CancelRequested = true;
    if (m_Worker.joinable())
        m_Worker.join();
}

std::string LogonStatus::Error() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_Error;
}

std::string LogonStatus::Host() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_Host;
}

void LogonStatus::Finish(State state, const std::string& error)
{
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Error = error;
    }
    m_State = state;
    m_Busy = false;
}

void LogonStatus::Run(std::string host, std::string port)
{
    WSADATA wsa;
    if (::WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        Finish(State::Offline, "Networking is unavailable.");
        return;
    }

    addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    addrinfo* addresses = nullptr;
    if (::getaddrinfo(host.c_str(), port.c_str(), &hints, &addresses) != 0 || !addresses)
    {
        ::WSACleanup();
        Finish(State::Offline, "Could not resolve " + host + ".");
        return;
    }

    m_State = State::Connecting;
    State result = State::Offline;
    std::string error = "Could not reach " + host + ".";
    for (addrinfo* a = addresses; a && result != State::Online && !m_CancelRequested; a = a->ai_next)
    {
        SOCKET s = ::socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (s == INVALID_SOCKET)
            continue;
        u_long non_blocking = 1;
        ::ioctlsocket(s, FIONBIO, &non_blocking);

        const auto start = std::chrono::steady_clock::now();
        bool connected = ::connect(s, a->ai_addr, (int)a->ai_addrlen) == 0;
        if (!connected && ::WSAGetLastError() == WSAEWOULDBLOCK)
        {
            // Wait in short slices so Cancel() stays responsive.
            for (int waited = 0; waited < kTimeoutMs && !m_CancelRequested; waited += 100)
            {
                fd_set writable, failed;
                FD_ZERO(&writable); FD_SET(s, &writable);
                FD_ZERO(&failed);   FD_SET(s, &failed);
                timeval tv = { 0, 100 * 1000 };
                if (::select(0, nullptr, &writable, &failed, &tv) > 0)
                {
                    int so_error = 0;
                    int len = sizeof(so_error);
                    ::getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&so_error, &len);
                    connected = FD_ISSET(s, &writable) && so_error == 0;
                    if (!connected)
                        error = (so_error == WSAECONNREFUSED) ? "The logon server refused the connection." : error;
                    break;
                }
                if (waited + 100 >= kTimeoutMs)
                    error = "Timed out connecting to " + host + ".";
            }
        }
        if (connected)
        {
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
            m_Latency = (int)ms;
            result = State::Online;
        }
        ::closesocket(s);
    }

    ::freeaddrinfo(addresses);
    ::WSACleanup();
    Finish(result, result == State::Online ? std::string() : error);
}
