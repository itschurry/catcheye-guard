#include <exception>
#include <iostream>

#include "catcheye/utils/logger.hpp"
#include "__APP_NS__/__APP_NS___app.hpp"

int main(int argc, char** argv)
{
    catcheye::initialize_logging("__APP_SLUG__", "log");
    try {
        const int exit_code = catcheye::__APP_NS__::run_app(argc, argv);
        if (const auto log = catcheye::logger()) {
            log->info("__APP_SLUG__ exiting with code {}", exit_code);
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
