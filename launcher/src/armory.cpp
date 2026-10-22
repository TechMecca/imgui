#include "armory.h"

#include "net.h"

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
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

// Talent points come either as one value or as an array per tree ("57 / 3 / 11").
std::string PointsText(const JsonValue* v)
{
    if (!v)
        return std::string();
    if (v->type != JsonValue::Array)
        return v->AsString();
    std::string out;
    for (const JsonValue& e : v->arr)
    {
        if (!out.empty())
            out += " / ";
        const std::string p = e.AsString();
        out += p.empty() ? "0" : p;
    }
    return out;
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
                c.Talents.emplace_back(tree, PointsText(e.Find("points")));
            }

    if (const JsonValue* equip = root.Find("equipment"))
    {
        for (const JsonValue& e : equip->arr)
            if (e.type == JsonValue::Object)
                c.Equipment.emplace_back((unsigned)strtoul(GetStr(e, "item").c_str(), nullptr, 10), GetStr(e, "name"));
        c.EquipmentCount = (int)c.Equipment.size();
    }

    c.Found = !c.Name.empty();
    if (!c.Found && c.Error.empty())
        c.Error = "Character not found.";
    return c;
}

//-----------------------------------------------------------------------------
// Profile page: the 19 paper-doll slots
//-----------------------------------------------------------------------------

const char* GearSlotName(int index)
{
    static const char* names[GearSlotCount] =
    {
        "Head", "Neck", "Shoulders", "Back", "Chest", "Shirt", "Tabard", "Wrists",
        "Hands", "Waist", "Legs", "Feet", "Finger", "Finger", "Trinket", "Trinket",
        "Main Hand", "Off Hand", "Ranged",
    };
    return (index >= 0 && index < GearSlotCount) ? names[index] : "";
}

namespace {

bool IsClassChar(char c)
{
    return isalnum((unsigned char)c) || c == '-' || c == '_';
}

// Finds 'token' as a whole word inside an HTML class attribute (so "item-slot" doesn't match "item-slots").
size_t FindClassToken(const std::string& html, const char* token, size_t from)
{
    const size_t len = strlen(token);
    for (size_t pos = html.find(token, from); pos != std::string::npos; pos = html.find(token, pos + 1))
    {
        const char before = pos > 0 ? html[pos - 1] : ' ';
        const char after = pos + len < html.size() ? html[pos + len] : ' ';
        if (!IsClassChar(before) && !IsClassChar(after))
            return pos;
    }
    return std::string::npos;
}

// Value of attr="..." at or after 'from' and before 'limit'.
bool AttrValue(const std::string& html, const char* attr, size_t from, size_t limit, std::string* out)
{
    const std::string key = std::string(attr) + "=\"";
    const size_t pos = html.find(key, from);
    if (pos == std::string::npos || pos >= limit)
        return false;
    const size_t start = pos + key.size();
    const size_t end = html.find('"', start);
    if (end == std::string::npos || end > limit)
        return false;
    *out = html.substr(start, end - start);
    return true;
}

std::string Unescape(std::string s)
{
    for (size_t pos; (pos = s.find("&amp;")) != std::string::npos; )
        s.replace(pos, 5, "&");
    return s;
}

// "item=51162&ench=3817&gems=3628:3518:0" -> value of 'key'
std::string QueryParam(const std::string& query, const char* key)
{
    const std::string k = std::string(key) + "=";
    size_t pos = 0;
    while ((pos = query.find(k, pos)) != std::string::npos)
    {
        if (pos == 0 || query[pos - 1] == '&' || query[pos - 1] == '?' || query[pos - 1] == ' ')
        {
            const size_t start = pos + k.size();
            const size_t end = query.find('&', start);
            return query.substr(start, end == std::string::npos ? std::string::npos : end - start);
        }
        pos += k.size();
    }
    return std::string();
}

} // namespace

