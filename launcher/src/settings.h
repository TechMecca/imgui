#pragma once

// Launcher preferences, stored in %APPDATA%\NorthrendLauncher\settings.ini.

#include "game.h"

#include <string>
#include <vector>

struct Settings
{
    std::string GameFolder;
    std::string Realmlist   = Game::DefaultRealmlist;
    std::string Realm       = "Icecrown";
    bool        ClearCache  = false;
    int         DisplayMode = 0; // Game::LaunchOptions::DisplayMode
    int         AfterLaunch = 1; // 0 = keep launcher open, 1 = minimize, 2 = close
    int         Accent      = 0;
    bool        Animations  = true;
    bool        Snow        = true;
    std::vector<std::string> ArmoryHistory; // recent "Name/Realm" searches, most recent first

    void Load();
    void Save() const;
};
