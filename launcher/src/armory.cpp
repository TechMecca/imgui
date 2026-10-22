#include "armory.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//-----------------------------------------------------------------------------
// Tiny tolerant JSON parser (enough for the armory summary response)
//-----------------------------------------------------------------------------

namespace {

struct JsonValue
{
    enum Type { Null, Bool, Number, String, Array, Object } type = Null;
    bool        boolean = false;
    double      number = 0.0;
    std::string str;
    std::vector<JsonValue> arr;
    std::vector<std::pair<std::string, JsonValue>> obj;

    const JsonValue* Find(const char* key) const
    {
        for (const auto& kv : obj)
            if (kv.first == key)
                return &kv.second;
        return nullptr;
    }

    // Numbers, bools and strings all collapse to text (the armory returns numbers as strings,
    // and sometimes null where it means zero).
    std::string AsString() const
    {
        if (type == String)
            return str;
        if (type == Bool)
            return boolean ? "true" : "false";
        if (type == Number)
        {
            char buf[32];
            if (number == (double)(long long)number)
                snprintf(buf, sizeof(buf), "%lld", (long long)number);
            else
                snprintf(buf, sizeof(buf), "%g", number);
            return buf;
        }
        return std::string();
    }

    bool AsBool() const
    {
        if (type == Bool)   return boolean;
        if (type == Number) return number != 0.0;
        if (type == String) return str == "true" || str == "1";
        return false;
    }
};

struct JsonParser
{
    const char* p;
    const char* end;

    explicit JsonParser(const std::string& s) : p(s.c_str()), end(s.c_str() + s.size()) {}

    void Skip()
    {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
            p++;
    }

    bool Parse(JsonValue& out)
    {
        Skip();
        if (p >= end)
            return false;
        switch (*p)
        {
        case '{': return ParseObject(out);
        case '[': return ParseArray(out);
        case '"': out.type = JsonValue::String; return ParseString(out.str);
        case 't': case 'f': return ParseBool(out);
        case 'n': return ParseNull(out);
        default:  return ParseNumber(out);
        }
    }

