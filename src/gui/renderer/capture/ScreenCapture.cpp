#include "gui/renderer/capture/ScreenCapture.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cctype>
#include <cerrno>
#include <csignal>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
extern char** environ;
#endif

namespace {

std::mutex& RecorderMutex()
{
    static std::mutex m;
    return m;
}

std::atomic<bool>& RecordingFlag()
{
    static std::atomic<bool> flag{ false };
    return flag;
}

#ifndef _WIN32
pid_t& RecorderPid()
{
    static pid_t pid = -1;
    return pid;
}

std::string& RecorderPath()
{
    static std::string path;
    return path;
}

std::string& RecorderBackend()
{
    static std::string backend;
    return backend;
}
#else
HANDLE& RecorderProcess()
{
    static HANDLE h = nullptr;
    return h;
}
HANDLE& RecorderStdin()
{
    static HANDLE h = nullptr;
    return h;
}
std::string& RecorderPathWin()
{
    static std::string path;
    return path;
}
#endif

std::string Timestamp()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    return std::format("{:04}{:02}{:02}_{:02}{:02}{:02}", tm.tm_year + 1900, tm.tm_mon + 1,
        tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

void EnsureParentDir(const std::string& path)
{
    std::error_code ec;
    const auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty())
        std::filesystem::create_directories(parent, ec);
}

std::string OutputDir()
{
    std::string dir = cfg::capture::output_dir;
    if (dir.empty())
        dir = "captures";
    // Drop trailing slashes so path joining stays clean.
    while (dir.size() > 1 && (dir.back() == '/' || dir.back() == '\\'))
        dir.pop_back();
    return dir;
}

bool HasLowerSuffix(const std::string& path, const char* suffix)
{
    const size_t n = std::strlen(suffix);
    if (path.size() < n)
        return false;
    for (size_t i = 0; i < n; ++i) {
        const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(path[path.size() - n + i])));
        const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(suffix[i])));
        if (a != b)
            return false;
    }
    return true;
}

#ifndef _WIN32

bool HasCommand(const std::string& cmd)
{
    if (cmd.empty())
        return false;
    if (cmd.find('/') != std::string::npos)
        return ::access(cmd.c_str(), X_OK) == 0;
    const char* path_env = ::getenv("PATH");
    const std::string paths = path_env ? path_env : "/usr/local/bin:/usr/bin:/bin";
    size_t start = 0;
    while (start <= paths.size()) {
        const size_t end = paths.find(':', start);
        const std::string dir = paths.substr(start, end == std::string::npos ? end : end - start);
        if (!dir.empty() && ::access((dir + "/" + cmd).c_str(), X_OK) == 0)
            return true;
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    return false;
}

// Spawn argv[0..] and wait for completion. Returns exit code, or -1 on error.
int RunBlocking(const std::vector<std::string>& args)
{
    if (args.empty())
        return -1;
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& a : args)
        argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);

    pid_t pid = -1;
    const int rc = ::posix_spawnp(&pid, argv[0], nullptr, nullptr, argv.data(), environ);
    if (rc != 0 || pid < 0) {
        LOGF(WARNING, "[capture] failed to launch '{}': {}", args[0], std::strerror(rc));
        return -1;
    }
    int status = 0;
    while (::waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR)
            return -1;
    }
    return (WIFEXITED(status) ? WEXITSTATUS(status) : -1);
}

bool WritePpm(const std::string& path, int w, int h, const std::vector<unsigned char>& rgb)
{
    if (w <= 0 || h <= 0 || rgb.size() != static_cast<size_t>(w) * static_cast<size_t>(h) * 3)
        return false;
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f.good())
        return false;
    f << "P6\n# SourceSight composited screen capture (overlay+game)\n" << w << " " << h << "\n255\n";
    f.write(reinterpret_cast<const char*>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
    f.close();
    return f.good() || static_cast<bool>(f);
}

