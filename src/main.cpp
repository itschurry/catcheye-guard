#include <exception>
#include <iostream>
#include "guard/guard_app.hpp"
#include "catcheye/utils/logger.hpp"

int main(int argc, char** argv) {
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
