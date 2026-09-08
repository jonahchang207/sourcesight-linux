#pragma once
#include <string>

enum Tab {
    PLAYER,
    WORLD,
    AIM,
    TRIGGERBOT,
    MACRO,
    SOUND_ESP,
    SETTINGS
};

struct TabItem
{
    Tab id;
    std::string label;
};

static const TabItem tabs[] =
{
    { Tab::PLAYER,     "Player" },
    { Tab::WORLD,      "World" },
    { Tab::AIM,        "Aim" },
    { Tab::TRIGGERBOT, "Trigger" },
    { Tab::MACRO,      "Macro" },
    { Tab::SOUND_ESP,  "Sound ESP" },
    { Tab::SETTINGS,   "Settings" }
};

class Menu {
public:
    ~Menu() = default;
    Menu(const Menu&) = delete;
    Menu(Menu&&) = delete;
    Menu& operator=(const Menu&) = delete;
    Menu& operator=(Menu&&) = delete;

    static bool Init();
    static void Render();

    static void RenderStartupHelp();

    static ImVec2 GetPos();
    static ImVec2 GetSize();
private:
    Menu() {};

    static Menu& GetInstance()
    {
        static Menu i{};
        return i;
    }

    bool InitImpl();
    void RenderImpl();
    void RenderStartupHelpImpl();

    void SetupStyles();
private:
    bool isSetup = true;

    ImVec2 pos;
    ImVec2 size;

    ImFont* font_heading = nullptr;
};
