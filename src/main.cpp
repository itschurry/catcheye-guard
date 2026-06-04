#include <exception>
#include <iostream>
#include <string_view>
#include "guard/app.hpp"
#include "catcheye/utils/logger.hpp"

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--help" || arg == "-h") {
            return catcheye::guard::run_app(argc, argv);
        }
    }

    catcheye::initialize_logging("catcheye_guard", "log");
    try {
        const int exit_code = catcheye::guard::run_app(argc, argv);
        if (const auto log = catcheye::logger()) {
            log->info("catcheye-guard exiting with code {}", exit_code);
        }
        catcheye::shutdown_logging();
        return exit_code;
    } catch (const std::exception& exception) {
        if (const auto log = catcheye::logger()) {
            log->error("startup failed: {}", exception.what());
        } else {
            std::cerr << exception.what() << '\n';
        }
        catcheye::shutdown_logging();
        return 1;
    }
}
