// Offline unit tests for the armory parsers and the icon decoder. Build with MinGW and run under Wine/Windows:
//   x86_64-w64-mingw32-g++ -std=c++17 -I.. tests/armory_parse_test.cpp src/armory.cpp src/net.cpp src/icons.cpp
//       -lwinhttp -lwindowscodecs -lole32 -luuid -o test.exe
//   test.exe [some.jpg]
#include "../src/armory.h"
#include "../src/icons.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <windows.h>

// icons.cpp uploads textures through the window; the tests never get that far.
namespace Window
{
    void* CreateTexture(const unsigned char*, int, int, float*, float*) { return nullptr; }
    void  ReleaseTexture(void*) {}
}

static int failures = 0;
#define CHECK(cond) do { if(!(cond)){ printf("FAIL: %s\n", #cond); failures++; } } while(0)

int main(int argc, char** argv)
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

    // Summary equipment list (item id + name, no slots).
    Character c6 = ParseCharacterSummary("{\"name\":\"A\",\"equipment\":[{\"name\":\"Hood\",\"item\":\"51231\",\"transmog\":\"\"},{\"name\":\"Ring\",\"item\":50402}],"
                                         "\"talents\":[{\"tree\":\"Affliction\",\"points\":[57,3,11]}]}");
    CHECK(c6.Equipment.size() == 2 && c6.Equipment[0].first == 51231 && c6.Equipment[0].second == "Hood");
    CHECK(c6.Equipment[1].first == 50402);
    CHECK(c6.Talents.size() == 1 && c6.Talents[0].second == "57 / 3 / 11");

    // Profile page gear: 8 left, 8 right, 3 bottom, filled and empty slots, &amp; in rel, gems/enchant.
    std::string html = "<div id=\"character-profile\"><div class=\"item-left item-slots-col\">";
    auto filled = [](int id, int q, const char* icon, const char* extra) {
        return std::string("<div class=\"item-slot\"><div class=\"icon-quality icon-quality") + std::to_string(q) +
               "\"><a href=\"https://wotlk.evowow.com/?item=" + std::to_string(id) + "\" rel=\"item=" + std::to_string(id) + extra +
               "\"><img src=\"http://cdn.warmane.com/wotlk/icons/large/" + icon + ".jpg\" /></a></div></div>";
    };
    auto empty = [](const char* name) { return std::string("<div class=\"item-slot\"><div class=\"tooltip\" data-tooltip=\"") + name + "\"></div></div>"; };
    html += filled(51231, 4, "inv_helmet_154", "&amp;ench=3820&amp;gems=41285:40133:0");
    html += empty("Neck");
    for (int i = 0; i < 6; i++) html += filled(40000 + i, 3, "inv_misc_test", "");
    html += "</div><div class=\"model\"></div><div class=\"item-right\">";
    for (int i = 0; i < 8; i++) html += filled(41000 + i, i == 7 ? 5 : 4, "inv_misc_right", i == 0 ? "&ench=0" : "");
    html += "</div><div class=\"item-bottom\">";
    html += filled(50731, 4, "inv_staff_101", "&ench=3834");
    html += empty("Off Hand");
    html += filled(50684, 4, "inv_wand_32", "");
    html += "</div></div><div class=\"recent-activity\"><a rel=\"item=99999\"><img src=\"x.jpg\"></a></div>";

    std::vector<GearItem> gear;
    std::string gear_error;
    CHECK(ParseProfileGear(html, &gear, &gear_error));
    CHECK(gear.size() == (size_t)GearSlotCount);
    if (gear.size() == (size_t)GearSlotCount)
    {
        CHECK(gear[0].ItemId == 51231 && gear[0].Quality == 4 && gear[0].Enchanted && gear[0].Gems == 2);
        CHECK(gear[0].IconUrl == "http://cdn.warmane.com/wotlk/icons/large/inv_helmet_154.jpg");
        CHECK(gear[1].ItemId == 0);                        // empty neck
        CHECK(gear[2].ItemId == 40000 && gear[2].Quality == 3 && !gear[2].Enchanted);
        CHECK(gear[8].ItemId == 41000 && !gear[8].Enchanted); // ench=0 is not an enchant
        CHECK(gear[15].ItemId == 41007 && gear[15].Quality == 5);
        CHECK(gear[16].ItemId == 50731 && gear[16].Enchanted);
        CHECK(gear[17].ItemId == 0);                       // empty off hand
        CHECK(gear[18].ItemId == 50684 && gear[18].IconUrl.find("inv_wand_32") != std::string::npos);
    }
    std::vector<GearItem> none;
    CHECK(!ParseProfileGear("<html>no gear here</html>", &none, &gear_error) && !gear_error.empty());

    // Icon decoding through WIC (optional: pass a JPEG/PNG path).
    if (argc > 1)
    {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        std::ifstream f(argv[1], std::ios::binary);
        std::ostringstream ss;
        ss << f.rdbuf();
        std::vector<unsigned char> px;
        int w = 0, h = 0;
        CHECK(DecodeImage(ss.str(), &px, &w, &h));
        CHECK(w > 0 && h > 0 && px.size() == (size_t)w * h * 4);
        printf("decoded %s: %dx%d\n", argv[1], w, h);
        CHECK(!DecodeImage("not an image", &px, &w, &h));
    }

    printf(failures == 0 ? "ALL PASS\n" : "%d FAILURES\n", failures);
    return failures;
}