// X11 fallback: composited root image -> PPM. glReadPixels would only see
// our own transparent overlay; the root pixmap is what holds game+overlay.
bool CaptureViaX11Root(std::string path)
{
    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        LOGF(WARNING, "[capture] XOpenDisplay failed; no X display for screenshot");
        return false;
    }

    const int screen = DefaultScreen(display);
    const ::Window root = RootWindow(display, screen);

    XWindowAttributes attr{};
    if (!XGetWindowAttributes(display, root, &attr) || attr.width <= 0 || attr.height <= 0) {
        LOGF(WARNING, "[capture] XGetWindowAttributes failed on root window");
        XCloseDisplay(display);
        return false;
    }
    const int w = attr.width;
    const int h = attr.height;

    XImage* img = XGetImage(display, root, 0, 0, static_cast<unsigned int>(w),
        static_cast<unsigned int>(h), AllPlanes, ZPixmap);
    if (!img) {
        LOGF(WARNING, "[capture] XGetImage failed on root window ({}x{})", w, h);
        XCloseDisplay(display);
        return false;
    }

    std::vector<unsigned char> rgb;
    rgb.resize(static_cast<size_t>(w) * static_cast<size_t>(h) * 3);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const unsigned long pixel = XGetPixel(img, x, y);
            const size_t i = (static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)) * 3;
            // XGetPixel already applies the visual's masks/offsets, but mask
            // geometry varies (including exotic visuals), so derive each
            // channel from its mask and scale to 8 bits (best-effort).
            auto extract = [](unsigned long px, unsigned long mask) -> unsigned char {
                if (mask == 0)
                    return 0;
                unsigned shift = 0;
                while (((mask >> shift) & 1U) == 0U)
                    ++shift;
                unsigned bits = 0;
                while (((mask >> (shift + bits)) & 1U) == 1U)
                    ++bits;
                unsigned v = static_cast<unsigned>((px & mask) >> shift);
                if (bits == 0)
                    return 0;
                // Scale to 8 bits.
                v = (v * 255U) / ((bits >= 8) ? 255U : ((1U << bits) - 1U));
                return static_cast<unsigned char>(v);
            };
            rgb[i + 0] = extract(pixel, img->red_mask);
            rgb[i + 1] = extract(pixel, img->green_mask);
            rgb[i + 2] = extract(pixel, img->blue_mask);
        }
    }

    XDestroyImage(img);
    XCloseDisplay(display);

    // We can only write PPM here; a requested .png/.jpg would be misleading.
    if (HasLowerSuffix(path, ".png") || HasLowerSuffix(path, ".jpg") || HasLowerSuffix(path, ".jpeg")) {
        std::string fixed = path;
        const size_t dot = fixed.find_last_of('.');
        if (dot != std::string::npos)
            fixed = fixed.substr(0, dot);
        fixed += ".ppm";
        LOGF(INFO, "[capture] PNG encoder unavailable for X11 path; writing PPM to '{}' (convert: ffmpeg -i {} out.png)", fixed, fixed);
        path = std::move(fixed);
    }

    EnsureParentDir(path);
    if (!WritePpm(path, w, h, rgb)) {
        LOGF(WARNING, "[capture] failed to write screenshot '{}'", path);
        return false;
    }
    LOGF(INFO, "[capture] screenshot (overlay+game, {}x{}) saved to '{}'", w, h, path);
    return true;
}

#endif

} // namespace

std::string ScreenCapture::DefaultScreenshotPath()
{
#ifndef _WIN32
    const char* ext = HasCommand("grim") ? ".png" : ".ppm";
#else
    const char* ext = ".bmp";
#endif
    return OutputDir() + std::format("/shot_{}{}", Timestamp(), ext);
}

std::string ScreenCapture::DefaultRecordingPath()
{
    return OutputDir() + std::format("/rec_{}.mp4", Timestamp());
}

