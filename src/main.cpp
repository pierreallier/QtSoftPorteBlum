#include "Application.h"
#include "gui/StartupSplash.h"

int main(int argc, char *argv[]) {
    Application app(argc, argv);
    if (app.arguments().value(1) == "--startup-splash") {
        if (app.arguments().size() != 3) return 2;
        return runStartupSplash(app.arguments().at(2));
    }
    if (!app.initialize()) return 1;
    return app.exec();
}
