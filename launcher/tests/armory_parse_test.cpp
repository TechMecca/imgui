// Offline unit test for the armory summary parser. Build with MinGW and run under Wine/Windows:
//   x86_64-w64-mingw32-g++ -std=c++17 -I../src tests/armory_parse_test.cpp src/armory.cpp -lwinhttp -o test.exe
#include "../src/armory.h"
#include <cassert>
#include <cstdio>

static int failures = 0;
#define CHECK(cond) do { if(!(cond)){ printf("FAIL: %s\n", #cond); failures++; } } while(0)

int main()
{
    const char* ok = "{\"name\":\"Metagoblin\",\"realm\":\"Icecrown\",\"online\":true,\"level\":\"80\","
        "\"faction\":\"Alliance\",\"gender\":\"Male\",\"race\":\"Gnome\",\"class\":\"Warlock\","
        "\"honorablekills\":\"13452\",\"guild\":\"Immortalis\",\"achievementpoints\":\"6780\","
        "\"equipment\":[{\"name\":\"A\",\"item\":\"1\"},{\"name\":\"B\",\"item\":\"2\"},{\"name\":\"C\",\"item\":\"3\"}],"
        "\"talents\":[{\"tree\":\"Affliction\",\"points\":\"57\"},{\"tree\":\"Destruction\",\"points\":\"11\"}],"
        "\"professions\":[{\"name\":\"Tailoring\",\"skill\":\"450\"},{\"name\":\"Engineering\",\"skill\":\"450\"}]}";
    Character c = ParseCharacterSummary(ok);
    CHECK(c.Found);
    CHECK(c.Error.empty());
    CHECK(c.Name == "Metagoblin");
    CHECK(c.Realm == "Icecrown");
    CHECK(c.Online == true);
    CHECK(c.Level == "80");
    CHECK(c.Class == "Warlock");
    CHECK(c.Guild == "Immortalis");
    CHECK(c.HonorableKills == "13452");
    CHECK(c.AchievementPoints == "6780");
    CHECK(c.EquipmentCount == 3);
    CHECK(c.Professions.size() == 2 && c.Professions[0].first == "Tailoring" && c.Professions[0].second == "450");
    CHECK(c.Talents.size() == 2 && c.Talents[0].first == "Affliction" && c.Talents[0].second == "57");

    // Numbers as real JSON numbers, achievementpoints null (armory sends null for zero).
    Character c2 = ParseCharacterSummary("{\"name\":\"Zero\",\"realm\":\"Lordaeron\",\"online\":false,\"level\":80,\"achievementpoints\":null,\"honorablekills\":0}");
    CHECK(c2.Found);
    CHECK(c2.Level == "80");
    CHECK(c2.AchievementPoints.empty());   // null -> empty, UI shows 0
    CHECK(c2.HonorableKills == "0");
    CHECK(c2.Online == false);

    // Not found.
    Character c3 = ParseCharacterSummary("{\"error\":\"Character not found.\"}");
    CHECK(!c3.Found);
    CHECK(c3.Error == "Character not found.");

    // Garbage.
    Character c4 = ParseCharacterSummary("<html>not json</html>");
    CHECK(!c4.Found);
    CHECK(!c4.Error.empty());

    // UTF-8 escape in a name.
    Character c5 = ParseCharacterSummary("{\"name\":\"Fr\\u00e8re\",\"realm\":\"Icecrown\"}");
    CHECK(c5.Found);
    CHECK(c5.Name == "Fr\xc3\xa8re");

    printf(failures == 0 ? "ALL PASS\n" : "%d FAILURES\n", failures);
    return failures;
}