bool ScreenCapture::CaptureScreenshot(const std::string& path)
{
#ifdef _WIN32
    std::string out = path.empty() ? DefaultScreenshotPath() : path;
    EnsureParentDir(out);

    const int w = GetSystemMetrics(SM_CXSCREEN);
    const int h = GetSystemMetrics(SM_CYSCREEN);
    if (w <= 0 || h <= 0)
        return false;

    HDC screen_dc = GetDC(nullptr);
    HDC mem_dc = CreateCompatibleDC(screen_dc);
    HBITMAP bmp = CreateCompatibleBitmap(screen_dc, w, h);
    HGDIOBJ old = SelectObject(mem_dc, bmp);
    // SRCCOPY from the desktop DC captures the composited output, i.e. game
    // below plus our layered overlay on top.
    const BOOL ok = BitBlt(mem_dc, 0, 0, w, h, screen_dc, 0, 0, SRCCOPY | CAPTUREBLT);
    SelectObject(mem_dc, old);

    bool saved = false;
    if (ok) {
        BITMAPINFOHEADER bi{};
        bi.biSize = sizeof(bi);
        bi.biWidth = w;
        bi.biHeight = -h; // top-down
        bi.biPlanes = 1;
        bi.biBitCount = 24;
        bi.biCompression = BI_RGB;
        const size_t stride = (static_cast<size_t>(w) * 3 + 3) & ~size_t{ 3 };
        std::vector<unsigned char> pixels(stride * static_cast<size_t>(h));
        if (GetDIBits(mem_dc, bmp, 0, static_cast<UINT>(h), pixels.data(),
                reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS)) {
            BITMAPFILEHEADER bf{};
            bf.bfType = 0x4D42;
            bf.bfOffBits = sizeof(bf) + sizeof(bi);
            bf.bfSize = bf.bfOffBits + static_cast<DWORD>(pixels.size());
            std::ofstream f(out, std::ios::binary | std::ios::trunc);
            if (f.good()) {
                f.write(reinterpret_cast<const char*>(&bf), sizeof(bf));
                f.write(reinterpret_cast<const char*>(&bi), sizeof(bi));
                f.write(reinterpret_cast<const char*>(pixels.data()),
                    static_cast<std::streamsize>(pixels.size()));
                saved = static_cast<bool>(f);
            }
        }
    }
    DeleteObject(bmp);
    DeleteDC(mem_dc);
    ReleaseDC(nullptr, screen_dc);
    if (saved)
        LOGF(INFO, "[capture] screenshot (overlay+game) saved to '{}'", out);
    else
        LOGF(WARNING, "[capture] screenshot failed");
    return saved;
#else
    std::string out = path.empty() ? DefaultScreenshotPath() : path;
    EnsureParentDir(out);

    // Preferred on Wayland/Hyprland: grim snapshots the composited output
    // (game + overlay), while XGetImage under XWayland would miss native
    // Wayland surfaces.
    if (HasCommand("grim")) {
        if (RunBlocking({ "grim", out }) == 0) {
            std::error_code ec;
            if (std::filesystem::exists(out, ec) && std::filesystem::file_size(out, ec) > 0) {
                LOGF(INFO, "[capture] screenshot (overlay+game) saved to '{}'", out);
                return true;
            }
        }
        LOGF(WARNING, "[capture] 'grim' failed, falling back to X11 root capture");
    }

    return CaptureViaX11Root(out);
#endif
}

bool ScreenCapture::StartRecording(const std::string& path, int fps)
{
#ifdef _WIN32
    std::lock_guard lock(RecorderMutex());
    if (RecordingFlag().load() && RecorderProcess() != nullptr) {
        LOGF(WARNING, "[capture] already recording to '{}'", RecorderPathWin());
        return false;
    }
    std::string out = path.empty() ? DefaultRecordingPath() : path;
    EnsureParentDir(out);
    if (fps < 1)
        fps = 1;
    if (fps > 240)
        fps = 240;

    // Requires ffmpeg on PATH. gdigrab captures the composited desktop.
    const std::string cmd = std::format(
        "ffmpeg -y -f gdigrab -framerate {} -i desktop -c:v libx264 -preset veryfast -pix_fmt yuv420p \"{}\"", fps, out);
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::string cmdline = "cmd.exe /C " + cmd;
    if (!CreateProcessA(nullptr, cmdline.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
            nullptr, nullptr, &si, &pi)) {
        LOGF(WARNING, "[capture] failed to launch ffmpeg for recording");
        return false;
    }
    CloseHandle(pi.hThread);
    RecorderProcess() = pi.hProcess;
    RecorderStdin() = nullptr;
    RecorderPathWin() = out;
    RecordingFlag().store(true);
    LOGF(INFO, "[capture] recording entire screen (overlay+game) to '{}' at {} fps", out, fps);
    return true;
#else
    {
        std::lock_guard lock(RecorderMutex());
        if (RecordingFlag().load()) {
            LOGF(WARNING, "[capture] already recording to '{}'", RecorderPath());
            return false;
        }
    }

    if (fps < 1)
        fps = 1;
    if (fps > 240)
        fps = 240;

    std::string out = path.empty() ? DefaultRecordingPath() : path;
    EnsureParentDir(out);

    std::vector<std::string> args;
    std::string backend;
    if (HasCommand("wf-recorder")) {
        // PipeWire capture: sees the full composited Hyprland output.
        args = { "wf-recorder", "-y", "-f", out };
        backend = "wf-recorder";
    } else if (HasCommand("ffmpeg")) {
        const char* display = ::getenv("DISPLAY");
        const std::string disp = (display && *display) ? display : ":0";
        if (::getenv("WAYLAND_DISPLAY"))
            LOGF(WARNING, "[capture] Wayland session without wf-recorder: x11grab may miss native Wayland surfaces; install wf-recorder for full overlay+game capture");
        args = { "ffmpeg", "-y", "-f", "x11grab", "-framerate", std::to_string(fps),
            "-i", disp, "-c:v", "libx264", "-preset", "veryfast", "-pix_fmt", "yuv420p", out };
        backend = "ffmpeg-x11grab";
    } else {
        LOGF(WARNING, "[capture] no recorder backend found (need 'wf-recorder' or 'ffmpeg')");
        return false;
    }

    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (auto& a : args)
        argv.push_back(a.data());
    argv.push_back(nullptr);

    // New process group so terminal SIGINT aimed at us is not also
    // delivered to the recorder; StopRecording() signals it explicitly.
    posix_spawnattr_t attr;
    ::posix_spawnattr_init(&attr);
    ::posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP);
    ::posix_spawnattr_setpgroup(&attr, 0);
    pid_t pid = -1;
    const int rc = ::posix_spawnp(&pid, argv[0], nullptr, &attr, argv.data(), environ);
    ::posix_spawnattr_destroy(&attr);
    if (rc != 0 || pid < 0) {
        LOGF(WARNING, "[capture] failed to launch '{}': {}", args[0], std::strerror(rc));
        return false;
    }

    {
        std::lock_guard lock(RecorderMutex());
        RecorderPid() = pid;
        RecorderPath() = out;
        RecorderBackend() = backend;
        RecordingFlag().store(true);
    }
    LOGF(INFO, "[capture] recording entire screen (overlay+game) via {} to '{}' at {} fps", backend, out, fps);
    return true;
