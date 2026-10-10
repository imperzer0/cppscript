#include <iostream>
#include <unistd.h>

#include "config.h"
#include "return_codes.h"
#include "lib.h"
#include "Log.hpp"

void print_version()
{
    // Print version and description
    std::cerr << APPNAME << " v" << APP_VERSION << "   " << DESCRIPTION << std::endl;
}

void print_help(const char* apppath)
{
    print_version();
    // Print help message
    std::cerr
        << "Usage: " << apppath << " <script> {<arguments>}" << std::endl
        << std::endl
        << "   Arguments are optional." << std::endl
        << std::endl;
}

int main(int argc, char* argv[], char* envp[])
{
    if (argc < 2)
    {
        // Try running cling if it's installed.
        if (is_available_in_path("cling"))
            execvpe("cling", std::vector{const_cast<char*>("cling")}.data(), envp);

        print_help(argv[0]);
        return 1;
    }

    // Print help on --help or -h
    if (argc == 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")))
    {
        print_help(argv[0]);
        return 0;
    }

    // Print version on --version or -v
    if (argc == 2 && (!strcmp(argv[1], "--version") || !strcmp(argv[1], "-v")))
    {
        print_version();
        return 0;
    }

    Log::Set_LogLevel(MainConfig::Instance().get_log_level());


    int envp_size = 0;
    for (; envp[envp_size] != nullptr; ++envp_size) { }

    std::string output = compile(argv[1], {envp, envp + envp_size}); // Compile the script

    // Run the binary
    run(output, {argv + 1, argv + argc}, {envp, envp + envp_size});

    // Perform automatic cache cleaning
    cache_autoclean(output);

    return ERROR_OK; // Exit Successfully
}
