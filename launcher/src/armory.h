#pragma once

// In-app Warmane armory lookup. Read-only public data; no account, no credentials.
//
// For each search the worker makes two requests to the armory:
//   <base>/api/character/<name>/<realm>/summary   JSON: name, class, guild, professions, item names...
//   <base>/character/<name>/<realm>/profile       HTML: the 19 gear slots with item IDs, quality and icons
// Results are cached for a few minutes so re-opening a character doesn't hit Warmane again.
// Set the ARMORY_API_BASE environment variable to point at a different base (used for testing).

#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// One paper-doll slot, in the armory's layout order (see GearSlotName).
struct GearItem
{
    unsigned    ItemId = 0;       // 0 = empty slot
    int         Quality = -1;     // 0 poor .. 4 epic, 5 legendary, 6 artifact, 7 heirloom
    std::string IconUrl;
    std::string Name;             // joined from the summary's equipment list by item ID
    bool        Enchanted = false;
    int         Gems = 0;         // socketed gems
};

constexpr int GearSlotCount = 19;
const char* GearSlotName(int index); // "Head", "Neck", ... "Ranged"

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
    std::vector<std::pair<unsigned, std::string>>    Equipment;   // item id, name (summary; no slots)
    int                                              EquipmentCount = 0;

    std::vector<GearItem> Gear;   // GearSlotCount entries, or empty if the profile page couldn't be read
    std::string           GearError;
};

class Armory
{
public:
    enum class State { Idle, Loading, Done, Error };

    Armory() = default;
    ~Armory();
    Armory(const Armory&) = delete;
    Armory& operator=(const Armory&) = delete;

    void        Search(const std::string& name, const std::string& realm); // ignored while already loading
    void        Cancel();

    State       GetState() const { return m_State.load(); }
    bool        Loading() const  { return m_State.load() == State::Loading; }
    Character   Result() const;
    std::string Query() const;   // "Name - Realm" currently shown/loading

private:
    void Run(std::string name, std::string realm);

    std::thread        m_Worker;
    std::atomic<State> m_State{State::Idle};
    std::atomic<bool>  m_Cancel{false};
    mutable std::mutex m_Mutex;
    Character          m_Result;
    std::string        m_Query;
    std::map<std::string, std::pair<std::chrono::steady_clock::time_point, Character>> m_Cache; // key: name|realm (lowercase)
};

// Exposed for the offline parser tests (tests/armory_parse_test.cpp).
Character ParseCharacterSummary(const std::string& json);
bool      ParseProfileGear(const std::string& html, std::vector<GearItem>* gear, std::string* error);
