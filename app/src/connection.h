#pragma once

// Server connection used by the "Connecting" window.
//
// The handshake runs on a worker thread so the UI keeps animating while it waits.
// THIS DEMO CONTAINS NO NETWORKING: ServerConnection::Run() just walks through the
// stages below and takes ~10 seconds in total. Put your real server code there.

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

enum class ConnectStage
{
    Idle,
    Resolving,       // look up the server address
    Handshake,       // open the connection
    Authenticating,  // send credentials / client info
    Verifying,       // wait for the server to accept this client
    Verified,        // done: the main window can be shown
    Failed,          // see ServerConnection::Error()
};

const char* ConnectStageLabel(ConnectStage stage);
int         ConnectStageIndex(ConnectStage stage); // 1..4 for the working stages, 0 otherwise
constexpr int ConnectStageCount = 4;

class ServerConnection
{
public:
    ServerConnection() = default;
    ~ServerConnection();
    ServerConnection(const ServerConnection&) = delete;
    ServerConnection& operator=(const ServerConnection&) = delete;

    void         Start();   // (re)starts the handshake on a worker thread
    void         Cancel();  // asks the worker to stop and waits for it

    ConnectStage Stage() const    { return m_Stage.load(); }
    float        Progress() const { return m_Progress.load(); } // 0..1, for the progress bar
    std::string  Error() const;

    // Demo only: make the fake handshake fail so the error screen can be seen (run with --demo-fail).
    void         SetDemoFailure(bool fail) { m_DemoFailure = fail; }

private:
    void Run();                                     // worker thread body
    bool Wait(float seconds, float progress_from, float progress_to);
    void SetStage(ConnectStage stage) { m_Stage.store(stage); }
    void Fail(const std::string& message);

    std::thread               m_Worker;
    std::atomic<ConnectStage> m_Stage{ConnectStage::Idle};
    std::atomic<float>        m_Progress{0.0f};
    std::atomic<bool>         m_CancelRequested{false};
    bool                      m_DemoFailure = false;
    mutable std::mutex        m_ErrorMutex;
    std::string               m_Error;
};
