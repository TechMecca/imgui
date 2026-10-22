#pragma once

// Everything that touches the World of Warcraft 3.3.5a client folder.
// The launcher only edits the client's text config files (realmlist.wtf, WTF\Config.wtf),
// optionally deletes the Cache folder, and starts Wow.exe. It never reads or writes game memory
// and never handles account credentials - logging in happens in the game client as usual.

#include <string>
#include <vector>

namespace Game
{
    constexpr const char*  DefaultRealmlist = "logon.warmane.com";
    constexpr unsigned int ExpectedBuild    = 12340; // 3.3.5a
    // Opened in the browser by the "Vote for points" button. You log in and vote on Warmane's
    // site yourself; the launcher does NOT automate voting (that breaks the vote sites' and
    // Warmane's rules). Change this if you prefer a different vote/account link.
    constexpr const char*  VoteURL          = "https://www.warmane.com/account";

    // Warmane's realms (October 2026). Update this table when Warmane adds or retires realms.
    // 'Name' must match the in-game realm name: it is written to Config.wtf so the client
    // goes straight to that realm after logging in.
    struct Realm { const char* Name; const char* Tag; const char* Description; };
    extern const Realm Realms[];
    extern const int   RealmCount;
    int FindRealm(const std::string& name); // index into Realms, or 0

    struct ClientInfo
    {
        bool                     Found = false; // Wow.exe exists in the folder
        unsigned int             Build = 0;     // e.g. 12340
        std::string              Version;       // e.g. "3.3.5.12340"
        std::vector<std::string> Locales;       // Data\<locale> folders, e.g. "enUS"
    };
    ClientInfo  Inspect(const std::string& folder);
    std::string DetectFolder(); // the launcher's own folder if Wow.exe is next to it, else ""

    struct LaunchOptions
    {
        std::string Folder;
        std::string Realmlist;
        std::string Realm;
        bool        ClearCache = false;
        int         DisplayMode = 0; // 0 = leave as is, 1 = windowed, 2 = fullscreen
    };
    // Writes the realmlist / realm / display settings, optionally clears the cache, then starts Wow.exe.
    // On success *out_process receives the process handle (release it with CloseProcess).
    bool Launch(const LaunchOptions& options, void** out_process, std::string* error);
    bool IsRunning(void* process);
    void CloseProcess(void* process);

    bool ClearCache(const std::string& folder, std::string* error);

    struct Addon { std::string Folder, Title, Version, Notes, Author; };
    std::vector<Addon> ScanAddons(const std::string& folder);
    std::string        AddonsFolder(const std::string& folder);

    void OpenInExplorer(const std::string& path);                   // creates the folder if needed
    void OpenURL(const std::string& url);                           // opens a link in the default browser
    bool PickFolder(void* owner_hwnd, const std::string& start, std::string* out_folder);
}
