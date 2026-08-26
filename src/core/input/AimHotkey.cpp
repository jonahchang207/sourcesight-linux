#include "AimHotkey.hpp"

#include "common.hpp"

#include <linux/input.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>

namespace {

constexpr int kBtnSide = 0x114; // BTN_SIDE (MB5)

// True when the device declares BTN_SIDE in its key capabilities.
bool HasSideButton(int fd) {
    unsigned long keybits[KEY_MAX / (sizeof(unsigned long) * 8) + 1];
    std::memset(keybits, 0, sizeof(keybits));
    if (::ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybits)), keybits) < 0)
        return false;
    const size_t idx = static_cast<size_t>(kBtnSide) / (sizeof(unsigned long) * 8);
    const unsigned long mask = 1UL << (static_cast<size_t>(kBtnSide) % (sizeof(unsigned long) * 8));
    return (keybits[idx] & mask) != 0;
}

} // namespace

AimHotkey::~AimHotkey() {
    Sync(false);
}

void AimHotkey::Sync(bool want_running) {
    if (want_running && !running_.load()) {
        running_ = true;
        thread_ = std::thread(&AimHotkey::Loop, this);
    }
    else if (!want_running && running_.load()) {
        running_ = false;
        if (thread_.joinable())
            thread_.join();
    }
}

bool AimHotkey::ConsumeToggle() {
    return toggle_.exchange(false);
}

void AimHotkey::Loop() {
    // Open every input device that exposes a side button, so a hardware mouse
    // change mid-game is picked up on the next poll.
    std::vector<int> fds;
    const auto rescan = [&]() {
        for (const int fd : fds)
            ::close(fd);
        fds.clear();
        for (const auto& entry : std::filesystem::directory_iterator("/dev/input")) {
            const std::string name = entry.path().filename().string();
            if (name.rfind("event", 0) != 0)
                continue;
            const int fd = ::open(entry.path().c_str(), O_RDONLY | O_NONBLOCK);
            if (fd < 0)
                continue;
            if (!HasSideButton(fd)) {
                ::close(fd);
                continue;
            }
            fds.push_back(fd);
        }
    };
    rescan();

    struct input_event event {};
    while (running_.load()) {
        if (fds.empty()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            rescan();
            continue;
        }

        fd_set set;
        FD_ZERO(&set);
        int max_fd = 0;
        for (const int fd : fds) {
            FD_SET(fd, &set);
            max_fd = std::max(max_fd, fd);
        }
        struct timeval timeout { 0, 100000 }; // 100 ms
        const int ready = ::select(max_fd + 1, &set, nullptr, nullptr, &timeout);
        if (ready <= 0)
            continue;

        for (const int fd : fds) {
            if (!FD_ISSET(fd, &set))
                continue;
            ssize_t n;
            while ((n = ::read(fd, &event, sizeof(event))) == static_cast<ssize_t>(sizeof(event))) {
                if (event.type == EV_KEY && event.code == kBtnSide && event.value == 1)
                    toggle_.store(true);
            }
        }
    }

    for (const int fd : fds)
        ::close(fd);
}
