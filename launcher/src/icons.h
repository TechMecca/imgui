#pragma once

// Item / portrait icons for the armory page.
// Each URL is downloaded once, kept on disk in %APPDATA%\NorthrendLauncher\icons, decoded with WIC
// on a worker thread, and turned into a texture on the render thread (Pump()).

#include "imgui.h"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

class IconCache
{
public:
    struct Icon
    {
        ImTextureID Texture = nullptr; // null until loaded
        ImVec2      Uv1 = ImVec2(1, 1); // icons are padded to power-of-two textures
        bool        Failed = false;
    };

    IconCache() = default;
    ~IconCache();
    IconCache(const IconCache&) = delete;
    IconCache& operator=(const IconCache&) = delete;

    const Icon* Get(const std::string& url); // queues the download on first use; main thread only
    void        Pump();                      // main thread, once per frame: upload decoded icons
    void        Shutdown();                  // stop the worker and free textures (before the D3D device goes)

private:
    struct Decoded
    {
        std::string                Url;
        std::vector<unsigned char> Pixels; // BGRA
        int                        Width = 0, Height = 0;
    };

    void Worker();

    std::unordered_map<std::string, Icon> m_Icons; // main thread only
    std::mutex                 m_Mutex;
    std::condition_variable    m_Wake;
    std::deque<std::string>    m_Queue;
    std::vector<Decoded>       m_Ready;
    bool                       m_Stop = false;
    std::thread                m_Thread;
};

// Decodes a JPEG/PNG/BMP/GIF file in memory to BGRA (WIC). Exposed for tests.
bool DecodeImage(const std::string& bytes, std::vector<unsigned char>* bgra, int* width, int* height);
