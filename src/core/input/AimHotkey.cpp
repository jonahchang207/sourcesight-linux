#include "AimHotkey.hpp"

#include "common.hpp"

#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>

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

constexpr int kBtnSide = 0x114; // BTN_SIDE (MB5 in evdev terms)
constexpr int kBtnExtra = 0x115; // BTN_EXTRA (rear side button on some mice)

// True when the device declares a side button in its key capabilities.
bool HasSideButton(int fd) {
    unsigned long keybits[KEY_MAX / (sizeof(unsigned long) * 8) + 1];
    std::memset(keybits, 0, sizeof(keybits));
    if (::ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybits)), keybits) < 0)
        return false;
    for (const int code : { kBtnSide, kBtnExtra }) {
        const size_t idx = static_cast<size_t>(code) / (sizeof(unsigned long) * 8);
        const unsigned long mask = 1UL << (static_cast<size_t>(code) % (sizeof(unsigned long) * 8));
        if ((keybits[idx] & mask) != 0)
            return true;
    }
    return false;
}

// Monitor an individual evdev fd. Returns true on EOF/error (device removed).
bool DrainEvents(int fd, std::atomic<bool>& toggle) {
    struct input_event event {};
    ssize_t n;
    while ((n = ::read(fd, &event, sizeof(event))) == static_cast<ssize_t>(sizeof(event))) {
        if (event.type == EV_KEY &&
            (event.code == kBtnSide || event.code == kBtnExtra) &&
            event.value == 1)
            toggle.store(true);
    }
    return n == 0; // EOF → device unplugged
}

} // namespace

AimHotkey::~AimHotkey() {
    Sync(false);
}

void AimHotkey::Sync(bool want_running) {
    if (want_running && !running_.load()) {
        running_ = true;
        evdev_thread_ = std::thread(&AimHotkey::EvdevLoop, this);
        x11_thread_ = std::thread(&AimHotkey::X11Loop, this);
    }
    else if (!want_running && running_.load()) {
        running_ = false;
        if (evdev_thread_.joinable())
            evdev_thread_.join();
        if (x11_thread_.joinable())
            x11_thread_.join();
    }
}

bool AimHotkey::ConsumeToggle() {
    return toggle_.exchange(false);
}

void AimHotkey::EvdevLoop() {
    // Rescan periodically so a hardware mouse swap mid-game is picked up.
    std::vector<int> fds;
    bool warned = false;
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
        if (fds.empty() && !warned) {
            warned = true;
            LOGF(WARNING, "[aim][hotkey] evdev MB5 unavailable (no read access — add user to the 'input' group); XInput2 fallback active");
        }
    };
    rescan();

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

        for (size_t i = 0; i < fds.size();) {
            if (!FD_ISSET(fds[i], &set)) {
                ++i;
                continue;
            }
            if (DrainEvents(fds[i], toggle_)) {
                ::close(fds[i]);
                fds.erase(fds.begin() + static_cast<std::ptrdiff_t>(i));
                continue;
            }
            ++i;
        }
    }

    for (const int fd : fds)
        ::close(fd);
}

void AimHotkey::X11Loop() {
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) {
        LOGF(WARNING, "[aim][hotkey] XInput2 MB5 listener unavailable (no display)");
        return;
    }

    int opcode = 0, event_base = 0, error_base = 0;
    if (!XQueryExtension(dpy, "XINPUT", &opcode, &event_base, &error_base)) {
        LOGF(WARNING, "[aim][hotkey] XInput2 extension not available; MB5 via X11 disabled");
        XCloseDisplay(dpy);
        return;
    }

    unsigned char mask[XIMaskLen(XI_RawButtonPress)] = {};
    XISetMask(mask, XI_RawButtonPress);
    XIEventMask evmask{};
    evmask.deviceid = XIAllDevices;
    evmask.mask_len = sizeof(mask);
    evmask.mask = mask;
    XISelectEvents(dpy, DefaultRootWindow(dpy), &evmask, 1);
    XFlush(dpy);

    // Side buttons map to X11 buttons 8/9 (thumb back/forward); 5 covers
    // sideways wheels and some mice that expose MB5 directly there.
    const auto is_side = [](int detail) {
        return detail == 5 || detail == 8 || detail == 9;
    };

    // Poll with a 100 ms timeout so Sync(false) can join this thread without
    // blocking forever on XNextEvent.
    const int conn = ConnectionNumber(dpy);
    while (running_.load()) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(conn, &rfds);
        struct timeval tv { 0, 100000 };
        const int ready = ::select(conn + 1, &rfds, nullptr, nullptr, &tv);
        if (ready <= 0)
            continue;
        while (XPending(dpy)) {
            XEvent ev{};
            XNextEvent(dpy, &ev);
            if (ev.type != GenericEvent || ev.xcookie.extension != opcode)
                continue;
            if (!XGetEventData(dpy, &ev.xcookie))
                continue;
            if (ev.xcookie.evtype == XI_RawButtonPress) {
                const auto* raw = reinterpret_cast<const XIRawEvent*>(ev.xcookie.data);
                if (raw && is_side(raw->detail))
                    toggle_.store(true);
            }
            XFreeEventData(dpy, &ev.xcookie);
        }
    }

    XCloseDisplay(dpy);
}