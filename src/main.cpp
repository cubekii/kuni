#include "AUI/Platform/Entry.h"
#include "App.h"


AUI_ENTRY {
    using namespace std::chrono_literals;
    auto app = _new<App>();
    app->run();

    return 0;
}