    bool ParseString(std::string& out)
    {
        if (*p != '"')
            return false;
        p++;
        out.clear();
        while (p < end && *p != '"')
        {
            if (*p == '\\' && p + 1 < end)
            {
                p++;
                switch (*p)
                {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case '/': out += '/';  break;
                case '\\': out += '\\'; break;
                case '"': out += '"';  break;
                case 'u':
                {
                    // Decode \uXXXX to UTF-8 (BMP only; good enough for names).
                    if (p + 4 >= end)
                        return false;
                    unsigned int cp = 0;
                    for (int i = 1; i <= 4; i++)
                    {
                        cp <<= 4;
                        const char c = p[i];
                        if (c >= '0' && c <= '9') cp |= (unsigned)(c - '0');
                        else if (c >= 'a' && c <= 'f') cp |= (unsigned)(c - 'a' + 10);
                        else if (c >= 'A' && c <= 'F') cp |= (unsigned)(c - 'A' + 10);
                    }
                    p += 4;
                    if (cp < 0x80) out += (char)cp;
                    else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
                    else { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
                    break;
                }
                default: out += *p; break;
                }
                p++;
            }
            else
            {
                out += *p++;
            }
        }
        if (p >= end)
            return false;
        p++; // closing quote
        return true;
    }

    bool ParseNumber(JsonValue& out)
    {
        const char* start = p;
        while (p < end && (*p == '-' || *p == '+' || *p == '.' || *p == 'e' || *p == 'E' || (*p >= '0' && *p <= '9')))
            p++;
        if (p == start)
            return false;
        out.type = JsonValue::Number;
        out.number = strtod(std::string(start, p).c_str(), nullptr);
        return true;
    }

    bool ParseBool(JsonValue& out)
    {
        if (end - p >= 4 && strncmp(p, "true", 4) == 0)  { out.type = JsonValue::Bool; out.boolean = true;  p += 4; return true; }
        if (end - p >= 5 && strncmp(p, "false", 5) == 0) { out.type = JsonValue::Bool; out.boolean = false; p += 5; return true; }
        return false;
    }

    bool ParseNull(JsonValue& out)
    {
        if (end - p >= 4 && strncmp(p, "null", 4) == 0) { out.type = JsonValue::Null; p += 4; return true; }
        return false;
    }

    bool ParseArray(JsonValue& out)
    {
        out.type = JsonValue::Array;
        p++; // [
        Skip();
        if (p < end && *p == ']') { p++; return true; }
        for (;;)
        {
            JsonValue v;
            if (!Parse(v))
                return false;
            out.arr.push_back(std::move(v));
            Skip();
            if (p < end && *p == ',') { p++; continue; }
            if (p < end && *p == ']') { p++; return true; }
            return false;
        }
    }

    bool ParseObject(JsonValue& out)
    {
        out.type = JsonValue::Object;
        p++; // {
        Skip();
        if (p < end && *p == '}') { p++; return true; }
        for (;;)
        {
            Skip();
            std::string key;
            if (p >= end || *p != '"' || !ParseString(key))
                return false;
            Skip();
            if (p >= end || *p != ':')
                return false;
            p++;
            JsonValue v;
            if (!Parse(v))
                return false;
            out.obj.emplace_back(std::move(key), std::move(v));
            Skip();
            if (p < end && *p == ',') { p++; continue; }
            if (p < end && *p == '}') { p++; return true; }
            return false;
        }
    }
};

std::string GetStr(const JsonValue& obj, const char* key)
{
    const JsonValue* v = obj.Find(key);
    return v ? v->AsString() : std::string();
}

} // namespace

Character ParseCharacterSummary(const std::string& json)
{
    Character c;
    JsonValue root;
    JsonParser parser(json);
    if (!parser.Parse(root) || root.type != JsonValue::Object)
    {
        c.Error = "The armory returned an unexpected response.";
        return c;
    }

    // The armory returns {"error":"Character not found."} for a miss.
    const std::string error = GetStr(root, "error");
    if (!error.empty())
    {
        c.Error = error;
        return c;
    }

    c.Name              = GetStr(root, "name");
    c.Realm             = GetStr(root, "realm");
    c.Level             = GetStr(root, "level");
    c.Faction           = GetStr(root, "faction");
    c.Gender            = GetStr(root, "gender");
    c.Race              = GetStr(root, "race");
    c.Class             = GetStr(root, "class");
    c.Guild             = GetStr(root, "guild");
    c.HonorableKills    = GetStr(root, "honorablekills");
    c.AchievementPoints = GetStr(root, "achievementpoints");
    if (const JsonValue* online = root.Find("online"))
        c.Online = online->AsBool();

    if (const JsonValue* profs = root.Find("professions"))
        for (const JsonValue& e : profs->arr)
            if (e.type == JsonValue::Object)
                c.Professions.emplace_back(GetStr(e, "name"), GetStr(e, "skill"));

    if (const JsonValue* talents = root.Find("talents"))
        for (const JsonValue& e : talents->arr)
            if (e.type == JsonValue::Object)
            {
                std::string tree = GetStr(e, "tree");
                if (tree.empty())
                    tree = GetStr(e, "name");
                c.Talents.emplace_back(tree, GetStr(e, "points"));
            }

    if (const JsonValue* equip = root.Find("equipment"))
        c.EquipmentCount = (int)equip->arr.size();

    c.Found = !c.Name.empty();
    if (!c.Found && c.Error.empty())
        c.Error = "Character not found.";
    return c;
}

//-----------------------------------------------------------------------------
// HTTP fetch (WinHTTP)
//-----------------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>

namespace {

std::wstring Widen(const std::string& s)
{
    if (s.empty())
        return std::wstring();
    const int n = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

std::string UrlEncode(const std::string& s)
{
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s)
    {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            out += (char)c;
        else { out += '%'; out += hex[c >> 4]; out += hex[c & 0x0F]; }
    }
    return out;
}

std::string ApiBase()
{
    char buf[512];
    const DWORD n = ::GetEnvironmentVariableA("ARMORY_API_BASE", buf, sizeof(buf)); // test hook
    if (n > 0 && n < sizeof(buf))
        return std::string(buf, n);
    return "https://armory.warmane.com";
}

// GETs 'url' and returns true with the body in 'out'. On failure, 'error' is set.
bool HttpGet(const std::wstring& url, std::string* out, std::string* error, const std::atomic<bool>& cancel)
{
    URL_COMPONENTS uc = {};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256] = {}, path[1024] = {};
    uc.lpszHostName = host; uc.dwHostNameLength = _countof(host);
    uc.lpszUrlPath = path; uc.dwUrlPathLength = _countof(path);
    if (!::WinHttpCrackUrl(url.c_str(), 0, 0, &uc))
    {
        *error = "Bad armory URL.";
        return false;
    }

