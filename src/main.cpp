#include <fmt/format.h>
#include "renderer.h"
int main() {
    try {
        ParticleRenderer app;
        app.init();
        app.run();
        app.cleanup();
    } catch(std::exception e) {
        fmt::println("{}", e.what());
    }
    return 0;
}