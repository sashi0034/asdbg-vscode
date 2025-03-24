#include "angelscript/angelscript/include/angelscript.h"
#include <iostream>

int main() {
    std::cout << ANGELSCRIPT_VERSION_STRING << std::endl;

    asIScriptEngine *engine = asCreateScriptEngine();

    std::cout << ANGELSCRIPT_VERSION << std::endl;

    if (engine) {
        engine->ShutDownAndRelease();
    }

    std::cout << "Hello, World!" << std::endl;

    return 0;
}