    HINTERNET session = ::WinHttpOpen(L"NorthrendLauncher/1.0 (armory lookup)",
                                      WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session)
    {
        *error = "Could not start a network session.";
        return false;
    }
    ::WinHttpSetTimeouts(session, 8000, 8000, 10000, 10000);

    bool ok = false;
    HINTERNET connect = ::WinHttpConnect(session, host, uc.nPort, 0);
    if (connect)
    {
        const DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET request = ::WinHttpOpenRequest(connect, L"GET", path, nullptr, WINHTTP_NO_REFERER,
                                                 WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
        if (request)
        {
            if (::WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                ::WinHttpReceiveResponse(request, nullptr))
            {
                DWORD status = 0, len = sizeof(status);
                ::WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                      WINHTTP_HEADER_NAME_BY_INDEX, &status, &len, WINHTTP_NO_HEADER_INDEX);
                if (status == 200)
                {
                    out->clear();
                    DWORD avail = 0;
                    do
                    {
                        if (cancel.load())
                            break;
                        if (!::WinHttpQueryDataAvailable(request, &avail))
                            break;
                        if (avail == 0)
                            continue;
                        std::string chunk(avail, '\0');
                        DWORD read = 0;
                        if (!::WinHttpReadData(request, &chunk[0], avail, &read))
                            break;
                        out->append(chunk.data(), read);
                    } while (avail > 0 && out->size() < 2 * 1024 * 1024);
                    ok = !cancel.load();
                    if (!ok)
                        *error = "Cancelled.";
                }
                else if (status == 404)
                {
                    *error = "Character not found.";
                }
                else
                {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "The armory returned HTTP %lu.", status);
                    *error = buf;
                }
            }
            else
            {
                *error = "Could not reach armory.warmane.com.";
            }
            ::WinHttpCloseHandle(request);
        }
        ::WinHttpCloseHandle(connect);
    }
    else
    {
        *error = "Could not reach armory.warmane.com.";
    }
    ::WinHttpCloseHandle(session);
    return ok;
}

} // namespace

//-----------------------------------------------------------------------------
// Armory worker
//-----------------------------------------------------------------------------

Armory::~Armory()
{
    Cancel();
}

std::string Armory::Query() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_Query;
}

Character Armory::Result() const
{
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_Result;
}

void Armory::Search(const std::string& name, const std::string& realm)
{
    if (m_State.load() == State::Loading)
        return;
    if (m_Worker.joinable())
        m_Worker.join();
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Query = name + " - " + realm;
        m_Result = Character();
    }
    m_Cancel = false;
    m_State = State::Loading;
    m_Worker = std::thread(&Armory::Run, this, name, realm);
}

void Armory::Cancel()
{
    m_Cancel = true;
    if (m_Worker.joinable())
        m_Worker.join();
}

void Armory::Run(std::string name, std::string realm)
{
    const std::string url = ApiBase() + "/api/character/" + UrlEncode(name) + "/" + UrlEncode(realm) + "/summary";
    std::string body, error;
    Character result;
    if (HttpGet(Widen(url), &body, &error, m_Cancel))
        result = ParseCharacterSummary(body);
    else
        result.Error = error;

    if (m_Cancel.load())
        return;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Result = result;
    }
    m_State = (result.Found ? State::Done : State::Error);
}
