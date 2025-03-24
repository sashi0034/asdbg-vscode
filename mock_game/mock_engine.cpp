#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "asdbg_backend.hpp"

namespace {
asdbg::AsdbgBackend g_asdbg{};
asdbg::DebugCommand g_previousCommand{};

std::string g_filename{"player.as"}; // This is not loaded, just a mock
int g_scriptLine{1};
int g_frameCount{};

void LineCallback() {
    if (g_previousCommand == asdbg::DebugCommand::StepOver) {
        const auto filepath = g_asdbg.GetAbsolutePath(g_filename);
        g_previousCommand = g_asdbg.TriggerBreakpoint(
            asdbg::Breakpoint{filepath, g_scriptLine});
    }

    if (g_previousCommand == asdbg::DebugCommand::StepIn) {
        // FIXME: Implement step in (This is same as step over for now)
        const auto filepath = g_asdbg.GetAbsolutePath(g_filename);
        g_previousCommand = g_asdbg.TriggerBreakpoint(
            asdbg::Breakpoint{filepath, g_scriptLine});
    }

    if (const auto bp = g_asdbg.FindBreakpoint(g_filename, g_scriptLine)) {
        std::cout << "Breakpoint hit: " << g_filename << ", " << g_scriptLine
                  << "\n";

        g_previousCommand = g_asdbg.TriggerBreakpoint(*bp);
    }
}

void MockScriptEngine() {
    while (g_frameCount < 10 * 10) {
        g_frameCount++;

        g_scriptLine++;

        LineCallback();

        if (g_scriptLine > 20) {
            g_scriptLine = 5;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::cout << "Frame: " << g_frameCount
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
