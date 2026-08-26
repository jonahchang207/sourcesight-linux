/*
    SourceSight
    Copyright (c) 2026 Jonah Chang (https://github.com/jonahchang207)

    All Rights Reserved. See the LICENSE file for full details.
    Unauthorized copying, distribution, or modification is strictly prohibited.
*/

#include <iostream>

#include "updater/Updater.hpp"
#include "core/engine/Engine.hpp"
#include "gui/renderer/Renderer.hpp"

#include <external/exception.hpp>

int main()
{
    c_exception_handler::setup();

    LogHelper::Init();

    LOGF(INFO, "Compiled {}, Welcome to SourceSight Linux!", __TIMESTAMP__);

#ifdef _WIN32
    // Needs to be ran as ADMINISTRATOR
    if (!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS))
        LOGF(WARNING, "Could not set application process priority to HIGH");
#endif

    if (!Updater::Init() || !Updater::Process()) {
        LOGF(FATAL, "Updater failed to run, so the application status could not be verified; continuing is not recommended");
        LOGF(INFO, "Press any key to ignore and continue execution...");
        std::cin.get();
    }

    if (!Engine::Init()) {
        LOGF(FATAL, "Engine failed to initialize, cannot continue execution");
        goto exit;
    }

    if (!Renderer::Init()) {
        LOGF(FATAL, "Renderer failed to initialize, cannot continue execution");
        goto exit;
    }

    LOGF(INFO, "Everything setup and ready, just... make sure you are not in \"Full Screen\"!");

    // Locking
    Renderer::Thread();

exit:
    LOGF(INFO, "That's it, I'm done. Hope you had a great time!");
    LogHelper::Destroy();
#ifdef _WIN32
    std::cin.get();
#endif
}
