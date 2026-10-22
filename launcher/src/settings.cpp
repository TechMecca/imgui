#include "settings.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <filesystem>
#include <fstream>
#include <stdlib.h>

namespace fs = std::filesystem;

static fs::path SettingsPath()
{
    wchar_t appdata[MAX_PATH] = {};
    if (::GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH) == 0)
        return fs::path(L"settings.ini");
    return fs::path(appdata) / L"NorthrendLauncher" / L"settings.ini";
}

void Settings::Load()
{
    std::ifstream f(SettingsPath(), std::ios::binary);
    std::string line;
    while (f && std::getline(f, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        const size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        const std::string key = line.substr(0, eq), value = line.substr(eq + 1);
        if (key == "game_folder")       GameFolder  = value;
        else if (key == "realmlist")    Realmlist   = value.empty() ? Game::DefaultRealmlist : value;
        else if (key == "realm")        Realm       = value;
        else if (key == "clear_cache")  ClearCache  = value == "1";
        else if (key == "display_mode") DisplayMode = atoi(value.c_str());
        else if (key == "after_launch") AfterLaunch = atoi(value.c_str());
        else if (key == "accent")       Accent      = atoi(value.c_str());
        else if (key == "animations")   Animations  = value == "1";
        else if (key == "snow")         Snow        = value == "1";
        else if (key == "armory_recent" && !value.empty() && ArmoryHistory.size() < 8) ArmoryHistory.push_back(value);
    }
}

void Settings::Save() const
{
    const fs::path path = SettingsPath();
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f << "game_folder="  << GameFolder              << "\r\n"
      << "realmlist="    << Realmlist               << "\r\n"
      << "realm="        << Realm                   << "\r\n"
      << "clear_cache="  << (ClearCache ? 1 : 0)    << "\r\n"
      << "display_mode=" << DisplayMode             << "\r\n"
      << "after_launch=" << AfterLaunch             << "\r\n"
      << "accent="       << Accent                  << "\r\n"
      << "animations="   << (Animations ? 1 : 0)    << "\r\n"
      << "snow="         << (Snow ? 1 : 0)          << "\r\n";
    for (const std::string& entry : ArmoryHistory)
        f << "armory_recent=" << entry << "\r\n";
}
