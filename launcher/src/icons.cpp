#include "icons.h"

#include "net.h"
#include "window.h"

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <wincodec.h>

#include <atomic>
#include <ctype.h>
#include <filesystem>
#include <functional>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

// WIC v1 factory: available from Windows XP SP3 / Vista onwards (the "2" factory needs Windows 8).
static const GUID kClsidWicImagingFactory1 = { 0xcacaf262, 0x9370, 0x4615, { 0xa1, 0x3b, 0x9f, 0x55, 0x39, 0xda, 0x4c, 0x0a } };

bool DecodeImage(const std::string& bytes, std::vector<unsigned char>* bgra, int* width, int* height)
{
    if (bytes.empty())
        return false;
    IWICImagingFactory* factory = nullptr;
    if (FAILED(::CoCreateInstance(kClsidWicImagingFactory1, nullptr, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, (void**)&factory)))
        return false;

    bool ok = false;
    IWICStream* stream = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    if (SUCCEEDED(factory->CreateStream(&stream)) &&
        SUCCEEDED(stream->InitializeFromMemory((BYTE*)bytes.data(), (DWORD)bytes.size())) &&
        SUCCEEDED(factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder)) &&
        SUCCEEDED(decoder->GetFrame(0, &frame)) &&
        SUCCEEDED(factory->CreateFormatConverter(&converter)) &&
        SUCCEEDED(converter->Initialize(frame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
    {
        UINT w = 0, h = 0;
        if (SUCCEEDED(converter->GetSize(&w, &h)) && w > 0 && h > 0 && w <= 1024 && h <= 1024)
        {
            bgra->resize((size_t)w * h * 4);
            if (SUCCEEDED(converter->CopyPixels(nullptr, w * 4, (UINT)bgra->size(), bgra->data())))
            {
                *width = (int)w;
                *height = (int)h;
                ok = true;
            }
        }
    }
    if (converter) converter->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (stream) stream->Release();
    factory->Release();
    return ok;
}

static fs::path IconDirectory()
{
    wchar_t appdata[MAX_PATH] = {};
    if (::GetEnvironmentVariableW(L"APPDATA", appdata, MAX_PATH) == 0)
        return fs::path(L"icons");
    return fs::path(appdata) / L"NorthrendLauncher" / L"icons";
}

// "http://cdn.warmane.com/wotlk/icons/large/inv_helmet_154.jpg" -> "inv_helmet_154.jpg"
static std::string CacheFileName(const std::string& url)
{
    std::string name = url.substr(url.find_last_of('/') + 1);
    name = name.substr(0, name.find_first_of("?#"));
    std::string safe;
    for (char c : name)
        if (isalnum((unsigned char)c) || c == '_' || c == '-' || c == '.')
            safe += c;
    if (safe.empty() || safe[0] == '.')
        safe = "icon_" + std::to_string(std::hash<std::string>()(url)) + ".img";
    return safe;
}

static bool ReadFile(const fs::path& path, std::string* out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    *out = ss.str();
    return !out->empty();
}

IconCache::~IconCache()
{
    Shutdown();
}

const IconCache::Icon* IconCache::Get(const std::string& url)
{
    if (url.empty())
        return nullptr;
    auto it = m_Icons.find(url);
    if (it != m_Icons.end())
        return &it->second;

    Icon& icon = m_Icons[url];
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (m_Stop)
        {
            icon.Failed = true;
            return &icon;
        }
        m_Queue.push_back(url);
        if (!m_Thread.joinable())
            m_Thread = std::thread(&IconCache::Worker, this);
    }
    m_Wake.notify_one();
    return &icon;
}

void IconCache::Pump()
{
    std::vector<Decoded> ready;
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        ready.swap(m_Ready);
    }
    for (Decoded& d : ready)
    {
        Icon& icon = m_Icons[d.Url];
        float u1 = 1.0f, v1 = 1.0f;
        void* texture = d.Pixels.empty() ? nullptr : Window::CreateTexture(d.Pixels.data(), d.Width, d.Height, &u1, &v1);
        if (texture)
        {
            icon.Texture = (ImTextureID)texture;
            icon.Uv1 = ImVec2(u1, v1);
        }
        else
        {
            icon.Failed = true;
        }
    }
}

void IconCache::Shutdown()
{
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Stop = true;
    }
    m_Wake.notify_all();
    if (m_Thread.joinable())
        m_Thread.join();
    for (auto& kv : m_Icons)
        if (kv.second.Texture)
        {
            Window::ReleaseTexture(kv.second.Texture);
            kv.second.Texture = nullptr;
        }
}

void IconCache::Worker()
{
    ::CoInitializeEx(nullptr, COINIT_MULTITHREADED); // WIC
    const fs::path dir = IconDirectory();
    std::error_code ec;
    fs::create_directories(dir, ec);
    const std::atomic<bool> never_cancel{false};

    for (;;)
    {
        std::string url;
        {
            std::unique_lock<std::mutex> lock(m_Mutex);
            m_Wake.wait(lock, [this] { return m_Stop || !m_Queue.empty(); });
            if (m_Stop)
                break;
            url = m_Queue.front();
            m_Queue.pop_front();
        }

        // Disk first, then the network (and remember it for next time).
        Decoded decoded;
        decoded.Url = url;
        const fs::path file = dir / CacheFileName(url);
        std::string bytes, error;
        bool from_disk = ReadFile(file, &bytes);
        if (!from_disk && HttpGet(url, &bytes, &error, &never_cancel))
        {
            std::ofstream out(file, std::ios::binary | std::ios::trunc);
            out.write(bytes.data(), (std::streamsize)bytes.size());
        }
        if (!DecodeImage(bytes, &decoded.Pixels, &decoded.Width, &decoded.Height))
        {
            decoded.Pixels.clear();
            if (from_disk)
                fs::remove(file, ec); // corrupt cache entry; refetch next time
        }

        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Ready.push_back(std::move(decoded));
    }
    ::CoUninitialize();
}
