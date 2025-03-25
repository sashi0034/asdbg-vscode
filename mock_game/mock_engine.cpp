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

int g_frameCount{};

void LineCallback(asIScriptContext *ctx) {
    const char *filename{};
    const auto lineNumber = ctx->GetLineNumber(0, nullptr, &filename);

    std::cout << filename << "\n";

    if (g_previousCommand == asdbg::DebugCommand::StepOver) {
        const auto filepath = g_asdbg.GetAbsolutePath(filename);
        g_previousCommand =
            g_asdbg.TriggerBreakpoint(asdbg::Breakpoint{filepath, lineNumber});
    }

    if (g_previousCommand == asdbg::DebugCommand::StepIn) {
        // FIXME: Implement step in (This is same as step over for now)
        const auto filepath = g_asdbg.GetAbsolutePath(filename);
        g_previousCommand =
            g_asdbg.TriggerBreakpoint(asdbg::Breakpoint{filepath, lineNumber});
    }

    if (const auto bp = g_asdbg.FindBreakpoint(filename, lineNumber)) {
        std::cout << "Breakpoint hit: " << filename << ", " << lineNumber
                  << "\n";

        g_previousCommand = g_asdbg.TriggerBreakpoint(*bp);
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
    g_asdbg.Start(running);

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

    ctx->SetLineCallback(asFUNCTION(LineCallback), nullptr, asCALL_CDECL);

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
