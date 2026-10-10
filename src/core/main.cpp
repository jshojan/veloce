#include "application.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    emu::Application app;

    if (!app.initialize(argc, argv)) {
        std::cerr << "Failed to initialize application" << std::endl;
        return 1;
    }

    try {
        app.run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        app.shutdown();
        return 1;
    }
    app.shutdown();

    return 0;
}
