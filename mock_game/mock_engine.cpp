#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "asdbg_backend.hpp"

#include "angelscript/angelscript/include/angelscript.h"

#include "angelscript/add_on/scriptarray/scriptarray.h"
#include "angelscript/add_on/scriptbuilder/scriptbuilder.h"
#include "angelscript/add_on/scriptdictionary/scriptdictionary.h"
#include "angelscript/add_on/scriptstdstring/scriptstdstring.h"

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

void MockScriptLoop() {

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

void ScriptSleep(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void ScriptPrintln(const std::string &msg) { std::cout << msg << std::endl; }

} // namespace

int main() {
    std::cout << "Mock engine started!\n"
              << "AngelScript version: " << ANGELSCRIPT_VERSION_STRING
              << std::endl;

    asIScriptEngine *engine = asCreateScriptEngine();

    RegisterStdString(engine);
    RegisterScriptArray(engine, true);
    RegisterScriptDictionary(engine);

    engine->RegisterGlobalFunction("void println(const string& in message)",
                                   asFUNCTION(ScriptPrintln), asCALL_CDECL);
    engine->RegisterGlobalFunction("void sleep(int ms)",
                                   asFUNCTION(ScriptSleep), asCALL_CDECL);

    // -----------------------------------------------

    std::atomic<bool> running{true};
    // g_asdbg.Start(running); // TODO

    // -----------------------------------------------

    CScriptBuilder builder{};
    builder.StartNewModule(engine, "lazy");
    builder.AddSectionFromFile("lazy.as");
    if (builder.BuildModule() != asSUCCESS) {
        std::cerr << "Failed to build the script module!\n";
        return 1;
    }

    asIScriptFunction *scriptMain =
        builder.GetModule()->GetFunctionByDecl("void main()");
    asIScriptContext *ctx = engine->CreateContext();

    ctx->Prepare(scriptMain);
    ctx->Execute();
    ctx->Release();

    // -----------------------------------------------

    std::cout << "Mock engine finished!\n";

    // Finish the engine
    running = false;
    g_asdbg.Shutdown();

    if (engine) {
        engine->ShutDownAndRelease();
    }

    return 0;
}
