#include "game.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shobjidl.h>

#include <algorithm>
#include <ctype.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <string.h>

namespace fs = std::filesystem;

namespace Game
{
    const Realm Realms[] =
    {
        { "Icecrown",  "x7 rates",    "Fast-paced realm with all content unlocked." },
        { "Lordaeron", "x1 rates",    "Blizzlike leveling with tougher encounters." },
        { "Onyxia",    "Progressive", "Raid tiers unlock phase by phase." },
        { "Blackrock", "Instant 80",  "PvP-only realm with max-level characters." },
    };
    const int RealmCount = (int)(sizeof(Realms) / sizeof(Realms[0]));

    int FindRealm(const std::string& name)
    {
        for (int i = 0; i < RealmCount; i++)
            if (_stricmp(Realms[i].Name, name.c_str()) == 0)
                return i;
        return 0;
    }

    //-------------------------------------------------------------------------
    // Helpers
    //-------------------------------------------------------------------------

    static std::wstring Widen(const std::string& s)
    {
        if (s.empty())
            return std::wstring();
        const int n = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
        std::wstring w(n, L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
        return w;
    }

    static std::string Narrow(const std::wstring& w)
    {
        if (w.empty())
            return std::string();
        const int n = ::WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
        std::string s(n, '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
        return s;
    }

    static fs::path ToPath(const std::string& utf8) { return fs::path(Widen(utf8)); }

    static void MakeWritable(const fs::path& p)
    {
        const DWORD attrs = ::GetFileAttributesW(p.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_READONLY))
            ::SetFileAttributesW(p.c_str(), attrs & ~FILE_ATTRIBUTE_READONLY);
    }

    static bool ReadText(const fs::path& p, std::string* out)
    {
        std::ifstream f(p, std::ios::binary);
        if (!f)
            return false;
        std::ostringstream ss;
        ss << f.rdbuf();
        *out = ss.str();
        return true;
    }

    static bool WriteText(const fs::path& p, const std::string& text)
    {
        MakeWritable(p); // a read-only realmlist.wtf makes the client try (and fail) to patch
        std::ofstream f(p, std::ios::binary | std::ios::trunc);
        if (!f)
            return false;
        f << text;
        return (bool)f;
    }

    static std::vector<std::string> SplitLines(const std::string& text)
    {
        std::vector<std::string> lines;
        std::istringstream ss(text);
        std::string line;
        while (std::getline(ss, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            lines.push_back(line);
        }
        while (!lines.empty() && lines.back().empty())
            lines.pop_back();
        return lines;
    }

    // Sets 'SET key "value"' in Config.wtf lines, replacing an existing entry for the key.
    static void SetConfigValue(std::vector<std::string>& lines, const char* key, const std::string& value)
    {
        const std::string entry = std::string("SET ") + key + " \"" + value + "\"";
        for (std::string& line : lines)
        {
            std::istringstream ss(line);
            std::string set, name;
            ss >> set >> name;
            if (_stricmp(set.c_str(), "SET") == 0 && _stricmp(name.c_str(), key) == 0)
            {
                line = entry;
                return;
            }
        }
        lines.push_back(entry);
    }

    static std::string Trim(const std::string& s)
    {
        size_t b = 0, e = s.size();
        while (b < e && isspace((unsigned char)s[b])) b++;
        while (e > b && isspace((unsigned char)s[e - 1])) e--;
        return s.substr(b, e - b);
    }

    // Removes WoW color codes: |cAARRGGBB ... |r
    static std::string StripColorCodes(const std::string& s)
    {
        std::string out;
        for (size_t i = 0; i < s.size(); i++)
        {
            if (s[i] == '|' && i + 1 < s.size())
            {
                if ((s[i + 1] == 'c' || s[i + 1] == 'C') && i + 9 < s.size()) { i += 9; continue; }
                if (s[i + 1] == 'r' || s[i + 1] == 'R') { i += 1; continue; }
            }
            out += s[i];
        }
        return Trim(out);
    }

    //-------------------------------------------------------------------------
    // Client
    //-------------------------------------------------------------------------

    ClientInfo Inspect(const std::string& folder)
    {
        ClientInfo info;
        if (folder.empty())
            return info;
        std::error_code ec;
        const fs::path root = ToPath(folder);
        const fs::path exe = root / L"Wow.exe";
        if (!fs::is_regular_file(exe, ec))
            return info;
        info.Found = true;

        DWORD handle = 0;
        const DWORD size = ::GetFileVersionInfoSizeW(exe.c_str(), &handle);
        if (size > 0)
        {
            std::vector<unsigned char> data(size);
            VS_FIXEDFILEINFO* ffi = nullptr;
            UINT len = 0;
            if (::GetFileVersionInfoW(exe.c_str(), 0, size, data.data()) &&
                ::VerQueryValueW(data.data(), L"\\", (void**)&ffi, &len) && ffi)
            {
                char buf[48];
                snprintf(buf, sizeof(buf), "%u.%u.%u.%u", HIWORD(ffi->dwFileVersionMS), LOWORD(ffi->dwFileVersionMS),
                         HIWORD(ffi->dwFileVersionLS), LOWORD(ffi->dwFileVersionLS));
                info.Version = buf;
                info.Build = LOWORD(ffi->dwFileVersionLS);
            }
        }

        // Locale folders look like Data\enUS, Data\deDE, ...
        for (fs::directory_iterator it(root / L"Data", ec), end; !ec && it != end; it.increment(ec))
        {
            const std::wstring name = it->path().filename().wstring();
            if (name.size() == 4 && iswlower(name[0]) && iswlower(name[1]) && iswupper(name[2]) && iswupper(name[3]) && it->is_directory(ec))
                info.Locales.push_back(Narrow(name));
        }
        return info;
    }

    std::string DetectFolder()
    {
        wchar_t module[MAX_PATH] = {};
        if (::GetModuleFileNameW(nullptr, module, MAX_PATH) == 0)
            return std::string();
        std::error_code ec;
        const fs::path dir = fs::path(module).parent_path();
        return fs::is_regular_file(dir / L"Wow.exe", ec) ? Narrow(dir.wstring()) : std::string();
    }

    bool ClearCache(const std::string& folder, std::string* error)
    {
        std::error_code ec;
        fs::remove_all(ToPath(folder) / L"Cache", ec);
        if (ec && error)
            *error = "Could not delete the Cache folder: " + ec.message();
        return !ec;
    }

    bool Launch(const LaunchOptions& options, void** out_process, std::string* error)
    {
        const fs::path root = ToPath(options.Folder);
        const fs::path exe = root / L"Wow.exe";
        std::error_code ec;
        if (!fs::is_regular_file(exe, ec))
        {
            *error = "Wow.exe was not found in the game folder.";
            return false;
        }

        const ClientInfo info = Inspect(options.Folder);
        if (info.Locales.empty())
        {
            *error = "No Data\\<locale> folder found. Is this a complete 3.3.5a client?";
            return false;
        }

        // 1) realmlist.wtf in every locale folder - this is the file the 3.3.5a client reads.
        const std::string host = options.Realmlist.empty() ? std::string(DefaultRealmlist) : options.Realmlist;
        for (const std::string& locale : info.Locales)
        {
            const fs::path realmlist = root / L"Data" / Widen(locale) / L"realmlist.wtf";
            if (!WriteText(realmlist, "set realmlist " + host + "\r\n"))
            {
                *error = "Could not write Data\\" + locale + "\\realmlist.wtf. If the game is in Program Files, move it or run the launcher as administrator.";
                return false;
            }
        }

        // 2) WTF\Config.wtf - keeps the same realmlist, remembers the realm, optional window mode.
        const fs::path config = root / L"WTF" / L"Config.wtf";
        fs::create_directories(config.parent_path(), ec);
        std::string text;
        ReadText(config, &text);
        const char* newline = (text.find("\r\n") != std::string::npos || text.empty()) ? "\r\n" : "\n";
        std::vector<std::string> lines = SplitLines(text);
        SetConfigValue(lines, "realmList", host);
        if (!options.Realm.empty())
            SetConfigValue(lines, "realmName", options.Realm);
        if (options.DisplayMode != 0)
            SetConfigValue(lines, "gxWindow", options.DisplayMode == 1 ? "1" : "0");
        std::string out;
        for (const std::string& line : lines)
            out += line + newline;
        if (!WriteText(config, out))
        {
            *error = "Could not write WTF\\Config.wtf.";
            return false;
        }

        // 3) Optional cache wipe (fixes stale item/quest data after realm changes).
        if (options.ClearCache)
            ClearCache(options.Folder, nullptr);

        // 4) Start the game from its own folder.
        STARTUPINFOW si = {};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi = {};
        std::wstring command = L"\"" + exe.wstring() + L"\"";
        if (!::CreateProcessW(exe.c_str(), &command[0], nullptr, nullptr, FALSE, 0, nullptr, root.c_str(), &si, &pi))
        {
            char buf[96];
            snprintf(buf, sizeof(buf), "Could not start Wow.exe (error %lu).", ::GetLastError());
            *error = buf;
            return false;
        }
        ::CloseHandle(pi.hThread);
        *out_process = pi.hProcess;
        return true;
    }

    bool IsRunning(void* process)
    {
        return process && ::WaitForSingleObject((HANDLE)process, 0) == WAIT_TIMEOUT;
    }

    void CloseProcess(void* process)
    {
        if (process)
            ::CloseHandle((HANDLE)process);
    }

    //-------------------------------------------------------------------------
    // Addons
    //-------------------------------------------------------------------------

    std::string AddonsFolder(const std::string& folder)
    {
        return Narrow((ToPath(folder) / L"Interface" / L"AddOns").wstring());
    }

    std::vector<Addon> ScanAddons(const std::string& folder)
    {
        std::vector<Addon> addons;
        if (folder.empty())
            return addons;
        std::error_code ec;
        const fs::path dir = ToPath(folder) / L"Interface" / L"AddOns";
        for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
        {
            if (!it->is_directory(ec))
                continue;
            const std::wstring name = it->path().filename().wstring();
            std::string toc;
            if (!ReadText(it->path() / (name + L".toc"), &toc))
                continue;

            Addon addon;
            addon.Folder = Narrow(name);
            for (std::string line : SplitLines(toc))
            {
                if (line.compare(0, 3, "\xEF\xBB\xBF") == 0) // UTF-8 BOM
                    line.erase(0, 3);
                if (line.compare(0, 2, "##") != 0)
                    continue;
                const size_t colon = line.find(':');
                if (colon == std::string::npos)
                    continue;
                const std::string key = Trim(line.substr(2, colon - 2));
                const std::string value = StripColorCodes(line.substr(colon + 1));
                if (_stricmp(key.c_str(), "Title") == 0)        addon.Title = value;
                else if (_stricmp(key.c_str(), "Version") == 0) addon.Version = value;
                else if (_stricmp(key.c_str(), "Notes") == 0)   addon.Notes = value;
                else if (_stricmp(key.c_str(), "Author") == 0)  addon.Author = value;
            }
            if (addon.Title.empty())
                addon.Title = addon.Folder;
            addons.push_back(addon);
        }
        std::sort(addons.begin(), addons.end(), [](const Addon& a, const Addon& b) { return _stricmp(a.Title.c_str(), b.Title.c_str()) < 0; });
        return addons;
    }

    //-------------------------------------------------------------------------
    // Shell
    //-------------------------------------------------------------------------

    void OpenInExplorer(const std::string& path)
    {
        std::error_code ec;
        const fs::path p = ToPath(path);
        fs::create_directories(p, ec);
        ::ShellExecuteW(nullptr, L"open", p.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }

    void OpenURL(const std::string& url)
    {
        ::ShellExecuteW(nullptr, L"open", Widen(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }

    bool PickFolder(void* owner_hwnd, const std::string& start, std::string* out_folder)
    {
        IFileOpenDialog* dialog = nullptr;
        if (FAILED(::CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_IFileOpenDialog, (void**)&dialog)))
            return false;

        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
        dialog->SetTitle(L"Select your World of Warcraft 3.3.5a folder (the one with Wow.exe)");
        if (!start.empty())
        {
            IShellItem* item = nullptr;
            if (SUCCEEDED(::SHCreateItemFromParsingName(Widen(start).c_str(), nullptr, IID_IShellItem, (void**)&item)))
            {
                dialog->SetFolder(item);
                item->Release();
            }
        }

        bool picked = false;
        if (SUCCEEDED(dialog->Show((HWND)owner_hwnd)))
        {
            IShellItem* result = nullptr;
            if (SUCCEEDED(dialog->GetResult(&result)))
            {
                PWSTR path = nullptr;
                if (SUCCEEDED(result->GetDisplayName(SIGDN_FILESYSPATH, &path)))
                {
                    *out_folder = Narrow(path);
                    ::CoTaskMemFree(path);
                    picked = true;
                }
                result->Release();
            }
        }
        dialog->Release();
        return picked;
    }
}