#endif
}

bool ScreenCapture::RecordEntireScreen(const std::string& path, int fps)
{
    return StartRecording(path, fps);
}

void ScreenCapture::StopRecording()
{
#ifdef _WIN32
    HANDLE proc = nullptr;
    std::string out;
    {
        std::lock_guard lock(RecorderMutex());
        if (!RecordingFlag().load() || RecorderProcess() == nullptr)
            return;
        proc = RecorderProcess();
        out = RecorderPathWin();
    }
    // Graceful 'q' is ideal for MP4 finalization; without a piped stdin the
    // best we can do is close then terminate as a fallback.
    if (WaitForSingleObject(proc, 3000) != WAIT_OBJECT_0)
        TerminateProcess(proc, 0);
    WaitForSingleObject(proc, 5000);
    CloseHandle(proc);
    {
        std::lock_guard lock(RecorderMutex());
        RecorderProcess() = nullptr;
        RecorderStdin() = nullptr;
        RecordingFlag().store(false);
    }
    LOGF(INFO, "[capture] stopped recording '{}'", out);
#else
    pid_t pid = -1;
    std::string out;
    {
        std::lock_guard lock(RecorderMutex());
        if (!RecordingFlag().load() || RecorderPid() < 0)
            return;
        pid = RecorderPid();
        out = RecorderPath();
    }

    // ffmpeg finalizes the MP4 trailer on SIGINT ('q' equivalent).
    ::kill(pid, SIGINT);
    bool exited = false;
    for (int i = 0; i < 50; ++i) {
        int status = 0;
        const pid_t r = ::waitpid(pid, &status, WNOHANG);
        if (r == pid) {
            exited = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!exited) {
        ::kill(pid, SIGTERM);
        for (int i = 0; i < 20 && !exited; ++i) {
            int status = 0;
            const pid_t r = ::waitpid(pid, &status, WNOHANG);
            if (r == pid)
                exited = true;
            else
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    if (!exited) {
        ::kill(pid, SIGKILL);
        int status = 0;
        ::waitpid(pid, &status, 0);
    }

    {
        std::lock_guard lock(RecorderMutex());
        if (RecorderPid() == pid) {
            RecorderPid() = -1;
            RecorderPath().clear();
            RecorderBackend().clear();
            RecordingFlag().store(false);
        }
    }
    LOGF(INFO, "[capture] stopped recording '{}'", out);
#endif
}

bool ScreenCapture::IsRecording()
{
#ifdef _WIN32
    std::lock_guard lock(RecorderMutex());
    if (!RecordingFlag().load() || RecorderProcess() == nullptr)
        return false;
    return WaitForSingleObject(RecorderProcess(), 0) != WAIT_OBJECT_0;
#else
    std::lock_guard lock(RecorderMutex());
    if (!RecordingFlag().load() || RecorderPid() < 0)
        return false;
    // Reap a naturally-exited recorder so we don't report stale state.
    int status = 0;
    const pid_t r = ::waitpid(RecorderPid(), &status, WNOHANG);
    if (r == RecorderPid()) {
        RecorderPid() = -1;
        RecorderPath().clear();
        RecorderBackend().clear();
        RecordingFlag().store(false);
        return false;
    }
    if (::kill(RecorderPid(), 0) != 0) {
        RecorderPid() = -1;
        RecorderPath().clear();
        RecorderBackend().clear();
        RecordingFlag().store(false);
        return false;
    }
    return true;
#endif
}

std::string ScreenCapture::ActiveRecordingPath()
{
#ifdef _WIN32
    std::lock_guard lock(RecorderMutex());
    return RecorderPathWin();
#else
    std::lock_guard lock(RecorderMutex());
    return RecorderPath();
#endif
}
