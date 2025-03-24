#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

#include "asdbg_backend.hpp"

int main() {
    std::cout << "Mock engine started.\n";

    std::atomic<bool> running{true};
    asdbg::AsdbgBackend asdbgBackend{running};

    std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    // Finish the engine
    running = false;

    return 0;
}
