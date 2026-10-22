#include "connection.h"

#include <chrono>

const char* ConnectStageLabel(ConnectStage stage)
{
    switch (stage)
    {
    case ConnectStage::Idle:           return "Starting";
    case ConnectStage::Resolving:      return "Resolving server";
    case ConnectStage::Handshake:      return "Establishing connection";
    case ConnectStage::Authenticating: return "Authenticating client";
    case ConnectStage::Verifying:      return "Verifying session";
    case ConnectStage::Verified:       return "Verified";
    case ConnectStage::Failed:         return "Connection failed";
    }
    return "";
}

int ConnectStageIndex(ConnectStage stage)
{
    switch (stage)
    {
    case ConnectStage::Resolving:      return 1;
    case ConnectStage::Handshake:      return 2;
    case ConnectStage::Authenticating: return 3;
    case ConnectStage::Verifying:      return 4;
    default:                           return 0;
    }
}

ServerConnection::~ServerConnection()
{
    Cancel();
}

void ServerConnection::Start()
{
    Cancel();
    {
        std::lock_guard<std::mutex> lock(m_ErrorMutex);
        m_Error.clear();
    }
    m_CancelRequested = false;
    m_Progress = 0.0f;
    m_Stage = ConnectStage::Resolving;
    m_Worker = std::thread(&ServerConnection::Run, this);
}

void ServerConnection::Cancel()
{
    m_CancelRequested = true;
    if (m_Worker.joinable())
        m_Worker.join();
}

std::string ServerConnection::Error() const
{
    std::lock_guard<std::mutex> lock(m_ErrorMutex);
    return m_Error;
}

void ServerConnection::Fail(const std::string& message)
{
    {
        std::lock_guard<std::mutex> lock(m_ErrorMutex);
        m_Error = message;
    }
    SetStage(ConnectStage::Failed);
}

// Sleeps in small steps so Cancel() is honoured quickly, moving the progress bar along the way.
// Returns false if the connection was cancelled.
bool ServerConnection::Wait(float seconds, float progress_from, float progress_to)
{
    using clock = std::chrono::steady_clock;
    const auto start = clock::now();
    for (;;)
    {
        if (m_CancelRequested)
            return false;
        const float elapsed = std::chrono::duration<float>(clock::now() - start).count();
        const float t = elapsed >= seconds ? 1.0f : elapsed / seconds;
        m_Progress = progress_from + (progress_to - progress_from) * t;
        if (t >= 1.0f)
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }
}

void ServerConnection::Run()
{
    // ------------------------------------------------------------------------------------
    // DEMO ONLY - no server code on purpose.
    // Each step just waits so the connecting window can be seen; 10 seconds in total.
    // Replace each Wait() with the real work for that step (DNS lookup, connect, login,
    // license/session check, ...). Call Fail("reason") and return if a step fails; the
    // connecting window will show the message with Retry / Exit buttons.
    // ------------------------------------------------------------------------------------
    struct Step { ConnectStage stage; float seconds; };
    const Step steps[] =
    {
        { ConnectStage::Resolving,      1.5f },
        { ConnectStage::Handshake,      3.0f },
        { ConnectStage::Authenticating, 3.0f },
        { ConnectStage::Verifying,      2.5f },
    };

    float total = 0.0f;
    for (const Step& step : steps)
        total += step.seconds;

    float done = 0.0f;
    for (const Step& step : steps)
    {
        SetStage(step.stage);
        if (!Wait(step.seconds, done / total, (done + step.seconds) / total))
            return;
        done += step.seconds;

        if (m_DemoFailure && step.stage == ConnectStage::Authenticating)
        {
            Fail("The server rejected this client (demo failure).");
            return;
        }
    }

    m_Progress = 1.0f;
    SetStage(ConnectStage::Verified);
}