bool ParseProfileGear(const std::string& html, std::vector<GearItem>* gear, std::string* error)
{
    // Slot order follows the armory's paper-doll: 8 on the left, 8 on the right, 3 weapons at the bottom.
    struct Column { const char* cls; int first; int count; };
    static const Column columns[] = { { "item-left", 0, 8 }, { "item-right", 8, 8 }, { "item-bottom", 16, 3 } };

    std::vector<GearItem> result(GearSlotCount);
    for (const Column& col : columns)
    {
        size_t pos = FindClassToken(html, col.cls, 0);
        if (pos == std::string::npos)
        {
            *error = std::string("Gear layout not found (") + col.cls + ").";
            return false;
        }
        for (int i = 0; i < col.count; i++)
        {
            pos = FindClassToken(html, "item-slot", pos + 1);
            if (pos == std::string::npos)
            {
                *error = "Gear layout changed.";
                return false;
            }
            // A slot's markup runs until the next slot (capped, so the last one can't swallow the page).
            size_t next = FindClassToken(html, "item-slot", pos + 1);
            const size_t limit = std::min(next == std::string::npos ? html.size() : next, pos + 1500);

            GearItem& item = result[col.first + i];
            std::string rel;
            if (AttrValue(html, "rel", pos, limit, &rel))
            {
                rel = Unescape(rel);
                item.ItemId = (unsigned)strtoul(QueryParam(rel, "item").c_str(), nullptr, 10);
                item.Enchanted = !QueryParam(rel, "ench").empty() && QueryParam(rel, "ench") != "0";
                const std::string gems = QueryParam(rel, "gems");
                for (size_t start = 0; start < gems.size(); )
                {
                    size_t end = gems.find(':', start);
                    if (end == std::string::npos)
                        end = gems.size();
                    if (strtoul(gems.substr(start, end - start).c_str(), nullptr, 10) != 0)
                        item.Gems++;
                    start = end + 1;
                }
            }
            if (item.ItemId == 0)
                continue; // empty slot

            for (size_t q = html.find("icon-quality", pos); q != std::string::npos && q < limit; q = html.find("icon-quality", q + 1))
                if (q + 12 < html.size() && isdigit((unsigned char)html[q + 12]))
                {
                    item.Quality = html[q + 12] - '0';
                    break;
                }

            const size_t img = html.find("<img", pos);
            std::string src;
            if (img != std::string::npos && img < limit && AttrValue(html, "src", img, limit, &src))
            {
                src = Unescape(src);
                if (src.compare(0, 2, "//") == 0)
                    src = "https:" + src;
                else if (!src.empty() && src[0] == '/')
                    src = "https://armory.warmane.com" + src;
                item.IconUrl = src;
            }
        }
    }
    *gear = result;
    return true;
}

//-----------------------------------------------------------------------------
// Armory worker
//-----------------------------------------------------------------------------

static const auto kCacheTtl = std::chrono::minutes(5);

static std::string ApiBase()
{
    char buf[512];
    const DWORD n = ::GetEnvironmentVariableA("ARMORY_API_BASE", buf, sizeof(buf)); // test hook
    if (n > 0 && n < sizeof(buf))
        return std::string(buf, n);
    return "https://armory.warmane.com";
}

static std::string Lower(std::string s)
{
    for (char& c : s)
        c = (char)tolower((unsigned char)c);
    return s;
}

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
    const std::string key = Lower(name) + "|" + Lower(realm);
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_Cache.find(key);
        if (it != m_Cache.end() && std::chrono::steady_clock::now() - it->second.first < kCacheTtl)
        {
            m_Result = it->second.second;
            m_State = State::Done;
            return;
        }
    }

    const std::string base = ApiBase();
    const std::string path = UrlEncode(name) + "/" + UrlEncode(realm);
    Character result;
    std::string body, error;
    unsigned long status = 0;

    if (HttpGet(base + "/api/character/" + path + "/summary", &body, &error, &m_Cancel, &status))
        result = ParseCharacterSummary(body);
    else
        result.Error = (status == 404) ? std::string("Character not found.")
                     : (status == 0)   ? std::string("Could not reach armory.warmane.com.") : error;

    // Gear layout + icons come from the profile page.
    if (result.Found && !m_Cancel.load())
    {
        std::string html;
        if (HttpGet(base + "/character/" + path + "/profile", &html, &error, &m_Cancel, &status) &&
            ParseProfileGear(html, &result.Gear, &result.GearError))
        {
            for (GearItem& g : result.Gear)
                for (const auto& e : result.Equipment)
                    if (g.ItemId != 0 && e.first == g.ItemId)
                    {
                        g.Name = e.second;
                        break;
                    }
        }
        else if (result.GearError.empty())
        {
            result.GearError = "Couldn't load the gear layout (" + error + ")";
        }
    }

    if (m_Cancel.load())
        return;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Result = result;
        if (result.Found)
            m_Cache[key] = std::make_pair(std::chrono::steady_clock::now(), result);
    }
    m_State = result.Found ? State::Done : State::Error;
}
