#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "asdbg_backend.hpp"

namespace {
asdbg::AsdbgBackend g_asdbg{};

std::string g_filename{"player.as"};
int g_scriptLine{1};
int g_frameCOunt{};

void LineCallback() {
    if (const auto bp = g_asdbg.FindBreakpoint(g_filename, g_scriptLine)) {
        std::cout << "Breakpoint hit: " << g_filename << ", " << g_scriptLine
                  << "\n";

        g_asdbg.TriggerBreakpoint(*bp);
    }
}

void MockScriptEngine() {
    while (g_frameCOunt < 10) {
        g_frameCOunt++;

        g_scriptLine++;

        LineCallback();

        if (g_scriptLine > 20) {
            g_scriptLine = 5;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "Frame: " << g_frameCOunt
                  << ", Script line: " << g_scriptLine << "\n";
    }
}
} // namespace

int main() {
    std::cout << "Mock engine started!\n";

    std::atomic<bool> running{true};
    g_asdbg.Start(running);

    // Run the mock script engine
    MockScriptEngine();

    std::cout << "Mock engine finished!\n";

    // Finish the engine
    running = false;
    g_asdbg.Shutdown();

    return 0;
}
