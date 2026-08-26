#include "KernelMouse.hpp"

#include "common.hpp"
#include "config/Current.hpp"

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <cstdlib>

namespace {

constexpr const char* kDevicePath = "/dev/person-mouse";

// _IOW(0xAA, 1, struct pm_size) — struct pm_size { int width; int height; }
constexpr unsigned long kPMIocSetSize = 0x4008AA01UL;

struct pm_size {
    int width;
    int height;
};

} // namespace

KernelMouse::~KernelMouse() {
    Close();
}

bool KernelMouse::Open(int width, int height) {
    Close();

    // Refuse to follow symlinks: the device node is a fixed kernel cdev.
    int flags = O_RDWR | O_CLOEXEC;
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif

    const int fd = ::open(kDevicePath, flags);
    if (fd < 0)
        return false;

    struct stat st {};
    if (::fstat(fd, &st) != 0 || !S_ISCHR(st.st_mode)) {
        ::close(fd);
        return false;
    }

    const pm_size size{ width, height };
    if (::ioctl(fd, kPMIocSetSize, &size) != 0) {
        ::close(fd);
        return false;
    }

    fd_ = fd;
    return true;
}

void KernelMouse::Close() {
    std::lock_guard<std::mutex> lock(write_mutex_);
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

bool KernelMouse::Available() const {
    return fd_ >= 0;
}

bool KernelMouse::Move(int dx, int dy) {
    if (fd_ < 0)
        return false;

    // Bypass: randomised delay between writes to avoid timing detection.
    if (cfg::bypass::timing_jitter) {
        const int min_us = std::max(0, cfg::bypass::write_delay_min_us);
        const int max_us = std::max(min_us, cfg::bypass::write_delay_max_us);
        const int delay = min_us + (rand() % (max_us - min_us + 1));
        ::usleep(delay);
    }

    char payload[64];
    const int length = std::snprintf(payload, sizeof(payload), "%d %d\n", dx, dy);
    if (length <= 0 || static_cast<size_t>(length) >= sizeof(payload))
        return false;

    std::lock_guard<std::mutex> lock(write_mutex_);
    if (fd_ < 0)
        return false;
    return ::write(fd_, payload, static_cast<size_t>(length)) == length;
}

bool KernelMouse::LeftClick() {
    if (fd_ < 0)
        return false;

    constexpr const char* kClick = "click\n";
    std::lock_guard<std::mutex> lock(write_mutex_);
    if (fd_ < 0)
        return false;
    return ::write(fd_, kClick, sizeof(kClick) - 1) == sizeof(kClick) - 1;
}

bool KernelMouse::MoveTo(float x, float y, float deadzone) {
    const auto current = CursorPosition();
    if (!current)
        return false;

    const int target_x = static_cast<int>(std::lround(x));
    const int target_y = static_cast<int>(std::lround(y));
    int dx = target_x - current->first;
    int dy = target_y - current->second;

    const int threshold = std::max(0, static_cast<int>(std::lround(deadzone)));
    if (std::abs(dx) <= threshold)
        dx = 0;
    if (std::abs(dy) <= threshold)
        dy = 0;
    if (dx == 0 && dy == 0)
        return true;
    return Move(dx, dy);
}

std::optional<std::pair<int, int>> KernelMouse::CursorPosition() {
    // Hyprland exposes the cursor as "x, y" (or as JSON with -j). Query the
    // JSON form first, then fall back to the plain text form.
    for (const char* command : { "hyprctl cursorpos -j", "hyprctl cursorpos" }) {
        FILE* pipe = ::popen(command, "r");
        if (!pipe)
            continue;

        std::string text;
        char buffer[256];
        size_t n;
        while ((n = ::fread(buffer, 1, sizeof(buffer), pipe)) > 0)
            text.append(buffer, n);
        const int result = ::pclose(pipe);
        if (result != 0 || text.empty())
            continue;

        // Strip surrounding whitespace.
        const auto first = text.find_first_not_of(" \t\r\n");
        const auto last = text.find_last_not_of(" \t\r\n");
        if (first == std::string::npos)
            continue;
        text = text.substr(first, last - first + 1);

        try {
            const auto parsed = nlohmann::json::parse(text);
            if (parsed.is_object() && parsed.contains("x") && parsed.contains("y")) {
                return std::make_pair(parsed["x"].get<int>(), parsed["y"].get<int>());
            }
        }
        catch (const std::exception&) {
            // Not JSON — Hyprland printed "x, y" instead.
        }

        int x = 0;
        int y = 0;
        char trailing = '\0';
        if (std::sscanf(text.c_str(), "%d , %d %c", &x, &y, &trailing) == 2)
            return std::make_pair(x, y);
    }
    return std::nullopt;
}
