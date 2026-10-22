#pragma once

// Checks whether the logon server accepts connections, on a worker thread.
// It's a reachability check only (TCP connect, then close): nothing is sent, no account data involved.

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

class LogonStatus
{
public:
    enum class State { Idle, Resolving, Connecting, Online, Offline };

    LogonStatus() = default;
    ~LogonStatus();
    LogonStatus(const LogonStatus&) = delete;
    LogonStatus& operator=(const LogonStatus&) = delete;

    // Starts a check of "host" or "host:port" (default port 3724). Ignored while a check is running.
    void        Check(const std::string& realmlist);
    void        Cancel();

    State       GetState() const  { return m_State.load(); }
    bool        Busy() const      { return m_Busy.load(); }
    int         LatencyMs() const { return m_Latency.load(); } // -1 until a check succeeds
    std::string Error() const;
    std::string Host() const;

private:
    void Run(std::string host, std::string port);
    void Finish(State state, const std::string& error);

    std::thread        m_Worker;
    std::atomic<State> m_State{State::Idle};
    std::atomic<bool>  m_Busy{false};
    std::atomic<bool>  m_CancelRequested{false};
    std::atomic<int>   m_Latency{-1};
    mutable std::mutex m_Mutex;
    std::string        m_Error;
    std::string        m_Host;
};
