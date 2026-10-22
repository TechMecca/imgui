#pragma once

// In-app Warmane armory lookup. Fetches a character's public summary from the armory's
// JSON API on a worker thread and exposes it for the UI to render. Read-only public data;
// no account, no credentials, nothing stored server-side.
//
// Endpoint: https://armory.warmane.com/api/character/<name>/<realm>/summary
// Set the ARMORY_API_BASE environment variable to point at a different base (used for testing).

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct Character
{
    bool        Found = false;
    std::string Error;            // set when the character wasn't found or the request failed

    std::string Name;
    std::string Realm;
    bool        Online = false;
    std::string Level;
    std::string Faction;          // "Alliance" / "Horde"
    std::string Gender;
    std::string Race;
    std::string Class;            // "Mage", "Death Knight", ...
    std::string Guild;
    std::string HonorableKills;
    std::string AchievementPoints;

    std::vector<std::pair<std::string, std::string>> Professions; // name, skill
    std::vector<std::pair<std::string, std::string>> Talents;     // spec/tree, points
    int                                              EquipmentCount = 0;
};

class Armory
{
public:
    enum class State { Idle, Loading, Done, Error };

    Armory() = default;
    ~Armory();
    Armory(const Armory&) = delete;
    Armory& operator=(const Armory&) = delete;

    void      Search(const std::string& name, const std::string& realm); // ignored while already loading
    void      Cancel();

    State     GetState() const { return m_State.load(); }
    bool      Loading() const  { return m_State.load() == State::Loading; }
    Character Result() const;
    std::string Query() const; // "Name - Realm" currently shown/loading

private:
    void Run(std::string name, std::string realm);

    std::thread        m_Worker;
    std::atomic<State> m_State{State::Idle};
    std::atomic<bool>  m_Cancel{false};
    mutable std::mutex m_Mutex;
    Character          m_Result;
    std::string        m_Query;
};

// Exposed for an offline unit test of the parser (see tests/).
Character ParseCharacterSummary(const std::string& json);
