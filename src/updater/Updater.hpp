#pragma once

#include "http/HttpHelper.hpp"

struct Status {
    bool unsafe = false;
    std::string notice;

    int version_current = 0;
    int version_minimum = 0;
};

class Updater {
public:
    ~Updater() = default;
    Updater(const Updater&) = delete;
    Updater(Updater&&) = delete;
    Updater& operator=(const Updater&) = delete;
    Updater& operator=(Updater&&) = delete;

    static bool Init();
    static bool Process();

    static Status GetStatus();
private:
    Updater() {};

    static Updater& GetInstance()
    {
        static Updater i{};
        return i;
    }

    bool InitImpl();
    bool ProcessImpl();
private:
    Status status;
    bool isSetup = false;
    int current_version = 115;
    std::string status_url = "https://raw.githubusercontent.com/jonahchang207/sourcesight-linux/main/.github/status.json";
};
